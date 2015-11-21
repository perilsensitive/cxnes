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
#include <errno.h>
#include <ctype.h>

#include "emu.h"
#include "input.h"
#include "video.h"
#include "actions.h"
#include "gtkconfig.h"

extern struct emu *emu;

static GtkWidget *grab_event_dialog;
static GtkWidget *label, *category, *action, *bind;
static GtkWidget *check_alt, *check_ctrl, *check_shift, *check_gui;
static GtkWidget *check_mod1, *check_mod2, *check_mod3, *check_kbd;
static GtkWidget *grab, *delete, *add, *set_all;
static GtkTreeStore *store;
static GtkWidget *tree;
static GtkTreeModel* filter_model;
static GtkWidget *category_combo;

int sdl_grab_event;
static int run_timer;
static int timer_count;
static int event_grabbed;
struct input_event grabbed_event;
static int mod_bits;
uint32_t binding_config_action_id;
static double orig_x, orig_y;

enum binding_columns {
	COLUMN_CATEGORY,
	COLUMN_ACTION,
	COLUMN_MODSTRING,
	COLUMN_BINDING,
	COLUMN_ACTION_ID,
	COLUMN_TYPE,
	COLUMN_DEVICE,
	COLUMN_INDEX,
	COLUMN_MISC,
	COLUMN_MODMASK,
	COLUMN_COUNT,
};

#if _WIN32
extern char *strtok_r(char *str, const char *delim, char **saveptr);
#endif
static gboolean row_is_visible(GtkTreeModel *model, GtkTreeIter *iter, gpointer data);
static void clear_store(GtkTreeStore *store);
static void apply_input_bindings(GtkTreeStore *store);
static void delete_callback(GtkButton *button, gpointer user_data);
static void mod_toggle_callback(GtkToggleButton *button, gpointer user_data);
static void binding_selected_callback(GtkTreeSelection *selection, gpointer user_data);
static void set_all_callback(GtkButton *button, gpointer data);
extern int input_convert_event(SDL_Event *event,
			       union input_new_event *new_event);
extern int convert_key_event(GdkEventKey *event, SDL_Event *sdlevent);

static void load_bindings(GtkTreeStore *store);
static void load_default_bindings(GtkTreeStore *store,
				  struct binding_item *bindings,
				  int modifiers);

static void setup_tree_actions(GtkTreeStore *store)
{
	GtkTreeIter action_iter;
	int i;

	gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), &action_iter);

	for (i = 0; emu_action_id_map[i].emu_action_id != ACTION_NONE; i++) {
		enum action_category category_id;

		category_id = emu_action_id_map[i].category;

		gtk_tree_store_append(store, &action_iter, NULL);
		gtk_tree_store_set(store, &action_iter,
				   COLUMN_CATEGORY,
				   category_names[category_id],
				   COLUMN_ACTION,
				   emu_action_id_map[i].description,
				   COLUMN_ACTION_ID,
				   emu_action_id_map[i].emu_action_id,
				   -1);
	}

	gtk_tree_store_append(store, &action_iter, NULL);
	gtk_tree_store_set(store, &action_iter,
			   COLUMN_CATEGORY, "Modifiers",
			   COLUMN_ACTION, "Mod1",
			   COLUMN_ACTION_ID, INPUT_MOD_MOD1,
			   -1);
	gtk_tree_store_append(store, &action_iter, NULL);
	gtk_tree_store_set(store, &action_iter,
			   COLUMN_CATEGORY, "Modifiers",
			   COLUMN_ACTION, "Mod2",
			   COLUMN_ACTION_ID, INPUT_MOD_MOD2,
			   -1);
	gtk_tree_store_append(store, &action_iter, NULL);
	gtk_tree_store_set(store, &action_iter,
			   COLUMN_CATEGORY, "Modifiers",
			   COLUMN_ACTION, "Mod3",
			   COLUMN_ACTION_ID, INPUT_MOD_MOD3,
			   -1);
	gtk_tree_store_append(store, &action_iter, NULL);
	gtk_tree_store_set(store, &action_iter,
			   COLUMN_CATEGORY, "Modifiers",
			   COLUMN_ACTION, "Kbd",
			   COLUMN_ACTION_ID, INPUT_MOD_KBD,
			   -1);

}

