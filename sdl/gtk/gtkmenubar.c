/*
  cxNES - NES/Famicom Emulator
  Copyright (C) 2011-2016 Ryan Jackson

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

#include <gtk/gtk.h>

#include "emu.h"
#include "input.h"
#include "actions.h"
#include "video.h"
#include "gtkconfig.h"
#include "license.h"
#include "text_buffer.h"

extern void quit_callback(void);
extern void fullscreen_callback(void);
extern int save_screenshot(void);

extern int running;
extern int fullscreen;
extern struct emu *emu;

static void remember_input_devices_callback(GtkRadioMenuItem *widget,
					    gpointer user_data);
static void input_port_connect_callback(GtkWidget *widget, gpointer user_data);
static GtkWidget *gui_build_input_menu(void);
static GtkWidget *gui_build_system_type_menu(const int mask);
extern void gui_volume_control_dialog(GtkWidget *, gpointer);
extern void gui_cheat_dialog(GtkWidget *, gpointer);
extern void gui_video_configuration_dialog(GtkWidget *, gpointer);
extern void gui_palette_configuration_dialog(GtkWidget *, gpointer);
extern void gui_binding_configuration_dialog(GtkWidget *, gpointer);
extern void gui_audio_configuration_dialog(GtkWidget *, gpointer);
extern void gui_path_configuration_dialog(GtkWidget *, gpointer);
extern void gui_misc_configuration_dialog(GtkWidget *, gpointer);
extern void gui_rom_configuration_dialog(GtkWidget *, gpointer);
extern void gui_joystick_dialog(GtkWidget *, gpointer);

extern int open_rom(struct emu *emu, char *filename, int patch_count, char **patchfiles);
extern int close_rom(struct emu *emu);

static void file_quit_callback(GtkWidget *widget, gpointer userdata)
{
	quit_callback();
}

static void emulator_fullscreen_callback(GtkWidget *widget, gpointer userdata)
{
	fullscreen_callback();
}

static void screenshot_callback(GtkWidget *widget, gpointer userdata)
{
	save_screenshot();
}

static void pause_resume_callback(GtkWidget *widget, gpointer userdata)
{
	int paused = emu_paused(emu);

	emu_pause(emu, !paused);
}

static void insert_coin_callback(GtkWidget *widget, gpointer userdata)
{
	struct emu_action *event;

	event = input_lookup_emu_action(ACTION_VS_COIN_SWITCH_1);
	if (event && event->handler) {
		event->handler(event->data, 1, event->id);
	}
}

static void eject_disk_callback(GtkWidget *widget, gpointer userdata)
{
	struct emu_action *event;

	event = input_lookup_emu_action(ACTION_FDS_EJECT);
	if (event && event->handler) {
		event->handler(event->data, 1, event->id);
	}
}

static void switch_disk_callback(GtkWidget *widget, gpointer userdata)
{
	struct emu_action *event;

	event = input_lookup_emu_action(ACTION_FDS_SELECT);
	if (event && event->handler) {
		event->handler(event->data, 1, event->id);
	}
}

static void hard_reset_callback(GtkWidget *widget, gpointer userdata)
{
	if (emu->loaded)
		emu_reset(emu, 1);
}

static void soft_reset_callback(GtkWidget *widget, gpointer userdata)
{
	if (emu->loaded)
		emu_reset(emu, 0);
}

static void rom_info_callback(GtkWidget *widget, gpointer userdata)
{
	GtkWidget *dialog;
	GtkWidget *gtkwindow;
	GtkDialogFlags flags;
	GtkWidget *view;
	GtkWidget *dialog_box;
	GtkWidget *scrolled_window;
	/* GtkWidget *label; */
	GtkTextBuffer *buffer;
	struct text_buffer *tbuffer;
	PangoFontDescription *pfd;
	int paused;

	video_show_cursor(1);
	gtkwindow = (GtkWidget *)userdata;
	scrolled_window = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_min_content_width(GTK_SCROLLED_WINDOW(scrolled_window), 800);
	gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(scrolled_window), 300);
	flags = GTK_DIALOG_DESTROY_WITH_PARENT;
	dialog = gtk_dialog_new_with_buttons("ROM Info",
					     GTK_WINDOW(gtkwindow), flags,
					     "_OK", GTK_RESPONSE_ACCEPT,
					     NULL);

	dialog_box = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
	gtk_container_set_border_width(GTK_CONTAINER(dialog_box), 8);
	gtk_window_set_transient_for(GTK_WINDOW(dialog),
				     GTK_WINDOW(gtkwindow));

	/* label = gtk_label_new("ROM Info"); */
	/* gtk_widget_set_halign(label, GTK_ALIGN_START); */
	view = gtk_text_view_new();
	gtk_widget_set_size_request(view, 300, 200);
	gtk_text_view_set_editable(GTK_TEXT_VIEW(view), FALSE);
	gtk_text_view_set_left_margin(GTK_TEXT_VIEW(view), 8);
	gtk_text_view_set_right_margin(GTK_TEXT_VIEW(view), 8);
	pfd = pango_font_description_from_string("monospace");
	gtk_widget_override_font(GTK_WIDGET(view), pfd);
	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(view));
	tbuffer = text_buffer_new();
	rom_get_info(emu->rom, tbuffer);
	gtk_text_buffer_set_text(buffer, text_buffer_get_text(tbuffer), -1);
	text_buffer_free(tbuffer);
	/* gtk_box_pack_start(GTK_BOX(dialog_box), label, FALSE, FALSE, 8); */
	gtk_box_pack_start(GTK_BOX(dialog_box), scrolled_window, FALSE, FALSE, 8);
	gtk_container_add(GTK_CONTAINER(scrolled_window), view);

	paused = emu_paused(emu);
	emu_pause(emu, 1);
	gtk_widget_show_all(dialog);

	gtk_dialog_run(GTK_DIALOG(dialog));

	if (!paused && emu_loaded(emu))
		emu_pause(emu, 0);

	pango_font_description_free(pfd);
	gtk_widget_destroy(dialog);
	video_show_cursor(0);
}

