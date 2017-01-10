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

#include <string.h>
#include <gtk/gtk.h>

#include "emu.h"
#include "gtkconfig.h"

extern struct emu *emu;

static void ntsc_filter_preset_callback(GtkWidget *widget, gpointer user_data);

void gui_ntsc_filter_settings_dialog(GtkWidget *widget, gpointer user_data);

static void rgba_to_gdkrgba(uint32_t rgba, GdkRGBA *color)
{
	int red;
	int green;
	int blue;
	int alpha;

	red = (rgba >> 24) & 0xff;
	green = (rgba >> 16) & 0xff;
	blue = (rgba >> 8) & 0xff;
	alpha = rgba & 0xff;

	color->red = (double)red / 255;
	color->green = (double)green / 255;
	color->blue = (double)blue / 255;
	color->alpha = (double)alpha / 255;
}

static void color_button_callback(GtkColorButton *button, gpointer user_data)
{
	int *ptr;
	GdkRGBA color;
	uint32_t rgba;
	int red;
	int green;
	int blue;
	int alpha;

	ptr = (int *)user_data;

	gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(button), &color);

	red = color.red * 255;
	green = color.green * 255;
	blue = color.blue * 255;
	alpha = color.alpha * 255;

	rgba = ((red & 0xff) << 24) | ((green & 0xff) << 16) |
		((blue & 0xff) << 8) | (alpha & 0xff);

	*ptr = rgba;
}

static void default_button_color_button_cb(GtkDialog *dialog,
					   gint response_id,
					   gpointer user_data)
{
	GtkWidget *widget;
	struct config *config;
	char *name;
	unsigned int *ptr;
	GdkRGBA color;

	if (response_id != RESET_TO_DEFAULT)
		return;

	widget = (GtkWidget *)user_data;
	config = (struct config *)g_object_get_data(G_OBJECT(widget),
						    "config_struct");
	name = (char *)g_object_get_data(G_OBJECT(widget),
					 "config_name");
	ptr = config_get_data_ptr(config, name);

	config_reset_value(config, name);

	rgba_to_gdkrgba(*ptr, &color);

	gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(widget), &color);
}

static GtkWidget *config_color_button(GtkWidget *dialog, struct config *config,
				      const char *name)
{
	GtkWidget *button;
	unsigned int *data;
	GdkRGBA color;

	button = gtk_color_button_new();
	if (!button)
		return NULL;

	data = config_get_data_ptr(config, name);

	rgba_to_gdkrgba(*data, &color);

	gtk_color_chooser_set_use_alpha(GTK_COLOR_CHOOSER(button), TRUE);
	gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(button), &color);

	g_object_set_data(G_OBJECT(button), "config_struct", config);
	g_object_set_data(G_OBJECT(button), "config_name", (char *)name);

	g_signal_connect(G_OBJECT(button), "color-set",
			 G_CALLBACK(color_button_callback), data);

	g_signal_connect(G_OBJECT(dialog), "response",
			 G_CALLBACK(default_button_color_button_cb),
			 button);

	return button;
}

void ntsc_auto_tune_callback(GtkToggleButton *toggle, gpointer user_data)
{
	GtkWidget *widget;

	widget = user_data;

	gtk_widget_set_sensitive(widget, !gtk_toggle_button_get_active(toggle));
}

static void configuration_setup_scanlines(GtkWidget *dialog, struct config *config)
{
	GtkWidget *filter_grid;
	GtkWidget *tmp;
	GtkWidget *scale;
	GtkWidget *dialog_box;

	dialog_box = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
	filter_grid = gtk_grid_new();
	gtk_grid_set_column_spacing(GTK_GRID(filter_grid), 8);
	gtk_box_pack_start(GTK_BOX(dialog_box), filter_grid,
			   FALSE, FALSE, 0);

	tmp = gtk_label_new_with_mnemonic("Scanline _Intensity:");
	gtk_grid_attach(GTK_GRID(filter_grid), tmp, 0, 2, 1, 1);
	

	scale = config_int_scale(0, 100, 0, 100, dialog, config,
			       "scanline_intensity");

	gtk_grid_attach(GTK_GRID(filter_grid), scale, 1, 2, 2, 1);

}

