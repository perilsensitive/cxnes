/*
  cxNES - NES/Famicom Emulator
  Copyright (C) 2011-2015 Ryan Jackson

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation.; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License along
  with this program; if not, write to the Free Software Foundation, Inc.,
  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include <SDL.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#if __unix__
#include <gdk/gdkx.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <X11/Xlib.h>
#elif _WIN32
#define WIN32_LEAN_AND_MEAN
#include <gdk/gdkwin32.h>
#endif

#include "emu.h"
#include "video.h"

struct gdk_keycode_map {
	guint hardware_keycode;
	SDL_Keycode keycode;
};

struct gdk_keycode_map_bucket {
	struct gdk_keycode_map *entries;
	int num_entries;
};

enum {
	NUM_BUCKETS = 16,
};

static struct gdk_keycode_map_bucket keycode_map[NUM_BUCKETS];

static int event_mask = (GDK_ALL_EVENTS_MASK & ~(GDK_POINTER_MOTION_HINT_MASK));

#if _WIN32
static int ignore_focus_events = 2;
#endif

static GdkCursor *blank_cursor;
static GdkCursor *crosshair_cursor;
static GdkDevice *mouse;
static GtkWidget *gtkwindow;
static GtkWidget *drawingarea;
static GtkWidget *menubar;
static GdkPixbuf *icon;
extern int running;
extern int fullscreen;
extern int cursor_visible;
extern struct emu *emu;
static int menubar_visible;

static int drawingarea_x, drawingarea_y;
static int drawingarea_height, drawingarea_width;

#if __unix__
static Window window_handle;
static Display *xdisplay;
#elif _WIN32
static HWND window_handle;
#endif

#if __unix__
static GLint glxattributes[] = {
  GLX_RGBA,
  GLX_RED_SIZE, 3,
  GLX_GREEN_SIZE, 3,
  GLX_BLUE_SIZE, 2,
  GLX_DOUBLEBUFFER,
  GLX_DEPTH_SIZE, 16,
  None
};
#endif

static GdkRGBA bg = {0, 0, 0, 255};

static SDL_Keycode keysym_map_nonprinting[] = {
	SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,
	SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,
	SDLK_BACKSPACE, SDLK_TAB, SDLK_UNKNOWN, SDLK_CLEAR,
	SDLK_UNKNOWN, SDLK_RETURN, SDLK_UNKNOWN, SDLK_UNKNOWN,

	SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_PAUSE,
	SDLK_SCROLLLOCK, SDLK_SYSREQ, SDLK_UNKNOWN, SDLK_UNKNOWN,
	SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_ESCAPE,
	SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,

	SDLK_APPLICATION, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,
	SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,
	SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,
	SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,

	SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,
	SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,
	SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,
	SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,

	SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,
	SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,
	SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,
	SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,

	SDLK_HOME, SDLK_LEFT, SDLK_UP, SDLK_RIGHT,
	SDLK_DOWN, SDLK_PAGEUP, SDLK_PAGEDOWN, SDLK_END,
	SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,
	SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,

	SDLK_SELECT, SDLK_PRINTSCREEN, SDLK_EXECUTE, SDLK_INSERT,
	SDLK_UNKNOWN, SDLK_UNDO, SDLK_AGAIN, SDLK_MENU,
	SDLK_FIND, SDLK_STOP, SDLK_HELP, SDLK_UNKNOWN,
	SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,

	SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,
	SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,
	SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,
	SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_MODE, SDLK_NUMLOCKCLEAR,

	SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,
	SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,
	SDLK_UNKNOWN, SDLK_KP_TAB, SDLK_UNKNOWN, SDLK_UNKNOWN,
	SDLK_UNKNOWN, SDLK_KP_ENTER, SDLK_UNKNOWN, SDLK_UNKNOWN,

	SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,
	SDLK_UNKNOWN, SDLK_KP_7, SDLK_KP_4, SDLK_KP_8,
	SDLK_KP_6, SDLK_KP_2, SDLK_KP_9, SDLK_KP_3,
	SDLK_KP_1, SDLK_KP_5, SDLK_KP_0, SDLK_KP_PERIOD,

	SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,
	SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,
	SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_KP_MULTIPLY, SDLK_KP_PLUS,
	SDLK_KP_COMMA, SDLK_KP_MINUS, SDLK_KP_PERIOD, SDLK_KP_DIVIDE,

	SDLK_KP_0, SDLK_KP_1, SDLK_KP_2, SDLK_KP_3,
	SDLK_KP_4, SDLK_KP_5, SDLK_KP_6, SDLK_KP_7,
	SDLK_KP_8, SDLK_KP_9, SDLK_UNKNOWN, SDLK_UNKNOWN,
	SDLK_UNKNOWN, SDLK_KP_EQUALS, SDLK_F1, SDLK_F2,

	SDLK_F3, SDLK_F4, SDLK_F5, SDLK_F6,
	SDLK_F7, SDLK_F8, SDLK_F9, SDLK_F10,
	SDLK_F11, SDLK_F12, SDLK_F13, SDLK_F14,
	SDLK_F15, SDLK_F16, SDLK_F17, SDLK_F18,

	SDLK_F19, SDLK_F20, SDLK_F21, SDLK_F22,
	SDLK_F23, SDLK_F24, SDLK_UNKNOWN, SDLK_UNKNOWN,
	SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,
	SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,

	SDLK_UNKNOWN, SDLK_LSHIFT, SDLK_RSHIFT, SDLK_LCTRL,
	SDLK_RCTRL, SDLK_CAPSLOCK, SDLK_UNKNOWN, SDLK_LGUI,
	SDLK_RGUI, SDLK_LALT, SDLK_RALT, SDLK_LGUI,
	SDLK_RGUI, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,

	SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,
	SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,
	SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,
	SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_DELETE,
};

static SDL_Keycode keysym_map_media[] = {
	SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_BRIGHTNESSUP, SDLK_BRIGHTNESSDOWN,
	SDLK_KBDILLUMTOGGLE, SDLK_KBDILLUMUP, SDLK_KBDILLUMDOWN, SDLK_UNKNOWN,
	SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,
	SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,

	SDLK_UNKNOWN, SDLK_VOLUMEDOWN, SDLK_MUTE, SDLK_VOLUMEUP,
	SDLK_AUDIOPLAY, SDLK_AUDIOSTOP, SDLK_AUDIOPREV, SDLK_AUDIONEXT,
	SDLK_AC_HOME, SDLK_MAIL, SDLK_UNKNOWN, SDLK_AC_SEARCH,
	SDLK_UNKNOWN, SDLK_CALCULATOR, SDLK_UNKNOWN, SDLK_UNKNOWN,

	SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,
	SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_AC_BACK, SDLK_AC_FORWARD,
	SDLK_AC_STOP, SDLK_AC_REFRESH, SDLK_UNKNOWN, SDLK_UNKNOWN,
	SDLK_EJECT, SDLK_UNKNOWN, SDLK_WWW, SDLK_SLEEP,

	SDLK_AC_BOOKMARKS, SDLK_UNKNOWN, SDLK_MEDIASELECT, SDLK_COMPUTER,
	SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,
	SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,
	SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,

	SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,
	SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,
	SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,
	SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,

	SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,
	SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_COPY,
	SDLK_CUT, SDLK_DISPLAYSWITCH, SDLK_UNKNOWN, SDLK_UNKNOWN,
	SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,

	SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,
	SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,
	SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,
	SDLK_UNKNOWN, SDLK_PASTE, SDLK_UNKNOWN, SDLK_UNKNOWN,
};

extern void input_poll_events();
extern void process_events(void);
extern GtkWidget *gui_build_menubar(GtkWidget *gtkwindow);

extern void gui_volume_control_dialog(GtkWidget *, gpointer);
extern void gui_cheat_dialog(GtkWidget *, gpointer);
extern void gui_video_configuration_dialog(GtkWidget *, gpointer);
extern void gui_binding_configuration_dialog(GtkWidget *, gpointer);
extern void gui_audio_configuration_dialog(GtkWidget *, gpointer);
extern void gui_path_configuration_dialog(GtkWidget *, gpointer);
extern void gui_misc_configuration_dialog(GtkWidget *, gpointer);
extern void gui_rom_configuration_dialog(GtkWidget *, gpointer);

extern int open_rom(struct emu *emu, char *filename, int patch_count, char **patchfiles);
extern int video_save_screenshot(const char *filename);

static SDL_Keycode keycode_map_insert(guint hardware_keycode)
{
	SDL_Keycode keycode;
	GdkKeymap *keymap;
	GdkKeymapKey *keys;
	guint *keyvals;
	gint n_entries;
	gboolean found;
	guint keyval;
	int i, hash_value, new_num_entries;
	struct gdk_keycode_map *tmp;

	keycode = SDLK_UNKNOWN;
	keymap = gdk_keymap_get_default();
	if (!keymap)
		return SDLK_UNKNOWN;

	found = gdk_keymap_get_entries_for_keycode(keymap,
						   hardware_keycode,
						   &keys,
						   &keyvals,
						   &n_entries);

	if (!found)
		return SDLK_UNKNOWN;

	for (i = 0; i < n_entries; i++) {
		if (!keys[i].group && !keys[i].level)
			break;
	}

	if (i == n_entries)
		goto cleanup;

	keyval = keyvals[i];

	/* Find the SDL keycode for the unmodified keyval */
	if ((keyval & 0xffffff00) == 0x0000ff00) {
		keycode = keysym_map_nonprinting[keyval & 0xff];
	} else if ((keyval & 0xffffff00) == 0x1008ff00) {
		int count = sizeof(keysym_map_media) /
			sizeof(keysym_map_media[0]);

		if ((keyval & 0xff) < count)
			keycode = keysym_map_media[keyval & 0xff];
		else
			keycode = SDLK_UNKNOWN;
	} else {
		keycode = gdk_keyval_to_unicode(keyval);
	}

	hash_value = hardware_keycode % NUM_BUCKETS;
	new_num_entries = keycode_map[hash_value].num_entries + 1;
	tmp = realloc(keycode_map[hash_value].entries,
		      new_num_entries * sizeof(*tmp));
	if (!tmp)
		goto cleanup;

	keycode_map[hash_value].entries = tmp;
	keycode_map[hash_value].num_entries = new_num_entries;

	tmp += new_num_entries - 1;
	tmp->hardware_keycode = hardware_keycode;
	tmp->keycode = keycode;