static void about_callback(GtkWidget *widget, gpointer userdata) {
	GtkWidget *aboutdialog;
	GdkPixbuf *logo;
	GtkWindow *gtkwindow;

	logo = gdk_pixbuf_new_from_file_at_size(PACKAGE_DATADIR "/icons/cxnes.svg",
						128, 128, NULL);

	gtkwindow = userdata;
	aboutdialog = gtk_about_dialog_new();

	gtk_about_dialog_set_license(GTK_ABOUT_DIALOG(aboutdialog), license);
	gtk_about_dialog_set_program_name(GTK_ABOUT_DIALOG(aboutdialog), "cxNES");
	gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(aboutdialog), PACKAGE_VERSION);
	gtk_about_dialog_set_comments(GTK_ABOUT_DIALOG(aboutdialog), "NES/Famicom Emulator");
	gtk_about_dialog_set_website(GTK_ABOUT_DIALOG(aboutdialog), "https://perilsensitive.github.io/cxnes");
	gtk_about_dialog_set_copyright(GTK_ABOUT_DIALOG(aboutdialog), "(c) 2015 Ryan Jackson");
	if (logo)
		gtk_about_dialog_set_logo(GTK_ABOUT_DIALOG(aboutdialog), logo);

	gtk_window_set_transient_for(GTK_WINDOW(aboutdialog),
				     GTK_WINDOW(gtkwindow));

	gtk_dialog_run(GTK_DIALOG(aboutdialog));
	gtk_widget_destroy(aboutdialog);
}

static void apply_patch_callback(GtkWidget *widget, gpointer user_data)
{
	int paused;
	char *file;
	char *filter_patterns[] = { "*.[iubIUB][pP][sS]", NULL };
	char *shortcuts[] = { NULL, NULL };
	char *patch_path;
	GtkWidget *gtkwindow = user_data;

	patch_path = config_get_path(emu->config, CONFIG_DATA_DIR_PATCH,
					 NULL, 1);
	shortcuts[0] = patch_path;

	paused = emu_paused(emu);

	if (!paused)
		emu_pause(emu, 1);

	file = file_dialog(gtkwindow,
			   "Select Patch File",
			   GTK_FILE_CHOOSER_ACTION_OPEN,
			   "_Apply",
			   patch_path,
			   NULL,
			   "Patch files",
			   filter_patterns,
			   shortcuts);

	if (patch_path)
		free(patch_path);

	if (file) {
		emu_patch_rom(emu, file);
		g_free(file);
	}

	if (!paused)
		emu_pause(emu, 0);
}

static void file_open(GtkWidget *widget, gpointer user_data)
{
	char *file;
	int paused;
	char *filter_patterns[] = { "*.[nN][eE][sS]",
				    "*.[uU][nN][iI][fF]",
				    "*.[uU][nN][fF]",
				    "*.[fF][dD][sS]",
				    "*.[nN][sS][fF]",
				    "*.[zZ][iI][pP]",
				    NULL };
	char *shortcuts[] = { (char *)emu->config->rom_path, NULL };
	GtkWidget *gtkwindow = user_data;

	paused = emu_paused(emu);

	if (!paused)
		emu_pause(emu, 1);

	file = file_dialog(gtkwindow,
			   "Select ROM",
			   GTK_FILE_CHOOSER_ACTION_OPEN,
			   "_Open",
			   emu->config->default_to_rom_path ?
			   emu->config->rom_path : NULL,
			   NULL,
			   "NES ROMs",
			   filter_patterns,
			   shortcuts);

	if (file) {
		open_rom(emu, file, 0, NULL);
		g_free(file);
	}
			   

	if (!paused)
		emu_pause(emu, 0);
}

static void load_state(GtkWidget *widget, gpointer user_data)
{
	char *file;
	int paused;
	char *filter_patterns[] = { "*.[sS][tT][aA]",
				    "*.[sS][tT][0-9]",
				    NULL };
	char *shortcuts[] = { NULL, NULL };
	char *state_path;
	GtkWidget *gtkwindow = user_data;

	paused = emu_paused(emu);

	if (!paused)
		emu_pause(emu, 1);

	state_path = config_get_path(emu->config, CONFIG_DATA_DIR_STATE,
					 NULL, 1);
	shortcuts[0] = state_path;

	file = file_dialog(gtkwindow,
			   "Select Savestate",
			   GTK_FILE_CHOOSER_ACTION_OPEN,
			   "_Load",
			   state_path,
			   NULL,
			   "Savestates",
			   filter_patterns,
			   shortcuts);

	free(state_path);

	if (!paused)
		emu_pause(emu, 0);

	if (file) {
		emu_load_state(emu, file);
		free(file);
	}

	if (!paused)
		emu_pause(emu, 0);
}

static void save_state(GtkWidget *widget, gpointer user_data)
{
	char *file;
	int paused;
	char *filter_patterns[] = { "*.[sS][tT][aA]",
				    "*.[sS][tT][0-9]",
				    NULL };
	char *shortcuts[] = { NULL, NULL };
	const char *default_state_file;
	char *state_path;
	GtkWidget *gtkwindow = user_data;

	paused = emu_paused(emu);

	if (!paused)
		emu_pause(emu, 1);

	default_state_file = emu->state_file;
	state_path = config_get_path(emu->config, CONFIG_DATA_DIR_STATE,
					 NULL, 1);
	shortcuts[0] = state_path;

	file = file_dialog(gtkwindow,
			   "Select Savestate",
			   GTK_FILE_CHOOSER_ACTION_SAVE,
			   "_Save",
			   state_path,
			   default_state_file,
			   "Savestates",
			   filter_patterns,
			   shortcuts);

	free(state_path);

	if (!paused)
		emu_pause(emu, 0);

	if (file) {
		emu_save_state(emu, file);
		free(file);
	}

	if (!paused)
		emu_pause(emu, 0);
}

