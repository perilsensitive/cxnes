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

#ifndef __GTKCONFIG_H__
#define __GTKCONFIG_H__

#define RESET_TO_DEFAULT 1

extern char *file_dialog(GtkWidget *parent,
			 const char *title,
			 GtkFileChooserAction action,
			 const char *ok_text,
			 const char *default_path,
			 const char *suggested_name,
			 const char *filter_name,
			 char **filter_patterns,
			 char **shortcuts);

extern void gui_configuration_dialog(const char *name,
				     void (*setup)(GtkWidget *, struct config *),
				     int rom_specific, GtkWidget *widget,
				     gpointer user_data);
extern void int_scale_callback(GtkScale *scale, gpointer user_data);
extern void toggle_callback(GtkToggleButton *toggle, gpointer user_data);
extern void spinbutton_int_callback(GtkSpinButton *button, gpointer user_data);
extern void spinbutton_double_callback(GtkSpinButton *button, gpointer user_data);
extern void combo_box_callback(GtkComboBox *combo, gpointer user_data);
extern void integer_combo_box_callback(GtkComboBox *combo, gpointer user_data);
extern void entry_callback(GtkEntry *entry, gpointer user_data);
extern void default_button_int_scale_cb(GtkDialog *dialog,
					gint response_id,
					gpointer user_data);
extern void default_button_int_spinbutton_cb(GtkDialog *dialog,
					     gint response_id,
					     gpointer user_data);
extern void default_button_double_spinbutton_cb(GtkDialog *dialog, gint response_id,
						gpointer user_data);
extern void default_button_checkbox_cb(GtkDialog *dialog, gint response_id,
				       gpointer user_data);
extern void default_button_combo_cb(GtkDialog *dialog, gint response_id,
				    gpointer user_data);
extern void default_button_integer_combo_cb(GtkDialog *dialog, gint response_id,
					    gpointer user_data);
extern void default_button_entry_cb(GtkDialog *dialog, gint response_id,
				    gpointer user_data);
extern GtkWidget *config_combo_box(GtkWidget *dialog,
				   struct config *config,
				   const char *name);
extern GtkWidget *config_integer_combo_box(GtkWidget *dialog,
					   struct config *config,
					   const char *name);
extern GtkWidget *config_int_scale(int vertical, int size,
				   int min, int max, GtkWidget *dialog,
				   struct config *config, const char *name);
extern GtkWidget *config_checkbox(GtkWidget *dialog,
				  const char *label,
				  struct config *config,
				  const char *name);
extern GtkWidget *config_int_spinbutton(GtkWidget *dialog,
					struct config *config,
					const char *name);
extern GtkWidget *config_double_spinbutton(GtkWidget *dialog,
					   struct config *config,
					   double step,
					   const char *name);
extern GtkWidget *config_entry(GtkWidget *dialog,
			       struct config *config,
			       const char *name);
extern void file_chooser_callback(GtkWidget *widget, gpointer user_data);
extern void folder_chooser_callback(GtkWidget *widget, gpointer user_data);

#endif