cleanup:
	g_free(keys);
	g_free(keyvals);

	return keycode;
}

static SDL_Keycode keycode_map_lookup(guint hardware_keycode)
{
	struct gdk_keycode_map_bucket *bucket;
	SDL_Keycode keycode;
	int i;

	bucket = &keycode_map[hardware_keycode % NUM_BUCKETS];

	for (i = 0; i < bucket->num_entries; i++) {
		if (bucket->entries[i].hardware_keycode == hardware_keycode)
			break;
	}

	if (i == bucket->num_entries)
		keycode = keycode_map_insert(hardware_keycode);
	else
		keycode = bucket->entries[i].keycode;

	/* printf("mapped hardware keycode %x to SDL_Keycode %x\n", */
	/*        hardware_keycode, keycode); */

	return keycode;
}

static void keycode_map_init(void)
{
	int i;

	for (i = 0; i < NUM_BUCKETS; i++) {
		keycode_map[i].entries = NULL;
		keycode_map[i].num_entries = 0;
	}
}

static void keycode_map_cleanup(void)
{
	int i;

	for (i = 0; i < NUM_BUCKETS; i++) {
		if (keycode_map[i].entries)
			free(keycode_map[i].entries);
	}
}

void gui_resize(int fs, int show_menubar)
{
	int width, height;
	gint menubar_height;
	
	if (!drawingarea)
		return;

	if (fs < 0) {
		fs = !fullscreen;
		show_menubar = !fs;
	}

	fullscreen = fs;
	
	if (fullscreen) {
		GdkRectangle geometry;
		GdkScreen *screen;
		GdkWindow *gdkwindow;
		int monitor;

		gdkwindow = gtk_widget_get_window(gtkwindow);
		screen = gdk_window_get_screen(gdkwindow);
		monitor = gdk_screen_get_monitor_at_window(screen, gdkwindow);
		gdk_screen_get_monitor_geometry(screen, monitor, &geometry);
		gtk_window_fullscreen(GTK_WINDOW(gtkwindow));
		width = geometry.width;
		height = geometry.height;
	} else {
		video_get_windowed_size(&width, &height);
		gtk_window_unfullscreen(GTK_WINDOW(gtkwindow));
	}

	if (show_menubar)
		gtk_widget_show(menubar);
	else
		gtk_widget_hide(menubar);

	if (show_menubar) {
		gtk_widget_get_preferred_height(menubar, NULL, &menubar_height);
		if (fullscreen)
			height -= menubar_height;
	}

	menubar_visible = show_menubar;

	gtk_widget_set_size_request(drawingarea, width, height);
}