static void file_close(GtkWidget *widget, gpointer userdata)
{
	close_rom(emu);
	video_clear();
}

void menu_shown_callback(GtkWidget *widget, gpointer user_data)
{
	GtkWidget *item;
	int (*is_visible)(void);
	int (*is_sensitive)(void);

	item = user_data;
	is_visible = g_object_get_data(G_OBJECT(item), "is_visible");
	is_sensitive = g_object_get_data(G_OBJECT(item), "is_sensitive");

	if (is_sensitive)
		gtk_widget_set_sensitive(item, is_sensitive());

	if (is_visible) {
		if (is_visible())
			gtk_widget_show(item);
		else
			gtk_widget_hide(item);
	}
}

struct input_device {
	const char *mnemonic;
	int id;
};

struct input_device port1_input_devices[] = {
	{ "Auto", IO_DEVICE_AUTO },
	{ "None", IO_DEVICE_NONE },
	{ "Controller _1", IO_DEVICE_CONTROLLER_1 },
	{ "Controller _2", IO_DEVICE_CONTROLLER_2 },
	{ "Controller _3", IO_DEVICE_CONTROLLER_3 },
	{ "Controller _4", IO_DEVICE_CONTROLLER_4 },
	{ "_Zapper", IO_DEVICE_ZAPPER_1, },
	{ "_Power Pad Side A", IO_DEVICE_POWERPAD_A1, },
	{ "_Power Pad Side B", IO_DEVICE_POWERPAD_B1, },
	{ "_Arkanoid Controller (NES)", IO_DEVICE_ARKANOID_NES_1 },
	{ "_SNES Mouse", IO_DEVICE_SNES_MOUSE_1 },
	{ NULL },
};

struct input_device port2_input_devices[] = {
	{ "Auto", IO_DEVICE_AUTO },
	{ "None", IO_DEVICE_NONE },
	{ "Controller _1", IO_DEVICE_CONTROLLER_1 },
	{ "Controller _2", IO_DEVICE_CONTROLLER_2 },
	{ "Controller _3", IO_DEVICE_CONTROLLER_3 },
	{ "Controller _4", IO_DEVICE_CONTROLLER_4 },
	{ "_Zapper", IO_DEVICE_ZAPPER_2 },
	{ "_Power Pad Side A", IO_DEVICE_POWERPAD_A2, },
	{ "_Power Pad Side B", IO_DEVICE_POWERPAD_B2, },
	{ "_Arkanoid Controller (NES)", IO_DEVICE_ARKANOID_NES_2 },
	{ "_SNES Mouse", IO_DEVICE_SNES_MOUSE_2 },
	{ NULL },
};

struct input_device port3_input_devices[] = {
	{ "Auto", IO_DEVICE_AUTO },
	{ "None", IO_DEVICE_NONE },
	{ "Controller _1", IO_DEVICE_CONTROLLER_1 },
	{ "Controller _2", IO_DEVICE_CONTROLLER_2 },
	{ "Controller _3", IO_DEVICE_CONTROLLER_3 },
	{ "Controller _4", IO_DEVICE_CONTROLLER_4 },
	{ "_SNES Mouse", IO_DEVICE_SNES_MOUSE_3 },
	{ NULL },
};

struct input_device port4_input_devices[] = {
	{ "Auto", IO_DEVICE_AUTO },
	{ "None", IO_DEVICE_NONE },
	{ "Controller _1", IO_DEVICE_CONTROLLER_1 },
	{ "Controller _2", IO_DEVICE_CONTROLLER_2 },
	{ "Controller _3", IO_DEVICE_CONTROLLER_3 },
	{ "Controller _4", IO_DEVICE_CONTROLLER_4 },
	{ "_SNES Mouse", IO_DEVICE_SNES_MOUSE_4 },
	{ NULL },
};

struct input_device expansion_port_input_devices[] = {
	{ "Auto", IO_DEVICE_AUTO },
	{ "None", IO_DEVICE_NONE },
	{ "Family BASIC _Keyboard", IO_DEVICE_KEYBOARD },
	{ "_Subor Keyboard", IO_DEVICE_SUBOR_KEYBOARD },
	{ "_Family Trainer Side A", IO_DEVICE_FTRAINER_A },
	{ "F_amily Trainer Side B", IO_DEVICE_FTRAINER_B },
	{ "_Arkanoid Controller (Famicom)", IO_DEVICE_ARKANOID_FC },
	{ "A_rkanoid II Controller", IO_DEVICE_ARKANOID_II },
	{ "_VS. Zapper", IO_DEVICE_VS_ZAPPER },
	{ NULL },
};

struct input_device *input_devices[5] = {
	port1_input_devices,
	port2_input_devices,
	port3_input_devices,
	port4_input_devices,
	expansion_port_input_devices,
};

static const char *input_menu_names[5] = { "Port _1", "Port _2",
					   "Port _3", "Port _4",
					   "_Expansion Port" };

static void select_input_callback(GtkRadioMenuItem *widget,
				      gpointer user_data)
{
	int port;
	int id;
	int connected;

	if (!gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget)))
		return;

	port = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widget),
						 "port"));
	id = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widget), "id"));

	connected = io_device_connected(emu->io, port);
	if (connected)
		io_device_connect(emu->io, port, 0);

	io_device_select(emu->io, port, id);

	if (connected)
		io_device_connect(emu->io, port, 1);
}

