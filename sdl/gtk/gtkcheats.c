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
#include <errno.h>

#include "file_io.h"
#include "emu.h"
#include "video.h"
#include "gtkconfig.h"

static GtkWidget *address_entry, *value_entry, *compare_entry;
static GtkWidget *gg_entry, *rocky_entry, *desc_entry;
static GtkWidget *modify_button, *add_button, *clear_fields_button;

static int raw_toggled, gg_toggled, rocky_toggled;
static int raw_valid, gg_valid, rocky_valid;

static int current_address, current_value, current_compare;

extern struct emu *emu;

enum {
	ENABLED_COLUMN,
	ADDRESS_COLUMN,
	VALUE_COLUMN,
	COMPARE_COLUMN,
	GAMEGENIE_COLUMN,
	ROCKY_COLUMN,
	DESCRIPTION_COLUMN,
	CHEAT_COLUMN,
	NUM_COLUMNS,
};

enum {
	SORTID_ENABLED,
	SORTID_ADDRESS,
	SORTID_VALUE,
	SORTID_COMPARE,
	SORTID_GAMEGENIE,
	SORTID_ROCKY,
	SORTID_DESCRIPTION,
};

static gint sort_iter_compare_func(GtkTreeModel *model,
				   GtkTreeIter *a,
				   GtkTreeIter *b,
				   gpointer user_data)
{
	gint sort_column;
	gboolean a_bool, b_bool;
	const gchar *a_str, *b_str;
	gint rc;

	sort_column = GPOINTER_TO_INT(user_data);

	switch (sort_column) {
	case SORTID_ENABLED:
		gtk_tree_model_get(model, a, sort_column, &a_bool, -1);
		gtk_tree_model_get(model, b, sort_column, &b_bool, -1);
		if (a_bool == b_bool)
			rc = 0;
		else
			rc = (!a ? -1 : 1);
		break;
	default:
		gtk_tree_model_get(model, a, sort_column, &a_str, -1);
		gtk_tree_model_get(model, b, sort_column, &b_str, -1);

		if (!a_str || !b_str) {
			if (!a_str && !b_str)
				rc = 0;
			else
				rc = (!a_str ? -1 : 1);
		} else if (sort_column == SORTID_DESCRIPTION) {
			rc = g_utf8_collate(a_str, b_str);
		} else {
			rc = strcmp(a_str, b_str);
		}
	}

	return rc;
}

static void update_tree_row(GtkTreeStore *store, GtkTreeIter *row,
			    struct cheat *cheat) {
	char address[10];
	char value[8];
	char compare[8];
	char genie[9];
	char rocky[9];

	genie[0] = '\0';
	rocky[0] = '\0';

	snprintf(address, sizeof(address), "%04X", cheat->address);
	snprintf(value, sizeof(value), "%02X", cheat->value);
	raw_to_game_genie(cheat->address, cheat->value,
			  cheat->compare, genie);
	raw_to_pro_action_rocky(cheat->address, cheat->value,
				cheat->compare, rocky);
	if (cheat->compare >= 0) {
		snprintf(compare, sizeof(compare), "%02X",
			 cheat->compare);
	} else {
		compare[0] = '\0';
	}

	gtk_tree_store_set(store, row,
			   ENABLED_COLUMN, cheat->enabled,
			   ADDRESS_COLUMN, address,
			   VALUE_COLUMN, value,
			   COMPARE_COLUMN, compare,
			   GAMEGENIE_COLUMN, genie,
			   ROCKY_COLUMN, rocky,
			   DESCRIPTION_COLUMN, cheat->description,
			   CHEAT_COLUMN, cheat,
			   -1);
}

static void cheat_selected_callback(GtkTreeSelection *selection, gpointer user_data)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	gchar *address, *value, *compare, *gg, *rocky, *desc;
	gboolean rc;

	rc = gtk_tree_selection_get_selected(selection, &model, &iter);
	if (!rc)
		return;

	gtk_tree_model_get(model, &iter,
			   ADDRESS_COLUMN, &address,
			   VALUE_COLUMN, &value,
			   COMPARE_COLUMN, &compare,
			   GAMEGENIE_COLUMN, &gg,
			   ROCKY_COLUMN, &rocky,
			   DESCRIPTION_COLUMN, &desc,
			   -1);

	if (address)
		gtk_entry_set_text(GTK_ENTRY(address_entry), address);
	if (value)
		gtk_entry_set_text(GTK_ENTRY(value_entry), value);
	if (compare)
		gtk_entry_set_text(GTK_ENTRY(compare_entry), compare);
	if (gg)
		gtk_entry_set_text(GTK_ENTRY(gg_entry), gg);
	if (rocky)
		gtk_entry_set_text(GTK_ENTRY(rocky_entry), rocky);
	if (desc)
		gtk_entry_set_text(GTK_ENTRY(desc_entry), desc);

	g_free(address);
	g_free(value);
	g_free(compare);
	g_free(gg);
	g_free(rocky);
	g_free(desc);
}