static GtkWidget *create_cropping_box(GtkWidget *dialog, struct config *config)
{
	GtkWidget *cropping_grid;

	GtkWidget *spin_ntsc_first_scanline;
	GtkWidget *spin_ntsc_last_scanline;
	GtkWidget *spin_ntsc_first_pixel;
	GtkWidget *spin_ntsc_last_pixel;
	GtkWidget *spin_pal_first_scanline;
	GtkWidget *spin_pal_last_scanline;
	GtkWidget *spin_pal_first_pixel;
	GtkWidget *spin_pal_last_pixel;

	GtkWidget *box, *scaling_box;

	GtkWidget *tmp;

	box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);

	cropping_grid = gtk_grid_new();
	gtk_grid_set_column_spacing(GTK_GRID(cropping_grid), 8);
	gtk_grid_set_row_spacing(GTK_GRID(cropping_grid), 8);
	gtk_box_pack_start(GTK_BOX(box), cropping_grid, FALSE, FALSE, 0);

	tmp = gtk_label_new("NTSC");
	gtk_grid_attach(GTK_GRID(cropping_grid), tmp, 1, 0, 1, 1);
	tmp = gtk_label_new("PAL");
	gtk_grid_attach(GTK_GRID(cropping_grid), tmp, 2, 0, 1, 1);

	tmp = gtk_label_new_with_mnemonic("F_irst Scanline:");
	gtk_widget_set_halign(tmp, GTK_ALIGN_START);
	gtk_grid_attach(GTK_GRID(cropping_grid), tmp, 0, 2, 1, 1);
	spin_ntsc_first_scanline =
		config_int_spinbutton(dialog, config,
				      "ntsc_first_scanline");
	gtk_label_set_mnemonic_widget(GTK_LABEL(tmp), spin_ntsc_first_scanline);
	spin_pal_first_scanline =
		config_int_spinbutton(dialog, config,
				      "pal_first_scanline");
	gtk_grid_attach(GTK_GRID(cropping_grid), spin_ntsc_first_scanline, 1, 2, 1, 1);
	gtk_grid_attach(GTK_GRID(cropping_grid), spin_pal_first_scanline, 2, 2, 1, 1);

	tmp = gtk_label_new_with_mnemonic("L_ast Scanline:");
	gtk_widget_set_halign(tmp, GTK_ALIGN_START);
	gtk_grid_attach(GTK_GRID(cropping_grid), tmp, 0, 3, 1, 1);
	spin_ntsc_last_scanline =
		config_int_spinbutton(dialog, config,
				      "ntsc_last_scanline");
	gtk_label_set_mnemonic_widget(GTK_LABEL(tmp), spin_ntsc_last_scanline);
	spin_pal_last_scanline =
		config_int_spinbutton(dialog, config,
				      "pal_last_scanline");
	gtk_grid_attach(GTK_GRID(cropping_grid), spin_ntsc_last_scanline, 1, 3, 1, 1);
	gtk_grid_attach(GTK_GRID(cropping_grid), spin_pal_last_scanline, 2, 3, 1, 1);

	tmp = gtk_label_new_with_mnemonic("First _Pixel:");
	gtk_widget_set_halign(tmp, GTK_ALIGN_START);
	gtk_grid_attach(GTK_GRID(cropping_grid), tmp, 0, 4, 1, 1);
	spin_ntsc_first_pixel =
		config_int_spinbutton(dialog, config,
				      "ntsc_first_pixel");
	gtk_label_set_mnemonic_widget(GTK_LABEL(tmp), spin_ntsc_first_pixel);
	spin_pal_first_pixel =
		config_int_spinbutton(dialog, config,"pal_first_pixel");
	gtk_grid_attach(GTK_GRID(cropping_grid), spin_ntsc_first_pixel, 1, 4, 1, 1);
	gtk_grid_attach(GTK_GRID(cropping_grid), spin_pal_first_pixel, 2, 4, 1, 1);

	tmp = gtk_label_new_with_mnemonic("Last P_ixel:");
	gtk_widget_set_halign(tmp, GTK_ALIGN_START);
	gtk_grid_attach(GTK_GRID(cropping_grid), tmp, 0, 5, 1, 1);
	spin_ntsc_last_pixel =
		config_int_spinbutton(dialog, config, "ntsc_last_pixel");
	spin_pal_last_pixel =
		config_int_spinbutton(dialog, config, "pal_last_pixel");
	gtk_grid_attach(GTK_GRID(cropping_grid), spin_ntsc_last_pixel, 1, 5, 1, 1);
	gtk_grid_attach(GTK_GRID(cropping_grid), spin_pal_last_pixel, 2, 5, 1, 1);
	gtk_label_set_mnemonic_widget(GTK_LABEL(tmp), spin_ntsc_last_pixel);

	scaling_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_box_pack_start(GTK_BOX(box), scaling_box, FALSE, FALSE, 0);

	tmp = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
	gtk_box_pack_start(GTK_BOX(box), tmp, FALSE, FALSE, 0);


	return box;
}