static void fourplayer_mode_callback(GtkRadioMenuItem *widget,
				      gpointer user_data)
{
	int mode;

	if (!gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget)))
		return;

	mode = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widget),
						 "mode"));

	io_set_four_player_mode(emu->io, mode, 0);
}

static void port_menu_show_callback(GtkWidget *widget, gpointer user_data)
{
	GtkRadioMenuItem *item;
	GSList *group;
	int port;
	int id, selected_id;
	GtkWidget *connected;
	char buffer[80];

	group = user_data;

	if (!group)
		return;

	while (group) {
		item = group->data;
		group = group->next;

		port = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item),
								"port"));
		id = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item), "id"));

		if ((id != IO_DEVICE_AUTO) && \
		    !io_find_device(emu->io, port, id)) {
			gtk_widget_hide(GTK_WIDGET(item));
			continue;
		} else {
			if (id == IO_DEVICE_AUTO) {
				struct io_device *device;

				device = io_find_auto_device(emu->io, port);
				snprintf(buffer, sizeof(buffer), "_Auto [%s]",
					 device ? device->name : "Unconnected");
				gtk_menu_item_set_label(GTK_MENU_ITEM(item),
							buffer);
								   
			}
			gtk_widget_show(GTK_WIDGET(item));
		}

		selected_id = io_get_selected_device_id(emu->io, port);

		if (id != selected_id)
			continue;

		g_signal_handlers_block_by_func(G_OBJECT(item),
						G_CALLBACK(select_input_callback),
						NULL);

		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), TRUE);

		g_signal_handlers_unblock_by_func(G_OBJECT(item),
						  G_CALLBACK(select_input_callback),
						  NULL);
	}

	connected = g_object_get_data(G_OBJECT(widget), "connected");

	g_signal_handlers_block_by_func(G_OBJECT(connected),
					G_CALLBACK(input_port_connect_callback),
					GINT_TO_POINTER(port));

	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(connected),
				       io_device_connected(emu->io, port));

	g_signal_handlers_unblock_by_func(G_OBJECT(connected),
					  G_CALLBACK(input_port_connect_callback),
					  GINT_TO_POINTER(port));
}

static void input_menu_item_show_callback(GtkWidget *widget, gpointer user_data)
{
	int port;

	if (!emu->loaded || !emu_system_is_vs(emu))
		return;
	
	port = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widget), "port"));

	if (port < 1 || port == 3 || port == 4)
		gtk_widget_hide(widget);
	else
		gtk_widget_show(widget);
}

static void input_menu_show_callback(GtkWidget *widget, gpointer user_data)
{

	GtkWidget *item;

	item = g_object_get_data(G_OBJECT(widget), "remember");


	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item),
				       emu->config->remember_input_devices);

	gtk_container_foreach(GTK_CONTAINER(widget),
			      input_menu_item_show_callback, NULL);

}

static void fourplayer_menu_show_callback(GtkWidget *widget, gpointer user_data)
{
	GtkRadioMenuItem *item;
	GSList *group;
	int mode, current_mode, auto_mode;
	char buffer[80];
	const gchar *label;
	GtkRadioMenuItem *auto_item;

	group = user_data;

	current_mode = io_get_four_player_mode(emu->io);
	auto_mode = io_get_auto_four_player_mode(emu->io);

	label = NULL;
	auto_item = NULL;

	while (group) {
		item = group->data;
		group = group->next;

		mode = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item),
								"mode"));

		if (mode == auto_mode)
			label = gtk_menu_item_get_label(GTK_MENU_ITEM(item));
		else if (mode == FOUR_PLAYER_MODE_AUTO)
			auto_item = item;

		if (current_mode != mode)
			continue;

		g_signal_handlers_block_by_func(G_OBJECT(item),
						G_CALLBACK(fourplayer_mode_callback),
						NULL);

		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), TRUE);

		g_signal_handlers_unblock_by_func(G_OBJECT(item),
						  G_CALLBACK(fourplayer_mode_callback),
						  NULL);
	}

	if (auto_item && label) {
		snprintf(buffer, sizeof(buffer), "Auto [%s]", label);
		gtk_menu_item_set_label(GTK_MENU_ITEM(auto_item), buffer);
	}
}

static void input_port_connect_callback(GtkWidget *widget, gpointer user_data)
{
	int active;
	int port = GPOINTER_TO_INT(user_data);

	active = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget));
	io_device_connect(emu->io, port, active);
}

static GtkWidget *gui_build_input_port_menu(int port)
{
	GtkWidget *menu;
	GSList *group;
	GtkWidget *item;
	int i;

	menu = gtk_menu_new();
	group = NULL;

	item = gtk_check_menu_item_new_with_mnemonic("_Connected");
	g_object_set_data(G_OBJECT(menu), "connected", item);
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), TRUE);
	g_signal_connect(G_OBJECT(item), "toggled",
			 G_CALLBACK(input_port_connect_callback), GINT_TO_POINTER(port));
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu),
			      gtk_separator_menu_item_new());

	item = gtk_radio_menu_item_new_with_mnemonic(group,
						     "_Auto");
	group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(item));
	g_object_set_data(G_OBJECT(item), "port", GINT_TO_POINTER(port));
	g_object_set_data(G_OBJECT(item), "id", GINT_TO_POINTER(IO_DEVICE_AUTO));
	g_signal_connect(G_OBJECT(item), "activate",
			 G_CALLBACK(select_input_callback), NULL);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);


	/* Auto is first in the list, but we did it by hand above */
	for (i = 1; input_devices[port][i].mnemonic; i++) {
		struct input_device *device;

		device = &input_devices[port][i];

		item = gtk_radio_menu_item_new_with_mnemonic(group,
							     device->mnemonic);
		group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(item));

		g_object_set_data(G_OBJECT(item), "port", GINT_TO_POINTER(port));
		g_object_set_data(G_OBJECT(item), "id", GINT_TO_POINTER(device->id));
		g_signal_connect(G_OBJECT(item), "activate",
				 G_CALLBACK(select_input_callback), NULL);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	}

	g_signal_connect(G_OBJECT(menu), "show",
			 G_CALLBACK(port_menu_show_callback), group);

	return menu;
}