static void setup_tree(GtkWidget **treeptr, GtkTreeStore **storeptr,
		       GtkTreeSelection **selectionptr)
{
	GtkWidget *tree;
	GtkTreeStore *store;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkTreeSelection *selection;
	int i;

	tree = gtk_tree_view_new();
	store = gtk_tree_store_new(10, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
				   G_TYPE_INT, G_TYPE_INT, G_TYPE_INT, G_TYPE_INT, G_TYPE_INT,
				   G_TYPE_INT, -1);
	filter_model = gtk_tree_model_filter_new(GTK_TREE_MODEL(store), NULL);
	gtk_tree_model_filter_set_visible_func(GTK_TREE_MODEL_FILTER(filter_model),
					       row_is_visible,
					       NULL, NULL);
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_set_fixed_height_mode(GTK_TREE_VIEW(tree), FALSE);
	gtk_tree_view_set_model(GTK_TREE_VIEW(tree), GTK_TREE_MODEL(filter_model));
	g_object_unref(G_OBJECT(filter_model));

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));
	g_signal_connect(G_OBJECT(selection), "changed",
			 G_CALLBACK(binding_selected_callback), NULL);

	column = gtk_tree_view_column_new_with_attributes("Action", renderer,
							  "text", COLUMN_ACTION, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);

	column = gtk_tree_view_column_new_with_attributes("Modifiers", renderer,
							  "text", COLUMN_MODSTRING, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);

	column = gtk_tree_view_column_new_with_attributes("Binding", renderer,
							  "text", COLUMN_BINDING, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);

	for (i = 0; i < 6; i++) {
		column = gtk_tree_view_column_new();
		gtk_tree_view_column_set_visible(column, FALSE);
		gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);
	}

	setup_tree_actions(store);

	load_bindings(store);
	gtk_tree_view_expand_all(GTK_TREE_VIEW(tree));

	*treeptr = tree;
	*storeptr = store;
	*selectionptr = selection;
}

static void add_modifier_to_view(GtkTreeStore *store, int mod, uint32_t type,
				 uint32_t device, uint32_t index, uint32_t misc)
{
	GtkTreeModel *model;
	GtkTreeIter action_iter, binding_iter;
	uint32_t data;
	char buffer[80];
	gchar *action_name;
	struct input_event input_event;

	if (mod > INPUT_MOD_KBD)
		return;

	model = GTK_TREE_MODEL(store);

	gtk_tree_model_iter_children(model, &action_iter, NULL);
	do {
		gtk_tree_model_get(model, &action_iter,
				   COLUMN_ACTION, &action_name,
				   COLUMN_ACTION_ID, &data, -1);

		if (mod == data)
			break;
	} while (gtk_tree_model_iter_next(model, &action_iter));

	input_event.event.common.type = type;
	input_event.event.common.device = device;
	input_event.event.common.index = index;
	input_event.event.common.misc = misc;
	if (get_binding_name(buffer, sizeof(buffer), &input_event) <0)
		return;

	gtk_tree_store_append(store, &binding_iter, &action_iter);
	gtk_tree_store_set(store, &binding_iter,
			   COLUMN_BINDING, buffer,
			   COLUMN_CATEGORY, "Modifiers",
			   /* COLUMN_ACTION, action_name, */
			   COLUMN_ACTION_ID, data,
			   COLUMN_TYPE, type,
			   COLUMN_DEVICE, device,
			   COLUMN_INDEX, index,
			   COLUMN_MISC, misc,
			   -1);

	g_free(action_name);
}

static int add_binding_to_view(GtkTreeStore *store, const char *category,
			       const char *binding_name, const char *modstring,
			       const char *action_name,	uint32_t action,	uint32_t type,
			       uint32_t device, uint32_t index,
			       uint32_t misc, uint32_t modmask, int select_new_row)
{
	GtkTreeModel *model;
	GtkTreeIter action_iter, binding_iter;
	int action_id;
	int t, d, i, m, mod;
	int found;

	model = GTK_TREE_MODEL(store);

	// find action
	gtk_tree_model_iter_children(model, &action_iter, NULL);

	action_id = ACTION_NONE;
	do {
		gtk_tree_model_get(model, &action_iter, COLUMN_ACTION_ID,
				   &action_id, -1);

		if (action == action_id)
			break;
	} while (gtk_tree_model_iter_next(model, &action_iter));

	if (action_id == ACTION_NONE)
		return 1;

	found = 0;
	// check for binding
	if (gtk_tree_model_iter_children(model, &binding_iter, &action_iter)) {
		do {
			gtk_tree_model_get(model, &binding_iter,
					   COLUMN_TYPE, &t,
					   COLUMN_DEVICE, &d,
					   COLUMN_INDEX, &i,
					   COLUMN_MISC, &m,
					   COLUMN_MODMASK, &mod,
					   -1);

			if ((type == t) && (device == d) &&
			    (index == i) && (misc == m) &&
			    (mod == modmask)) {
				found = 1;
				break;
			} else {
				/* printf("type: %x t: %x device: %x d: %x index: %x i: %x misc: %x m: %x mod: %x modmask: %x\n", type, t, device, d, index, i, misc, m, mod, modmask); */
			}
		} while (gtk_tree_model_iter_next(model, &binding_iter));
	}

	if (!found) {
		gtk_tree_store_append(store, &binding_iter, &action_iter);

		gtk_tree_store_set(store, &binding_iter,
				   COLUMN_CATEGORY, category,
				   /* COLUMN_ACTION, action_name, */
				   COLUMN_BINDING, binding_name,
				   COLUMN_MODSTRING, modstring,
				   COLUMN_ACTION_ID, action,
				   COLUMN_TYPE, type,
				   COLUMN_DEVICE, device,
				   COLUMN_INDEX, index,
				   COLUMN_MISC, misc,
				   COLUMN_MODMASK, modmask,
				   -1);
	}

	if (select_new_row) {
		GtkTreePath *path;
//		GtkTreeSelection *selection;
		path = gtk_tree_model_get_path(model, &binding_iter);
		gtk_tree_view_expand_to_path(GTK_TREE_VIEW(tree), path);
		gtk_tree_view_set_cursor(GTK_TREE_VIEW(tree), path, NULL,
					 FALSE);
		/* selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree)); */
		/* binding_selected_callback(selection, NULL); */
	}

	return 0;
}