static void configuration_setup_custom_palette(GtkWidget *dialog, struct config *config)
{
	GtkWidget *palette_options_grid;
	GtkWidget *palette_entry;
	GtkWidget *button_custom_palette;
	GtkWidget *dialog_box, *tmp;

	dialog_box = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
	palette_options_grid = gtk_grid_new();
	gtk_box_pack_start(GTK_BOX(dialog_box), palette_options_grid,
			   FALSE, FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(palette_options_grid), 4);
	gtk_grid_set_column_spacing(GTK_GRID(palette_options_grid), 8);
	gtk_grid_set_row_spacing(GTK_GRID(palette_options_grid), 8);

	/* Add widgets for custom palette file */
	tmp = gtk_label_new_with_mnemonic("_Custom Palette Path:");
	palette_entry = config_entry(dialog, config, "palette_path");
	gtk_grid_attach(GTK_GRID(palette_options_grid), tmp, 1, 0, 1, 1);
	gtk_grid_attach(GTK_GRID(palette_options_grid), palette_entry, 2, 0, 2, 1);
	gtk_entry_set_width_chars(GTK_ENTRY(palette_entry), 25);
	button_custom_palette = gtk_button_new_with_label("Browse...");
	gtk_grid_attach(GTK_GRID(palette_options_grid), button_custom_palette, 4, 0, 1, 1);
	g_object_set_data(G_OBJECT(button_custom_palette), "dialog",
			  dialog);
	g_signal_connect(G_OBJECT(button_custom_palette), "clicked",
			 G_CALLBACK(file_chooser_callback), palette_entry);
}