static GtkWidget *gui_build_input_menu(void)
{
	GtkWidget *menu, *submenu;
	GtkWidget *item, *fourplayer_menu_item;
	GSList *group;
	int i;

	menu = gtk_menu_new();

	for (i = 0; i < 5; i++) {
		item = gtk_menu_item_new_with_mnemonic(input_menu_names[i]);
		g_object_set_data(G_OBJECT(item), "port", GINT_TO_POINTER(i + 1));
		submenu = gui_build_input_port_menu(i);
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), submenu);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	}

	g_signal_connect(G_OBJECT(menu), "show",
			 G_CALLBACK(input_menu_show_callback), NULL);

	gtk_menu_shell_append(GTK_MENU_SHELL(menu),
			      gtk_separator_menu_item_new());

	fourplayer_menu_item = gtk_menu_item_new_with_mnemonic("_Four-Player Mode");
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), fourplayer_menu_item);
	submenu = gtk_menu_new();
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(fourplayer_menu_item), submenu);

	gtk_menu_shell_append(GTK_MENU_SHELL(menu),
			      gtk_separator_menu_item_new());

	item = gtk_check_menu_item_new_with_mnemonic("_Remember Input Devices");
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	g_object_set_data(G_OBJECT(menu), "remember", item);
	g_signal_connect(G_OBJECT(item), "toggled",
			 G_CALLBACK(remember_input_devices_callback), NULL);
	group = NULL;

	item = gtk_radio_menu_item_new_with_mnemonic(group, "_Auto");
	group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(item));
	gtk_menu_shell_append(GTK_MENU_SHELL(submenu), item);
	g_object_set_data(G_OBJECT(item), "mode",
			  GINT_TO_POINTER(FOUR_PLAYER_MODE_AUTO));
	g_signal_connect(G_OBJECT(item), "activate",
			 G_CALLBACK(fourplayer_mode_callback), NULL);

	item = gtk_radio_menu_item_new_with_mnemonic(group, "_Disabled");
	group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(item));
	gtk_menu_shell_append(GTK_MENU_SHELL(submenu), item);
	g_object_set_data(G_OBJECT(item), "mode",
			  GINT_TO_POINTER(FOUR_PLAYER_MODE_NONE));
	g_signal_connect(G_OBJECT(item), "activate",
			 G_CALLBACK(fourplayer_mode_callback), NULL);

	item = gtk_radio_menu_item_new_with_mnemonic(group, "Four _Score (NES)");
	group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(item));
	gtk_menu_shell_append(GTK_MENU_SHELL(submenu), item);
	g_object_set_data(G_OBJECT(item), "mode",
			  GINT_TO_POINTER(FOUR_PLAYER_MODE_NES));
	g_signal_connect(G_OBJECT(item), "activate",
			 G_CALLBACK(fourplayer_mode_callback), NULL);

	item = gtk_radio_menu_item_new_with_mnemonic(group, "_Famicom");
	group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(item));
	gtk_menu_shell_append(GTK_MENU_SHELL(submenu), item);
	g_object_set_data(G_OBJECT(item), "mode",
			  GINT_TO_POINTER(FOUR_PLAYER_MODE_FC));
	g_signal_connect(G_OBJECT(item), "activate",
			 G_CALLBACK(fourplayer_mode_callback), NULL);

	g_signal_connect(G_OBJECT(menu), "show",
	                 G_CALLBACK(fourplayer_menu_show_callback), group);

	return menu;
}

static int is_sensitive_if_loaded(void)
{
	return emu_loaded(emu);
}

static int is_visible_if_fds(void)
{
	if (emu_loaded(emu) && board_get_type(emu->board) == BOARD_TYPE_FDS)
		return 1;
	else
		return 0;
}

static int is_visible_if_vs(void)
{
	if (emu_loaded(emu) && emu_system_is_vs(emu))
		return 1;
	else
		return 0;
}

static int is_visible_if_has_dip_switches(void)
{
	if (emu_loaded(emu) && board_get_num_dip_switches(emu->board))
		return 1;
	else
		return 0;
}

/*
  Capture key events while in menus in order to capture the release events for the
  accelerator modifiers.  This prevents the emu core from thinking the modifier
  keys involved in the accelerator are still pressed, leading to "stuck" keys.
 */
static void generic_menu_unmap_callback(GtkWidget *widget, gpointer user_data)
{
	input_release_all();
}

static GtkWidget *gui_add_menu_item(GtkMenuShell *menu, const gchar *label,
				    void (*activate_callback)(GtkWidget *widget,
							      gpointer userdata),
				    gpointer userdata,
				    int (*is_visible)(void),
				    int (*is_sensitive)(void))
{
	GtkWidget *item;

	item = gtk_menu_item_new_with_mnemonic(label);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	if (activate_callback) {
		g_signal_connect(G_OBJECT(item), "activate",
				 G_CALLBACK(activate_callback), userdata);
	}

	g_signal_connect(G_OBJECT(item), "unmap",
			 G_CALLBACK(generic_menu_unmap_callback), userdata);

	if (is_visible || is_sensitive) {
		g_signal_connect(G_OBJECT(menu), "show",
				 G_CALLBACK(menu_shown_callback), item);

		if (is_visible) {
			g_object_set_data(G_OBJECT(item), "is_visible",
					  is_visible);
		}

		if (is_sensitive) {
			g_object_set_data(G_OBJECT(item), "is_sensitive",
					  is_sensitive);
		}
	}

	return item;
}

