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
#include "gtkconfig.h"
#include "video.h"
#include "audio.h"

extern struct emu *emu;

void int_scale_callback(GtkScale *scale, gpointer user_data)
{
	int *ptr;

	ptr = (int *)user_data;

	*ptr = gtk_range_get_value(GTK_RANGE(scale));
}

void toggle_callback(GtkToggleButton *toggle, gpointer user_data)
{
	int *ptr;

	ptr = (int *)user_data;

	*ptr = gtk_toggle_button_get_active(toggle);
}

void spinbutton_int_callback(GtkSpinButton *button, gpointer user_data)
{
	int *ptr;

	ptr = (int *)user_data;

	*ptr = gtk_spin_button_get_value(button);
}

void spinbutton_double_callback(GtkSpinButton *button, gpointer user_data)
{
	double *ptr;

	ptr = (double *)user_data;

	*ptr = gtk_spin_button_get_value(button);
}

void combo_box_callback(GtkComboBox *combo, gpointer user_data)
{
	char **ptr;
	char *tmp;

	ptr = (char **)user_data;

	tmp = strdup(gtk_combo_box_get_active_id(combo));

	free(*ptr);
	*ptr = tmp;
}

void integer_combo_box_callback(GtkComboBox *combo, gpointer user_data)
{
	int *ptr;
	const char *tmp;
	long value;

	ptr = (int *)user_data;

	tmp = gtk_combo_box_get_active_id(combo);
	value = strtol(tmp, NULL, 0);
	*ptr = value;
}

void entry_callback(GtkEntry *entry, gpointer user_data)
{
	char **ptr;
	char *tmp;

	ptr = (char **)user_data;

	tmp = strdup(gtk_entry_get_text(GTK_ENTRY(entry)));

	free(*ptr);
	*ptr = tmp;
}

void default_button_int_scale_cb(GtkDialog *dialog,
					gint response_id,
					gpointer user_data)
{
	GtkWidget *widget;
	struct config *config;
	char *name;
	int *ptr;

	if (response_id != RESET_TO_DEFAULT)
		return;

	widget = (GtkWidget *)user_data;
	config = (struct config *)g_object_get_data(G_OBJECT(widget),
						    "config_struct");
	name = (char *)g_object_get_data(G_OBJECT(widget),
					 "config_name");
	ptr = config_get_data_ptr(config, name);

	config_reset_value(config, name);
	gtk_range_set_value(GTK_RANGE(widget), *ptr);
}

void default_button_int_spinbutton_cb(GtkDialog *dialog,
					     gint response_id,
					     gpointer user_data)
{
	GtkWidget *widget;
	struct config *config;
	char *name;
	int *ptr;

	if (response_id != RESET_TO_DEFAULT)
		return;

	widget = (GtkWidget *)user_data;
	config = (struct config *)g_object_get_data(G_OBJECT(widget),
						    "config_struct");
	name = (char *)g_object_get_data(G_OBJECT(widget),
					 "config_name");
	ptr = config_get_data_ptr(config, name);

	config_reset_value(config, name);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(widget), *ptr);
}

void default_button_double_spinbutton_cb(GtkDialog *dialog, gint response_id,
						gpointer user_data)
{
	GtkWidget *widget;
	struct config *config;
	char *name;
	double *ptr;

	if (response_id != RESET_TO_DEFAULT)
		return;

	widget = (GtkWidget *)user_data;
	config = (struct config *)g_object_get_data(G_OBJECT(widget),
						    "config_struct");
	name = (char *)g_object_get_data(G_OBJECT(widget),
					 "config_name");
	ptr = config_get_data_ptr(config, name);

	config_reset_value(config, name);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(widget), *ptr);
}

void default_button_checkbox_cb(GtkDialog *dialog, gint response_id,
				       gpointer user_data)
{
	GtkWidget *widget;
	struct config *config;
	char *name;
	int *ptr;

	if (response_id != RESET_TO_DEFAULT)
		return;

	widget = (GtkWidget *)user_data;
	config = (struct config *)g_object_get_data(G_OBJECT(widget),
						    "config_struct");
	name = (char *)g_object_get_data(G_OBJECT(widget),
					 "config_name");
	ptr = config_get_data_ptr(config, name);

	config_reset_value(config, name);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), *ptr);
}

void default_button_combo_cb(GtkDialog *dialog, gint response_id,
				    gpointer user_data)
{
	GtkWidget *widget;
	struct config *config;
	char *name;
	char **ptr;

	if (response_id != RESET_TO_DEFAULT)
		return;

	widget = (GtkWidget *)user_data;
	config = (struct config *)g_object_get_data(G_OBJECT(widget),
						    "config_struct");
	name = (char *)g_object_get_data(G_OBJECT(widget),
					 "config_name");
	ptr = config_get_data_ptr(config, name);

	config_reset_value(config, name);
	gtk_combo_box_set_active_id(GTK_COMBO_BOX(widget), *ptr);
}

void default_button_integer_combo_cb(GtkDialog *dialog, gint response_id,
					    gpointer user_data)
{
	GtkWidget *widget;
	struct config *config;
	char *name;
	int *ptr;
	char buffer[32];

	if (response_id != RESET_TO_DEFAULT)
		return;

	widget = (GtkWidget *)user_data;
	config = (struct config *)g_object_get_data(G_OBJECT(widget),
						    "config_struct");
	name = (char *)g_object_get_data(G_OBJECT(widget),
					 "config_name");
	ptr = config_get_data_ptr(config, name);

	snprintf(buffer, sizeof(buffer), "%d", *ptr);

	config_reset_value(config, name);
	gtk_combo_box_set_active_id(GTK_COMBO_BOX(widget), buffer);
}

void default_button_entry_cb(GtkDialog *dialog, gint response_id,
				    gpointer user_data)
{
	GtkWidget *widget;
	struct config *config;
	char *name;
	char **ptr;

	widget = (GtkWidget *)user_data;
	if (response_id == GTK_RESPONSE_ACCEPT) {
		g_signal_emit_by_name(G_OBJECT(widget), "activate", NULL);
		return;
	} else if (response_id != RESET_TO_DEFAULT) {
		return;
	}

	config = (struct config *)g_object_get_data(G_OBJECT(widget),
						    "config_struct");
	name = (char *)g_object_get_data(G_OBJECT(widget),
					 "config_name");
	ptr = config_get_data_ptr(config, name);

	config_reset_value(config, name);

	gtk_entry_set_text(GTK_ENTRY(widget), *ptr ? *ptr : "");
}

GtkWidget *config_combo_box(GtkWidget *dialog,
				   struct config *config,
				   const char *name)
{
	GtkWidget *combo;
	int i;
	const char **data;
	const char **valid_values;
	const char **valid_value_names;
	int valid_value_count;

	combo = gtk_combo_box_text_new();
	if (!combo)
		return NULL;

	data = config_get_data_ptr(config, name);
	valid_values =
		(const char **)config_get_valid_values(name,
						       &valid_value_count,
						       &valid_value_names);

	g_object_set_data(G_OBJECT(combo), "config_struct", config);
	g_object_set_data(G_OBJECT(combo), "config_name", (char *)name);

	for (i = 0; i < valid_value_count; i++) {
		gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combo),
					  valid_values[i],
					  valid_value_names[i]);
	}

	if (*data)
		gtk_combo_box_set_active_id(GTK_COMBO_BOX(combo), *data);

	g_signal_connect(G_OBJECT(combo), "changed",
			 G_CALLBACK(combo_box_callback), data);

	g_signal_connect(G_OBJECT(dialog), "response",
			 G_CALLBACK(default_button_combo_cb),
			 combo);

	return combo;
}