static gboolean motion_event_callback(GtkWidget *widget, GdkEventMotion *motion,
				      gpointer data)
{
	double x, y;
	SDL_Event sdlevent;

	if (!orig_x && !orig_y) {
		orig_x = motion->x;
		orig_y = motion->y;
		return TRUE;
	}

	x = motion->x - orig_x;
	y = motion->y - orig_y;

	if (x < 0)
		x = -x;

	if (y < 0)
		y = -y;

	/* Filter out accidental mouse motion by requiring the mouse
	   to have moved at least 50 pixels in any direction.
	*/
	if ((x < 50) && (y < 50))
		return TRUE;

	sdlevent.type = SDL_MOUSEMOTION;
	sdlevent.motion.which = 0;
	sdlevent.motion.windowID = 0;
	if (input_convert_event(&sdlevent, &grabbed_event.event)) {
		if ((binding_config_action_id & ACTION_TYPE_MASK) !=
		    ACTION_TYPE_MOUSE) {
			return TRUE;
		}
		event_grabbed = 1;
	}

	return TRUE;
}

static gboolean null_key_event_callback(GtkWidget *widdget, GdkEventKey *event,
					gpointer user_data)
{
	return TRUE;
}

static gboolean key_event_callback(GtkWidget *widget, GdkEventKey *event,
				  gpointer user_data)
{
	SDL_Event sdlevent;
	if (event->type == GDK_KEY_PRESS) {
		if (!convert_key_event(event, &sdlevent))
			return TRUE;

		if (input_convert_event(&sdlevent, &grabbed_event.event)) {
			if ((binding_config_action_id & ACTION_TYPE_MASK) !=
			    ACTION_TYPE_DIGITAL) {
				return TRUE;
			}
			event_grabbed = 1;
		}
	}

	return TRUE;
}

static gboolean button_event_callback(GtkWidget *widget, GdkEventButton *button,
				      gpointer data)
{
	SDL_Event sdlevent;

	if (button->type == GDK_BUTTON_PRESS) {
		sdlevent.type = SDL_MOUSEBUTTONDOWN;
		sdlevent.button.state = SDL_PRESSED;
		sdlevent.button.windowID = 0;
		sdlevent.button.which = 0;
		sdlevent.button.button = button->button;
		sdlevent.button.clicks = 1;
		if (input_convert_event(&sdlevent, &grabbed_event.event)) {
			if ((binding_config_action_id & ACTION_TYPE_MASK) !=
			    ACTION_TYPE_DIGITAL) {
				return TRUE;
			}
			event_grabbed = 1;
		}
	}

	return TRUE;
}

static gboolean _event_timer_callback(gpointer data)
{
	if (sdl_grab_event == 0)
		event_grabbed = 1;

	timer_count -= 250;
	if (event_grabbed || timer_count <= 0) {
		if (grab_event_dialog) {
			gtk_widget_destroy(grab_event_dialog);
			grab_event_dialog = NULL;
		}
		return FALSE;
	}

	return TRUE;
}