static gboolean motion_event_callback(GtkWidget *widget, GdkEventMotion *motion,
				      gpointer data)
{
	int state;
	int x, y;

	x = motion->x - drawingarea_x;
	y = motion->y - drawingarea_y;

	if ((x < 0 || x >= drawingarea_width) ||
	    (y < 0 || y >= drawingarea_height)) {
		return FALSE;
	}

	state = (motion->state >> 8) & 0x1f;

	video_mouse_motion(x, y, state);

	return FALSE;
}

static gboolean button_event_callback(GtkWidget *widget, GdkEventButton *button,
				      gpointer data)
{
	int state;
	int x, y;

	x = button->x - drawingarea_x;
	y = button->y - drawingarea_y;

	if ((x < 0 || x >= drawingarea_width) ||
	    (y < 0 || y >= drawingarea_height)) {
		return FALSE;
	}

	state = (button->type == GDK_BUTTON_PRESS);

	video_mouse_button(button->button, state, x, y);

	return FALSE;
}

void gui_toggle_menubar(void)
{
	gui_resize(fullscreen, !menubar_visible);
}

int convert_key_event(GdkEventKey *event, SDL_Event *sdlevent)
{
	SDL_Keycode sdlkeycode;

	/* Drop this event on the floor if there is no assigned
	   keyval set or it's VoidSymbol. */
	if (!event->keyval || (event->keyval == GDK_KEY_VoidSymbol))
		return 0;

	sdlkeycode = keycode_map_lookup(event->hardware_keycode);
	if (sdlkeycode == SDLK_UNKNOWN)
		return 0;

	sdlevent->key.repeat = 0;
	sdlevent->key.windowID = 0;
	sdlevent->key.keysym.scancode = 0;
	sdlevent->key.keysym.mod = 0;

	switch (event->type) {
	case GDK_KEY_PRESS:
		sdlevent->type = SDL_KEYDOWN;
		sdlevent->key.state = SDL_PRESSED;
		break;

	case GDK_KEY_RELEASE:
		sdlevent->type = SDL_KEYUP;
		sdlevent->key.state = SDL_RELEASED;
		break;

	default:
		return 0;
		break;
	}

	sdlevent->key.keysym.sym = sdlkeycode;
	return 1;
}