/* Update the toggle for a tree view row based on the state of the
   cheat's enabled flag.
*/
static gboolean refresh_toggle(GtkTreeModel *model, GtkTreePath *path,
			       GtkTreeIter *iter, gpointer user_data)
{
	struct cheat *cheat;

	gtk_tree_model_get(model, iter, CHEAT_COLUMN, &cheat, -1);
	gtk_tree_store_set(GTK_TREE_STORE(model), iter, ENABLED_COLUMN,
			   cheat->enabled, -1);

	return FALSE;
}

static void cheat_toggled_callback(GtkCellRendererToggle *renderer, gchar *path,
				   gpointer user_data)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	struct cheat *cheat;
	gboolean rc;

	model = (GtkTreeModel *)user_data;
	rc = gtk_tree_model_get_iter_from_string(model, &iter, path);
	if (!rc)
		return;

	gtk_tree_model_get(model, &iter, CHEAT_COLUMN, &cheat, -1);
	if (cheat->enabled) {
		disable_cheat(emu->cheats, cheat);
	} else {
		/* Only one cheat can be enabled for a given address,
		   so when enabling a cheat others at the same address
		   are disabled.  If there were multiple cheats at this
		   address, all but the toggled one was disabled. Updated
		   the checkboxes to reflect the new status.
		*/
		rc = enable_cheat(emu->cheats, cheat);
		if (rc)
			gtk_tree_model_foreach(model, refresh_toggle, NULL);
	}

	gtk_tree_store_set(GTK_TREE_STORE(model), &iter, ENABLED_COLUMN,
			   cheat->enabled, -1);
}

static void sync_cheat_tree(GtkTreeStore *store, struct cheat *first)
{
	GtkTreeIter row;
	struct cheat *cheat;

	for (cheat = first; cheat; cheat = cheat->next) {
		gtk_tree_store_append(store, &row, NULL);
		update_tree_row(store, &row, cheat);
	}
}

static void load_callback(GtkButton *button, gpointer user_data)
{
	struct cheat **last;
	GtkTreeStore *store;
	GtkWidget *parent;
	char *cheat_path;

	char *file;
	char *shortcuts[] = { NULL, NULL };
	char *filter_patterns[] = { "*.[cC][hH][tT]", "*.[vV][cC][tT]",
	                            "*.[gG][eE][nN]", NULL };

	store = (GtkTreeStore *)user_data;
	parent = g_object_get_data(G_OBJECT(button), "parent");

	cheat_path = config_get_path(emu->config, CONFIG_DATA_DIR_CHEAT,
				     NULL, -1);
	shortcuts[0] = cheat_path;
	file = file_dialog(parent, "Select Cheat File",
			   GTK_FILE_CHOOSER_ACTION_OPEN,
			   "_Load", cheat_path, NULL,
			   "Cheat Files", filter_patterns,
			   shortcuts);

	free(cheat_path);

	last = &emu->cheats->cheat_list;
	while (*last) {
		last = &((*last)->next);
	}

	if (file) {
		cheat_load_file(emu, file);
		sync_cheat_tree(store, *last);
		g_free(file);
	}
}