static void grab_event_callback(GtkButton *button, gpointer user_data)
{
	GtkTreeSelection *selection;
	GdkWindow *parent_gdk_window;
	GtkWindow *parent;
	const gchar *category_text, *action_text;
	gint response;
	char buffer[80];
	uint32_t action_type;
	const char *fmt;
	int modify;

	parent = NULL;
	parent_gdk_window = gtk_widget_get_parent_window(GTK_WIDGET(button));
	gdk_window_get_user_data(parent_gdk_window, (gpointer *)&parent);

	action_type = binding_config_action_id & ACTION_TYPE_MASK;

	switch (action_type) {
	case ACTION_TYPE_DIGITAL:
		fmt = "Capturing event for %s %s\n\n"
			"Press key, mouse button or joystick button"
			" or move joystick axis or hat";
		break;
	case ACTION_TYPE_ANALOG:
		fmt = "Capturing event for %s %s\n\n"
			"Move joystick axis";
		break;
	case ACTION_TYPE_MOUSE:
		fmt = "Capturing event for %s %s\n\n"
			"Move mouse";
		break;
	default:
		return;
	}

	category_text = gtk_entry_get_text(GTK_ENTRY(category));
	action_text = gtk_entry_get_text(GTK_ENTRY(action));

	/* FIXME parent? and flags? */
	grab_event_dialog = gtk_message_dialog_new(parent, GTK_DIALOG_DESTROY_WITH_PARENT,
						   GTK_MESSAGE_OTHER, GTK_BUTTONS_CANCEL,
						   fmt, category_text, action_text);

	timer_count = 5000;
	run_timer = 1;
	event_grabbed = 0;
	orig_x = orig_y = 0.0;

	g_timeout_add(250, _event_timer_callback, grab_event_dialog);

	g_signal_connect(G_OBJECT(grab_event_dialog), "key_press_event",
			 G_CALLBACK(key_event_callback), NULL);

	g_signal_connect(G_OBJECT(grab_event_dialog), "button_press_event",
			 G_CALLBACK(button_event_callback), NULL);

	g_signal_connect(G_OBJECT(grab_event_dialog), "motion_notify_event",
			 G_CALLBACK(motion_event_callback), NULL);

	sdl_grab_event = 1;
	response = gtk_dialog_run(GTK_DIALOG(grab_event_dialog));
	if (response == GTK_RESPONSE_CANCEL) {
		run_timer = 0;
		if (grab_event_dialog) {
			gtk_widget_destroy(grab_event_dialog);
			grab_event_dialog = NULL;
		}
	}

	modify = GPOINTER_TO_INT(user_data);

	if (event_grabbed) {
		gboolean rc;
		GtkTreeModel *filter_model, *model;
		GtkTreeIter filter_iter, iter;

		selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));
		rc = gtk_tree_selection_get_selected(selection, &filter_model, &filter_iter);
		if (!rc)
			return;

		model = gtk_tree_model_filter_get_model(GTK_TREE_MODEL_FILTER(filter_model));
		gtk_tree_model_filter_convert_iter_to_child_iter(GTK_TREE_MODEL_FILTER(filter_model),
								 &iter, &filter_iter);

		switch (grabbed_event.event.common.type) {
		case INPUT_EVENT_TYPE_JOYAXIS:
			if ((binding_config_action_id & ACTION_TYPE_MASK) ==
			    ACTION_TYPE_DIGITAL) {
				grabbed_event.event.common.type =
					INPUT_EVENT_TYPE_JOYAXISBUTTON;
				grabbed_event.event.common.misc =
					grabbed_event.event.jaxis.value >= 0 ? 1 : -1;
			}
			break;
		}

		get_binding_name(buffer, sizeof(buffer), &grabbed_event);
		gtk_entry_set_text(GTK_ENTRY(bind), buffer);


		if (modify) {
			gtk_tree_store_set(GTK_TREE_STORE(model), &iter,
					   COLUMN_BINDING,
					   buffer,
					   COLUMN_TYPE,
					   grabbed_event.event.common.type,
					   COLUMN_DEVICE,
					   grabbed_event.event.common.device,
					   COLUMN_INDEX,
					   grabbed_event.event.common.index,
					   COLUMN_MISC,
					   grabbed_event.event.common.misc,
					   -1);
		} else {
			const gchar *name, *category_name;

			name = gtk_entry_get_text(GTK_ENTRY(action));
			category_name = gtk_entry_get_text(GTK_ENTRY(category));
			add_binding_to_view(store, category_name, buffer, NULL,
					    name, binding_config_action_id,
					    grabbed_event.event.common.type,
					    grabbed_event.event.common.device,
					    grabbed_event.event.common.index,
					    grabbed_event.event.common.misc,
					    0, 1);
		}
	}
}

static gboolean row_is_visible(GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
	char *category;
	uint32_t id;
	const gchar *tmp;

	tmp = gtk_combo_box_get_active_id(GTK_COMBO_BOX(category_combo));
	
	gtk_tree_model_get(model, iter,
			   COLUMN_CATEGORY, &category,
			   COLUMN_ACTION_ID, &id, -1);

	if (category && !strcmp(category, tmp) &&
	    (id != ACTION_NONE)) {
		g_free(category);
		return TRUE;
	}

	return FALSE;
}

static void category_combo_callback(GtkComboBox *combo, gpointer data)
{
	gtk_tree_model_filter_refilter(GTK_TREE_MODEL_FILTER(filter_model));
	gtk_tree_view_expand_all(GTK_TREE_VIEW(tree));
}