/* File Menu */

static GtkWidget *gui_build_file_menu(void)
{
	GtkMenuShell *menu;

	menu = GTK_MENU_SHELL(gtk_menu_new());

	gui_add_menu_item(menu, "_Open...", file_open, NULL, NULL, NULL);
	gui_add_menu_item(menu, "_Close", file_close, NULL, NULL,
			  is_sensitive_if_loaded);

	gtk_menu_shell_append(GTK_MENU_SHELL(menu),
			      gtk_separator_menu_item_new());

	gui_add_menu_item(menu, "_Apply Patch", apply_patch_callback, NULL,
			  NULL, is_sensitive_if_loaded);

	gtk_menu_shell_append(GTK_MENU_SHELL(menu),
			      gtk_separator_menu_item_new());

	gui_add_menu_item(menu, "S_ave Screenshot", screenshot_callback, NULL,
			  NULL, is_sensitive_if_loaded);

	gtk_menu_shell_append(GTK_MENU_SHELL(menu),
			      gtk_separator_menu_item_new());

	gui_add_menu_item(menu, "_Load State...", load_state, NULL,
			  NULL, is_sensitive_if_loaded);
	gui_add_menu_item(menu, "_Save State...", save_state, NULL,
			  NULL, is_sensitive_if_loaded);

	gtk_menu_shell_append(GTK_MENU_SHELL(menu),
			      gtk_separator_menu_item_new());

	gui_add_menu_item(menu, "_Quit", file_quit_callback, NULL, NULL, NULL);

	return GTK_WIDGET(menu);
}

/* Emulator Menu */

static void remember_system_type_callback(GtkRadioMenuItem *widget,
					  gpointer user_data)
{
	int active;

	active = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget));

	if (active != emu->config->remember_system_type) {
		emu_set_remember_system_type(emu, active);
	}
}

static void remember_input_devices_callback(GtkRadioMenuItem *widget,
					    gpointer user_data)
{
	int active;


	active = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget));

	if (active != emu->config->remember_input_devices) {
		io_set_remember_input_devices(emu->io, active);
	}
}

static void fps_display_callback(GtkRadioMenuItem *widget,
				      gpointer user_data)
{
	int active;

	active = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget));

	if (active != emu->config->fps_display_enabled) {
		emu->config->fps_display_enabled = active;
		video_apply_config(emu);
		config_save_main_config(emu->config);
	}
}

static void emulator_menu_show_callback(GtkWidget *widget, gpointer user_data)
{
	GtkWidget *system_type, *submenu;
	GtkWidget *separator;
	GtkWidget *fps;
	const char *id;
	int loaded;
	int is_fds;

	system_type = g_object_get_data(G_OBJECT(widget), "system_type");
	fps = g_object_get_data(G_OBJECT(widget), "fps");
	separator = g_object_get_data(G_OBJECT(widget), "separator");

	if (emu_loaded(emu) && board_get_type(emu->board) == BOARD_TYPE_FDS) {
		is_fds = 1;
	} else {
		is_fds = 0;
	}

	loaded = emu_loaded(emu);
	gtk_widget_set_sensitive(system_type, loaded);

	if (loaded) {
		if (emu_system_is_vs(emu))
			id = "vs_menu";
		else if (emu->system_type == EMU_SYSTEM_TYPE_PLAYCHOICE)
			id = "playchoice_menu";
		else
			id = "console_menu";
	} else {
		id = NULL;
	}

	if (!is_fds && !emu_system_is_vs(emu) &&
	    !board_get_num_dip_switches(emu->board)) {
		gtk_widget_hide(separator);
	} else {
		gtk_widget_show(separator);
	}

	if (id) {
		submenu = g_object_get_data(G_OBJECT(widget), id);

		gtk_menu_item_set_submenu(GTK_MENU_ITEM(system_type), submenu);
		gtk_widget_show_all(submenu);
	}


	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(fps),
				       emu->config->fps_display_enabled);
}

static void system_type_callback(GtkRadioMenuItem *widget,
				      gpointer user_data)
{
	int system_type;

	if (!gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget)))
		return;

	system_type = GPOINTER_TO_INT(user_data);

	emu_set_system_type(emu, system_type);
}

static void system_type_menu_show_callback(GtkWidget *menu, gpointer user_data)
{
	GtkRadioMenuItem *item;
	GSList *group;
	int system_type;
	const gchar *label;
	GtkRadioMenuItem *auto_item;
	GtkWidget *remember;
	char buffer[30];

	group = user_data;
	label = NULL;
	auto_item = NULL;

	remember = g_object_get_data(G_OBJECT(menu), "remember");

	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(remember),
				       emu->config->remember_system_type);

	while (group) {
		item = group->data;
		group = group->next;

		system_type = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item),
								"system_type"));

		if (system_type == emu->guessed_system_type) {
			label = gtk_menu_item_get_label(GTK_MENU_ITEM(item));
		} else if (system_type == EMU_SYSTEM_TYPE_AUTO) {
			auto_item = item;
		}

		if (emu->system_type != system_type)
			continue;

		g_signal_handlers_block_by_func(G_OBJECT(item),
						G_CALLBACK(system_type_callback),
						NULL);

		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), TRUE);

		g_signal_handlers_unblock_by_func(G_OBJECT(item),
						  G_CALLBACK(system_type_callback),
						  NULL);
	}

	if (auto_item && label) {
		snprintf(buffer, sizeof(buffer), "Auto [%s]", label);
		gtk_menu_item_set_label(GTK_MENU_ITEM(auto_item), buffer);
	}
}