static void import_from_db_callback(GtkButton *button, gpointer user_data)
{
	struct cheat **last;
	GtkTreeStore *store;
	GtkWidget *parent;
	char *cheat_db_path;
	char *cheat_db_file;
	int length;

	char *file;
	char *shortcuts[] = { NULL, NULL };
	char *filter_patterns[] = { "*.[cC][hH][tT]", "*.[vV][cC][tT]",
	                            "*.[gG][eE][nN]", NULL };

	store = (GtkTreeStore *)user_data;
	parent = g_object_get_data(G_OBJECT(button), "parent");

	cheat_db_path = config_get_path(emu->config,
			CONFIG_DATA_DIR_CHEAT_DB,
			NULL, 0);

	if (!cheat_db_path)
		return;

	length = strlen(cheat_db_path) + 1 +
		 strlen(emu->rom->info.name) + 4;

	cheat_db_file = malloc(length + 1);

	if (cheat_db_file) {
		snprintf(cheat_db_file, length + 1,
			 "%s%s%s.cht", cheat_db_path, PATHSEP,
			 emu->rom->info.name);
	}

	shortcuts[0] = cheat_db_path;

	file = file_dialog(parent, "Select Cheat File",
			   GTK_FILE_CHOOSER_ACTION_OPEN,
			   "_Load", cheat_db_file, NULL,
			   "Cheat Files", filter_patterns,
			   shortcuts);

	free(cheat_db_file);

	if (cheat_db_path)
		free(cheat_db_path);

	last = &emu->cheats->cheat_list;
	while (*last) {
		last = &((*last)->next);
	}

	if (file) {
		cheat_load_file(emu, file);
		sync_cheat_tree(store, *last);
		g_free(file);
	}
}

static void save_callback(GtkButton *button, gpointer user_data)
{
	GtkWidget *parent;
	char *cheat_path;
	const char *default_cheat_file;

	char *file;
	char *shortcuts[] = { NULL, NULL };
	char *filter_patterns[] = { "*.[cC][hH][tT]", NULL };

	default_cheat_file = emu->cheat_file;
	cheat_path = config_get_path(emu->config, CONFIG_DATA_DIR_CHEAT, NULL, 1);

	create_directory(cheat_path, 1, 0);

	parent = (GtkWidget *)user_data;
       
	shortcuts[0] = cheat_path;
	file = file_dialog(parent, "Save Cheat File",
			   GTK_FILE_CHOOSER_ACTION_SAVE,
			   "_Save", cheat_path, default_cheat_file,
			   "Cheat Files", filter_patterns,
			   shortcuts);

	free(cheat_path);

	if (file) {
		cheat_save_file(emu, file);
		g_free(file);
	}
}

static void modify_cheat_callback(GtkButton *button, gpointer user_data)
{
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeView *view;
	GtkTreeIter iter;
	struct cheat *cheat;
	const gchar *desc;
	gboolean rc;

	view = g_object_get_data(G_OBJECT(button), "view");

	selection = (GtkTreeSelection *)user_data;
	rc = gtk_tree_selection_get_selected(selection, &model, &iter);
	if (!rc)
		return;

	gtk_tree_model_get(model, &iter, CHEAT_COLUMN, &cheat, -1);

	cheat->address = current_address;
	cheat->value = current_value;
	cheat->compare = current_compare;

	if (cheat->description) {
		free(cheat->description);
		cheat->description = NULL;
	}

	desc = gtk_entry_get_text(GTK_ENTRY(desc_entry));

	/* FIXME trim whitespace? */

	if (desc && *desc)
		cheat->description = strdup(desc);

	update_tree_row(GTK_TREE_STORE(model), &iter, cheat);
	path = gtk_tree_model_get_path(model, &iter);

	/* Hack to force the contents of the edit/modify
	   entry boxes to be reloaded.
	*/
	gtk_tree_selection_unselect_path(selection, path);
	gtk_tree_view_set_cursor(view, path, NULL, FALSE);
}

static void add_cheat_callback(GtkButton *button, gpointer user_data)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreePath *path;
	GtkTreeView *view;
	struct cheat *cheat;
	const gchar *desc;

	model = (GtkTreeModel *)user_data;

	view = g_object_get_data(G_OBJECT(button), "view");

	cheat = malloc(sizeof(*cheat));
	if (!cheat)
		return;

	memset(cheat, 0, sizeof(*cheat));

	cheat->address = current_address;
	cheat->value = current_value;
	cheat->compare = current_compare;
	cheat->description = NULL;

	desc = gtk_entry_get_text(GTK_ENTRY(desc_entry));

	/* FIXME trim whitespace? */

	if (desc && *desc)
		cheat->description = strdup(desc);

	insert_cheat(emu->cheats, cheat);

	gtk_tree_store_append(GTK_TREE_STORE(model), &iter, NULL);
	update_tree_row(GTK_TREE_STORE(model), &iter, cheat);
	path = gtk_tree_model_get_path(model, &iter);
	gtk_tree_view_set_cursor(view, path, NULL, FALSE);
}