/* Try to map GDK key events to SDL events, then push those into the SDL event queue.
   The synthetic events dont have meaningful values for modifier state or scancode, so
   code should not depend on either field.  Additionally, the keyboard state isn't
   updated either.  None of the emulator code depends on these because it keeps track
   of pressed/released and modifier state itself.
*/
static int keyevent_callback(GtkWidget *widget, GdkEventKey *event, gpointer user_data) {
	SDL_Event sdlevent;
	if (convert_key_event(event, &sdlevent))
		SDL_PushEvent(&sdlevent);

	return FALSE;
}

static int area_realize(GtkWidget *widget, void *data)
{
	GtkWidget *window = (GtkWidget *) data;
	GtkWidget *area = (GtkWidget *) g_object_get_data (G_OBJECT (window), "area");
	GdkWindow *gdkwindow;

	gdkwindow = gtk_widget_get_window(area);
	gdk_window_ensure_native(gdkwindow);
#if GTK_MINOR_VERSION >= 12
	gdk_window_set_event_compression(gdkwindow, FALSE);
#endif

#if __unix__
	window_handle = gdk_x11_window_get_xid(gdkwindow);
#elif _WIN32
	window_handle = gdk_win32_window_get_handle(gdkwindow);
#endif


	return FALSE;
}