GtkWidget *config_integer_combo_box(GtkWidget *dialog,
					   struct config *config,
					   const char *name)
{
	GtkWidget *combo;
	int i;
	const int *data;
	const int *valid_values;
	int valid_value_count;
	char buffer[32];

	combo = gtk_combo_box_text_new();
	if (!combo)
		return NULL;

	data = config_get_data_ptr(config, name);
	valid_values =
		(const int*)config_get_valid_values(name,
						   &valid_value_count,
						   NULL);

	g_object_set_data(G_OBJECT(combo), "config_struct", config);
	g_object_set_data(G_OBJECT(combo), "config_name", (char *)name);

	for (i = 0; i < valid_value_count; i++) {
		snprintf(buffer, sizeof(buffer), "%d", valid_values[i]);
		gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combo),
					  buffer, buffer);

		if (*data == valid_values[i]) {
			gtk_combo_box_set_active_id(GTK_COMBO_BOX(combo),
						    buffer);
		}
	}

	g_signal_connect(G_OBJECT(combo), "changed",
			 G_CALLBACK(integer_combo_box_callback), (int *)data);

	g_signal_connect(G_OBJECT(dialog), "response",
			 G_CALLBACK(default_button_integer_combo_cb),
			 combo);

	return combo;
}

GtkWidget *config_int_scale(int vertical, int size,
				   int min, int max, GtkWidget *dialog,
				   struct config *config, const char *name)
{
	GtkWidget *scale;
	GtkOrientation orientation;
	int *data;

	if (vertical)
		orientation = GTK_ORIENTATION_VERTICAL;
	else
		orientation = GTK_ORIENTATION_HORIZONTAL;

	scale = gtk_scale_new_with_range(orientation, min, max, 1);
	if (!scale)
		return NULL;

	if (vertical)
		gtk_widget_set_size_request(scale, -1, size);
	else
		gtk_widget_set_size_request(scale, size, -1);

	gtk_range_set_increments(GTK_RANGE(scale), 1.0, 1.0);

	data = config_get_data_ptr(config, name);

	g_object_set_data(G_OBJECT(scale), "config_struct", config);
	g_object_set_data(G_OBJECT(scale), "config_name", (char *)name);

	if (*data)
		gtk_range_set_value(GTK_RANGE(scale), *data);

	g_signal_connect(G_OBJECT(scale), "value-changed",
			 G_CALLBACK(int_scale_callback), data);

	g_signal_connect(G_OBJECT(dialog), "response",
			 G_CALLBACK(default_button_int_scale_cb),
			 scale);

	return scale;
}
			       

GtkWidget *config_checkbox(GtkWidget *dialog,
				  const char *label,
				  struct config *config,
				  const char *name)
{
	GtkWidget *checkbox;
	int *data;

	checkbox = gtk_check_button_new_with_mnemonic(label);
	if (!checkbox)
		return NULL;

	data = config_get_data_ptr(config, name);

	g_object_set_data(G_OBJECT(checkbox), "config_struct", config);
	g_object_set_data(G_OBJECT(checkbox), "config_name", (char *)name);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbox), *data);

	g_signal_connect(G_OBJECT(checkbox), "toggled",
			 G_CALLBACK(toggle_callback), data);

	g_signal_connect(G_OBJECT(dialog), "response",
			 G_CALLBACK(default_button_checkbox_cb),
			 checkbox);

	return checkbox;
}

GtkWidget *config_int_spinbutton(GtkWidget *dialog,
					struct config *config,
					const char *name)
{
	GtkWidget *spinbutton;
	int *data;
	int min, max, step;

	min = config_get_int_min(name);
	max = config_get_int_max(name);
	step = 1;

	spinbutton = gtk_spin_button_new_with_range(min, max, step);
	if (!spinbutton)
		return NULL;

	data = config_get_data_ptr(config, name);

	g_object_set_data(G_OBJECT(spinbutton), "config_struct", config);
	g_object_set_data(G_OBJECT(spinbutton), "config_name", (char *)name);

	gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinbutton), *data);

	g_signal_connect(G_OBJECT(spinbutton), "value-changed",
			 G_CALLBACK(spinbutton_int_callback), data);

	g_signal_connect(G_OBJECT(dialog), "response",
			 G_CALLBACK(default_button_int_spinbutton_cb),
			 spinbutton);

	return spinbutton;
}

GtkWidget *config_double_spinbutton(GtkWidget *dialog,
					   struct config *config,
					   double step,
					   const char *name)
{
	GtkWidget *spinbutton;
	double *data;
	double min;
	double max;

	min = config_get_float_min(name);
	max = config_get_float_max(name);

	spinbutton = gtk_spin_button_new_with_range(min, max, step);
	if (!spinbutton)
		return NULL;

	data = config_get_data_ptr(config, name);

	g_object_set_data(G_OBJECT(spinbutton), "config_struct", config);
	g_object_set_data(G_OBJECT(spinbutton), "config_name", (char *)name);

	gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinbutton), *data);

	g_signal_connect(G_OBJECT(spinbutton), "value-changed",
			 G_CALLBACK(spinbutton_double_callback), data);

	g_signal_connect(G_OBJECT(dialog), "response",
			 G_CALLBACK(default_button_double_spinbutton_cb),
			 spinbutton);

	return spinbutton;
}

GtkWidget *config_entry(GtkWidget *dialog,
			       struct config *config,
			       const char *name)
{
	GtkWidget *entry;
	char **data;

	entry = gtk_entry_new();
	if (!entry)
		return NULL;

	data = config_get_data_ptr(config, name);

	g_object_set_data(G_OBJECT(entry), "config_struct", config);
	g_object_set_data(G_OBJECT(entry), "config_name", (char *)name);

	if (*data)
		gtk_entry_set_text(GTK_ENTRY(entry), *data);

	g_signal_connect(G_OBJECT(entry), "activate",
			 G_CALLBACK(entry_callback), data);

	g_signal_connect(G_OBJECT(dialog), "response",
			 G_CALLBACK(default_button_entry_cb),
			 entry);

	return entry;
}

void file_chooser_callback(GtkWidget *widget, gpointer user_data)
{
	GtkEntry *entry;
	GtkWidget *dialog;
	char *file;
	
	dialog = g_object_get_data(G_OBJECT(widget), "dialog");
	
	entry = (GtkEntry *)user_data;
	file = file_dialog(dialog, "Select File",
			   GTK_FILE_CHOOSER_ACTION_OPEN,
			   NULL,
			   gtk_entry_get_text(GTK_ENTRY(entry)),
			   NULL,
			   NULL, NULL, NULL);

	if (file) {
		gtk_entry_set_text(GTK_ENTRY(entry), file);
		g_free(file);
	}
}

void folder_chooser_callback(GtkWidget *widget, gpointer user_data)
{
	char *file;
	GtkWidget *dialog;
	GtkEntry *entry;

	dialog = g_object_get_data(G_OBJECT(widget), "dialog");
	
	entry = (GtkEntry *)user_data;
	file = file_dialog(dialog, "Select Folder",
			   GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER,
			   NULL,
			   gtk_entry_get_text(GTK_ENTRY(entry)),
			   NULL,
			   NULL, NULL, NULL);

	if (file) {
		gtk_entry_set_text(GTK_ENTRY(entry), file);
		g_free(file);
	}
}