static void configuration_setup_bindings(GtkWidget *dialog, struct config *config)
{
	GtkWidget *box, *hbox;
	GtkWidget *grid;
	GtkWidget *binding_window;
	GtkWidget *button;
	GtkTreeSelection *selection;
	GtkTreePath *path;
	int i;

	box = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
	gtk_box_set_spacing(GTK_BOX(box), 8);
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	grid = gtk_grid_new();

	gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
	gtk_grid_set_row_spacing(GTK_GRID(grid), 8);

	category_combo = gtk_combo_box_text_new();
	for (i = 0; i < ACTION_CATEGORY_COUNT; i++) {
		gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(category_combo),
					  category_names[i],
					  category_names[i]);
	}

	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(category_combo),
				  "Modifiers", "Modifiers");

	gtk_combo_box_set_wrap_width(GTK_COMBO_BOX(category_combo), 2);

	gtk_combo_box_set_active_id(GTK_COMBO_BOX(category_combo),
				    category_names[0]);
	g_signal_connect(G_OBJECT(category_combo), "changed",
			 G_CALLBACK(category_combo_callback), NULL);

	setup_tree(&tree, &store, &selection);
	binding_window = gtk_scrolled_window_new(NULL, NULL);
	gtk_box_pack_start(GTK_BOX(box), category_combo, FALSE, TRUE, 8);
	gtk_box_pack_start(GTK_BOX(box), binding_window, TRUE, TRUE, 8);
	gtk_box_pack_start(GTK_BOX(box), hbox, FALSE, FALSE, 8);

	gtk_box_pack_start(GTK_BOX(box), grid, TRUE, TRUE, 8);
	gtk_container_add(GTK_CONTAINER(binding_window), tree);
	gtk_widget_set_size_request(binding_window, 600, 400);

	button = gtk_button_new_with_mnemonic("_Add");
	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(grab_event_callback),
			 GINT_TO_POINTER(0));
	gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
	add = button;

	button = gtk_button_new_with_mnemonic("A_dd All");
	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(set_all_callback),
			 GINT_TO_POINTER(0));
	gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
	set_all = button;

	button = gtk_button_new_with_mnemonic("_Grab Event");
	g_object_set_data(G_OBJECT(button), "parent", dialog);
	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(grab_event_callback),
			 GINT_TO_POINTER(1));
	gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
	grab = button;

	button = gtk_button_new_with_mnemonic("_Remove");
	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(delete_callback), selection);
	gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
	delete = button;

	label = gtk_label_new("Category: ");
	category = gtk_entry_new();
	g_signal_connect(G_OBJECT(category), "key_press_event",
			 G_CALLBACK(null_key_event_callback), NULL);
	gtk_grid_attach(GTK_GRID(grid), label, 0, 0, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), category, 1, 0, 1, 1);
	
	label = gtk_label_new("Action: ");
	action = gtk_entry_new();
	g_signal_connect(G_OBJECT(action), "key_press_event",
			 G_CALLBACK(null_key_event_callback), NULL);
	gtk_grid_attach(GTK_GRID(grid), label, 0, 2, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), action, 1, 2, 1, 1);

	label = gtk_label_new("Binding: ");
	bind = gtk_entry_new();
	g_signal_connect(G_OBJECT(bind), "key_press_event",
			 G_CALLBACK(null_key_event_callback), NULL);
	gtk_entry_set_width_chars(GTK_ENTRY(bind), 35);
	gtk_grid_attach(GTK_GRID(grid), label, 0, 1, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), bind, 1, 1, 1, 1);

	check_alt = gtk_check_button_new_with_mnemonic("_Alt");
	check_ctrl = gtk_check_button_new_with_mnemonic("C_trl");
	check_shift = gtk_check_button_new_with_mnemonic("_Shift");
	check_gui = gtk_check_button_new_with_mnemonic("_GUI");
	check_mod1 = gtk_check_button_new_with_mnemonic("Mod_1");
	check_mod2 = gtk_check_button_new_with_mnemonic("Mod_2");
	check_mod3 = gtk_check_button_new_with_mnemonic("Mod_3");
	check_kbd = gtk_check_button_new_with_mnemonic("_Kbd");

	g_signal_connect(G_OBJECT(check_alt), "toggled",
			 G_CALLBACK(mod_toggle_callback),
			 GINT_TO_POINTER(INPUT_MOD_BITS_ALT));
	g_object_set_data(G_OBJECT(check_alt), "selection",
			  selection);
	g_signal_connect(G_OBJECT(check_shift), "toggled",
			 G_CALLBACK(mod_toggle_callback),
			 GINT_TO_POINTER(INPUT_MOD_BITS_SHIFT));
	g_object_set_data(G_OBJECT(check_shift), "selection",
			  selection);
	g_signal_connect(G_OBJECT(check_ctrl), "toggled",
			 G_CALLBACK(mod_toggle_callback),
			 GINT_TO_POINTER(INPUT_MOD_BITS_CTRL));
	g_object_set_data(G_OBJECT(check_ctrl), "selection",
			  selection);
	g_signal_connect(G_OBJECT(check_gui), "toggled",
			 G_CALLBACK(mod_toggle_callback),
			 GINT_TO_POINTER(INPUT_MOD_BITS_GUI));
	g_object_set_data(G_OBJECT(check_gui), "selection",
			  selection);
	g_signal_connect(G_OBJECT(check_mod1), "toggled",
			 G_CALLBACK(mod_toggle_callback),
			 GINT_TO_POINTER(INPUT_MOD_BITS_MOD1));
	g_object_set_data(G_OBJECT(check_mod1), "selection",
			  selection);
	g_signal_connect(G_OBJECT(check_mod2), "toggled",
			 G_CALLBACK(mod_toggle_callback),
			 GINT_TO_POINTER(INPUT_MOD_BITS_MOD2));
	g_object_set_data(G_OBJECT(check_mod2), "selection",
			  selection);
	g_signal_connect(G_OBJECT(check_mod3), "toggled",
			 G_CALLBACK(mod_toggle_callback),
			 GINT_TO_POINTER(INPUT_MOD_BITS_MOD3));
	g_object_set_data(G_OBJECT(check_mod3), "selection",
			  selection);
	g_signal_connect(G_OBJECT(check_kbd), "toggled",
			 G_CALLBACK(mod_toggle_callback),
			 GINT_TO_POINTER(INPUT_MOD_BITS_KBD));
	g_object_set_data(G_OBJECT(check_kbd), "selection",
			  selection);

	gtk_grid_attach(GTK_GRID(grid), check_shift, 4, 0, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), check_ctrl, 5, 0, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), check_alt, 6, 0, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), check_gui, 7, 0, 1, 1);

	gtk_grid_attach(GTK_GRID(grid), check_mod1, 4, 1, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), check_mod2, 5, 1, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), check_mod3, 6, 1, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), check_kbd, 7, 1, 1, 1);

	path = gtk_tree_path_new_first();
	gtk_tree_view_expand_to_path(GTK_TREE_VIEW(tree), path);
	gtk_tree_view_set_cursor(GTK_TREE_VIEW(tree), path, NULL,
				 FALSE);
	gtk_tree_path_free(path);
}