void fullscreen_callback(void)
{
	gui_resize(!fullscreen, (!fullscreen ? 0 : 1));
	gtk_widget_override_background_color(drawingarea, GTK_STATE_FLAG_NORMAL, &bg);
}

void quit_callback(void)
{
	SDL_Event event;

	running = 0;

	event.type = SDL_QUIT;
	SDL_PushEvent(&event);
}

/* Glue GDK focus change event to SDL focus change event */
static void focus_change_callback(GtkWidget *widget, GdkEventFocus *event, gpointer data)
{
#if _WIN32
	if (ignore_focus_events) {
		ignore_focus_events--;
		return;
	}
#endif

	if (event->type != GDK_FOCUS_CHANGE)
		return;

	video_focus(event->in);
}

static void window_show_callback(GtkWidget *widget, gpointer data)
{
	gui_resize(fullscreen, fullscreen ? 0 : 1);
}

static gboolean draw_callback(GtkWidget *widget, void *cairo_context, gpointer data)
{
	if (emu_loaded(emu) && !emu_paused(emu))
		return TRUE;

	video_redraw();

	return TRUE;
}

/* Glue window state changes to SDL window state change */
static void window_state_callback(GtkWidget *widget, GdkEventWindowState *event,
				  gpointer data)
{
	if (event->changed_mask & GDK_WINDOW_STATE_WITHDRAWN) {
		if (event->new_window_state & GDK_WINDOW_STATE_WITHDRAWN) {
			video_hidden();
		}
	}

	if (event->changed_mask & GDK_WINDOW_STATE_ICONIFIED) {
		if (event->new_window_state & GDK_WINDOW_STATE_ICONIFIED) {
			video_minimized();
		} else if (!(event->new_window_state &
			     GDK_WINDOW_STATE_MAXIMIZED)) {
			video_restored();
		}
	}

	if (event->changed_mask & GDK_WINDOW_STATE_MAXIMIZED) {
		if (event->new_window_state & GDK_WINDOW_STATE_MAXIMIZED) {
			video_maximized();
		} else if (!(event->new_window_state &
			     GDK_WINDOW_STATE_ICONIFIED)) {
			video_restored();
		}
	}
}

/* Glue the resize event for the drawing area to SDL_SetWindowSize() */
static void size_allocate_cb(GtkWidget *widget, GdkRectangle *allocation, gpointer data)
{
	video_resize(allocation->width, allocation->height);
	drawingarea_x = allocation->x;
	drawingarea_y = allocation->y;
	drawingarea_height = allocation->height;
	drawingarea_width = allocation->width;
	video_clear();
}

static gboolean configure_callback(GtkWidget *widget, GdkEventConfigure *event, gpointer data)
{
//	gtk_widget_override_background_color(drawingarea, GTK_STATE_FLAG_NORMAL, &bg);
	return FALSE;
}