static void configuration_setup_volume(GtkWidget *dialog, struct config *config)
{
	GtkWidget *volume_control_grid;

	GtkWidget *scale_master_volume;
	GtkWidget *scale_apu_pulse0_volume;
	GtkWidget *scale_apu_pulse1_volume;
	GtkWidget *scale_apu_triangle_volume;
	GtkWidget *scale_apu_noise_volume;
	GtkWidget *scale_apu_dmc_volume;
	GtkWidget *scale_vrc6_pulse0_volume;
	GtkWidget *scale_vrc6_pulse1_volume;
	GtkWidget *scale_vrc6_sawtooth_volume;
	GtkWidget *scale_vrc7_channel0_volume;
	GtkWidget *scale_vrc7_channel1_volume;
	GtkWidget *scale_vrc7_channel2_volume;
	GtkWidget *scale_vrc7_channel3_volume;
	GtkWidget *scale_vrc7_channel4_volume;
	GtkWidget *scale_vrc7_channel5_volume;
	GtkWidget *scale_mmc5_pulse0_volume;
	GtkWidget *scale_mmc5_pulse1_volume;
	GtkWidget *scale_mmc5_pcm_volume;
	GtkWidget *scale_sunsoft5b_channel0_volume;
	GtkWidget *scale_sunsoft5b_channel1_volume;
	GtkWidget *scale_sunsoft5b_channel2_volume;
	GtkWidget *scale_fds_volume;
	GtkWidget *scale_namco163_channel0_volume;
	GtkWidget *scale_namco163_channel1_volume;
	GtkWidget *scale_namco163_channel2_volume;
	GtkWidget *scale_namco163_channel3_volume;
	GtkWidget *scale_namco163_channel4_volume;
	GtkWidget *scale_namco163_channel5_volume;
	GtkWidget *scale_namco163_channel6_volume;
	GtkWidget *scale_namco163_channel7_volume;
	GtkWidget *box;
	GtkWidget *dialog_box;
	
	GtkWidget *tmp;

	dialog_box = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
	box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_box_pack_start(GTK_BOX(dialog_box), box, FALSE, FALSE, 8);

	volume_control_grid = gtk_grid_new();
	gtk_grid_set_column_spacing(GTK_GRID(volume_control_grid), 10);
	gtk_grid_set_row_spacing(GTK_GRID(volume_control_grid), 10);

	scale_master_volume = config_int_scale(0, 200, 0, 100, dialog, config,
					       "master_volume");

	scale_apu_pulse0_volume = config_int_scale(0, 200, 0, 100, dialog,
						   config,
						   "apu_pulse0_volume");

	scale_apu_pulse1_volume = config_int_scale(0, 200, 0, 100, dialog,
						   config,
						   "apu_pulse1_volume");

	scale_apu_triangle_volume = config_int_scale(0, 200, 0, 100, dialog,
						     config,
						     "apu_triangle_volume");

	scale_apu_noise_volume = config_int_scale(0, 200, 0, 100, dialog,
						  config,
						  "apu_noise_volume");

	scale_apu_dmc_volume = config_int_scale(0, 200, 0, 100, dialog,
						config,
						"apu_dmc_volume");

	scale_vrc6_pulse0_volume = config_int_scale(0, 200, 0, 100, dialog,
						    config,
						    "vrc6_pulse0_volume");

	scale_vrc6_pulse1_volume = config_int_scale(0, 200, 0, 100, dialog,
						    config,
						    "vrc6_pulse1_volume");

	scale_vrc6_sawtooth_volume = config_int_scale(0, 200, 0, 100, dialog,
						config,
						"vrc6_sawtooth_volume");

	scale_vrc7_channel0_volume = config_int_scale(0, 200, 0, 100, dialog,
						      config,
						      "vrc7_channel0_volume");

	scale_vrc7_channel1_volume = config_int_scale(0, 200, 0, 100, dialog,
						      config,
						      "vrc7_channel1_volume");

	scale_vrc7_channel2_volume = config_int_scale(0, 200, 0, 100, dialog,
						      config,
						      "vrc7_channel2_volume");

	scale_vrc7_channel3_volume = config_int_scale(0, 200, 0, 100, dialog,
						      config,
						      "vrc7_channel3_volume");

	scale_vrc7_channel4_volume = config_int_scale(0, 200, 0, 100, dialog,
						      config,
						      "vrc7_channel4_volume");

	scale_vrc7_channel5_volume = config_int_scale(0, 200, 0, 100, dialog,
						      config,
						      "vrc7_channel5_volume");

	scale_mmc5_pulse0_volume = config_int_scale(0, 200, 0, 100, dialog,
						    config,
						    "mmc5_pulse0_volume");

	scale_mmc5_pulse1_volume = config_int_scale(0, 200, 0, 100, dialog,
						    config,
						    "mmc5_pulse1_volume");

	scale_mmc5_pcm_volume = config_int_scale(0, 200, 0, 100, dialog,
						 config,
						 "mmc5_pcm_volume");

	scale_fds_volume = config_int_scale(0, 200, 0, 100, dialog,
						config,
						"fds_volume");

	scale_sunsoft5b_channel0_volume = config_int_scale(0, 200, 0, 100, dialog,
						config,
						"sunsoft5b_channel0_volume");

	scale_sunsoft5b_channel1_volume = config_int_scale(0, 200, 0, 100, dialog,
						config,
						"sunsoft5b_channel1_volume");

	scale_sunsoft5b_channel2_volume = config_int_scale(0, 200, 0, 100, dialog,
						config,
						"sunsoft5b_channel2_volume");

	scale_namco163_channel0_volume = config_int_scale(0, 200, 0, 100, dialog,
							  config,
							  "namco163_channel0_volume");

	scale_namco163_channel1_volume = config_int_scale(0, 200, 0, 100, dialog,
							  config,
							  "namco163_channel1_volume");

	scale_namco163_channel2_volume = config_int_scale(0, 200, 0, 100, dialog,
							  config,
							  "namco163_channel2_volume");

	scale_namco163_channel3_volume = config_int_scale(0, 200, 0, 100, dialog,
							  config,
							  "namco163_channel3_volume");

	scale_namco163_channel4_volume = config_int_scale(0, 200, 0, 100, dialog,
							  config,
							  "namco163_channel4_volume");

	scale_namco163_channel5_volume = config_int_scale(0, 200, 0, 100, dialog,
							  config,
							  "namco163_channel5_volume");

	scale_namco163_channel6_volume = config_int_scale(0, 200, 0, 100, dialog,
							  config,
							  "namco163_channel6_volume");

	scale_namco163_channel7_volume = config_int_scale(0, 200, 0, 100, dialog,
							  config,
							  "namco163_channel7_volume");

	gtk_grid_attach(GTK_GRID(volume_control_grid), scale_master_volume,
			1, 0, 1, 1);
	gtk_grid_attach(GTK_GRID(volume_control_grid), scale_apu_pulse0_volume,
			1, 1, 1, 1);
	gtk_grid_attach(GTK_GRID(volume_control_grid), scale_apu_pulse1_volume,
			1, 2, 1, 1);
	gtk_grid_attach(GTK_GRID(volume_control_grid), scale_apu_triangle_volume,
			1, 3, 1, 1);
	gtk_grid_attach(GTK_GRID(volume_control_grid), scale_apu_noise_volume,
			1, 4, 1, 1);
	gtk_grid_attach(GTK_GRID(volume_control_grid), scale_apu_dmc_volume,
			1, 5, 1, 1);
	gtk_grid_attach(GTK_GRID(volume_control_grid), scale_fds_volume,
			1, 6, 1, 1);
	gtk_grid_attach(GTK_GRID(volume_control_grid), scale_mmc5_pulse0_volume,
			1, 7, 1, 1);
	gtk_grid_attach(GTK_GRID(volume_control_grid), scale_mmc5_pulse1_volume,
			1, 8, 1, 1);
	gtk_grid_attach(GTK_GRID(volume_control_grid), scale_mmc5_pcm_volume,
			1, 9, 1, 1);

	gtk_grid_attach(GTK_GRID(volume_control_grid), scale_vrc6_pulse0_volume,
			3, 0, 1, 1);
	gtk_grid_attach(GTK_GRID(volume_control_grid), scale_vrc6_pulse1_volume,
			3, 1, 1, 1);
	gtk_grid_attach(GTK_GRID(volume_control_grid), scale_vrc6_sawtooth_volume,
			3, 2, 1, 1);
	gtk_grid_attach(GTK_GRID(volume_control_grid), scale_vrc7_channel0_volume,
			3, 3, 1, 1);
	gtk_grid_attach(GTK_GRID(volume_control_grid), scale_vrc7_channel1_volume,
			3, 4, 1, 1);
	gtk_grid_attach(GTK_GRID(volume_control_grid), scale_vrc7_channel2_volume,
			3, 5, 1, 1);
	gtk_grid_attach(GTK_GRID(volume_control_grid), scale_vrc7_channel3_volume,
			3, 6, 1, 1);
	gtk_grid_attach(GTK_GRID(volume_control_grid), scale_vrc7_channel4_volume,
			3, 7, 1, 1);
	gtk_grid_attach(GTK_GRID(volume_control_grid), scale_vrc7_channel5_volume,
			3, 8, 1, 1);
	gtk_grid_attach(GTK_GRID(volume_control_grid), scale_sunsoft5b_channel0_volume,
			3, 9, 1, 1);
	gtk_grid_attach(GTK_GRID(volume_control_grid), scale_sunsoft5b_channel1_volume,
			5, 0, 1, 1);
	gtk_grid_attach(GTK_GRID(volume_control_grid), scale_sunsoft5b_channel2_volume,
			5, 1, 1, 1);
	gtk_grid_attach(GTK_GRID(volume_control_grid), scale_namco163_channel0_volume,
			5, 2, 1, 1);
	gtk_grid_attach(GTK_GRID(volume_control_grid), scale_namco163_channel1_volume,
			5, 3, 1, 1);
	gtk_grid_attach(GTK_GRID(volume_control_grid), scale_namco163_channel2_volume,
			5, 4, 1, 1);
	gtk_grid_attach(GTK_GRID(volume_control_grid), scale_namco163_channel3_volume,
			5, 5, 1, 1);
	gtk_grid_attach(GTK_GRID(volume_control_grid), scale_namco163_channel4_volume,
			5, 6, 1, 1);
	gtk_grid_attach(GTK_GRID(volume_control_grid), scale_namco163_channel5_volume,
			5, 7, 1, 1);
	gtk_grid_attach(GTK_GRID(volume_control_grid), scale_namco163_channel6_volume,
			5, 8, 1, 1);
	gtk_grid_attach(GTK_GRID(volume_control_grid), scale_namco163_channel7_volume,
			5, 9, 1, 1);

	tmp = gtk_label_new_with_mnemonic("Master");
	gtk_grid_attach(GTK_GRID(volume_control_grid), tmp, 0, 0, 1, 1);
	tmp = gtk_label_new_with_mnemonic("APU Pulse 0");
	gtk_grid_attach(GTK_GRID(volume_control_grid), tmp, 0, 1, 1, 1);
	tmp = gtk_label_new_with_mnemonic("APU Pulse 1");
	gtk_grid_attach(GTK_GRID(volume_control_grid), tmp, 0, 2, 1, 1);
	tmp = gtk_label_new_with_mnemonic("APU Triangle");
	gtk_grid_attach(GTK_GRID(volume_control_grid), tmp, 0, 3, 1, 1);
	tmp = gtk_label_new_with_mnemonic("APU Noise");
	gtk_grid_attach(GTK_GRID(volume_control_grid), tmp, 0, 4, 1, 1);
	tmp = gtk_label_new_with_mnemonic("APU DMC");
	gtk_grid_attach(GTK_GRID(volume_control_grid), tmp, 0, 5, 1, 1);
	tmp = gtk_label_new_with_mnemonic("FDS");
	gtk_grid_attach(GTK_GRID(volume_control_grid), tmp, 0, 6, 1, 1);
	tmp = gtk_label_new_with_mnemonic("MMC5 Pulse 0");
	gtk_grid_attach(GTK_GRID(volume_control_grid), tmp, 0, 7, 1, 1);
	tmp = gtk_label_new_with_mnemonic("MMC5 Pulse 1");
	gtk_grid_attach(GTK_GRID(volume_control_grid), tmp, 0, 8, 1, 1);
	tmp = gtk_label_new_with_mnemonic("MMC5 PCM");
	gtk_grid_attach(GTK_GRID(volume_control_grid), tmp, 0, 9, 1, 1);

	tmp = gtk_label_new_with_mnemonic("VRC6 Pulse 0");
	gtk_grid_attach(GTK_GRID(volume_control_grid), tmp, 2, 0, 1, 1);
	tmp = gtk_label_new_with_mnemonic("VRC6 Pulse 1");
	gtk_grid_attach(GTK_GRID(volume_control_grid), tmp, 2, 1, 1, 1);
	tmp = gtk_label_new_with_mnemonic("VRC6 Sawtooth");
	gtk_grid_attach(GTK_GRID(volume_control_grid), tmp, 2, 2, 1, 1);
	tmp = gtk_label_new_with_mnemonic("VRC7 Channel 0");
	gtk_grid_attach(GTK_GRID(volume_control_grid), tmp, 2, 3, 1, 1);
	tmp = gtk_label_new_with_mnemonic("VRC7 Channel 1");
	gtk_grid_attach(GTK_GRID(volume_control_grid), tmp, 2, 4, 1, 1);
	tmp = gtk_label_new_with_mnemonic("VRC7 Channel 2");
	gtk_grid_attach(GTK_GRID(volume_control_grid), tmp, 2, 5, 1, 1);
	tmp = gtk_label_new_with_mnemonic("VRC7 Channel 3");
	gtk_grid_attach(GTK_GRID(volume_control_grid), tmp, 2, 6, 1, 1);
	tmp = gtk_label_new_with_mnemonic("VRC7 Channel 4");
	gtk_grid_attach(GTK_GRID(volume_control_grid), tmp, 2, 7, 1, 1);
	tmp = gtk_label_new_with_mnemonic("VRC7 Channel 5");
	gtk_grid_attach(GTK_GRID(volume_control_grid), tmp, 2, 8, 1, 1);
	tmp = gtk_label_new_with_mnemonic("Sunsoft 5B Channel 0");
	gtk_grid_attach(GTK_GRID(volume_control_grid), tmp, 2, 9, 1, 1);
	tmp = gtk_label_new_with_mnemonic("Sunsoft 5B Channel 1");
	gtk_grid_attach(GTK_GRID(volume_control_grid), tmp, 4, 0, 1, 1);
	tmp = gtk_label_new_with_mnemonic("Sunsoft 5B Channel 2");
	gtk_grid_attach(GTK_GRID(volume_control_grid), tmp, 4, 1, 1, 1);
	tmp = gtk_label_new_with_mnemonic("Namco 163 Channel 0");
	gtk_grid_attach(GTK_GRID(volume_control_grid), tmp, 4, 2, 1, 1);
	tmp = gtk_label_new_with_mnemonic("Namco 163 Channel 1");
	gtk_grid_attach(GTK_GRID(volume_control_grid), tmp, 4, 3, 1, 1);
	tmp = gtk_label_new_with_mnemonic("Namco 163 Channel 2");
	gtk_grid_attach(GTK_GRID(volume_control_grid), tmp, 4, 4, 1, 1);
	tmp = gtk_label_new_with_mnemonic("Namco 163 Channel 3");
	gtk_grid_attach(GTK_GRID(volume_control_grid), tmp, 4, 5, 1, 1);
	tmp = gtk_label_new_with_mnemonic("Namco 163 Channel 4");
	gtk_grid_attach(GTK_GRID(volume_control_grid), tmp, 4, 6, 1, 1);
	tmp = gtk_label_new_with_mnemonic("Namco 163 Channel 5");
	gtk_grid_attach(GTK_GRID(volume_control_grid), tmp, 4, 7, 1, 1);
	tmp = gtk_label_new_with_mnemonic("Namco 163 Channel 6");
	gtk_grid_attach(GTK_GRID(volume_control_grid), tmp, 4, 8, 1, 1);
	tmp = gtk_label_new_with_mnemonic("Namco 163 Channel 7");
	gtk_grid_attach(GTK_GRID(volume_control_grid), tmp, 4, 9, 1, 1);

	gtk_box_pack_start(GTK_BOX(box), volume_control_grid, TRUE, TRUE, 8);
}