void gui_binding_configuration_dialog(GtkWidget *widget, gpointer user_data)
{
	GtkWidget *dialog;
	GtkWidget *dialog_box;
	GtkWidget *gtkwindow;
	GtkDialogFlags flags;
	struct config *config;
	int paused;
	gint response;

	gtkwindow = (GtkWidget *)user_data;
	flags = GTK_DIALOG_DESTROY_WITH_PARENT;
	dialog = gtk_dialog_new_with_buttons("Input Binding Config",
					     GTK_WINDOW(gtkwindow), flags,
					     "_Defaults", RESET_TO_DEFAULT,
					     "_OK", GTK_RESPONSE_ACCEPT,
					     "_Cancel", GTK_RESPONSE_REJECT,
					     NULL);

	dialog_box = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
	gtk_container_set_border_width(GTK_CONTAINER(dialog_box), 8);
	gtk_window_set_transient_for(GTK_WINDOW(dialog),
				     GTK_WINDOW(gtkwindow));

	config = config_copy(emu->config);

	configuration_setup_bindings(dialog, config);

	paused = emu_paused(emu);
	video_show_cursor(1);
	emu_pause(emu, 1);
	gtk_widget_show_all(dialog);

	while (1) {
		response = gtk_dialog_run(GTK_DIALOG(dialog));

		if (response == RESET_TO_DEFAULT) {
			clear_store(store);
			load_default_bindings(store, default_bindings, 0);
			/* load_default_bindings(store, default_modifiers, 1); */
			gtk_tree_view_expand_all(GTK_TREE_VIEW(tree));
			continue;
		}
		break;
	}

	if (response == GTK_RESPONSE_ACCEPT) {
		input_reset_bindings();
		apply_input_bindings(store);
		input_configure_keyboard_modifiers();
		config_save_main_config(config);
	}

	if (!paused && emu_loaded(emu))
		emu_pause(emu, 0);

	gtk_widget_destroy(dialog);
	video_show_cursor(0);
}

static void load_default_bindings(GtkTreeStore *store,
				  struct binding_item *bindings,
				  int modifiers)
{
	struct input_event event;
	char binding_name[80];
	char modifier_string[80];
	uint32_t emu_action;
	char *saveptr;
	char *token;
	char *tmp;
	int mod;
	int i;

	for (i = 0; bindings[i].name; i++) {
		char *name = bindings[i].name;
		char *value = bindings[i].value;

		if (input_parse_binding(name, &event.event, &mod) != 0)
			continue;

		get_binding_name(binding_name, sizeof(binding_name),
				 &event);

		tmp = strdup(value);
		if (!tmp)
			continue;

		saveptr = NULL;
		token = strtok_r(tmp, ",", &saveptr);
		while (token) {
			char *end;
			char *name, *category;

			while (isspace(*token))
				token++;

			end = token + strlen(token) - 1;
			while (isspace(*end)) {
				*end = '\0';
				end--;
			}

			get_event_name_and_category(emu_action, &name, &category);

			if (modifiers) {
				int j;

				mod = -1;
				for (j = 0; j < INPUT_MOD_COUNT; j++) {
					if (strcasecmp(token, modifier_names[j]) == 0) {
						mod = j;
						break;
					}
				}

				if (mod >= 0) {
					add_modifier_to_view(store, mod,
							     event.event.common.type,
							     event.event.common.device,
							     event.event.common.index,
							     event.event.common.misc);
				}
			} else {
				if (emu_action_lookup_by_name(token, &emu_action) < 0)
					goto loop_end;

				get_modifier_string(modifier_string,
						    sizeof(modifier_string),
						    mod);

				printf("Binding name: %s\n", binding_name);

				add_binding_to_view(store, category, binding_name,
						    modifier_string, name,
						    emu_action,
						    event.event.common.type,
						    event.event.common.device,
						    event.event.common.index,
						    event.event.common.misc,
						    mod, 0);
			}

loop_end:

			token = strtok_r(NULL, ",", &saveptr);
		}

		free(tmp);
	}
}