static void delete_cheat_callback(GtkButton *button, gpointer user_data)
{
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;
	struct cheat *cheat;
	gboolean rc;

	selection = (GtkTreeSelection *)user_data;
	rc = gtk_tree_selection_get_selected(selection, &model, &iter);
	if (!rc)
		return;

	gtk_tree_model_get(model, &iter, CHEAT_COLUMN, &cheat, -1);

	gtk_tree_store_remove(GTK_TREE_STORE(model), &iter);
	remove_cheat(emu->cheats, cheat);
}

static void clear_cheats_callback(GtkButton *button, gpointer user_data)
{
	GtkTreeModel *model;

	model = (GtkTreeModel *)user_data;
	gtk_tree_store_clear(GTK_TREE_STORE(model));
	cheat_deinit(emu->cheats);
}

static void clear_fields_callback(GtkButton *button, gpointer user_data)
{
	gtk_entry_set_text(GTK_ENTRY(address_entry), "");
	gtk_entry_set_text(GTK_ENTRY(value_entry), "");
	gtk_entry_set_text(GTK_ENTRY(compare_entry), "");
	gtk_entry_set_text(GTK_ENTRY(gg_entry), "");
	gtk_entry_set_text(GTK_ENTRY(rocky_entry), "");
	gtk_entry_set_text(GTK_ENTRY(desc_entry), "");
}

static void validate_raw(void)
{
	const gchar *address, *value, *compare;
	unsigned long a, v, c;
	char buffer[10];
	int a_valid, v_valid, c_valid;
	char *end;

	address = gtk_entry_get_text(GTK_ENTRY(address_entry));
	value = gtk_entry_get_text(GTK_ENTRY(value_entry));
	compare = gtk_entry_get_text(GTK_ENTRY(compare_entry));

	a_valid = v_valid = c_valid = 0;

	errno = 0;
	a = strtoul(address, &end, 16);
	if (!errno && (*address && !*end) && a < 0x10000)
		a_valid = 1;

	errno = 0;
	v = strtoul(value, &end, 16);
	if (!errno && (*value && !*end) && v < 0x100)
		v_valid = 1;

	if (!*compare) {
		c_valid = 1;
		c = -1;
	} else {
		errno = 0;
		c = strtoul(compare, &end, 16);
		if (!errno && (*compare && !*end) && c < 0x100)
			c_valid = 1;
	}

	if (a_valid && v_valid && c_valid) {
		current_address = a;
		current_value = v;
		current_compare = c;
		raw_to_game_genie(a, v, c, buffer);
		gtk_entry_set_text(GTK_ENTRY(gg_entry), buffer);
		raw_to_pro_action_rocky(a, v, c, buffer);
		gtk_entry_set_text(GTK_ENTRY(rocky_entry), buffer);
	} else {
			gtk_entry_set_text(GTK_ENTRY(gg_entry), "");
			gtk_entry_set_text(GTK_ENTRY(rocky_entry), "");
	}

	gtk_widget_set_sensitive(modify_button,
				 (a_valid && v_valid && c_valid));
	gtk_widget_set_sensitive(add_button,
				 (a_valid && v_valid && c_valid));
}

static void validate_gg(void)
{
	const gchar *gg;
	char buffer[10];
	int a, v, c;
	int valid;

	gg = gtk_entry_get_text(GTK_ENTRY(gg_entry));

	valid = 0;
	if (*gg && parse_game_genie_code(gg, &a, &v, &c) == 0)
		valid = 1;

	if (valid) {
		current_address = a;
		current_value = v;
		current_compare = c;
		snprintf(buffer, sizeof(buffer), "%04X", a);
		gtk_entry_set_text(GTK_ENTRY(address_entry), buffer);
		snprintf(buffer, sizeof(buffer), "%02X", v);
		gtk_entry_set_text(GTK_ENTRY(value_entry), buffer);
		if (c >= 0) {
			snprintf(buffer, sizeof(buffer), "%02X", c);
			gtk_entry_set_text(GTK_ENTRY(compare_entry), buffer);
			raw_to_pro_action_rocky(a, v, c, buffer);
			gtk_entry_set_text(GTK_ENTRY(rocky_entry), buffer);
		} else {
			gtk_entry_set_text(GTK_ENTRY(compare_entry), "");
			gtk_entry_set_text(GTK_ENTRY(rocky_entry), "");
		}
	} else {
			gtk_entry_set_text(GTK_ENTRY(address_entry), "");
			gtk_entry_set_text(GTK_ENTRY(value_entry), "");
			gtk_entry_set_text(GTK_ENTRY(compare_entry), "");
			gtk_entry_set_text(GTK_ENTRY(rocky_entry), "");
	}

	gtk_widget_set_sensitive(modify_button, valid);
	gtk_widget_set_sensitive(add_button, valid);
}