char *file_dialog(GtkWidget *parent,
		  const char *title,
		  GtkFileChooserAction action,
		  const char *ok_text,
		  const char *default_path,
		  const char *suggested_name,
		  const char *filter_name,
		  char **filter_patterns,
		  char **shortcuts)
{
	GtkWidget *dialog;
	GtkFileFilter *filter;
	GFile *folder;
	char *file;
	int i;
	int rc;

	if (!ok_text)
		ok_text = "_OK";

	video_show_cursor(1);

	dialog = gtk_file_chooser_dialog_new(title,
					     GTK_WINDOW(parent),
					     action,
					     "_Cancel",
					     GTK_RESPONSE_CANCEL,
					     ok_text,
					     GTK_RESPONSE_ACCEPT,
					     NULL);

	if (filter_name && filter_patterns &&
	    filter_patterns[0]) {
		filter = gtk_file_filter_new();
		gtk_file_filter_set_name(filter, filter_name);

		for (i = 0; filter_patterns[i]; i++) {
			gtk_file_filter_add_pattern(filter,
						    filter_patterns[i]);
		}

		gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog),
					    filter);
	}

	filter = gtk_file_filter_new();
	gtk_file_filter_set_name(filter, "All Files");
	gtk_file_filter_add_pattern(filter, "*");
	gtk_file_filter_add_pattern(filter, "*.*");
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog),
				    filter);

	if (default_path) {
		GFileType type;

		folder = g_file_new_for_path(default_path);
		type = g_file_query_file_type(folder, G_FILE_QUERY_INFO_NONE,
					      NULL);

		if (type == G_FILE_TYPE_DIRECTORY) {
			gtk_file_chooser_set_current_folder_file(
				GTK_FILE_CHOOSER(dialog),
				folder,
				NULL);
		} else {
			gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dialog),
						      default_path);
		}

		g_object_unref(folder);
	}

	if (suggested_name && (action == GTK_FILE_CHOOSER_ACTION_SAVE)) {
		gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog),
						  suggested_name);
	}

	for (i = 0; shortcuts && shortcuts[i]; i++) {
		gtk_file_chooser_add_shortcut_folder(GTK_FILE_CHOOSER(dialog),
			shortcuts[i], NULL);
	}

	file = NULL;
	rc = gtk_dialog_run(GTK_DIALOG(dialog));
	if (rc == GTK_RESPONSE_ACCEPT) {
		char *tmp;

		tmp = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
		if (tmp)
			file = g_strdup(tmp);
	}

	gtk_widget_destroy(dialog);

	video_show_cursor(0);

	return file;
}

void gui_cleanup(void)
{
	keycode_map_cleanup();

	if (blank_cursor)
		g_object_unref(G_OBJECT(blank_cursor));

	if (crosshair_cursor)
		g_object_unref(G_OBJECT(crosshair_cursor));
}

static gboolean f10_accel_hack(GtkAccelGroup *group, GObject *acceleratable,
		    guint keyval, GdkModifierType modifier)
{
	return TRUE;
}

/* In GTK, F10 activates the menu.  Rather than make the user figure out
   the workaround for this, just set up an accelerator for F10 here that
   does nothing.  The F10 key press/release event is already caught and
   handled like all other key events, so all we need to do is prevent
   GTK from activating the menu.
*/
static void f10_accelerator_fix(void)
{
	GtkAccelGroup *group;
	GClosure *f10_closure;

	group = gtk_accel_group_new();
	f10_closure = g_cclosure_new_swap(G_CALLBACK(f10_accel_hack), NULL,
					  NULL);
	gtk_accel_group_connect(group, GDK_KEY_F10, 0, 0, f10_closure);
	gtk_window_add_accel_group(GTK_WINDOW(gtkwindow), group);
}