static void configuration_setup_overclocking(GtkWidget *dialog, struct config *config)
{
	GtkWidget *dialog_box;
	GtkWidget *box;
	GtkWidget *hbox;
	GtkWidget *tmp;
	GtkWidget *label;
	
	dialog_box = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
	box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
	gtk_box_pack_start(GTK_BOX(dialog_box), box, FALSE, FALSE, 8);

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	label = gtk_label_new_with_mnemonic("Default _overclock mode:");
	tmp = config_combo_box(dialog, config, "default_overclock_mode");
	gtk_box_pack_start(GTK_BOX(box), hbox, FALSE, FALSE, 8);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 8);
	gtk_box_pack_start(GTK_BOX(hbox), tmp, FALSE, FALSE, 0);

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	label = gtk_label_new_with_mnemonic("Extra _scanlines to emulate:");
	tmp = config_int_spinbutton(dialog, config, "overclock_scanlines");
	gtk_box_pack_start(GTK_BOX(box), hbox, FALSE, FALSE, 8);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 8);
	gtk_box_pack_start(GTK_BOX(hbox), tmp, FALSE, FALSE, 0);

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	label = gtk_label_new_with_mnemonic("Frames to emulate before overclocking:");
	tmp = config_int_spinbutton(dialog, config, "frames_before_overclock");
	gtk_box_pack_start(GTK_BOX(box), hbox, FALSE, FALSE, 8);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 8);
	gtk_box_pack_start(GTK_BOX(hbox), tmp, FALSE, FALSE, 0);

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	label = gtk_label_new_with_mnemonic("Raw PCM sample threshold:");
	tmp = config_int_spinbutton(dialog, config, "overclock_pcm_sample_threshold");
	gtk_box_pack_start(GTK_BOX(box), hbox, FALSE, FALSE, 8);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 8);
	gtk_box_pack_start(GTK_BOX(hbox), tmp, FALSE, FALSE, 0);

}