static void configuration_setup_yiq_palette(GtkWidget *dialog, struct config *config)
{
	GtkWidget *ntsc_palette_options_grid;
	GtkWidget *spin_ntsc_saturation;
	GtkWidget *spin_ntsc_hue;
	GtkWidget *spin_ntsc_contrast;
	GtkWidget *spin_ntsc_brightness;
	GtkWidget *spin_ntsc_gamma;
	GtkWidget *combo_ntsc_rgb_decoder;
	GtkWidget *dialog_box;

	GtkWidget *tmp;

	dialog_box = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
	ntsc_palette_options_grid = gtk_grid_new();
	gtk_box_pack_start(GTK_BOX(dialog_box), ntsc_palette_options_grid,
	                   FALSE, FALSE, 8);
	gtk_container_set_border_width(GTK_CONTAINER(ntsc_palette_options_grid), 4);
	gtk_grid_set_column_homogeneous(GTK_GRID(ntsc_palette_options_grid), TRUE);
	gtk_grid_set_column_spacing(GTK_GRID(ntsc_palette_options_grid), 8);
	gtk_grid_set_row_spacing(GTK_GRID(ntsc_palette_options_grid), 8);

	tmp = gtk_label_new_with_mnemonic("_Saturation:");
	gtk_widget_set_halign(tmp, GTK_ALIGN_START);
	gtk_grid_attach(GTK_GRID(ntsc_palette_options_grid), tmp, 0, 0, 1, 1);
	spin_ntsc_saturation = config_double_spinbutton(dialog, config, 0.01,
							"ntsc_palette_saturation");
	gtk_label_set_mnemonic_widget(GTK_LABEL(tmp), spin_ntsc_saturation);
	gtk_grid_attach(GTK_GRID(ntsc_palette_options_grid), spin_ntsc_saturation, 1, 0, 1, 1);
	tmp = gtk_label_new_with_mnemonic("_Hue:");
	gtk_widget_set_halign(tmp, GTK_ALIGN_START);
	gtk_grid_attach(GTK_GRID(ntsc_palette_options_grid), tmp, 0, 1, 1, 1);
	spin_ntsc_hue = config_double_spinbutton(dialog, config, 0.1,
						 "ntsc_palette_hue");
	gtk_label_set_mnemonic_widget(GTK_LABEL(tmp), spin_ntsc_hue);
	gtk_grid_attach(GTK_GRID(ntsc_palette_options_grid), spin_ntsc_hue, 1, 1, 1, 1);
	tmp = gtk_label_new_with_mnemonic("_Contrast:");
	gtk_widget_set_halign(tmp, GTK_ALIGN_START);
	gtk_grid_attach(GTK_GRID(ntsc_palette_options_grid), tmp, 2, 1, 1, 1);
	spin_ntsc_contrast = config_double_spinbutton(dialog, config, 0.01,
						      "ntsc_palette_contrast");
	gtk_label_set_mnemonic_widget(GTK_LABEL(tmp), spin_ntsc_contrast);
	gtk_grid_attach(GTK_GRID(ntsc_palette_options_grid), spin_ntsc_contrast, 3, 1, 1, 1);
	tmp = gtk_label_new_with_mnemonic("_Brightness:");
	gtk_widget_set_halign(tmp, GTK_ALIGN_START);
	gtk_grid_attach(GTK_GRID(ntsc_palette_options_grid), tmp, 2, 0, 1, 1);
	spin_ntsc_brightness = config_double_spinbutton(dialog, config, 0.01,
							"ntsc_palette_brightness");
	gtk_label_set_mnemonic_widget(GTK_LABEL(tmp), spin_ntsc_brightness);
	gtk_grid_attach(GTK_GRID(ntsc_palette_options_grid), spin_ntsc_brightness, 3, 0, 1, 1);
	tmp = gtk_label_new_with_mnemonic("_Gamma:");
	gtk_widget_set_halign(tmp, GTK_ALIGN_START);
	gtk_grid_attach(GTK_GRID(ntsc_palette_options_grid), tmp, 2, 2, 1, 1);
	spin_ntsc_gamma = config_double_spinbutton(dialog, config, 0.01,
						   "ntsc_palette_gamma");
	gtk_label_set_mnemonic_widget(GTK_LABEL(tmp), spin_ntsc_gamma);
	gtk_grid_attach(GTK_GRID(ntsc_palette_options_grid), spin_ntsc_gamma, 3, 2, 1, 1);

	tmp = gtk_label_new_with_mnemonic("_RGB Decoder:");
	gtk_widget_set_halign(tmp, GTK_ALIGN_START);
	gtk_grid_attach(GTK_GRID(ntsc_palette_options_grid), tmp, 0, 2, 1, 1);
	combo_ntsc_rgb_decoder = config_combo_box(dialog, config,
						  "ntsc_rgb_decoder");
	gtk_label_set_mnemonic_widget(GTK_LABEL(tmp), combo_ntsc_rgb_decoder);
	gtk_grid_attach(GTK_GRID(ntsc_palette_options_grid), combo_ntsc_rgb_decoder,
			1, 2, 1, 1);
}

static void configuration_setup_cropping(GtkWidget *dialog, struct config *config)
{
	GtkWidget *dialog_box;
	GtkWidget *cropping_box;

	dialog_box = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
	cropping_box = create_cropping_box(dialog, config);
	gtk_box_pack_start(GTK_BOX(dialog_box), cropping_box, FALSE, FALSE, 8);
}