static void load_binding(GtkTreeStore *store, struct input_event *event)
{
	int i;
	char *name, *category;
	char binding_name[80];
	char modifier_string[80];

	get_binding_name(binding_name, sizeof(binding_name), event);

	if (event->modifier >= 0) {
		add_modifier_to_view(store, event->modifier,
				     event->event.common.type,
				     event->event.common.device,
				     event->event.common.index,
				     event->event.common.misc);
	}

	for (i = 0; i < event->mapping_count; i++) {
		uint32_t id = event->mappings[i].emu_action->id;
		uint32_t mod_bits = event->mappings[i].mod_bits;
		get_event_name_and_category(id, &name, &category);
		get_modifier_string(modifier_string, sizeof(modifier_string), mod_bits);
		add_binding_to_view(store, category, binding_name, modifier_string, name,
				    id, event->event.common.type,
				    event->event.common.device, event->event.common.index,
				    event->event.common.misc, mod_bits, 0);
	}
}

static void delete_callback(GtkButton *button, gpointer user_data)
{
	GtkTreeSelection *selection;
	GtkTreeModel *filter_model, *model;
	GtkTreeIter filter_iter, iter, child_iter;
	gboolean rc;
	char *binding_string;

	selection = user_data;

	rc = gtk_tree_selection_get_selected(selection, &filter_model,
					     &filter_iter);
	if (!rc)
		return;

	model = gtk_tree_model_filter_get_model(GTK_TREE_MODEL_FILTER(filter_model));
	gtk_tree_model_filter_convert_iter_to_child_iter(GTK_TREE_MODEL_FILTER(filter_model),
							 &iter, &filter_iter);

	gtk_tree_model_get(model, &iter,
			   COLUMN_BINDING, &binding_string,
			   -1);

	if (binding_string) {
		gtk_tree_store_remove(GTK_TREE_STORE(model), &iter);
	} else {
		if (!gtk_tree_model_iter_nth_child(model, &child_iter,
						   &iter, 0)) {
			return;
		}

		while (1) {
			if (!gtk_tree_store_remove(GTK_TREE_STORE(model),
						   &child_iter)) {
				break;
			}
		}
	}
}

static void mod_toggle_callback(GtkToggleButton *button, gpointer user_data)
{
	gboolean toggled;
	char buffer[80];
	gint mask;
	GtkTreeSelection *selection;
	GtkTreeIter filter_iter, iter;
	GtkTreeModel *filter_model, *model;
	gboolean rc;

	model = GTK_TREE_MODEL(store);
	selection = g_object_get_data(G_OBJECT(button), "selection");

	rc = gtk_tree_selection_get_selected(selection, &filter_model, &filter_iter);
	if (!rc)
		return;

	model = gtk_tree_model_filter_get_model(GTK_TREE_MODEL_FILTER(filter_model));
	gtk_tree_model_filter_convert_iter_to_child_iter(GTK_TREE_MODEL_FILTER(filter_model),
								 &iter, &filter_iter);
	toggled = gtk_toggle_button_get_active(button);

	mask = GPOINTER_TO_INT(user_data);

	if (toggled)
		mod_bits |= mask;
	else
		mod_bits &= ~mask;

	get_modifier_string(buffer, sizeof(buffer), mod_bits);
	gtk_tree_store_set(GTK_TREE_STORE(model), &iter,
			   COLUMN_MODSTRING, buffer,
			   COLUMN_MODMASK, mod_bits, -1);
}

static void binding_selected_callback(GtkTreeSelection *selection, gpointer user_data)
{
	GtkTreeModel *model;
	GtkTreeIter iter, parent_iter;
	gchar *category_string, *action_string, *binding_string;
	gboolean rc;
	gboolean is_event, is_binding, is_modifier;

	rc = gtk_tree_selection_get_selected(selection, &model, &iter);
	if (!rc)
		return;

	gtk_tree_model_get(model, &iter,
			   COLUMN_CATEGORY, &category_string,
			   COLUMN_BINDING, &binding_string,
			   COLUMN_MODMASK, &mod_bits,
			   COLUMN_TYPE, &grabbed_event.event.common.type,
			   COLUMN_DEVICE, &grabbed_event.event.common.device,
			   COLUMN_INDEX, &grabbed_event.event.common.index,
			   COLUMN_MISC, &grabbed_event.event.common.misc,
			   COLUMN_ACTION_ID, &binding_config_action_id,
			   -1);

	is_modifier = !strcmp(category_string, "Modifiers");
		

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_mod1),
				     mod_bits & INPUT_MOD_BITS_MOD1);
	
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_mod2),
				     mod_bits & INPUT_MOD_BITS_MOD2);
	
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_mod3),
				     mod_bits & INPUT_MOD_BITS_MOD3);
	
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_kbd),
				     mod_bits & INPUT_MOD_BITS_KBD);
	
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_ctrl),
				     mod_bits & INPUT_MOD_BITS_CTRL);
	
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_shift),
				     mod_bits & INPUT_MOD_BITS_SHIFT);
	
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_alt),
				     mod_bits & INPUT_MOD_BITS_ALT);
	
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_gui),
				     mod_bits & INPUT_MOD_BITS_GUI);

	gtk_entry_set_text(GTK_ENTRY(category),
			   category_string ? category_string : "");

	gtk_entry_set_text(GTK_ENTRY(bind),
			   binding_string ? binding_string : "");

	is_binding = is_event = FALSE;
	if (binding_string) {
		is_binding = TRUE;
		gtk_tree_model_iter_parent(model, &parent_iter, &iter);
		gtk_tree_model_get(model, &parent_iter,
				   COLUMN_ACTION, &action_string, -1);
	} else {
		gtk_tree_model_get(model, &iter,
				   COLUMN_ACTION, &action_string, -1);
		is_event = TRUE;
	}

	gtk_entry_set_text(GTK_ENTRY(action),
			   action_string ? action_string : "");

	gtk_widget_set_sensitive(grab, is_binding);
	gtk_widget_set_sensitive(add, is_binding || is_event);
	gtk_widget_set_sensitive(delete, is_binding || is_event);

	gtk_widget_set_sensitive(check_alt, is_binding && !is_modifier);
	gtk_widget_set_sensitive(check_ctrl, is_binding && !is_modifier);
	gtk_widget_set_sensitive(check_shift, is_binding && !is_modifier);
	gtk_widget_set_sensitive(check_gui, is_binding && !is_modifier);
	gtk_widget_set_sensitive(check_mod1, is_binding && !is_modifier);
	gtk_widget_set_sensitive(check_mod2, is_binding && !is_modifier);
	gtk_widget_set_sensitive(check_mod3, is_binding && !is_modifier);
	gtk_widget_set_sensitive(check_kbd, is_binding && !is_modifier);

	g_free(category_string);
	if (action_string)
		g_free(action_string);
	g_free(binding_string);
}