static void configuration_setup_audio(GtkWidget *dialog, struct config *config)
{
	GtkWidget *combo_sample_rate;
	GtkWidget *combo_pcm_filter;
	GtkWidget *check_force_stereo;
	GtkWidget *check_dynamic_rate_adjustment;
	GtkWidget *spin_buffer_size;
	GtkWidget *dialog_box;
	GtkWidget *dialog_v_box;
	GtkWidget *tmp;
	GtkWidget *tmpbox;
	GtkWidget *max_sample_rate_adjustment;
	GtkWidget *adjustment_low_watermark;
	GtkWidget *adjustment_buffer_range;
	
	dialog_box = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
	dialog_v_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
	gtk_box_pack_start(GTK_BOX(dialog_box), dialog_v_box, FALSE, FALSE, 8);

	tmpbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	tmp = gtk_label_new_with_mnemonic("_Sample rate:");
	gtk_box_pack_start(GTK_BOX(tmpbox), tmp, FALSE, FALSE, 8);
	combo_sample_rate = config_integer_combo_box(dialog, config, "sample_rate");
	gtk_label_set_mnemonic_widget(GTK_LABEL(tmp), combo_sample_rate);
	gtk_box_pack_start(GTK_BOX(tmpbox), combo_sample_rate, FALSE, FALSE, 8);
	gtk_box_pack_start(GTK_BOX(dialog_v_box), tmpbox, FALSE, FALSE, 0);

	tmpbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	tmp = gtk_label_new_with_mnemonic("_Buffer size:");
	gtk_box_pack_start(GTK_BOX(tmpbox), tmp, FALSE, FALSE, 8);
	spin_buffer_size = config_int_spinbutton(dialog, config,
						 "audio_buffer_size");
	gtk_label_set_mnemonic_widget(GTK_LABEL(tmp), spin_buffer_size);
	gtk_box_pack_start(GTK_BOX(tmpbox), spin_buffer_size, FALSE, FALSE, 8);
	gtk_box_pack_start(GTK_BOX(dialog_v_box), tmpbox, FALSE, FALSE, 0);
	
	tmpbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	tmp = gtk_label_new_with_mnemonic("_Raw PCM filter mode:");
	gtk_box_pack_start(GTK_BOX(tmpbox), tmp, FALSE, FALSE, 8);
	combo_pcm_filter = config_combo_box(dialog, config, "raw_pcm_filter");
	gtk_label_set_mnemonic_widget(GTK_LABEL(tmp), combo_pcm_filter);
	gtk_box_pack_start(GTK_BOX(tmpbox), combo_pcm_filter, FALSE, FALSE, 8);
	gtk_box_pack_start(GTK_BOX(dialog_v_box), tmpbox, FALSE, FALSE, 0);

	tmpbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	check_force_stereo = config_checkbox(dialog, "_Force stereo", config,
					     "force_stereo");
	gtk_box_pack_start(GTK_BOX(tmpbox), check_force_stereo, FALSE, FALSE, 8);
	gtk_box_pack_start(GTK_BOX(dialog_v_box), tmpbox, FALSE, FALSE, 0);

	tmpbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	check_dynamic_rate_adjustment =
		config_checkbox(dialog, "_Dynamic rate adjustment",
				config, "dynamic_rate_adjustment_enabled");
	gtk_box_pack_start(GTK_BOX(tmpbox), check_dynamic_rate_adjustment,
			   FALSE, FALSE, 8);
	gtk_box_pack_start(GTK_BOX(dialog_v_box), tmpbox, FALSE, FALSE, 0);


	tmpbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_box_pack_start(GTK_BOX(dialog_v_box), tmpbox, FALSE, FALSE, 0);
	tmp = gtk_label_new_with_mnemonic("_Maximum sample rate adjustment percentage:");
	max_sample_rate_adjustment = config_double_spinbutton(dialog, config, 0.0001,
						 "dynamic_rate_adjustment_max");
	gtk_label_set_mnemonic_widget(GTK_LABEL(tmp), max_sample_rate_adjustment);
	gtk_box_pack_start(GTK_BOX(tmpbox), tmp, FALSE, FALSE, 8);
	gtk_box_pack_start(GTK_BOX(tmpbox), max_sample_rate_adjustment, FALSE, FALSE, 8);

	tmpbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_box_pack_start(GTK_BOX(dialog_v_box), tmpbox, FALSE, FALSE, 0);
	tmp = gtk_label_new_with_mnemonic("D_ynamic rate adjustment low watermark (frames):");
	adjustment_low_watermark = config_double_spinbutton(dialog, config, 0.01,
	                                             "dynamic_rate_adjustment_low_watermark");
	gtk_label_set_mnemonic_widget(GTK_LABEL(tmp), adjustment_low_watermark);
	gtk_box_pack_start(GTK_BOX(tmpbox), tmp, FALSE, FALSE, 8);
	gtk_box_pack_start(GTK_BOX(tmpbox), adjustment_low_watermark, FALSE, FALSE, 8);

	tmpbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_box_pack_start(GTK_BOX(dialog_v_box), tmpbox, FALSE, FALSE, 0);
	tmp = gtk_label_new_with_mnemonic("Dy_namic rate adjustment buffer range (frames):");
	adjustment_buffer_range = config_double_spinbutton(dialog, config, 0.01,
	                                             "dynamic_rate_adjustment_buffer_range");
	gtk_label_set_mnemonic_widget(GTK_LABEL(tmp), adjustment_buffer_range);
	gtk_box_pack_start(GTK_BOX(tmpbox), tmp,  FALSE, FALSE, 8);
	gtk_box_pack_start(GTK_BOX(tmpbox), adjustment_buffer_range, FALSE, FALSE, 8);

}

static void configuration_setup_rom_specific(GtkWidget *dialog,
					     struct config *config)
{
	GtkWidget *dialog_box, *box;
	GtkWidget *check;
	GtkWidget *tmp;
	GtkWidget *input_frame;
	GtkWidget *combo;
	GtkWidget *label;
	GtkWidget *grid;
	GtkWidget *emu_frame;
#if 0
	GtkWidget *button;
	GtkWidget *entry;
#endif
	
	dialog_box = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