static void configuration_setup_osd(GtkWidget *dialog, struct config *config)
{
	GtkWidget *dialog_box;
	GtkWidget *osd_options_grid;
	GtkWidget *spin_osd_min_font_size;
	GtkWidget *spin_osd_delay;
	GtkWidget *osd_fg_color;
	GtkWidget *osd_bg_color;
	GtkWidget *button_font_path;
	GtkWidget *check_osd_enabled;
	GtkWidget *font_path_entry;

	GtkWidget *tmp;

	dialog_box = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
	osd_options_grid = gtk_grid_new();
	gtk_box_pack_start(GTK_BOX(dialog_box), osd_options_grid, FALSE, FALSE, 8);
	gtk_grid_set_row_spacing(GTK_GRID(osd_options_grid), 8);
	gtk_grid_set_column_spacing(GTK_GRID(osd_options_grid), 8);

	check_osd_enabled = config_checkbox(dialog, "_On-Screen Display enabled",
					    config, "osd_enabled");
	spin_osd_min_font_size = config_int_spinbutton(dialog, config,
						       "osd_min_font_size");
	tmp = gtk_label_new_with_mnemonic("_Minimum font size:");
	gtk_widget_set_halign(tmp, GTK_ALIGN_START);
	gtk_grid_attach(GTK_GRID(osd_options_grid), tmp, 0, 1, 1, 1);
	gtk_grid_attach(GTK_GRID(osd_options_grid), spin_osd_min_font_size, 1, 1, 1, 1);

	gtk_grid_attach(GTK_GRID(osd_options_grid), check_osd_enabled, 0, 0, 1, 1);

	tmp = gtk_label_new_with_mnemonic("Seconds OSD messages dis_played:");
	gtk_widget_set_halign(tmp, GTK_ALIGN_START);

	spin_osd_delay = config_int_spinbutton(dialog, config,
					       "osd_delay");

	gtk_grid_attach(GTK_GRID(osd_options_grid), tmp, 0, 2, 1, 1);
	gtk_grid_attach(GTK_GRID(osd_options_grid), spin_osd_delay, 1, 2, 1, 1);

	tmp = gtk_label_new_with_mnemonic("F_oreground color:");
	gtk_widget_set_halign(tmp, GTK_ALIGN_START);
	osd_fg_color = config_color_button(dialog, config, "osd_fg_rgba");
	gtk_grid_attach(GTK_GRID(osd_options_grid), tmp, 0, 3, 1, 1);
	gtk_grid_attach(GTK_GRID(osd_options_grid), osd_fg_color, 1, 3, 1, 1);

	tmp = gtk_label_new_with_mnemonic("_Background color:");
	gtk_widget_set_halign(tmp, GTK_ALIGN_START);
	osd_bg_color = config_color_button(dialog, config, "osd_bg_rgba");
	gtk_grid_attach(GTK_GRID(osd_options_grid), tmp, 0, 4, 1, 1);
	gtk_grid_attach(GTK_GRID(osd_options_grid), osd_bg_color, 1, 4, 1, 1);

	tmp = gtk_label_new_with_mnemonic("Font p_ath:");
	gtk_widget_set_halign(tmp, GTK_ALIGN_START);
	font_path_entry = config_entry(dialog, config, "osd_font_path");
	// set width?
	button_font_path = gtk_button_new_with_label("Browse...");

	gtk_grid_attach(GTK_GRID(osd_options_grid), tmp, 0, 5, 1, 1);
	gtk_grid_attach(GTK_GRID(osd_options_grid), font_path_entry, 1, 5, 2, 1);
	gtk_grid_attach(GTK_GRID(osd_options_grid), button_font_path, 3, 5, 1, 1);
	g_object_set_data(G_OBJECT(button_font_path), "dialog", dialog);
	g_signal_connect(G_OBJECT(button_font_path), "clicked",
			 G_CALLBACK(file_chooser_callback), font_path_entry);
}

void gui_cropping_configuration_dialog(GtkWidget *widget, gpointer user_data)
{
	gui_configuration_dialog("Cropping Settings",
				 configuration_setup_cropping, 0,
				 widget, user_data);
}

void gui_osd_configuration_dialog(GtkWidget *widget, gpointer user_data)
{
	gui_configuration_dialog("On-Screen Display Settings",
				 configuration_setup_osd, 0,
				 widget, user_data);
}

void gui_scanline_settings_dialog(GtkWidget *widget, gpointer user_data)
{
	gui_configuration_dialog("Emulated Scanline Settings",
				 configuration_setup_scanlines, 0,
				 widget, user_data);
}