static GtkWidget *gui_build_system_type_menu(int value)
{
	GtkWidget *menu,*item;
	GSList *group;
	int i;

	menu = gtk_menu_new();

	group = NULL;

	for (i = 0; system_type_info[i].type != EMU_SYSTEM_TYPE_UNDEFINED; i++) {
		if (((system_type_info[i].type & 0xf0) != value) &&
		    (system_type_info[i].type != EMU_SYSTEM_TYPE_AUTO)) {
			continue;
		}

		item = gtk_radio_menu_item_new_with_mnemonic(group,
							     system_type_info[i].description);
		group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(item));
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
		g_object_set_data(G_OBJECT(item), "system_type",
				  GINT_TO_POINTER(system_type_info[i].type));
		g_signal_connect(G_OBJECT(item), "activate",
				 G_CALLBACK(system_type_callback),
				 GINT_TO_POINTER(system_type_info[i].type));
	}

	gtk_menu_shell_append(GTK_MENU_SHELL(menu),
			      gtk_separator_menu_item_new());

	item = gtk_check_menu_item_new_with_mnemonic("_Remember System Type");
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	g_object_set_data(G_OBJECT(menu), "remember", item);
	g_signal_connect(G_OBJECT(item), "toggled",
			 G_CALLBACK(remember_system_type_callback), NULL);

	g_signal_connect(G_OBJECT(menu), "show",
			 G_CALLBACK(system_type_menu_show_callback),
			 group);

	g_object_ref(G_OBJECT(menu));

	return menu;
}

static void dip_switch_toggle_callback(GtkWidget *widget, gpointer user_data)
{
	int switch_num;
	int active;

	active = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget));
	switch_num = GPOINTER_TO_INT(user_data);
	board_set_dip_switch(emu->board, switch_num + 1, active);
}

static void dip_switch_menu_item_show_callback(GtkWidget *widget, gpointer user_data)
{
	int switch_total;
	int switch_num;

	switch_total = board_get_num_dip_switches(emu->board);
	switch_num = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widget), "switch"));

	g_signal_handlers_block_by_func(G_OBJECT(widget),
					G_CALLBACK(dip_switch_toggle_callback),
					NULL);

	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(widget),
				       board_get_dip_switches(emu->board) &
				       (1 << switch_num));

	g_signal_handlers_unblock_by_func(G_OBJECT(widget),
					G_CALLBACK(dip_switch_toggle_callback),
					NULL);

	if (switch_num >= switch_total)
		gtk_widget_hide(widget);
	else
		gtk_widget_show(widget);
}

static void dip_switch_menu_show_callback(GtkWidget *widget, gpointer user_data)
{
	gtk_container_foreach(GTK_CONTAINER(widget),
			      dip_switch_menu_item_show_callback, NULL);
}

static GtkWidget *gui_build_dip_switches_menu(void)
{
	GtkWidget *menu;
	GtkWidget *item;
	struct config *config;
	char buffer[80];
	int i;
	const char *config_names[] = {
		"dip_switch_1",
		"dip_switch_2",
		"dip_switch_3",
		"dip_swItch_4",
		"dip_switch_5",
		"dip_switch_6",
		"dip_switch_7",
		"dip_switch_8",
	};

	menu = gtk_menu_new();

	config = emu->config;

	for (i = 0; i < 8; i++) {
		int *data;

		snprintf(buffer, sizeof(buffer), "DIP Switch _%d", i + 1);
		item = gtk_check_menu_item_new_with_mnemonic(buffer);
		if (!item)
			continue;

		data = config_get_data_ptr(config, config_names[i]);

		g_object_set_data(G_OBJECT(item), "switch",
				  GINT_TO_POINTER(i));

		g_object_set_data(G_OBJECT(item), "config_ptr", data);

		g_signal_connect(G_OBJECT(item), "toggled",
				 G_CALLBACK(dip_switch_toggle_callback),
				 GINT_TO_POINTER(i));
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	}

	g_signal_connect(G_OBJECT(menu), "show",
			 G_CALLBACK(dip_switch_menu_show_callback),
			 NULL);

	return menu;
}