	input_frame = gtk_frame_new(NULL);
	tmp = gtk_label_new_with_mnemonic(NULL);
	gtk_label_set_markup(GTK_LABEL(tmp), "<b>Input Options</b>");
	gtk_frame_set_label_widget(GTK_FRAME(input_frame), tmp);
	gtk_frame_set_shadow_type(GTK_FRAME(input_frame), GTK_SHADOW_NONE);
	box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
	gtk_container_add(GTK_CONTAINER(input_frame), box);
	gtk_box_pack_start(GTK_BOX(dialog_box), input_frame, FALSE, FALSE, 16);
	grid = gtk_grid_new();
	gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
	gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
	gtk_box_pack_start(GTK_BOX(box), grid, FALSE, FALSE, 0);

	label = gtk_label_new_with_mnemonic("_Vs. Controller Mode");
	combo = config_combo_box(dialog, config, "vs_controller_mode");
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), combo);
	gtk_grid_attach(GTK_GRID(grid), label, 0, 0, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), combo, 1, 0, 1, 1);

	if (!emu_system_is_vs(emu)) {
		gtk_widget_set_sensitive(label, FALSE);
		gtk_widget_set_sensitive(combo, FALSE);
	}

	check = config_checkbox(dialog, "_Swap Start and Select buttons", config,
					     "swap_start_select");
	gtk_box_pack_start(GTK_BOX(box), check, FALSE, FALSE, 0);
	check = config_checkbox(dialog, "Swap _A and B buttons", config,
					     "swap_a_b");
	gtk_box_pack_start(GTK_BOX(box), check, FALSE, FALSE, 0);

	emu_frame = gtk_frame_new(NULL);
	tmp = gtk_label_new_with_mnemonic(NULL);
	gtk_label_set_markup(GTK_LABEL(tmp), "<b>Emulator Options</b>");
	gtk_frame_set_label_widget(GTK_FRAME(emu_frame), tmp);
	gtk_frame_set_shadow_type(GTK_FRAME(emu_frame), GTK_SHADOW_NONE);
	box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
	gtk_container_add(GTK_CONTAINER(emu_frame), box);
	gtk_box_pack_start(GTK_BOX(dialog_box), emu_frame, FALSE, FALSE, 16);

	check = config_checkbox(dialog, "_Swap pulse channel duty cycles", config,
					     "swap_start_select");
	gtk_box_pack_start(GTK_BOX(box), check, FALSE, FALSE, 0);

	check = config_checkbox(dialog, "Swap red/green _PPU color emphasis bits", config,
					     "swap_ppu_emphasis");
	gtk_box_pack_start(GTK_BOX(box), check, FALSE, FALSE, 0);

#if 0
	grid = gtk_grid_new();
	gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
	gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
	gtk_box_pack_start(GTK_BOX(box), grid, FALSE, FALSE, 0);

	label = gtk_label_new_with_mnemonic("_Periodic savestate path (defaults to slot 1):");
	entry = config_entry(dialog, config, "periodic_savestate_path");
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry);
	button = gtk_button_new_with_label("Browse...");
	g_object_set_data(G_OBJECT(button), "dialog", dialog);
	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(file_chooser_callback),
			 entry);
	
	gtk_grid_attach(GTK_GRID(grid), label, 0, 0, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), entry, 1, 0, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), button, 2, 0, 1, 1);
#endif

}

static void configuration_setup_misc(GtkWidget *dialog, struct config *config)
{
	GtkWidget *dialog_box, *box, *box2;
	GtkWidget *check;
	GtkWidget *tmp;
	GtkWidget *emu_frame, *input_frame, *fds_frame, *ines_frame;
	GtkWidget *state_frame;
	GtkWidget *cheats_frame;
	GtkWidget *grid;
	GtkWidget *combo;
	GtkWidget *main_grid;
#if 0
	GtkWidget *spinbutton;
#endif
	GtkWidget *scale;
#if __unix__
	GtkWidget *entry;
#endif
	int i;
	
	dialog_box = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
	main_grid = gtk_grid_new();
	gtk_grid_set_column_spacing(GTK_GRID(main_grid), 8);
	gtk_grid_set_row_spacing(GTK_GRID(main_grid), 16);
	gtk_box_pack_start(GTK_BOX(dialog_box), main_grid, FALSE, FALSE, 8);

	emu_frame = gtk_frame_new(NULL);
	tmp = gtk_label_new_with_mnemonic(NULL);
	gtk_label_set_markup(GTK_LABEL(tmp), "<b>Emulator Options</b>");
	gtk_frame_set_label_widget(GTK_FRAME(emu_frame), tmp);
	gtk_frame_set_shadow_type(GTK_FRAME(emu_frame), GTK_SHADOW_NONE);
	box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
	gtk_container_add(GTK_CONTAINER(emu_frame), box);
	gtk_grid_attach(GTK_GRID(main_grid), emu_frame, 0, 0, 1, 1);

	grid = gtk_grid_new();
	gtk_box_pack_start(GTK_BOX(box), grid, FALSE, FALSE, 8);
	gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
	gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
	tmp = gtk_label_new_with_mnemonic("_Preferred Console Type:");
	combo = config_combo_box(dialog,
				 config, "preferred_console_type");
	gtk_label_set_mnemonic_widget(GTK_LABEL(tmp), combo);
	gtk_grid_attach(GTK_GRID(grid), tmp, 0, 0, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), combo, 1, 0, 1, 1);

	check= config_checkbox(dialog, "Enable _ROM database", config,
			       "db_enabled");
	gtk_box_pack_start(GTK_BOX(box), check, FALSE, FALSE, 0);

	check = config_checkbox(dialog, "Run in _background", config,
					     "run_in_background");
	gtk_box_pack_start(GTK_BOX(box), check, FALSE, FALSE, 0);

	check = config_checkbox(dialog, "Run in background when playing _NSFs", config,
					     "nsf_run_in_background");
	gtk_box_pack_start(GTK_BOX(box), check, FALSE, FALSE, 0);
	check = config_checkbox(dialog, "Auto_patch enabled", config,
					     "autopatch_enabled");

	gtk_box_pack_start(GTK_BOX(box), check, FALSE, FALSE, 0);

	tmp = gtk_label_new_with_mnemonic("_Alternate emulation speed:");
	box2 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_box_pack_start(GTK_BOX(box), box2, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box2), tmp, FALSE, FALSE, 0);

	scale = config_int_scale(0, 240, 1, 240, dialog, config, "alternate_speed");
	gtk_label_set_mnemonic_widget(GTK_LABEL(tmp), scale);
	gtk_range_set_round_digits(GTK_RANGE(scale), 0);
	gtk_box_pack_start(GTK_BOX(box2), scale, FALSE, FALSE, 0);

	check = config_checkbox(dialog, "Mute audio when running at alternate speed", config,
					     "alternate_speed_mute");
	gtk_box_pack_start(GTK_BOX(box), check, FALSE, FALSE, 0);

#if __unix__
	tmp = gtk_label_new_with_mnemonic("Screensaver deactivate command:");
	entry = config_entry(dialog, config, "screensaver_deactivate_command");
	gtk_label_set_mnemonic_widget(GTK_LABEL(tmp), entry);
	gtk_entry_set_width_chars(GTK_ENTRY(entry), 42);
	gtk_grid_attach(GTK_GRID(grid), tmp, 0, 1, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), entry, 1, 1, 1, 1);