static void validate_rocky(void)
{
	const gchar *rocky;
	char buffer[10];
	int a, v, c;
	int valid;

	rocky = gtk_entry_get_text(GTK_ENTRY(rocky_entry));

	valid = 0;
	if (*rocky && parse_pro_action_rocky_code(rocky, &a, &v, &c) == 0)
		valid = 1;

	if (valid) {
		current_address = a;
		current_value = v;
		current_compare = c;
		raw_to_game_genie(a, v, c, buffer);
		gtk_entry_set_text(GTK_ENTRY(gg_entry), buffer);
		snprintf(buffer, sizeof(buffer), "%04X", a);
		gtk_entry_set_text(GTK_ENTRY(address_entry), buffer);
		snprintf(buffer, sizeof(buffer), "%02X", v);
		gtk_entry_set_text(GTK_ENTRY(value_entry), buffer);
		snprintf(buffer, sizeof(buffer), "%02X", c);
		gtk_entry_set_text(GTK_ENTRY(compare_entry), buffer);
	} else {
		gtk_entry_set_text(GTK_ENTRY(address_entry), "");
		gtk_entry_set_text(GTK_ENTRY(value_entry), "");
		gtk_entry_set_text(GTK_ENTRY(compare_entry), "");
		gtk_entry_set_text(GTK_ENTRY(gg_entry), "");
	}

	gtk_widget_set_sensitive(modify_button, valid);
	gtk_widget_set_sensitive(add_button, valid);
}

static void raw_changed_callback(GtkEditable *editable, gpointer user_data)
{
	if (!raw_toggled)
		return;

	validate_raw();
}

static void gg_changed_callback(GtkEditable *editable, gpointer user_data)
{
	if (!gg_toggled)
		return;

	validate_gg();
}

static void rocky_changed_callback(GtkEditable *editable, gpointer user_data)
{
	if (!rocky_toggled)
		return;

	validate_rocky();
}

static void raw_toggle_callback(GtkToggleButton *button, gpointer user_data)
{
	gboolean toggled;

	toggled = gtk_toggle_button_get_active(button);

	raw_toggled = toggled;
	if (!raw_toggled)
		raw_valid = 0;
	else
		validate_raw();

	gtk_widget_set_sensitive(address_entry, toggled);
	gtk_widget_set_sensitive(value_entry, toggled);
	gtk_widget_set_sensitive(compare_entry, toggled);
}

static void gg_toggle_callback(GtkToggleButton *button, gpointer user_data)
{
	GtkWidget *widget;
	gboolean toggled;

	toggled = gtk_toggle_button_get_active(button);
	widget = (GtkWidget *)user_data;
	gtk_widget_set_sensitive(widget, toggled);

	gg_toggled = toggled;
	if (!toggled)
		gg_valid = 0;
	else
		validate_gg();
}

static void rocky_toggle_callback(GtkToggleButton *button, gpointer user_data)
{
	GtkWidget *widget;
	gboolean toggled;

	toggled = gtk_toggle_button_get_active(button);
	widget = (GtkWidget *)user_data;
	gtk_widget_set_sensitive(widget, toggled);

	rocky_toggled = toggled;
	if (!toggled)
		rocky_valid = 0;
	else
		validate_rocky();
}