static void load_bindings(GtkTreeStore *store)
{
	int i;

	for (i = 0; i < EVENT_HASH_SIZE; i++) {
		struct input_event *e;

		e = event_hash[i];
		while (e) {
			load_binding(store, e);
			e = e->next;
		}
	}
}

static void apply_input_bindings(GtkTreeStore *store)
{
	GtkTreeModel *model;
	GtkTreeIter action_iter, binding_iter;

	model = GTK_TREE_MODEL(store);

	if (!gtk_tree_model_iter_children(model, &action_iter, NULL))
		return;

	do {
		gchar *category;

		gtk_tree_model_get(model, &action_iter, COLUMN_CATEGORY,
				   &category, -1);
		
		if (!gtk_tree_model_iter_children(model, &binding_iter,
						  &action_iter)) {
			continue;
		}

		do {
			int modmask;
			uint32_t emu_actionid;
			union input_new_event event;
			struct emu_action *emu_action;

			gtk_tree_model_get(model, &binding_iter,
					   COLUMN_ACTION_ID, &emu_actionid,
					   COLUMN_MODMASK, &modmask,
					   COLUMN_TYPE,
					   &event.common.type,
					   COLUMN_DEVICE,
					   &event.common.device,
					   COLUMN_INDEX,
					   &event.common.index,
					   COLUMN_MISC,
					   &event.common.misc,
					   -1);

			/* Skip empty bindings that the user may have
			   created but not edited.
			*/
			if (event.common.type == 0)
				continue;
					
			if (strcmp(category, "Modifiers") == 0) {
				input_add_modifier(&event, emu_actionid);
			} else {
				emu_action =
					input_lookup_emu_action(emu_actionid);
				if (!emu_action) {
					emu_action = input_insert_emu_action(emu_actionid);
				}
				input_insert_event(&event, modmask,
						   emu_action);
			}
		} while (gtk_tree_model_iter_next(model, &binding_iter));
		g_free(category);
	} while (gtk_tree_model_iter_next(model, &action_iter));
}

static void clear_store(GtkTreeStore *store)
{
	GtkTreeModel *model;
	GtkTreeIter action_iter, binding_iter;

	model = GTK_TREE_MODEL(store);

	if (!gtk_tree_model_iter_children(model, &action_iter, NULL))
		return;

	do {
		if (!gtk_tree_model_iter_children(model, &binding_iter,
						  &action_iter)) {
			continue;
		}

		while (gtk_tree_store_remove(store, &binding_iter)) {};

	} while (gtk_tree_model_iter_next(model, &action_iter));
}

static void set_all_callback(GtkButton *button, gpointer data)
{
	GtkTreePath *path;
	GtkTreeIter iter;

	gtk_tree_model_get_iter_first(filter_model, &iter);

	do {
		path = gtk_tree_model_get_path(filter_model, &iter);
		gtk_tree_view_set_cursor(GTK_TREE_VIEW(tree), path, NULL,
					 FALSE);

		grab_event_callback(button, data);
		if (!event_grabbed)
			break;
		gtk_tree_model_get_iter(filter_model, &iter, path);
		gtk_tree_path_free(path);
	} while (gtk_tree_model_iter_next(filter_model, &iter));

	gtk_tree_model_get_iter_first(filter_model, &iter);
	path = gtk_tree_model_get_path(filter_model, &iter);
	gtk_tree_view_set_cursor(GTK_TREE_VIEW(tree), path, NULL,
				 FALSE);
	gtk_tree_path_free(path);
}