void *gui_init(int argc, char **argv)
{
	GdkDisplay *gdk_display;
	GdkDeviceManager *device_manager;
	GtkWidget *box;
	int window_w, window_h;
#if __unix__
	GdkScreen *screen;
	int xscreen;
	XVisualInfo *vinfo = NULL;
	GdkVisual *gdkvisual;
#endif

	gtk_init(&argc, &argv);
	keycode_map_init();

	icon = gdk_pixbuf_new_from_file_at_size(PACKAGE_DATADIR "/icons/cxnes.png",
						128, 128, NULL);

	blank_cursor = gdk_cursor_new(GDK_BLANK_CURSOR);
	crosshair_cursor = gdk_cursor_new(GDK_CROSSHAIR);

	gtkwindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);

	gtk_window_set_title(GTK_WINDOW(gtkwindow), "cxNES");
	gtk_window_set_resizable(GTK_WINDOW(gtkwindow), FALSE);
	gtk_window_set_icon(GTK_WINDOW(gtkwindow), icon);

	box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_add(GTK_CONTAINER(gtkwindow), box);
	gtk_container_set_border_width(GTK_CONTAINER(box), 0);

	/* File menu */
	menubar = gui_build_menubar(gtkwindow);
	gtk_box_pack_start(GTK_BOX(box), menubar, FALSE, FALSE, 0);

	/* Now the drawing area */
	drawingarea = gtk_drawing_area_new();
	video_get_windowed_size(&window_w, &window_h);
	gtk_widget_set_size_request(drawingarea, window_w, window_h);

#if __unix__
	screen = gdk_screen_get_default();
	xdisplay = gdk_x11_get_default_xdisplay();
	xscreen = DefaultScreen(xdisplay);
	vinfo = glXChooseVisual(xdisplay, xscreen, glxattributes);

	if (!vinfo)
		return NULL;

	gdkvisual = gdk_x11_screen_lookup_visual(screen, vinfo->visualid);
	XFree(vinfo);

	if (!gdkvisual)
		return NULL;

	gtk_widget_set_visual(drawingarea, gdkvisual);
#endif

	g_object_set_data(G_OBJECT(gtkwindow), "area", drawingarea);

	gtk_box_pack_start(GTK_BOX(box), drawingarea, TRUE, TRUE, 0);

	/* Using a GTK DrawingArea as the window results in
	   no key or mouse events making it through to the
	   application when running on Linux, so we need
	   to generate synthetic events from the GTK ones.
	   On Windows, key events (at least) seem to actually
	   make it through, resulting in two copies of each
	   event being processed.  Tell SDL to ignore these
	   events so that only the synthetic ones appear in
	   the event queue.
	*/
	SDL_EventState(SDL_KEYDOWN, SDL_IGNORE);
	SDL_EventState(SDL_KEYUP, SDL_IGNORE);
	SDL_EventState(SDL_MOUSEBUTTONDOWN, SDL_IGNORE);
	SDL_EventState(SDL_MOUSEBUTTONUP, SDL_IGNORE);
	SDL_EventState(SDL_MOUSEMOTION, SDL_IGNORE);
	SDL_EventState(SDL_MOUSEWHEEL, SDL_IGNORE);
	//SDL_EventState(SDL_WINDOWEVENT, SDL_IGNORE);
	//SDL_EventState(SDL_SYSWMEVENT, SDL_IGNORE);

	/* Key events */
	g_signal_connect(G_OBJECT(gtkwindow), "key_press_event",
			 G_CALLBACK(keyevent_callback), NULL);
	g_signal_connect(G_OBJECT(gtkwindow), "key_release_event",
			 G_CALLBACK(keyevent_callback), NULL);

	/* Handle resize events */
	g_signal_connect(G_OBJECT(gtkwindow), "configure-event",
			 G_CALLBACK(configure_callback), NULL);
	g_signal_connect(G_OBJECT(drawingarea), "size-allocate",
			 G_CALLBACK(size_allocate_cb), NULL);

	g_signal_connect(G_OBJECT(drawingarea), "draw",
			 G_CALLBACK(draw_callback), NULL);

	/* Quit */
	g_signal_connect(G_OBJECT(gtkwindow), "delete_event",
			 G_CALLBACK(quit_callback), NULL);

	/* About */
	/* Focus events */
	g_signal_connect(G_OBJECT(gtkwindow), "focus-in-event",
			 G_CALLBACK(focus_change_callback), NULL);
	g_signal_connect(G_OBJECT(gtkwindow), "focus-out-event",
			 G_CALLBACK(focus_change_callback), NULL);

	/* Button events */
	g_signal_connect(G_OBJECT(gtkwindow), "button-press-event",
			 G_CALLBACK(button_event_callback), NULL);
	g_signal_connect(G_OBJECT(gtkwindow), "button-release-event",
			 G_CALLBACK(button_event_callback), NULL);

	/* Motion events */
	g_signal_connect(G_OBJECT(gtkwindow), "motion-notify-event",
			 G_CALLBACK(motion_event_callback), NULL);

	/* This gets the native window handle to pass back to SDL */
	g_signal_connect(drawingarea, "realize",
			 G_CALLBACK(area_realize), gtkwindow);

	g_signal_connect(gtkwindow, "show",
			 G_CALLBACK(window_show_callback), NULL);

	g_signal_connect(gtkwindow, "window-state-event",
			 G_CALLBACK(window_state_callback), NULL);

	//gtk_widget_set_events(gtkwindow, event_mask);
	gtk_widget_set_events(drawingarea, event_mask);
	gtk_widget_override_background_color(drawingarea, GTK_STATE_FLAG_NORMAL, &bg);
	gtk_widget_show_all(gtkwindow);