void gui_cheat_dialog(GtkWidget *widget, gpointer user_data)
{
	GtkWidget *dialog;
	GtkWidget *gtkwindow;
	GtkWidget *dialog_box;
	GtkWidget *cheat_tree;
	GtkWidget *cheat_window;
	GtkWidget *button_box;
	GtkWidget *box;
	GtkTreeStore *cheat_store;
	GtkTreeSortable *sortable;
	GtkDialogFlags flags;
	GtkWidget *button;
	GtkCellRenderer *text_renderer, *enabled_checkbox_renderer;
	GtkTreeViewColumn *column;
	GtkTreeSelection *selection;
	GtkWidget *label;
	GtkWidget *grid;
	GtkWidget *radio_raw, *radio_genie, *radio_rocky;
	int paused;
	int col;

	video_show_cursor(1);

	gtkwindow = (GtkWidget *)user_data;
	flags = GTK_DIALOG_DESTROY_WITH_PARENT;
	dialog = gtk_dialog_new_with_buttons("Cheats",
					     GTK_WINDOW(gtkwindow), flags,
					     "_OK", GTK_RESPONSE_ACCEPT,
					     NULL);
	dialog_box = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

	button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);

	gtk_window_set_transient_for(GTK_WINDOW(dialog),
				     GTK_WINDOW(gtkwindow));

	cheat_window = gtk_scrolled_window_new(NULL, NULL);
	gtk_box_pack_start(GTK_BOX(dialog_box), cheat_window, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(dialog_box), button_box, FALSE, FALSE, 0);
	gtk_widget_set_size_request(cheat_window, 800, 200);

	cheat_tree = gtk_tree_view_new();
	gtk_container_add(GTK_CONTAINER(cheat_window), cheat_tree);

	gtk_tree_view_set_fixed_height_mode(GTK_TREE_VIEW(cheat_tree),
					    FALSE);

	/* columns: enabled addr value genie compare description cheat_pointer */
	cheat_store = gtk_tree_store_new(NUM_COLUMNS, G_TYPE_BOOLEAN,
					 G_TYPE_STRING, G_TYPE_STRING,
					 G_TYPE_STRING, G_TYPE_STRING,
					 G_TYPE_STRING, G_TYPE_STRING,
					 G_TYPE_POINTER);

	sortable = GTK_TREE_SORTABLE(cheat_store);

	for (col = 0; col < NUM_COLUMNS - 1; col++) {
		gtk_tree_sortable_set_sort_func(sortable, col,
						sort_iter_compare_func,
						GINT_TO_POINTER(col),
						NULL);
	}

	gtk_tree_view_set_model(GTK_TREE_VIEW(cheat_tree),
				GTK_TREE_MODEL(cheat_store));
	g_object_unref(G_OBJECT(cheat_store));

	text_renderer = gtk_cell_renderer_text_new();
	enabled_checkbox_renderer = gtk_cell_renderer_toggle_new();
	g_signal_connect(G_OBJECT(enabled_checkbox_renderer), "toggled",
			 G_CALLBACK(cheat_toggled_callback), cheat_store);

	column = gtk_tree_view_column_new_with_attributes("Enable",
							  enabled_checkbox_renderer,
							  "active",
							  ENABLED_COLUMN, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(cheat_tree), column);
	gtk_tree_view_column_set_sort_column_id(column, SORTID_ENABLED);

	column = gtk_tree_view_column_new_with_attributes("Address",
							  text_renderer,
							  "text", ADDRESS_COLUMN,
							  NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(cheat_tree), column);
	gtk_tree_view_column_set_sort_column_id(column, SORTID_ADDRESS);

	column = gtk_tree_view_column_new_with_attributes("Value",
							  text_renderer,
							  "text", VALUE_COLUMN,
							  NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(cheat_tree), column);
	gtk_tree_view_column_set_sort_column_id(column, SORTID_VALUE);

	column = gtk_tree_view_column_new_with_attributes("Compare",
							  text_renderer,
							  "text", COMPARE_COLUMN,
							  NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(cheat_tree), column);
	gtk_tree_view_column_set_sort_column_id(column, SORTID_COMPARE);

	column = gtk_tree_view_column_new_with_attributes("Game Genie",
							  text_renderer,
							  "text",
							  GAMEGENIE_COLUMN,
							  NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(cheat_tree), column);
	gtk_tree_view_column_set_sort_column_id(column, SORTID_GAMEGENIE);

	column = gtk_tree_view_column_new_with_attributes("Pro Action Rocky",
							  text_renderer,
							  "text",
							  ROCKY_COLUMN,
							  NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(cheat_tree), column);
	gtk_tree_view_column_set_sort_column_id(column, SORTID_ROCKY);

	column = gtk_tree_view_column_new_with_attributes("Description",
							  text_renderer,
							  "text",
							  DESCRIPTION_COLUMN,
							  NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(cheat_tree), column);
	gtk_tree_view_column_set_sort_column_id(column, SORTID_DESCRIPTION);

	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_visible(column, FALSE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(cheat_tree), column);

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(cheat_tree));
	g_signal_connect(G_OBJECT(selection), "changed",
			 G_CALLBACK(cheat_selected_callback), NULL);

	/* Set up buttons */
	button = gtk_button_new_with_mnemonic("_Remove");
	gtk_box_pack_start(GTK_BOX(button_box), button, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(delete_cheat_callback), selection);

	button = gtk_button_new_with_mnemonic("Rem_ove All");
	gtk_box_pack_start(GTK_BOX(button_box), button, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(clear_cheats_callback), cheat_store);

	button = gtk_button_new_with_mnemonic("_Load...");
	gtk_box_pack_start(GTK_BOX(button_box), button, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(load_callback), cheat_store);
	g_object_set_data(G_OBJECT(button), "parent", dialog);

	button = gtk_button_new_with_mnemonic("_Import from Database...");
	gtk_box_pack_start(GTK_BOX(button_box), button, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(import_from_db_callback), cheat_store);
	g_object_set_data(G_OBJECT(button), "parent", dialog);

	button = gtk_button_new_with_mnemonic("_Save...");
	gtk_box_pack_start(GTK_BOX(button_box), button, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(save_callback), dialog);

	box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 32);
	gtk_box_pack_start(GTK_BOX(dialog_box), box, FALSE, FALSE, 8);

	grid = gtk_grid_new();
	gtk_grid_set_column_spacing(GTK_GRID(grid), 4);
	gtk_grid_set_row_spacing(GTK_GRID(grid), 4);
	gtk_box_pack_start(GTK_BOX(box), grid, FALSE, FALSE, 0);

	/* Set up entry boxes for raw code */
	radio_raw = gtk_radio_button_new(NULL);
	g_signal_connect(G_OBJECT(radio_raw), "toggled",
			 G_CALLBACK(raw_toggle_callback), NULL);
	
	label = gtk_label_new_with_mnemonic("Ra_w");
	gtk_grid_attach(GTK_GRID(grid), radio_raw, 0, 0, 1, 1);
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), radio_raw);
	gtk_grid_attach(GTK_GRID(grid), label, 1, 0, 1, 1);
	label = gtk_label_new_with_mnemonic("Addr_ess:");
	address_entry = gtk_entry_new();
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), address_entry);
	//gtk_entry_set_max_width_chars(GTK_ENTRY(address_entry), 6);
	gtk_entry_set_width_chars(GTK_ENTRY(address_entry), 6);
	gtk_grid_attach(GTK_GRID(grid), label, 1, 1, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), address_entry, 2, 1, 1, 1);
	g_signal_connect(G_OBJECT(address_entry), "changed",
			 G_CALLBACK(raw_changed_callback), NULL);

	label = gtk_label_new_with_mnemonic("_Value:");
	value_entry = gtk_entry_new();
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), value_entry);
	//gtk_entry_set_max_width_chars(GTK_ENTRY(value_entry), 4);
	gtk_entry_set_width_chars(GTK_ENTRY(value_entry), 4);
	gtk_grid_attach(GTK_GRID(grid), label, 1, 2, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), value_entry, 2, 2, 1, 1);
	g_signal_connect(G_OBJECT(value_entry), "changed",
			 G_CALLBACK(raw_changed_callback), NULL);

	label = gtk_label_new_with_mnemonic("C_ompare:");
	compare_entry = gtk_entry_new();
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), compare_entry);
	//gtk_entry_set_max_width_chars(GTK_ENTRY(compare_entry), 4);
	gtk_entry_set_width_chars(GTK_ENTRY(compare_entry), 4);
	gtk_grid_attach(GTK_GRID(grid), label, 1, 3, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), compare_entry, 2, 3, 1, 1);
	g_signal_connect(G_OBJECT(compare_entry), "changed",
			 G_CALLBACK(raw_changed_callback), NULL);

	grid = gtk_grid_new();
	gtk_grid_set_column_spacing(GTK_GRID(grid), 4);
	gtk_grid_set_row_spacing(GTK_GRID(grid), 4);
	gtk_box_pack_start(GTK_BOX(box), grid, FALSE, FALSE, 0);

	radio_genie = gtk_radio_button_new(NULL);
	label = gtk_label_new_with_mnemonic("_Game Genie");
	gtk_grid_attach(GTK_GRID(grid), radio_genie, 0, 0, 1, 1);
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), radio_genie);
	gtk_grid_attach(GTK_GRID(grid), label, 1, 0, 1, 1);
	gtk_radio_button_join_group(GTK_RADIO_BUTTON(radio_genie),
				    GTK_RADIO_BUTTON(radio_raw));
	gg_entry = gtk_entry_new();
	gtk_widget_set_sensitive(gg_entry, FALSE);
	//gtk_entry_set_max_width_chars(GTK_ENTRY(gg_entry), 8);
	gtk_entry_set_width_chars(GTK_ENTRY(gg_entry), 8);
	gtk_grid_attach(GTK_GRID(grid), gg_entry, 1, 1, 1, 1);
	g_signal_connect(G_OBJECT(radio_genie), "toggled",
			 G_CALLBACK(gg_toggle_callback), gg_entry);
	g_signal_connect(G_OBJECT(gg_entry), "changed",
			 G_CALLBACK(gg_changed_callback), NULL);

	radio_rocky = gtk_radio_button_new(NULL);
	label = gtk_label_new_with_mnemonic("_Pro Action Rocky");
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), radio_rocky);
	gtk_grid_attach(GTK_GRID(grid), radio_rocky, 0, 2, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), label, 1, 2, 1, 1);
	gtk_radio_button_join_group(GTK_RADIO_BUTTON(radio_rocky),
				    GTK_RADIO_BUTTON(radio_raw));
	rocky_entry = gtk_entry_new();
	gtk_widget_set_sensitive(rocky_entry, FALSE);
	//gtk_entry_set_max_width_chars(GTK_ENTRY(rocky_entry), 10);
	gtk_entry_set_width_chars(GTK_ENTRY(rocky_entry), 10);
	gtk_grid_attach(GTK_GRID(grid), rocky_entry, 1, 3, 1, 1);
	g_signal_connect(G_OBJECT(radio_rocky), "toggled",
			 G_CALLBACK(rocky_toggle_callback), rocky_entry);
	g_signal_connect(G_OBJECT(rocky_entry), "changed",
			 G_CALLBACK(rocky_changed_callback), NULL);

	grid = gtk_grid_new();
	gtk_grid_set_column_spacing(GTK_GRID(grid), 4);
	gtk_grid_set_row_spacing(GTK_GRID(grid), 4);
	gtk_box_pack_start(GTK_BOX(box), grid, FALSE, FALSE, 0);
	label = gtk_label_new_with_mnemonic("_Description:");
	desc_entry = gtk_entry_new();
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), desc_entry);
	gtk_entry_set_width_chars(GTK_ENTRY(desc_entry), 35);
	gtk_grid_attach(GTK_GRID(grid), label, 0, 0, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), desc_entry, 1, 0, 3, 1);

	/* Hack to create two empty rows in the grid */
	label = gtk_label_new_with_mnemonic("");
	gtk_grid_attach(GTK_GRID(grid), label, 1, 1, 3, 1);
	label = gtk_label_new_with_mnemonic("");
	gtk_grid_attach(GTK_GRID(grid), label, 1, 2, 3, 1);

	button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
	gtk_grid_attach(GTK_GRID(grid), button_box, 1, 3, 3, 1);
	modify_button = gtk_button_new_with_mnemonic("_Modify");
	add_button = gtk_button_new_with_mnemonic("_Add");
	clear_fields_button = gtk_button_new_with_mnemonic("_Clear");
	gtk_box_pack_start(GTK_BOX(button_box), modify_button, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(button_box), add_button, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(button_box), clear_fields_button,
			   TRUE, TRUE, 0);

	g_signal_connect(G_OBJECT(modify_button), "clicked",
			 G_CALLBACK(modify_cheat_callback), selection);
	g_object_set_data(G_OBJECT(modify_button), "view", cheat_tree);
	g_signal_connect(G_OBJECT(add_button), "clicked",
			 G_CALLBACK(add_cheat_callback), cheat_store);
	g_signal_connect(G_OBJECT(clear_fields_button), "clicked",
			 G_CALLBACK(clear_fields_callback), NULL);
	g_object_set_data(G_OBJECT(add_button), "view", cheat_tree);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_raw),
				     TRUE);
	raw_toggled = 1;
	gg_toggled = 0;
	rocky_toggled = 0;

	sync_cheat_tree(cheat_store, emu->cheats->cheat_list);

	paused = emu_paused(emu);
	emu_pause(emu, 1);
	gtk_widget_show_all(dialog);

	gtk_dialog_run(GTK_DIALOG(dialog));

	if (!paused)
		emu_pause(emu, 0);

	gtk_widget_destroy(dialog);
	video_show_cursor(0);
}
