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

#include <gtk/gtk.h>
#include <errno.h>

#include "emu.h"
#include "input.h"
#include "video.h"
#include "gtkconfig.h"

extern struct emu *emu;

static GtkTreeStore *store;
static GtkWidget *tree;

enum binding_columns {
	COLUMN_ID,
	COLUMN_NAME,
	COLUMN_GUID,
	COLUMN_GAME_CONTROLLER,
};

static void clear_store(GtkTreeStore *store)
{
	GtkTreeModel *model;
	GtkTreeIter iter;

	model = GTK_TREE_MODEL(store);

	if (!gtk_tree_model_get_iter_first(model, &iter))
		return;

	while (gtk_tree_store_remove(store, &iter));
}

static void setup_tree(GtkWidget **treeptr, GtkTreeStore **storeptr,
		       GtkTreeSelection **selectionptr)
{
	GtkWidget *tree;
	GtkTreeStore *store;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkTreeIter iter;
	GtkTreeSelection *selection;
	int i;
	int num_joysticks;
	char buffer[80];

	tree = gtk_tree_view_new();
	store = gtk_tree_store_new(4, G_TYPE_INT, G_TYPE_STRING, G_TYPE_STRING,
				   G_TYPE_STRING, -1);
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_set_fixed_height_mode(GTK_TREE_VIEW(tree), FALSE);
	gtk_tree_view_set_model(GTK_TREE_VIEW(tree), GTK_TREE_MODEL(store));
	g_object_unref(G_OBJECT(store));

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));

	column = gtk_tree_view_column_new_with_attributes("ID", renderer,
							  "text", COLUMN_ID, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);

	column = gtk_tree_view_column_new_with_attributes("Device", renderer,
							  "text", COLUMN_NAME, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);

	column = gtk_tree_view_column_new_with_attributes("GUID", renderer,
							  "text", COLUMN_GUID, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);

	column = gtk_tree_view_column_new_with_attributes("GameController", renderer,
							  "text", COLUMN_GAME_CONTROLLER, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);

	num_joysticks = input_get_num_joysticks();
	gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), &iter);
	for (i = 0; i < num_joysticks; i++) {
		const char *name;
		int is_gamecontroller;

		name = input_get_joystick_name(i);
		if (name == NULL)
			continue;

		input_get_joystick_guid(i, buffer, sizeof(buffer));
		is_gamecontroller = input_joystick_is_gamecontroller(i);

		if (is_gamecontroller < 0)
			is_gamecontroller = 0;

		gtk_tree_store_append(store, &iter, NULL);
		gtk_tree_store_set(store, &iter,
				   COLUMN_ID, i,
				   COLUMN_NAME, name,
				   COLUMN_GUID, buffer,
				   COLUMN_GAME_CONTROLLER,
				   is_gamecontroller ? "Yes" : "No",
				   -1);

	}

	*treeptr = tree;
	*storeptr = store;
	*selectionptr = selection;
}

static void configuration_setup_joysticks(GtkWidget *dialog, struct config *config)
{
	GtkWidget *box, *hbox;
	GtkWidget *grid;
	GtkWidget *list_window;
	GtkTreeSelection *selection;

	box = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	grid = gtk_grid_new();

	gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
	gtk_grid_set_row_spacing(GTK_GRID(grid), 8);

	setup_tree(&tree, &store, &selection);
	list_window = gtk_scrolled_window_new(NULL, NULL);
	gtk_box_pack_start(GTK_BOX(box), list_window, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(box), hbox, FALSE, FALSE, 8);

	gtk_box_pack_start(GTK_BOX(box), grid, TRUE, TRUE, 0);
	gtk_container_add(GTK_CONTAINER(list_window), tree);
	gtk_widget_set_size_request(list_window, 600, 250);

}

void gui_joystick_dialog(GtkWidget *widget, gpointer user_data)
{
	GtkWidget *dialog;
	GtkWidget *gtkwindow;
	GtkDialogFlags flags;
	int paused;

	gtkwindow = (GtkWidget *)user_data;
	flags = GTK_DIALOG_DESTROY_WITH_PARENT;
	dialog = gtk_dialog_new_with_buttons("Joystick Info",
					     GTK_WINDOW(gtkwindow), flags,
					     "_OK", GTK_RESPONSE_ACCEPT,
					     NULL);

	gtk_window_set_transient_for(GTK_WINDOW(dialog),
				     GTK_WINDOW(gtkwindow));

	configuration_setup_joysticks(dialog, NULL);

	paused = emu_paused(emu);
	video_show_cursor(1);
	emu_pause(emu, 1);
	gtk_widget_show_all(dialog);

	gtk_dialog_run(GTK_DIALOG(dialog));

	clear_store(store);

	if (!paused && emu_loaded(emu))
		emu_pause(emu, 0);

	gtk_widget_destroy(dialog);
	video_show_cursor(0);
}