void gui_video_configuration_dialog(GtkWidget *widget, gpointer user_data)
{
	gui_configuration_dialog("On-Scren Display Configuration",
				 configuration_setup_osd, 0,
				 widget, user_data);
}

void gui_custom_palette_configuration_dialog(GtkWidget *widget, gpointer user_data)
{
	gui_configuration_dialog("Custom Palette Settings",
				 configuration_setup_custom_palette, 0,
				 widget, user_data);
}

void gui_yiq_palette_configuration_dialog(GtkWidget *widget, gpointer user_data)
{
	gui_configuration_dialog("YIQ Palette Settings",
				 configuration_setup_yiq_palette, 0,
				 widget, user_data);
}

static void configuration_setup_ntsc_filter(GtkWidget *dialog, struct config *config)
{
	GtkWidget *dialog_box;
	GtkWidget *spin;
	GtkWidget *combo;
	GtkWidget *label;
	GtkWidget *grid;
	GtkWidget *check;
	GtkWidget *button_grid;
	GtkWidget *button;

	dialog_box = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
	grid = gtk_grid_new();
	gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
	gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
	gtk_box_pack_start(GTK_BOX(dialog_box), grid, FALSE, FALSE, 8);

	label = gtk_label_new_with_mnemonic("Merge _fields:");
	combo = config_combo_box(dialog, config, "ntsc_filter_merge_fields");
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), combo);
	gtk_grid_attach(GTK_GRID(grid), label, 0, 0, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), combo, 1, 0, 1, 1);

	check = config_checkbox(dialog, "Auto-_tune filter", config,
				"ntsc_filter_auto_tune");
	gtk_grid_attach(GTK_GRID(grid), check, 0, 1, 1, 1);

	label = gtk_label_new_with_mnemonic("_Sharpness:");
	spin = config_double_spinbutton(dialog, config, 0.01, "ntsc_filter_sharpness");
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), spin);
	gtk_grid_attach(GTK_GRID(grid), label, 0, 2, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), spin, 1, 2, 1, 1);
	g_signal_connect(G_OBJECT(check), "toggled",
			 G_CALLBACK(ntsc_auto_tune_callback), spin);
	ntsc_auto_tune_callback(GTK_TOGGLE_BUTTON(check), spin);
	g_object_set_data(G_OBJECT(dialog), "sharpness", spin);

	label = gtk_label_new_with_mnemonic("_Resolution:");
	spin = config_double_spinbutton(dialog, config, 0.01, "ntsc_filter_resolution");
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), spin);
	gtk_grid_attach(GTK_GRID(grid), label, 0, 3, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), spin, 1, 3, 1, 1);
	g_signal_connect(G_OBJECT(check), "toggled",
			 G_CALLBACK(ntsc_auto_tune_callback), spin);
	ntsc_auto_tune_callback(GTK_TOGGLE_BUTTON(check), spin);
	g_object_set_data(G_OBJECT(dialog), "resolution", spin);

	label = gtk_label_new_with_mnemonic("_Artifacts:");
	spin = config_double_spinbutton(dialog, config, 0.01, "ntsc_filter_artifacts");
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), spin);
	gtk_grid_attach(GTK_GRID(grid), label, 0, 4, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), spin, 1, 4, 1, 1);
	g_signal_connect(G_OBJECT(check), "toggled",
			 G_CALLBACK(ntsc_auto_tune_callback), spin);
	ntsc_auto_tune_callback(GTK_TOGGLE_BUTTON(check), spin);
	g_object_set_data(G_OBJECT(dialog), "artifacts", spin);

	label = gtk_label_new_with_mnemonic("_Fringing:");
	spin = config_double_spinbutton(dialog, config, 0.01, "ntsc_filter_fringing");
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), spin);
	gtk_grid_attach(GTK_GRID(grid), label, 0, 5, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), spin, 1, 5, 1, 1);
	g_signal_connect(G_OBJECT(check), "toggled",
			 G_CALLBACK(ntsc_auto_tune_callback), spin);
	ntsc_auto_tune_callback(GTK_TOGGLE_BUTTON(check), spin);
	g_object_set_data(G_OBJECT(dialog), "fringing", spin);

	label = gtk_label_new_with_mnemonic("_Bleed:");
	spin = config_double_spinbutton(dialog, config, 0.01, "ntsc_filter_bleed");
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), spin);
	gtk_grid_attach(GTK_GRID(grid), label, 0, 6, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), spin, 1, 6, 1, 1);
	g_signal_connect(G_OBJECT(check), "toggled",
			 G_CALLBACK(ntsc_auto_tune_callback), spin);
	ntsc_auto_tune_callback(GTK_TOGGLE_BUTTON(check), spin);
	g_object_set_data(G_OBJECT(dialog), "bleed", spin);

	button_grid = gtk_grid_new();
	gtk_grid_set_row_spacing(GTK_GRID(button_grid), 8);
	gtk_grid_set_column_spacing(GTK_GRID(button_grid), 8);
	gtk_grid_set_column_homogeneous(GTK_GRID(button_grid), TRUE);
	button = gtk_button_new_with_mnemonic("_Composite");
	g_object_set_data(G_OBJECT(button), "preset", GINT_TO_POINTER(0));
	gtk_grid_attach(GTK_GRID(button_grid), button, 0, 0, 1, 1);
	g_signal_connect(G_OBJECT(check), "toggled",
			 G_CALLBACK(ntsc_auto_tune_callback), button);
	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(ntsc_filter_preset_callback), dialog);

	ntsc_auto_tune_callback(GTK_TOGGLE_BUTTON(check), button);
	button = gtk_button_new_with_mnemonic("S-_Video");
	g_object_set_data(G_OBJECT(button), "preset", GINT_TO_POINTER(1));
	gtk_grid_attach(GTK_GRID(button_grid), button, 1, 0, 1, 1);
	g_signal_connect(G_OBJECT(check), "toggled",
			 G_CALLBACK(ntsc_auto_tune_callback), button);
	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(ntsc_filter_preset_callback), dialog);

	ntsc_auto_tune_callback(GTK_TOGGLE_BUTTON(check), button);
	button = gtk_button_new_with_mnemonic("_RGB");
	g_object_set_data(G_OBJECT(button), "preset", GINT_TO_POINTER(2));
	gtk_grid_attach(GTK_GRID(button_grid), button, 2, 0, 1, 1);
	g_signal_connect(G_OBJECT(check), "toggled",
			 G_CALLBACK(ntsc_auto_tune_callback), button);
	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(ntsc_filter_preset_callback), dialog);

	ntsc_auto_tune_callback(GTK_TOGGLE_BUTTON(check), button);

	gtk_box_pack_start(GTK_BOX(dialog_box), button_grid, FALSE, FALSE, 8);
}

