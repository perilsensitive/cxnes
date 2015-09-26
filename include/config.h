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

#ifndef __CONFIG_H__
#define __CONFIG_H__

#include "emu.h"

enum {
	CONFIG_DATA_DIR_SAVE,
	CONFIG_DATA_DIR_CONFIG,
	CONFIG_DATA_DIR_CHEAT,
	CONFIG_DATA_DIR_SCREENSHOT,
	CONFIG_DATA_DIR_PATCH,
	CONFIG_DATA_DIR_ROM,
	CONFIG_DATA_DIR_STATE,
	CONFIG_DATA_FILE_OSD_FONT,
	CONFIG_DATA_FILE_FDS_BIOS,
	CONFIG_DATA_FILE_MAIN_CFG,
	CONFIG_DATA_FILE_NSF_ROM,
	CONFIG_DATA_FILE_GAMECONTROLLER_DB,
	CONFIG_DATA_FILE_USER_GAMECONTROLLER_DB,
	CONFIG_DATA_FILE_ROM_DB,
};

#if defined __unix__
#define DEFAULT_DATA_DIR_BASE ".cxnes"
#define PATHSEP "/"
#elif defined _WIN32
#define DEFAULT_DATA_DIR_BASE ""
#define PATHSEP "\\"
#endif

struct config {
	/* PPU/Video options */
	const char *sprite_limit_mode;

	int fps_display_enabled;
	int scanline_renderer_enabled;
	int window_scaling_factor;
	int fullscreen;
	int autohide_cursor;
	int vsync;
	int stretch_to_fit;
	const char *scaling_mode;

	int run_in_background;
	int nsf_run_in_background;

	const char *palette;
	const char *palette_path;
	const char *ntsc_pixel_aspect_ratio;
	int ntsc_first_scanline;
	int ntsc_last_scanline;
	int ntsc_first_pixel;
	int ntsc_last_pixel;

	const char *pal_pixel_aspect_ratio;
	int pal_first_scanline;
	int pal_last_scanline;
	int pal_first_pixel;
	int pal_last_pixel;

	double ntsc_palette_saturation;
	double ntsc_palette_hue;
	double ntsc_palette_contrast;
	double ntsc_palette_brightness;
	double ntsc_palette_gamma;

	double ntsc_filter_sharpness;
	double ntsc_filter_fringing;
	double ntsc_filter_artifacts;
	double ntsc_filter_resolution;
	double ntsc_filter_bleed;
	const char *ntsc_rgb_decoder;
	const char *ntsc_filter_merge_fields;
	int ntsc_filter_auto_tune;

	const char *osd_font_path;
	int osd_enabled;
	int osd_min_font_size;
	int osd_fg_rgba;
	int osd_bg_rgba;
	int osd_delay;

	/* Audio/APU options */
	int sample_rate;
	int audio_buffer_size;
	int force_stereo;
	int dynamic_rate_adjustment_enabled;
	int master_volume;
	int apu_pulse0_volume;
	int apu_pulse1_volume;
	int apu_triangle_volume;
	int apu_noise_volume;
	int apu_dmc_volume;
	int fds_volume;
	int vrc6_pulse0_volume;
	int vrc6_pulse1_volume;
	int vrc6_sawtooth_volume;
	int namco163_volume;
	int mmc5_pulse0_volume;
	int mmc5_pulse1_volume;
	int mmc5_pcm_volume;
	int sunsoft5b_channel_volume[3];
	int namco163_channel_volume[8];
	int swap_pulse_duty_cycles;
	const char *raw_pcm_filter;

	/* Misc emulator options */

	int gui_enabled;
	const char *save_path;
	const char *screenshot_path;
	const char *config_path;
	const char *cheat_path;
	const char *patch_path;
	const char *rom_path;
	const char *nsf_rom_path;
	const char *fds_bios_path;
	const char *maincfg_path;
	const char *romcfg_path;
	const char *state_path;
	int skip_maincfg;
	int skip_romcfg;
	int nvram_uses_memmap;
	int autopatch_enabled;
	const char *timing;
	int save_uses_romdir;
	int config_uses_romdir;
	int cpu_trace_enabled;
	int blargg_test_rom_hack_enabled;
	int screensaver_deactivate_delay;
	int nsf_first_track;
	int mmc6_compat_hack_enabled;
	int fds_bios_patch_enabled;
	int fds_auto_disk_change_enabled;
	int fds_hide_license_screen;
	int fds_hide_bios_title_screen;
	int guess_region_from_filename;
	int auto_wram;
	int dip_switch[8];

	/* Input/Controller options */

	const char *default_port1_device;
	const char *default_port2_device;
	const char *default_port3_device;
	const char *default_port4_device;
	const char *default_exp_device;
	const char *four_player_mode;
	const char *vs_controller_mode;
	int mask_opposite_directions;
	int vs_swap_start_select;
	int swap_start_select;
	int swap_a_b;
	int vs_coin_on_start;
	int combine_p1p2;
	int arkanoid_paddle2_connected; /* FIXME ugh */

	int cheats_enabled;

	int autoload_cheats;
	int autosave_cheats;

	int autoload_state;
	int autosave_state;

	int periodic_savestate_enabled;
	int periodic_savestate_delay;
	const char *periodic_savestate_path;

#if GUI_ENABLED
	int default_to_rom_path;
#endif

	double dynamic_rate_adjustment_max;
	double dynamic_rate_adjustment_low_watermark;
	double dynamic_rate_adjustment_buffer_range;

	const char *preferred_console_type;
	const char *rom_console_type;
	const char *rom_vs_ppu_type;

	const char *video_filter;

	int db_enabled;
	int turbo_speed;
	int scanline_intensity;
	int scanlines_enabled;

	int gamecontroller;
};

void config_reset_value(struct config *, const char *name);
void *config_get_data_ptr(struct config *, const char *name);
void main_config_set(struct config *, const char *name, const char *value);
void rom_config_set(struct config *, const char *name, const char *value);
struct config *config_copy(struct config *config);
int config_load_main_config(struct config *config);
int config_load_rom_config(struct config *config, char *filename);
int config_save_main_config(struct config *config);
int config_save_rom_config(struct config *config, const char *path);
void config_print_current_config(struct config *);
char *config_get_path(struct config *, int prefix, const char *path);
char *config_get_path_new(struct config *, int prefix, const char *path, int user);
char *config_get_cfg_dir(struct config *);
char *config_get_fds_bios(struct config *);
char *config_get_nsf_rom(struct config *);
char *config_get_cheat_dir(struct config *);
struct config *config_init(struct emu *emu);
void config_cleanup(struct config *);
void config_shutdown(void);
const void *config_get_valid_values(const char *name, int *count,
				    const char ***value_names);

int config_get_int_min(const char *name);
int config_get_int_max(const char *name);
unsigned int config_get_unsigned_min(const char *name);
unsigned int config_get_unsigned_max(const char *name);
double config_get_float_min(const char *name);
double config_get_float_max(const char *name);
void config_load_default_bindings(void);
int config_set_portable_mode(void);

#endif				/* __CONFIG_H__ */