#endif
	grid = gtk_grid_new();
	gtk_box_pack_start(GTK_BOX(box), grid, FALSE, FALSE, 8);
	gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
	gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
	tmp = gtk_label_new_with_mnemonic("Console loglevel");
	combo = config_combo_box(dialog,
				 config, "loglevel");
	gtk_label_set_mnemonic_widget(GTK_LABEL(tmp), combo);
	gtk_grid_attach(GTK_GRID(grid), tmp, 0, 0, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), combo, 1, 0, 1, 1);

	input_frame = gtk_frame_new(NULL);
	tmp = gtk_label_new_with_mnemonic(NULL);
	gtk_label_set_markup(GTK_LABEL(tmp), "<b>Input Options</b>");
	gtk_frame_set_label_widget(GTK_FRAME(input_frame), tmp);
	gtk_frame_set_shadow_type(GTK_FRAME(input_frame), GTK_SHADOW_NONE);
	box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
	gtk_container_add(GTK_CONTAINER(input_frame), box);
	gtk_grid_attach(GTK_GRID(main_grid), input_frame, 1, 0, 1, 1);

	check = config_checkbox(dialog,
				"P_revent Up+Down and Left+Right on controllers",
				config,
				"mask_opposite_directions");
	gtk_box_pack_start(GTK_BOX(box), check, FALSE, FALSE, 0);
	check = config_checkbox(dialog,
				"_Map Start/Select to 'Insert Coin' for VS. System games",
				config,
				"vs_coin_on_start");
	gtk_box_pack_start(GTK_BOX(box), check, FALSE, FALSE, 0);
	check = config_checkbox(dialog,
				"S_wap Start and Select for VS. System games",
				config,
				"vs_swap_start_select");
	gtk_box_pack_start(GTK_BOX(box), check, FALSE, FALSE, 0);
	check = config_checkbox(dialog,
				"SDL _GameController API Support",
				config,
				"gamecontroller");
	gtk_box_pack_start(GTK_BOX(box), check, FALSE, FALSE, 0);

	tmp = gtk_label_new_with_mnemonic("_Turbo rate:");
	box2 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_box_pack_start(GTK_BOX(box), box2, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box2), tmp, FALSE, FALSE, 0);

	scale = config_int_scale(0, 200, 0, 7, dialog, config, "turbo_speed");
	gtk_label_set_mnemonic_widget(GTK_LABEL(tmp), scale);
	gtk_scale_set_draw_value(GTK_SCALE(scale), FALSE);
	gtk_range_set_round_digits(GTK_RANGE(scale), 0);
	for (i = 0; i < 8; i++) {
		gtk_scale_add_mark(GTK_SCALE(scale), i, GTK_POS_BOTTOM, NULL);
	}

	gtk_box_pack_start(GTK_BOX(box2), scale, FALSE, FALSE, 0);

	cheats_frame = gtk_frame_new(NULL);
	tmp = gtk_label_new_with_mnemonic(NULL);
	gtk_label_set_markup(GTK_LABEL(tmp), "<b>Cheat Options</b>");
	gtk_frame_set_label_widget(GTK_FRAME(cheats_frame), tmp);
	gtk_frame_set_shadow_type(GTK_FRAME(cheats_frame), GTK_SHADOW_NONE);
	box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
	gtk_container_add(GTK_CONTAINER(cheats_frame), box);
	gtk_grid_attach(GTK_GRID(main_grid), cheats_frame, 0, 1, 1, 1);

	check = config_checkbox(dialog,
				"Automatically load cheats when _loading ROM",
				config,
				"autoload_cheats");
	gtk_box_pack_start(GTK_BOX(box), check, FALSE, FALSE, 0);
	check = config_checkbox(dialog,
				"Automatically save cheats when _closing ROM",
				config,
				"autosave_cheats");
	gtk_box_pack_start(GTK_BOX(box), check, FALSE, FALSE, 0);

	state_frame = gtk_frame_new(NULL);
	tmp = gtk_label_new_with_mnemonic(NULL);
	gtk_label_set_markup(GTK_LABEL(tmp), "<b>Savestate Options</b>");
	gtk_frame_set_label_widget(GTK_FRAME(state_frame), tmp);
	gtk_frame_set_shadow_type(GTK_FRAME(state_frame), GTK_SHADOW_NONE);
	box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
	gtk_container_add(GTK_CONTAINER(state_frame), box);
	gtk_grid_attach(GTK_GRID(main_grid), state_frame, 0, 2, 1, 1);

	check = config_checkbox(dialog,
				"Automatically load state when l_oading ROM",
				config,
				"autoload_state");
	gtk_box_pack_start(GTK_BOX(box), check, FALSE, FALSE, 0);
	check = config_checkbox(dialog,
				"Automatically save state when cl_osing ROM",
				config,
				"autosave_state");
	gtk_box_pack_start(GTK_BOX(box), check, FALSE, FALSE, 0);
#if 0
	check = config_checkbox(dialog,
				"Periodically save state _while playing",
				config,
				"periodic_savestate_enabled");
	gtk_box_pack_start(GTK_BOX(box), check, FALSE, FALSE, 0);
	tmp = gtk_label_new_with_mnemonic("Periodic savestate _delay (in seconds):");
	spinbutton = config_int_spinbutton(dialog, config,
					   "periodic_savestate_delay");
	gtk_label_set_mnemonic_widget(GTK_LABEL(tmp), spinbutton);
	box2 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_box_pack_start(GTK_BOX(box), box2, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box2), tmp, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box2), spinbutton, FALSE, FALSE, 0);
#endif					   

	ines_frame = gtk_frame_new(NULL);
	tmp = gtk_label_new_with_mnemonic(NULL);
	gtk_label_set_markup(GTK_LABEL(tmp), "<b>ROM Options</b>");
	gtk_frame_set_label_widget(GTK_FRAME(ines_frame), tmp);
	gtk_frame_set_shadow_type(GTK_FRAME(ines_frame), GTK_SHADOW_NONE);
	box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
	gtk_container_add(GTK_CONTAINER(ines_frame), box);
	gtk_grid_attach(GTK_GRID(main_grid), ines_frame, 1, 1, 1, 1);
	
	check = config_checkbox(dialog,
				"_Allocate 8KiB WRAM at $6000 (if possible) (iNES only)",
				config,
				"auto_wram");
	gtk_box_pack_start(GTK_BOX(box), check, FALSE, FALSE, 0);
	check = config_checkbox(dialog,
				"_Guess system type from ROM filename",
				config,
				"guess_system_type_from_filename");
	gtk_box_pack_start(GTK_BOX(box), check, FALSE, FALSE, 0);

	fds_frame = gtk_frame_new(NULL);
	tmp = gtk_label_new_with_mnemonic(NULL);
	gtk_label_set_markup(GTK_LABEL(tmp), "<b>Famicom Disk System Options</b>");
	gtk_frame_set_label_widget(GTK_FRAME(fds_frame), tmp);
	gtk_frame_set_shadow_type(GTK_FRAME(fds_frame), GTK_SHADOW_NONE);
	box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
	gtk_container_add(GTK_CONTAINER(fds_frame), box);
	gtk_grid_attach(GTK_GRID(main_grid), fds_frame, 1, 2, 1, 1);

	check = config_checkbox(dialog,
				"Use _high-level disk I/O",
				config,
				"fds_bios_patch_enabled");
	gtk_box_pack_start(GTK_BOX(box), check, FALSE, FALSE, 0);
	check = config_checkbox(dialog,
				"Enable automatic _disk change",
				config,
				"fds_auto_disk_change_enabled");
	gtk_box_pack_start(GTK_BOX(box), check, FALSE, FALSE, 0);
	check = config_checkbox(dialog,
				"_Skip BIOS title screen",
				config,
				"fds_hide_bios_title_screen");
	gtk_box_pack_start(GTK_BOX(box), check, FALSE, FALSE, 0);
	check = config_checkbox(dialog,
				"S_kip license screen",
				config,
				"fds_hide_license_screen");
	gtk_box_pack_start(GTK_BOX(box), check, FALSE, FALSE, 0);

}