#if GTK_MINOR_VERSION >= 12
	gdk_window_set_event_compression(gtk_widget_get_window(gtkwindow), FALSE);
#endif
	
	gdk_display = gdk_display_get_default();
	device_manager = gdk_display_get_device_manager(gdk_display);
	mouse = gdk_device_manager_get_client_pointer(device_manager);

	f10_accelerator_fix();

	return (void *)window_handle;
}

void gui_set_window_title(const char *title)
{
	gtk_window_set_title(GTK_WINDOW(gtkwindow), title);
}

void gui_grab(int grab)
{
	if (grab) {
		video_show_cursor(0);
		gdk_device_grab(mouse, gtk_widget_get_window(gtkwindow),
				GDK_OWNERSHIP_WINDOW, FALSE, event_mask,
				NULL, GDK_CURRENT_TIME);
	} else {
		gdk_device_ungrab(mouse, GDK_CURRENT_TIME);
//		video_show_cursor(1);
	}
}

void gui_show_cursor(int show)
{
	GdkWindow *gdkwindow;
	gdkwindow = gtk_widget_get_window(drawingarea);

	if (show) {
		gdk_window_set_cursor(gdkwindow, NULL);
	} else {
		gdk_window_set_cursor(gdkwindow, blank_cursor);
	}
}

void gui_show_crosshairs(int show)
{
	GdkWindow *gdkwindow;
	gdkwindow = gtk_widget_get_window(drawingarea);

	if (show) {
		gdk_window_set_cursor(gdkwindow, crosshair_cursor);
	} else if (!cursor_visible) {
		gdk_window_set_cursor(gdkwindow, blank_cursor);
	} else {
		gdk_window_set_cursor(gdkwindow, NULL);
	}
}


void gui_error(const char *message)
{
	GtkWidget *dialog;
	int paused;

	paused = emu_paused(emu);

	if (!paused) {
		emu_pause(emu, 1);
	}

	video_show_cursor(1);

	dialog = gtk_message_dialog_new(GTK_WINDOW(gtkwindow),
					GTK_DIALOG_DESTROY_WITH_PARENT,
					GTK_MESSAGE_ERROR,
					GTK_BUTTONS_CLOSE, "%s",
				        message);

	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);

	if (!paused) {
		emu_pause(emu, 0);
	}

	video_show_cursor(0);
}

static gboolean gui_process_sdl_events(gpointer user_data)
{
	struct emu *emu;

	emu = user_data;
	process_events();
	input_poll_events();
	input_process_queue(1);
	if (emu_loaded(emu) && !emu_paused(emu)) {
		return FALSE;
	}

	return TRUE;
}

void gui_enable_event_timer(void)
{
	g_timeout_add(100, gui_process_sdl_events, emu);
}