static GtkWidget *gui_build_emulator_menu(void)
{
	GtkMenuShell *menu;
	GtkWidget *submenu;
	GtkWidget *item;

	menu = GTK_MENU_SHELL(gtk_menu_new());

	gui_add_menu_item(menu, "_Hard Reset", hard_reset_callback,
			  NULL, NULL, is_sensitive_if_loaded);
	gui_add_menu_item(menu, "_Soft Reset", soft_reset_callback,
			  NULL, NULL, is_sensitive_if_loaded);
	gui_add_menu_item(menu, "_Fullscreen", emulator_fullscreen_callback,
			  NULL, NULL, NULL);
	gui_add_menu_item(menu, "_Pause/Resume", pause_resume_callback,
			  NULL, NULL, is_sensitive_if_loaded);

	gtk_menu_shell_append(GTK_MENU_SHELL(menu),
			      gtk_separator_menu_item_new());

	gui_add_menu_item(menu, "Insert _Coin", insert_coin_callback,
			  NULL, is_visible_if_vs, NULL);

	item = gui_add_menu_item(menu, "DIP _Switches", NULL, NULL,
				 is_visible_if_has_dip_switches, NULL);
	submenu = gui_build_dip_switches_menu();
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), submenu);

	gui_add_menu_item(menu, "Insert/Eject _Disk", eject_disk_callback,
			  NULL, is_visible_if_fds, NULL);
	gui_add_menu_item(menu, "_Switch Disk", switch_disk_callback,
			  NULL, is_visible_if_fds, NULL);

	item = gtk_separator_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	g_object_set_data(G_OBJECT(menu), "separator", item);

	item = gtk_menu_item_new_with_mnemonic("System _Type");
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	g_object_set_data(G_OBJECT(menu), "system_type", item);

	gtk_menu_shell_append(GTK_MENU_SHELL(menu),
			      gtk_separator_menu_item_new());

	item = gtk_check_menu_item_new_with_mnemonic("FPS _Display");
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	g_object_set_data(G_OBJECT(menu), "fps", item);
	g_signal_connect(G_OBJECT(item), "toggled",
			 G_CALLBACK(fps_display_callback), NULL);

	gtk_menu_shell_append(GTK_MENU_SHELL(menu),
			      gtk_separator_menu_item_new());

	item = gui_add_menu_item(menu, "_Input", NULL, NULL, NULL,
				 is_sensitive_if_loaded);
	submenu = gui_build_input_menu();
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), submenu);

	submenu = 
		gui_build_system_type_menu(0x00);
	g_object_set_data(G_OBJECT(menu), "console_menu", submenu);

	submenu = 
		gui_build_system_type_menu(0x10);
	g_object_set_data(G_OBJECT(menu), "vs_menu", submenu);

	submenu = 
		gui_build_system_type_menu(0x20);
	g_object_set_data(G_OBJECT(menu), "playchoice_menu", submenu);

	g_signal_connect(G_OBJECT(menu), "show",
			 G_CALLBACK(emulator_menu_show_callback), NULL);

	return GTK_WIDGET(menu);
}

/* Options Menu */

static GtkWidget *gui_build_options_menu(GtkWidget *gtkwindow)
{
	GtkMenuShell *menu;

	menu = GTK_MENU_SHELL(gtk_menu_new());

	gui_add_menu_item(menu, "_Video Configuration...",
			  gui_video_configuration_dialog, gtkwindow,
			  NULL, NULL);
	gui_add_menu_item(menu, "Pa_lette Configuration...",
			  gui_palette_configuration_dialog, gtkwindow,
			  NULL, NULL);
	gui_add_menu_item(menu, "_Audio Configuration...",
			  gui_audio_configuration_dialog, gtkwindow,
			  NULL, NULL);
	gui_add_menu_item(menu, "V_olume Control...",
			  gui_volume_control_dialog, gtkwindow,
			  NULL, NULL);
	gui_add_menu_item(menu, "_Input Binding Configuration...",
			  gui_binding_configuration_dialog, gtkwindow,
			  NULL, NULL);
	gui_add_menu_item(menu, "_Path Configuration...",
			  gui_path_configuration_dialog, gtkwindow,
			  NULL, NULL);
	gui_add_menu_item(menu, "_Cheats...",
			  gui_cheat_dialog, gtkwindow,
			  NULL, NULL);
	gui_add_menu_item(menu, "_Misc Configuration...",
			  gui_misc_configuration_dialog, gtkwindow,
			  NULL, NULL);
	gui_add_menu_item(menu, "_ROM-Specific Configuration...",
			  gui_rom_configuration_dialog, gtkwindow,
			  NULL, is_sensitive_if_loaded);
	gui_add_menu_item(menu, "_Joystick Info...",
			  gui_joystick_dialog, gtkwindow,
			  NULL, NULL);

	return GTK_WIDGET(menu);
}

static GtkWidget *gui_build_help_menu(GtkWidget *gtkwindow)
{
	GtkMenuShell *menu;

	menu = GTK_MENU_SHELL(gtk_menu_new());

	gui_add_menu_item(menu, "_ROM Info...", rom_info_callback,
			  gtkwindow, NULL, is_sensitive_if_loaded);

	gui_add_menu_item(menu, "_About...", about_callback,
			  gtkwindow, NULL, NULL);

	return GTK_WIDGET(menu);
}

GtkWidget *gui_build_menubar(GtkWidget *gtkwindow)
{
	GtkMenuShell *menubar;
	GtkWidget *item;
	GtkWidget *submenu;

	menubar = GTK_MENU_SHELL(gtk_menu_bar_new());

	submenu = gui_build_file_menu();
	item = gui_add_menu_item(menubar, "_File",
				 NULL,
				 NULL, NULL, NULL);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), submenu);
	g_signal_handlers_disconnect_by_func(G_OBJECT(item),
					     G_CALLBACK(generic_menu_unmap_callback),
					     NULL);

	submenu = gui_build_emulator_menu();
	item = gui_add_menu_item(menubar, "_Emulator",
				 NULL,
				 NULL, NULL, NULL);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), submenu);
	g_signal_handlers_disconnect_by_func(G_OBJECT(item),
					     G_CALLBACK(generic_menu_unmap_callback),
					     NULL);

	submenu = gui_build_options_menu(gtkwindow);
	item = gui_add_menu_item(menubar, "_Options",
				 NULL,
				 NULL, NULL, NULL);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), submenu);
	g_signal_handlers_disconnect_by_func(G_OBJECT(item),
					     G_CALLBACK(generic_menu_unmap_callback),
					     NULL);

	submenu = gui_build_help_menu(gtkwindow);
	item = gui_add_menu_item(menubar, "_Help",
				 NULL,
				 NULL, NULL, NULL);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), submenu);
	g_signal_handlers_disconnect_by_func(G_OBJECT(item),
					     G_CALLBACK(generic_menu_unmap_callback),
					     NULL);

	return GTK_WIDGET(menubar);
}