static void configuration_setup_path(GtkWidget *dialog, struct config *config)
{
	GtkWidget *box;
	GtkWidget *grid;
	GtkWidget *label;
	GtkWidget *entry;
	GtkWidget *button;
	GtkWidget *check;

	box = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
	gtk_box_set_spacing(GTK_BOX(box), 8);

	grid = gtk_grid_new();
	gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
	gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
	gtk_box_pack_start(GTK_BOX(box), grid, FALSE, FALSE, 8);


	label = gtk_label_new_with_mnemonic("_ROM Path:");
	gtk_widget_set_halign(label, GTK_ALIGN_START);
	entry = config_entry(dialog, config, "rom_path");
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry);
	gtk_entry_set_width_chars(GTK_ENTRY(entry), 42);
	button = gtk_button_new_with_label("Browse...");
	g_object_set_data(G_OBJECT(button), "dialog", dialog);
	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(folder_chooser_callback),
			 entry);
	gtk_grid_attach(GTK_GRID(grid), label,  0, 0, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), entry,  1, 0, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), button, 2, 0, 1, 1);

	check = config_checkbox(dialog,
				"_Use ROM path as default path in 'Open ROM' dialog",
				config,
				"default_to_rom_path");
	//gtk_box_pack_start(GTK_BOX(box), check, FALSE, FALSE, 0);
	gtk_grid_attach(GTK_GRID(grid), check, 1, 1, 2, 1);
	label = gtk_label_new_with_mnemonic("_Save Path:");
	gtk_widget_set_halign(label, GTK_ALIGN_START);
	entry = config_entry(dialog, config, "save_path");
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry);
	gtk_entry_set_width_chars(GTK_ENTRY(entry), 42);
	button = gtk_button_new_with_label("Browse...");
	g_object_set_data(G_OBJECT(button), "dialog", dialog);
	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(folder_chooser_callback),
			 entry);
	gtk_grid_attach(GTK_GRID(grid), label,  0, 2, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), entry,  1, 2, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), button, 2, 2, 1, 1);

	check = config_checkbox(dialog,
				"Store new sa_ve files in same directory as ROM",
				config,
				"save_uses_romdir");
	gtk_grid_attach(GTK_GRID(grid), check, 1, 3, 2, 1);

	label = gtk_label_new_with_mnemonic("ROM _Configuration Path:");
	gtk_widget_set_halign(label, GTK_ALIGN_START);
	entry = config_entry(dialog, config, "romcfg_path");
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry);
	gtk_entry_set_width_chars(GTK_ENTRY(entry), 42);
	button = gtk_button_new_with_label("Browse...");
	g_object_set_data(G_OBJECT(button), "dialog", dialog);
	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(folder_chooser_callback),
			 entry);
	gtk_grid_attach(GTK_GRID(grid), label,  0, 4, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), entry,  1, 4, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), button, 2, 4, 1, 1);

	check = config_checkbox(dialog,
				"Store new ROM-specific con_fig files in same directory as ROM",
				config,
				"config_uses_romdir");
	gtk_grid_attach(GTK_GRID(grid), check, 1, 5, 2, 1);

	label = gtk_label_new_with_mnemonic("Saves_tate Path:");
	gtk_widget_set_halign(label, GTK_ALIGN_START);
	entry = config_entry(dialog, config, "state_path");
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry);
	gtk_entry_set_width_chars(GTK_ENTRY(entry), 42);
	button = gtk_button_new_with_label("Browse...");
	g_object_set_data(G_OBJECT(button), "dialog", dialog);
	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(folder_chooser_callback),
			 entry);
	gtk_grid_attach(GTK_GRID(grid), label,  0, 6, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), entry,  1, 6, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), button, 2, 6, 1, 1);

	label = gtk_label_new_with_mnemonic("Screens_hot Path:");
	gtk_widget_set_halign(label, GTK_ALIGN_START);
	entry = config_entry(dialog, config, "screenshot_path");
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry);
	gtk_entry_set_width_chars(GTK_ENTRY(entry), 42);
	button = gtk_button_new_with_label("Browse...");
	g_object_set_data(G_OBJECT(button), "dialog", dialog);
	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(folder_chooser_callback),
			 entry);
	gtk_grid_attach(GTK_GRID(grid), label,  0, 7, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), entry,  1, 7, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), button, 2, 7, 1, 1);

	label = gtk_label_new_with_mnemonic("_Patch Path:");
	gtk_widget_set_halign(label, GTK_ALIGN_START);
	entry = config_entry(dialog, config, "patch_path");
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry);
	gtk_entry_set_width_chars(GTK_ENTRY(entry), 42);
	button = gtk_button_new_with_label("Browse...");
	g_object_set_data(G_OBJECT(button), "dialog", dialog);
	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(folder_chooser_callback),
			 entry);
	gtk_grid_attach(GTK_GRID(grid), label,  0, 8, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), entry,  1, 8, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), button, 2, 8, 1, 1);

	label = gtk_label_new_with_mnemonic("FDS _BIOS Path:");
	gtk_widget_set_halign(label, GTK_ALIGN_START);
	entry = config_entry(dialog, config, "fds_bios_path");
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry);
	gtk_entry_set_width_chars(GTK_ENTRY(entry), 42);
	button = gtk_button_new_with_label("Browse...");
	g_object_set_data(G_OBJECT(button), "dialog", dialog);
	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(file_chooser_callback),
			 entry);
	gtk_grid_attach(GTK_GRID(grid), label,  0, 9, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), entry,  1, 9, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), button, 2, 9, 1, 1);

}

#if 0
static void volume_control_dialog_response_cb(GtkDialog *dialog,
					      gint response_id,
					      gpointer user_data)
{
	if (response_id == GTK_RESPONSE_ACCEPT) {
		gtk_widget_destroy(GTK_WIDGET(dialog));
	}
}
#endif

void gui_volume_control_dialog(GtkWidget *widget, gpointer user_data)
{

	gui_configuration_dialog("Volume Control", configuration_setup_volume,
	                         0, widget, user_data);
}

void gui_configuration_dialog(const char *name,
			      void (*setup)(GtkWidget *, struct config *),
			      int rom_specific,
			      GtkWidget *widget,
			      gpointer user_data)
{
	GtkWidget *dialog;
	GtkWidget *dialog_box;
	GtkWidget *gtkwindow;
	GtkDialogFlags flags;
	struct config *config;
	int paused;
	gint response;

	video_show_cursor(1);
	gtkwindow = (GtkWidget *)user_data;
	flags = GTK_DIALOG_DESTROY_WITH_PARENT;
	dialog = gtk_dialog_new_with_buttons(name,
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

	setup(dialog, emu->config);

	paused = emu_paused(emu);
	emu_pause(emu, 1);
	gtk_widget_show_all(dialog);

	while (1) {
		response = gtk_dialog_run(GTK_DIALOG(dialog));

		if (response == RESET_TO_DEFAULT)
			continue;
		break;
	}

	if (response == GTK_RESPONSE_ACCEPT) {
		config_cleanup(config);

		if (emu_loaded(emu))
			emu_apply_config(emu);

		log_apply_config(emu);
		video_apply_config(emu);
		audio_apply_config(emu);
		if (rom_specific) {
			emu_save_rom_config(emu);
		} else {
			config_save_main_config(emu->config);
		}
	} else {
		config_replace(emu->config, config);
		config_cleanup(config);
		if (emu_loaded(emu)) {
			emu_apply_config(emu);
		}

		log_apply_config(emu);
		video_apply_config(emu);
		audio_apply_config(emu);
	}

	if (!paused && emu_loaded(emu))
		emu_pause(emu, 0);

	gtk_widget_destroy(dialog);
	video_show_cursor(0);
}

void gui_path_configuration_dialog(GtkWidget *widget, gpointer user_data)
{
	gui_configuration_dialog("Path Configuration",
				 configuration_setup_path, 0,
				 widget, user_data);
}

void gui_audio_configuration_dialog(GtkWidget *widget, gpointer user_data)
{
	gui_configuration_dialog("Audio Configuration",
				 configuration_setup_audio, 0,
				 widget, user_data);
}

void gui_overclocking_configuration_dialog(GtkWidget *widget, gpointer user_data)
{
	gui_configuration_dialog("Overclocking Configuration",
				 configuration_setup_overclocking, 0,
				 widget, user_data);
}

void gui_misc_configuration_dialog(GtkWidget *widget, gpointer user_data)
{
	gui_configuration_dialog("Miscellaneous Configuration",
				 configuration_setup_misc, 0,
				 widget, user_data);
}

void gui_rom_configuration_dialog(GtkWidget *widget, gpointer user_data)
{
	gui_configuration_dialog("ROM-Specific Configuration",
				 configuration_setup_rom_specific, 1,
				 widget, user_data);
}