static void ntsc_filter_preset_callback(GtkWidget *widget, gpointer user_data)
{
	GtkWidget *dialog;
	GtkWidget *sharpness;
	GtkWidget *resolution;
	GtkWidget *artifacts;
	GtkWidget *fringing;
	GtkWidget *bleed;
	int preset;

	const float presets[3][5] = {
		{0, 0, 0, 0 ,0},
		{0.2, 0.2, -1, -1,  0},
		{0.2, 0.7, -1, -1, -1},
	};

	dialog = user_data;
	preset = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widget), "preset"));
	sharpness = g_object_get_data(G_OBJECT(dialog), "sharpness");
	resolution = g_object_get_data(G_OBJECT(dialog), "resolution");
	artifacts = g_object_get_data(G_OBJECT(dialog), "artifacts");
	fringing = g_object_get_data(G_OBJECT(dialog), "fringing");
	bleed = g_object_get_data(G_OBJECT(dialog), "bleed");

	gtk_spin_button_set_value(GTK_SPIN_BUTTON(sharpness), presets[preset][0]);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(resolution), presets[preset][1]);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(artifacts), presets[preset][2]);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(fringing), presets[preset][3]);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(bleed), presets[preset][4]);
}

void gui_ntsc_filter_settings_dialog(GtkWidget *widget, gpointer user_data)
{
	gui_configuration_dialog("NTSC Filter Settings",
				 configuration_setup_ntsc_filter, 0,
				 widget, user_data);
}
