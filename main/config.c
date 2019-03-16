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

#include <stddef.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#if defined _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <direct.h>
#endif

#include "emu.h"
#include "file_io.h"
#include "actions.h"
#include "input.h"

#include "config_private.h"

#ifndef PACKAGE_NAME
#define PACKAGE_NAME "cxnes"
#endif

#ifndef DATADIR
#define DATADIR ""
#endif

#define BUF_SIZE 256
#define DEFAULT_DATA_DIR "cxnes"
#define DEFAULT_MAIN_CONFIG "cxnes.cfg"
#define DEFAULT_NSF_ROM "nsf.rom"
#define DEFAULT_GAMECONTROLLER_DB "gamecontrollerdb.txt"
#define DEFAULT_ROM_DB "romdb.txt"
#define DEFAULT_CHEAT_DB "cheatdb"
#define DEFAULT_CHEAT_PATH "cheat"
#define DEFAULT_SAVE_PATH "save"
#define DEFAULT_FDS_SAVE_PATH "fds"
#define DEFAULT_PATCH_PATH "patch"
#define DEFAULT_STATE_PATH "state"
#define DEFAULT_ROMCFG_PATH "romcfg"
#define DEFAULT_SCREENSHOT_PATH "screenshot"

#ifndef DEFAULT_OSD_FONT
#define DEFAULT_OSD_FONT ""
#endif

static const char *valid_loglevels[] = {
	"debug",
	"info",
	"warn",
	"error",
	"critical",
};

static const char *valid_loglevel_names[] = {
	"DEBUG",
	"INFO",
	"WARN",
	"ERROR",
	"CRITICAL",
};

static const char *valid_sprite_limit_modes[] = {
	"never",
	"always",
	"auto",
};

static const char *valid_sprite_limit_mode_names[] = {
	"Disabled",
	"Enabled",
	"Automatic",
};

static const char *valid_preferred_console_type_names[] = {
	"Automatic",
	"Famicom (RP2C02 PPU)",
	"Famicom RGB (RP2C03B PPU)",
	"NTSC NES (RP2C02 PPU)",
	"NTSC NES RGB (RP2C03B PPU)",
	"PAL NES (RP2C07 PPU)",
	"Dendy",
};

static const char *valid_rom_system_type_names[] = {
	"Preferred",
	"Automatic",
	"Famicom (RP2C02 PPU)",
	"Famicom RGB (RP2C03B PPU)",
	"NTSC NES (RP2C02 PPU)",
	"NTSC NES RGB (RP2C03B PPU)",
	"PAL NES (RP2C07 PPU)",
	"Dendy",
	"RP2C03B", "RP2C03G", "RP2C04-0001", "RP2C04-0002",
	"RP2C04-0003", "RP2C04-0004", "RP2C03B", "RP2C03C",
	"RC2C05-01", "RC2C05-02", "RC2C05-03", "RC2C05-04",
	"RC2C05-05", "Vs. Super Mario Bros.", "Vs. Super SkyKid",
	"Vs. Pinball (Japan)", "Vs. Pinball (USA)", "Vs. Ice Climber",
	"Vs. Raid on Bungeling Bay", "PlayChoice",
};

static const char *valid_rom_system_type_values[] = {
	"preferred", "auto", "famicom", "famicom_rgb", "nes", "nes_rgb",
	"pal_nes", "dendy",
	"rp2c03b", "rp2c03g", "rp2c04-0001", "rp2c04-0002",
	"rp2c04-0003", "rp2c04-0004", "rc2c03b", "rc2c03c",
	"rc2c05-01","rc2c05-02","rc2c05-03","rc2c05-04",
	"rc2c05-05", "vs-super-mario-bros", "vs-super-skykid",
	"vs-pinball-japan", "vs-pinball-usa", "vs-ice-climber",
	"vs-raid-on-bungeling-bay", "playchoice",
};

static const char *valid_rom_overclock_values[] = {
	"default",
	"disabled",
	"post-render",
	"vblank",
};

static const char *valid_rom_overclock_names[] = {
	"Default",
	"Disabled",
	"Post-render",
	"VBlank",
};

static const char *valid_default_overclock_mode_values[] = {
	"disabled",
	"post-render",
	"vblank",
};

static const char *valid_default_overclock_mode_names[] = {
	"Disabled",
	"Post-render",
	"VBlank",
};

static const char *valid_preferred_console_type_values[] = {
	"auto", "famicom", "famicom_rgb", "nes", "nes_rgb",
	"pal_nes", "dendy"
};

static const char *valid_rgb_decoder_names[] = {
	"Consumer", "FCC", "Alternative",
};

static const char *valid_rgb_decoder_values[] = {
	"consumer", "fcc", "alternative",
};

static const char *valid_video_filter_values[] = {
	"none", "ntsc", "scale2x", "scale3x", "scale4x", "hq2x", "hq3x", "hq4x",
};

static const char *valid_video_filter_names[] = {
	"None", "NTSC", "Scale2x", "Scale3x", "Scale4x", "HQ2X", "HQ3X", "HQ4X",
};

static const char *valid_palette_names[] = {
	"Auto", "YIQ", "RP2C03B", "RC2C03B", "RP2C04",
	"Nostalgia (FBX)", "NES-Classic (FBX)",
	"FCEUX Default", "Custom"
};

static const char *valid_ntsc_filter_merge_fields_values[] = {
	"auto", "on", "off",
};

static const char *valid_ntsc_filter_merge_fields_names[] = {
	"Auto", "On", "Off",
};

static const char *valid_palettes[] = {
	"auto", "yiq", "rp2c03b","rc2c03b", "rp2c04",
	"firebrandx_nostalgia", "firebrandx_nes_classic",
	"fceux", "custom"
};

static const char *valid_aspect_ratio_names[] = {
	"NTSC (8:7)", "PAL (11:8)", "Square (1:1)",
};

static const char *valid_aspect_ratios[] = {
	"ntsc", "pal", "square"
};

static const char *valid_pcm_filter_names[] = {
	"Never", "During sample playback", "Always",
};

static const char *valid_raw_pcm_filters[] = {
	"never", "playback", "always"
};

static const char *valid_four_player_names[] = {
	"Auto", "None", "NES (Four Score/Satellite)", "Famicom",
};

static const char *valid_four_player_modes[] = {
	"auto", "none", "nes", "famicom",
};

static const char *port1_device_names[] = {
	"Auto", "None",
	"Controller 1", "Controller 2",
	"Controller 3", "Controller 4",
	"SNES Controller 1", "SNES Controller 2",
	"SNES Controller 3", "SNES Controller 4",
	"Zapper", "Power Pad (Side A)",
	"Power Pad (Side B)", "NES Arkanoid Controller",
	"SNES Mouse", "VS. Zapper",
};

static const char *valid_port1_devices[] = {
	"auto", "none", "controller1", "controller2",
	"controller3", "controller4",
	"snes_controller1", "snes_controller2",
	"snes_controller3", "snes_controller4",
	"zapper", "powerpad_a", "powerpad_b",
	"arkanoid_nes", "snes_mouse", "vs_zapper",
};

static const char *port2_device_names[] = {
	"Auto", "None",
	"Controller 1", "Controller 2",
	"Controller 3", "Controller 4",
	"SNES Controller 1", "SNES Controller 2",
	"SNES Controller 3", "SNES Controller 4",
	"Zapper", "Power Pad (Side A)",
	"Power Pad (Side B)", "NES Arkanoid Controller",
	"SNES Mouse",
};

static const char *valid_port2_devices[] = {
	"auto", "none", "controller1", "controller2", "controller3",
	"controller4",
	"snes_controller1", "snes_controller2",
	"snes_controller3", "snes_controller4",
	"zapper", "powerpad_a", "powerpad_b",
	"arkanoid_nes", "snes_mouse"
};

static const char *port3_4_device_names[] = {
	"Auto", "None",
	"Controller 1", "Controller 2",
	"Controller 3", "Controller 4",
	"SNES Controller 1", "SNES Controller 2",
	"SNES Controller 3", "SNES Controller 4",
	"SNES Mouse",
};

static const char *valid_port3_4_devices[] = {
	"auto", "none", "controller1", "controller2",
	"controller3", "controller4",
	"snes_controller1", "snes_controller2",
	"snes_controller3", "snes_controller4",
	"snes_mouse"
};

static const char *exp_device_names[] = {
	"Auto", "None", "Famicom Arkanoid Controller", "Arkanoid II Controller",
	"VS. Zapper", "Family BASIC Keyboard",
	"Family Trainer (Side A)",
	"Family Trainer (Side B)",
	"Bandai Hyper Shot",
	"Konami Hyper Shot",
};

static const char *valid_exp_devices[] = {
	"auto", "none", "arkanoid_fc", "arkanoid2", "vs_zapper",
	"familykeyboard", "suborkeyboard", "ftrainer_a", "ftrainer_b",
	"bandai_hyper_shot", "konami_hyper_shot",
};

static const int valid_sample_rates[] = {
	44100, 48000, 96000
};

static const char *valid_scaling_mode_names[] = {
	"Nearest-neighbor",
	"Bilinear filtering",
	"Nearest-neighbor scaling, bilinear stretching",
};

static const char *valid_scaling_modes[] = {
	"nearest", "linear", "nearest_then_linear",
};

#define CONFIG_BOOLEAN(var, def)		\
{ \
	.name = #var, \
	.type = CONFIG_TYPE_BOOLEAN, \
	.offset = offsetof(struct config, var), \
	.default_value.bool = (def), \
}

#define CONFIG_BOOLEAN_NOSAVE(var, def)		\
{ \
	.name = #var, \
	.type = CONFIG_TYPE_BOOLEAN_NOSAVE, \
	.offset = offsetof(struct config, var), \
	.default_value.bool = (def), \
}

#define CONFIG_INTEGER_FULL(var, def, list, minimum, maximum, check)	\
{ \
	.name = #var, \
	.type = CONFIG_TYPE_INTEGER, \
	.offset = offsetof(struct config, var), \
	.default_value.integer = (def), \
	.valid_values.integer_list = (list), \
	.valid_value_count = sizeof(list) / sizeof(int),	\
	.validator = check, \
	.min.svalue = (minimum),		\
	.max.svalue = (maximum),	\
}

#define CONFIG_INTEGER_LIST(var, def, list) \
	CONFIG_INTEGER_FULL(var, def, list, 0, 0, NULL)

#define CONFIG_INTEGER_VALIDATOR(var, def, check) \
	CONFIG_INTEGER_FULL(var, def, NULL, 0, 0, check)

#define CONFIG_INTEGER(var, def, minimum, maximum)		\
	CONFIG_INTEGER_FULL(var, def, NULL, minimum, maximum, NULL)

#define CONFIG_UNSIGNED_INTEGER_FULL(var, def, list, minimum, maximum, check)	\
{ \
	.name = #var, \
	.type = CONFIG_TYPE_UNSIGNED_INTEGER, \
	.offset = offsetof(struct config, var), \
	.default_value.unsigned_int = (def), \
	.valid_values.unsigned_int_list = (list), \
	.valid_value_count = sizeof(list) / sizeof(unsigned int),	\
	.validator = check, \
	.min.uvalue = (minimum),		\
	.max.uvalue = (maximum),	\
}

#define CONFIG_UNSIGNED_INTEGER_LIST(var, def, list) \
	CONFIG_UNSIGNED_INTEGER_FULL(var, def, list, 0, 0, NULL)

#define CONFIG_UNSIGNED_INTEGER_VALIDATED(var, def, check) \
	CONFIG_UNSIGNED_INTEGER_FULL(var, def, NULL, 0, 0, check)

#define CONFIG_UNSIGNED_INTEGER(var, def, minimum, maximum)		\
	CONFIG_UNSIGNED_INTEGER_FULL(var, def, NULL, minimum, maximum, NULL)

#define CONFIG_FLOAT_FULL(var, def, list, minimum, maximum, check) \
{ \
	.name = #var, \
	.type = CONFIG_TYPE_FLOAT, \
	.offset = offsetof(struct config, var), \
	.default_value.fpoint = (def), \
	.valid_values.float_list = (list), \
	.valid_value_count = sizeof(list) / sizeof(float),	\
	.validator = check, \
	.min.fvalue = (minimum),		\
	.max.fvalue = (maximum),	\
}

#define CONFIG_FLOAT_LIST(var, def, list) \
	CONFIG_FLOAT_FULL(var, def, list, 0, 0, NULL)

#define CONFIG_FLOAT_VALIDATED(var, def, check) \
	CONFIG_FLOAT_FULL(var, def, NULL, 0, 0, check)

#define CONFIG_FLOAT(var, def, minimum, maximum)		\
	CONFIG_FLOAT_FULL(var, def, NULL, minimum, maximum, NULL)

#define CONFIG_STRING_FULL(var, def, list, names, check)	\
{ \
	.name = #var, \
	.type = CONFIG_TYPE_STRING, \
	.offset = offsetof(struct config, var), \
	.default_value.string = (def), \
	.valid_values.string_list = (list), \
	.valid_value_count = sizeof(list) / sizeof(char *),	\
	.valid_value_names = names, \
	.validator = check, \
}

#define CONFIG_STRING_LIST(var, def, list, names)		\
	CONFIG_STRING_FULL(var, def, list, names, NULL)

#define CONFIG_STRING_VALIDATED(var, def, check) \
	CONFIG_STRING_FULL(var, def, NULL, NULL, check)

#define CONFIG_STRING(var, def) \
	CONFIG_STRING_FULL(var, def, NULL, NULL, NULL)

#define CONFIG_END() { .name = NULL, .type = CONFIG_NONE }

struct config_parameter rom_config_parameters[] = {
	CONFIG_STRING_LIST(overclock_mode, "default",
	                   valid_rom_overclock_values,
	                   valid_rom_overclock_names),
	CONFIG_BOOLEAN(remember_overclock_mode, 0),
	CONFIG_BOOLEAN(remember_input_devices, 0),
	CONFIG_BOOLEAN(remember_system_type, 0),
	CONFIG_STRING_LIST(rom_system_type, "preferred",
			   valid_rom_system_type_values,
			   valid_rom_system_type_names),
	CONFIG_BOOLEAN(swap_a_b, 0),
	CONFIG_BOOLEAN(swap_start_select, 0),
	CONFIG_STRING_LIST(default_port1_device, "auto",
			   valid_port1_devices, port1_device_names),
	CONFIG_STRING_LIST(default_port2_device, "auto",
			   valid_port2_devices, port2_device_names),
	CONFIG_STRING_LIST(default_port3_device, "auto",
			   valid_port3_4_devices, port3_4_device_names),
	CONFIG_STRING_LIST(default_port4_device, "auto",
			   valid_port3_4_devices, port3_4_device_names),
	CONFIG_STRING_LIST(default_exp_device, "auto",
			   valid_exp_devices, exp_device_names),
	CONFIG_STRING_LIST(four_player_mode, "auto",
			   valid_four_player_modes,
			   valid_four_player_names),

#if 0
	CONFIG_STRING(periodic_savestate_path, NULL),
#endif
	{
		.name = "dip_switch_1",
		.type = CONFIG_TYPE_BOOLEAN,
		.offset = offsetof(struct config, dip_switch[0]),
		.default_value.integer = 0,
		.valid_value_count = 0,
	},
	{
		.name = "dip_switch_2",
		.type = CONFIG_TYPE_BOOLEAN,
		.offset = offsetof(struct config, dip_switch[1]),
		.default_value.integer = 0,
		.valid_value_count = 0,
	},
	{
		.name = "dip_switch_3",
		.type = CONFIG_TYPE_BOOLEAN,
		.offset = offsetof(struct config, dip_switch[2]),
		.default_value.integer = 0,
		.valid_value_count = 0,
	},
	{
		.name = "dip_switch_4",
		.type = CONFIG_TYPE_BOOLEAN,
		.offset = offsetof(struct config, dip_switch[3]),
		.default_value.integer = 0,
		.valid_value_count = 0,
	},
	{
		.name = "dip_switch_5",
		.type = CONFIG_TYPE_BOOLEAN,
		.offset = offsetof(struct config, dip_switch[4]),
		.default_value.integer = 0,
		.valid_value_count = 0,
	},
	{
		.name = "dip_switch_6",
		.type = CONFIG_TYPE_BOOLEAN,
		.offset = offsetof(struct config, dip_switch[5]),
		.default_value.integer = 0,
		.valid_value_count = 0,
	},
	{
		.name = "dip_switch_7",
		.type = CONFIG_TYPE_BOOLEAN,
		.offset = offsetof(struct config, dip_switch[6]),
		.default_value.integer = 0,
		.valid_value_count = 0,
	},
	{
		.name = "dip_switch_8",
		.type = CONFIG_TYPE_BOOLEAN,
		.offset = offsetof(struct config, dip_switch[7]),
		.default_value.integer = 0,
		.valid_value_count = 0,
	},
	CONFIG_END(),
};

struct config_parameter config_parameters[] = {
	CONFIG_STRING_LIST(sprite_limit_mode, "never",
			   valid_sprite_limit_modes,
			   valid_sprite_limit_mode_names),
	CONFIG_STRING_LIST(loglevel, "info",
			   valid_loglevels,
			   valid_loglevel_names),
	CONFIG_BOOLEAN(scanline_renderer_enabled, 1),
	CONFIG_BOOLEAN(fps_display_enabled, 0),
	CONFIG_INTEGER(window_scaling_factor, 1, 1, 8),
	CONFIG_BOOLEAN(fullscreen, 0),
	CONFIG_BOOLEAN(autohide_cursor, 1),
	CONFIG_BOOLEAN(vsync, 1),
	CONFIG_BOOLEAN(stretch_to_fit, 0),
	CONFIG_BOOLEAN(gamecontroller, 1),

	CONFIG_INTEGER(alternate_speed, 180, 1, 240),
	CONFIG_BOOLEAN(alternate_speed_mute, 1),

	CONFIG_STRING_LIST(palette, "auto", valid_palettes,
			   valid_palette_names),
	CONFIG_STRING(palette_path, NULL),
	CONFIG_STRING(cheat_path, NULL),
	CONFIG_STRING_LIST(ntsc_pixel_aspect_ratio, "square",
			   valid_aspect_ratios, valid_aspect_ratio_names),
	CONFIG_INTEGER(ntsc_first_scanline, 8, 0, 24),
	CONFIG_INTEGER(ntsc_last_scanline, 231, 215, 239),
	CONFIG_INTEGER(ntsc_first_pixel, 0, 0, 16),
	CONFIG_INTEGER(ntsc_last_pixel, 255, 239, 255),

	CONFIG_STRING_LIST(pal_pixel_aspect_ratio, "square",
			   valid_aspect_ratios, valid_aspect_ratio_names),
	CONFIG_INTEGER(pal_first_scanline, 1, 0, 24),
	CONFIG_INTEGER(pal_last_scanline, 239, 215, 239),
	CONFIG_INTEGER(pal_first_pixel, 0, 0, 16),
	CONFIG_INTEGER(pal_last_pixel, 255, 239, 255),

	CONFIG_FLOAT(ntsc_palette_saturation, 1.5, 0, 2),
	CONFIG_FLOAT(ntsc_palette_hue, 0, -180, 180),
	CONFIG_FLOAT(ntsc_palette_contrast, 1.0, 0.5, 1.5),
	CONFIG_FLOAT(ntsc_palette_brightness, 1.0, 0.5, 1.5),
	CONFIG_FLOAT(ntsc_palette_gamma, 0.0, -1, 1),
	CONFIG_STRING_LIST(ntsc_rgb_decoder, "consumer",
			   valid_rgb_decoder_values,
			   valid_rgb_decoder_names),

	CONFIG_FLOAT(ntsc_filter_sharpness, 0, -1, 1),
	CONFIG_FLOAT(ntsc_filter_fringing, 0, -1, 1),
	CONFIG_FLOAT(ntsc_filter_artifacts, 0, -1, 1),
	CONFIG_FLOAT(ntsc_filter_resolution, 0, -1, 1),
	CONFIG_FLOAT(ntsc_filter_bleed, 0, -1, 1),
	CONFIG_BOOLEAN(ntsc_filter_auto_tune, 1),
	CONFIG_STRING_LIST(ntsc_filter_merge_fields, "auto",
			   valid_ntsc_filter_merge_fields_values,
			   valid_ntsc_filter_merge_fields_names),

	CONFIG_STRING(osd_font_path, NULL),
	CONFIG_BOOLEAN(osd_enabled, 1),
	CONFIG_INTEGER(osd_min_font_size, 8, 1, 64),

	CONFIG_UNSIGNED_INTEGER(osd_fg_rgba, 0xffffffff, 0, 0xffffffff),
	CONFIG_UNSIGNED_INTEGER(osd_bg_rgba, 0x000064A0, 0, 0xffffffff),
	CONFIG_INTEGER(osd_delay, 2, 0, 5),

	CONFIG_STRING_LIST(default_overclock_mode, "disabled",
			   valid_default_overclock_mode_values,
			   valid_default_overclock_mode_names),
	CONFIG_INTEGER_LIST(sample_rate, 48000, valid_sample_rates),
	CONFIG_INTEGER(audio_buffer_size, 2048, 1024, 8192), /* FIXME min? max? */
	CONFIG_BOOLEAN(dynamic_rate_adjustment_enabled, 1),
	CONFIG_BOOLEAN(force_stereo, 0),
	CONFIG_INTEGER(master_volume, 100, 0, 200),
	CONFIG_INTEGER(apu_pulse0_volume, 100, 0, 100),
	CONFIG_INTEGER(apu_pulse1_volume, 100, 0, 100),
	CONFIG_INTEGER(apu_triangle_volume, 100, 0, 100),
	CONFIG_INTEGER(apu_noise_volume, 100, 0, 100),
	CONFIG_INTEGER(apu_dmc_volume, 100, 0, 100),
	CONFIG_INTEGER(vrc6_pulse0_volume, 100, 0, 100),
	CONFIG_INTEGER(vrc6_pulse1_volume, 100, 0, 100),
	CONFIG_INTEGER(vrc6_sawtooth_volume, 100, 0, 100),
	CONFIG_INTEGER(mmc5_pulse0_volume, 100, 0, 100),
	CONFIG_INTEGER(mmc5_pulse1_volume, 100, 0, 100),
	CONFIG_INTEGER(mmc5_pcm_volume, 100, 0, 100),
	{
		.name = "vrc7_channel0_volume",
		.type = CONFIG_TYPE_INTEGER,
		.offset = offsetof(struct config, vrc7_channel_volume[0]),
		.default_value.integer = 100,
		.valid_value_count = 0,
		.min.svalue = 0,
		.max.svalue = 100,
	},
	{
		.name = "vrc7_channel1_volume",
		.type = CONFIG_TYPE_INTEGER,
		.offset = offsetof(struct config, vrc7_channel_volume[1]),
		.default_value.integer = 100,
		.valid_value_count = 0,
		.min.svalue = 0,
		.max.svalue = 100,
	},
	{
		.name = "vrc7_channel2_volume",
		.type = CONFIG_TYPE_INTEGER,
		.offset = offsetof(struct config, vrc7_channel_volume[2]),
		.default_value.integer = 100,
		.valid_value_count = 0,
		.min.svalue = 0,
		.max.svalue = 100,
	},
	{
		.name = "vrc7_channel3_volume",
		.type = CONFIG_TYPE_INTEGER,
		.offset = offsetof(struct config, vrc7_channel_volume[3]),
		.default_value.integer = 100,
		.valid_value_count = 0,
		.min.svalue = 0,
		.max.svalue = 100,
	},
	{
		.name = "vrc7_channel4_volume",
		.type = CONFIG_TYPE_INTEGER,
		.offset = offsetof(struct config, vrc7_channel_volume[4]),
		.default_value.integer = 100,
		.valid_value_count = 0,
		.min.svalue = 0,
		.max.svalue = 100,
	},
	{
		.name = "vrc7_channel5_volume",
		.type = CONFIG_TYPE_INTEGER,
		.offset = offsetof(struct config, vrc7_channel_volume[5]),
		.default_value.integer = 100,
		.valid_value_count = 0,
		.min.svalue = 0,
		.max.svalue = 100,
	},
	{
		.name = "sunsoft5b_channel0_volume",
		.type = CONFIG_TYPE_INTEGER,
		.offset = offsetof(struct config, sunsoft5b_channel_volume[0]),
		.default_value.integer = 100,
		.valid_value_count = 0,
		.min.svalue = 0,
		.max.svalue = 100,
	},
	{
		.name = "sunsoft5b_channel1_volume",
		.type = CONFIG_TYPE_INTEGER,
		.offset = offsetof(struct config, sunsoft5b_channel_volume[1]),
		.default_value.integer = 100,
		.valid_value_count = 0,
		.min.svalue = 0,
		.max.svalue = 100,
	},
	{
		.name = "sunsoft5b_channel2_volume",
		.type = CONFIG_TYPE_INTEGER,
		.offset = offsetof(struct config, sunsoft5b_channel_volume[2]),
		.default_value.integer = 100,
		.valid_value_count = 0,
		.min.svalue = 0,
		.max.svalue = 100,
	},
	{
		.name = "namco163_channel0_volume",
		.type = CONFIG_TYPE_INTEGER,
		.offset = offsetof(struct config, namco163_channel_volume[0]),
		.default_value.integer = 100,
		.valid_value_count = 0,
		.min.svalue = 0,
		.max.svalue = 100,
	},
	{
		.name = "namco163_channel1_volume",
		.type = CONFIG_TYPE_INTEGER,
		.offset = offsetof(struct config, namco163_channel_volume[1]),
		.default_value.integer = 100,
		.valid_value_count = 0,
		.min.svalue = 0,
		.max.svalue = 100,
	},
	{
		.name = "namco163_channel2_volume",
		.type = CONFIG_TYPE_INTEGER,
		.offset = offsetof(struct config, namco163_channel_volume[2]),
		.default_value.integer = 100,
		.valid_value_count = 0,
		.min.svalue = 0,
		.max.svalue = 100,
	},
	{
		.name = "namco163_channel3_volume",
		.type = CONFIG_TYPE_INTEGER,
		.offset = offsetof(struct config, namco163_channel_volume[3]),
		.default_value.integer = 100,
		.valid_value_count = 0,
		.min.svalue = 0,
		.max.svalue = 100,
	},
	{
		.name = "namco163_channel4_volume",
		.type = CONFIG_TYPE_INTEGER,
		.offset = offsetof(struct config, namco163_channel_volume[4]),
		.default_value.integer = 100,
		.valid_value_count = 0,
		.min.svalue = 0,
		.max.svalue = 100,
	},
	{
		.name = "namco163_channel5_volume",
		.type = CONFIG_TYPE_INTEGER,
		.offset = offsetof(struct config, namco163_channel_volume[5]),
		.default_value.integer = 100,
		.valid_value_count = 0,
		.min.svalue = 0,
		.max.svalue = 100,
	},
	{
		.name = "namco163_channel6_volume",
		.type = CONFIG_TYPE_INTEGER,
		.offset = offsetof(struct config, namco163_channel_volume[6]),
		.default_value.integer = 100,
		.valid_value_count = 0,
		.min.svalue = 0,
		.max.svalue = 100,
	},
	{
		.name = "namco163_channel7_volume",
		.type = CONFIG_TYPE_INTEGER,
		.offset = offsetof(struct config, namco163_channel_volume[7]),
		.default_value.integer = 100,
		.valid_value_count = 0,
		.min.svalue = 0,
		.max.svalue = 100,
	},
	CONFIG_INTEGER(fds_volume, 100, 0, 100),
	CONFIG_BOOLEAN(swap_pulse_duty_cycles, 0),
	CONFIG_STRING_LIST(raw_pcm_filter, "never", valid_raw_pcm_filters,
		valid_pcm_filter_names),

	CONFIG_STRING(screensaver_deactivate_command, "xdg-screensaver reset"),
	CONFIG_BOOLEAN(gui_enabled, 1),
	CONFIG_STRING(save_path, NULL),
	CONFIG_STRING(fds_save_path, NULL),
	CONFIG_STRING(screenshot_path, NULL),
	CONFIG_STRING(config_path, NULL),
	CONFIG_STRING(rom_path, NULL),
	CONFIG_STRING(patch_path, NULL),
	CONFIG_STRING(nsf_rom_path, NULL),
	CONFIG_STRING(fds_bios_path, NULL),
	CONFIG_STRING(maincfg_path, NULL),
	CONFIG_STRING(romcfg_path, NULL),
	CONFIG_BOOLEAN(skip_maincfg, 0),
	CONFIG_BOOLEAN(skip_romcfg, 0),
	CONFIG_BOOLEAN(nvram_uses_memmap, 1),
	CONFIG_BOOLEAN(autopatch_enabled, 1),

	CONFIG_STRING_LIST(scaling_mode, "nearest_then_linear",
			   valid_scaling_modes,
			   valid_scaling_mode_names),

	CONFIG_FLOAT(dynamic_rate_adjustment_max, 0.0055, 0, 0.010),
	CONFIG_FLOAT(dynamic_rate_adjustment_low_watermark, 0.5, 0.0, 3.0),
	CONFIG_FLOAT(dynamic_rate_adjustment_buffer_range, 1.2, 1.0, 3.0),

	CONFIG_BOOLEAN(save_uses_romdir, 0),
	CONFIG_BOOLEAN(config_uses_romdir, 0),
	CONFIG_BOOLEAN_NOSAVE(cpu_trace_enabled, 0),
	CONFIG_BOOLEAN(blargg_test_rom_hack_enabled, 0),
	CONFIG_INTEGER(screensaver_deactivate_delay, 60, 0, 3600),

	CONFIG_INTEGER(nsf_first_track, 1, 1, 255),
	CONFIG_BOOLEAN(mmc6_compat_hack_enabled, 1),
	CONFIG_BOOLEAN(fds_bios_patch_enabled, 0),
	CONFIG_BOOLEAN(fds_auto_disk_change_enabled, 0),
	CONFIG_BOOLEAN(fds_hide_bios_title_screen, 0),
	CONFIG_BOOLEAN(fds_hide_license_screen, 0),
	CONFIG_BOOLEAN(fds_use_patch_for_saves, 1),
	CONFIG_BOOLEAN(guess_system_type_from_filename, 1),
	CONFIG_BOOLEAN(auto_wram, 1),

	CONFIG_BOOLEAN(mask_opposite_directions, 1),
	CONFIG_BOOLEAN(arkanoid_paddle2_connected, 1),

	CONFIG_BOOLEAN(run_in_background, 0),
	CONFIG_BOOLEAN(nsf_run_in_background, 1),

	CONFIG_BOOLEAN(vs_swap_start_select, 1),
	CONFIG_BOOLEAN(vs_coin_on_start, 1),

	CONFIG_BOOLEAN(cheats_enabled, 1),
	CONFIG_BOOLEAN(autoload_cheats, 0),
	CONFIG_BOOLEAN(autosave_cheats, 0),

	CONFIG_BOOLEAN(autoload_state, 0),
	CONFIG_BOOLEAN(autosave_state, 0),

#if 0
	CONFIG_BOOLEAN(periodic_savestate_enabled, 0),
	CONFIG_INTEGER(periodic_savestate_delay, 0, 0, 3600),
#endif

	CONFIG_INTEGER(turbo_speed, 5, 0, 7),
	CONFIG_INTEGER(scanline_intensity, 50, 0, 100),
	CONFIG_BOOLEAN(scanlines_enabled, 0),

	CONFIG_STRING(state_path, NULL),
	CONFIG_BOOLEAN(db_enabled, 1),

	CONFIG_INTEGER(frames_before_overclock, 360, 0, 600),
	CONFIG_INTEGER(overclock_scanlines, 262, 0, 1000),
	CONFIG_INTEGER(overclock_pcm_sample_threshold, 10, 1, 50),

#if GUI_ENABLED
	CONFIG_BOOLEAN(default_to_rom_path, 0),
#endif

	CONFIG_STRING_LIST(video_filter, "none", valid_video_filter_values,
		valid_video_filter_names),

	CONFIG_STRING_LIST(preferred_console_type, "auto",
			   valid_preferred_console_type_values,
			   valid_preferred_console_type_names),

	CONFIG_END()
};

struct binding_item default_bindings[] = {
/* Misc. Input Bindings */
	{ .name = "Keyboard ScrollLock", .value = "KEYBOARD_ENABLE, KEYBOARD_DISABLE" },
	{ .name = "Keyboard Tab", .value = "ALT_SPEED" },

	{ .name = "Keyboard P", .value = "PAUSE" },
	{ .name = "Keyboard Pause", .value = "PAUSE" },
	{ .name = "Keyboard Escape", .value = "TOGGLE_MENUBAR" },

	{ .name = "Keyboard 0", .value = "STATE_SELECT_0" },
	{ .name = "Keyboard 1", .value = "STATE_SELECT_1" },
	{ .name = "Keyboard 2", .value = "STATE_SELECT_2" },
	{ .name = "Keyboard 3", .value = "STATE_SELECT_3" },
	{ .name = "Keyboard 4", .value = "STATE_SELECT_4" },
	{ .name = "Keyboard 5", .value = "STATE_SELECT_5" },
	{ .name = "Keyboard 6", .value = "STATE_SELECT_6" },
	{ .name = "Keyboard 7", .value = "STATE_SELECT_7" },
	{ .name = "Keyboard 8", .value = "STATE_SELECT_8" },
	{ .name = "Keyboard 9", .value = "STATE_SELECT_9" },

	{ .name = "Keyboard F2", .value = "TOGGLE_FULLSCREEN" },
	{ .name = "Keyboard F3", .value = "TOGGLE_FPS" },
	{ .name = "Keyboard F4", .value = "QUIT" },
	{ .name = "Keyboard F5", .value = "STATE_SAVE_SELECTED" },
	{ .name = "Keyboard F6", .value = "FDS_SELECT" },
	{ .name = "Keyboard F7", .value = "STATE_LOAD_SELECTED" },
	{ .name = "Keyboard F8", .value = "VS_SERVICE_SWITCH,FDS_EJECT" },
	{ .name = "Keyboard F8", .value = "VS_COIN_SWITCH_1,FDS_FLIP" },
	{ .name = "Keyboard F9", .value = "SOFT_RESET" },
	{ .name = "Keyboard F11", .value = "HARD_RESET" },
	{ .name = "Keyboard F12", .value = "SAVE_SCREENSHOT" },

/* Controller Input Bindings */
	{ .name = "Keyboard Up", .value = "CONTROLLER_1_UP" },
	{ .name = "Keyboard Down", .value = "CONTROLLER_1_DOWN" },
	{ .name = "Keyboard Left", .value = "CONTROLLER_1_LEFT" },
	{ .name = "Keyboard Right", .value = "CONTROLLER_1_RIGHT" },
	{ .name = "Keyboard Return", .value = "CONTROLLER_1_START" },
	{ .name = "Keyboard s", .value = "CONTROLLER_1_SELECT" },
	{ .name = "Keyboard d", .value = "CONTROLLER_1_B" },
	{ .name = "Keyboard f", .value = "CONTROLLER_1_A" },
	{ .name = "Joystick 0 D-Pad Up", .value = "CONTROLLER_1_UP" },
	{ .name = "Joystick 0 D-Pad Down", .value = "CONTROLLER_1_DOWN" },
	{ .name = "Joystick 0 D-Pad Left", .value = "CONTROLLER_1_LEFT" },
	{ .name = "Joystick 0 D-Pad Right", .value = "CONTROLLER_1_RIGHT" },
	{ .name = "Joystick 0 Start", .value = "CONTROLLER_1_START" },
	{ .name = "Joystick 0 Back", .value = "CONTROLLER_1_SELECT" },
	{ .name = "Joystick 0 X", .value = "CONTROLLER_1_B" },
	{ .name = "Joystick 0 A", .value = "CONTROLLER_1_A" },
	{ .name = "Joystick 0 Y", .value = "CONTROLLER_1_SNES_X" },
	{ .name = "Joystick 0 B", .value = "CONTROLLER_1_SNES_A" },
	{ .name = "Joystick 0 Left Shoulder", .value = "CONTROLLER_1_SNES_L" },
	{ .name = "Joystick 0 Right Shoulder", .value = "CONTROLLER_1_SNES_R" },
	{ .name = "Joystick 0 Left Y -", .value = "CONTROLLER_1_UP" },
	{ .name = "Joystick 0 Left Y +", .value = "CONTROLLER_1_DOWN" },
	{ .name = "Joystick 0 Left X -", .value = "CONTROLLER_1_LEFT" },
	{ .name = "Joystick 0 Left X +", .value = "CONTROLLER_1_RIGHT" },
	{ .name = "Joystick 1 D-Pad Up", .value = "CONTROLLER_2_UP" },
	{ .name = "Joystick 1 D-Pad Down", .value = "CONTROLLER_2_DOWN" },
	{ .name = "Joystick 1 D-Pad Left", .value = "CONTROLLER_2_LEFT" },
	{ .name = "Joystick 1 D-Pad Right", .value = "CONTROLLER_2_RIGHT" },
	{ .name = "Joystick 1 Start", .value = "CONTROLLER_2_START" },
	{ .name = "Joystick 1 Back", .value = "CONTROLLER_2_SELECT" },
	{ .name = "Joystick 1 X", .value = "CONTROLLER_2_B" },
	{ .name = "Joystick 1 A", .value = "CONTROLLER_2_A" },
	{ .name = "Joystick 1 Y", .value = "CONTROLLER_2_SNES_X" },
	{ .name = "Joystick 1 B", .value = "CONTROLLER_2_SNES_A" },
	{ .name = "Joystick 1 Left Shoulder", .value = "CONTROLLER_2_SNES_L" },
	{ .name = "Joystick 1 Right Shoulder", .value = "CONTROLLER_2_SNES_R" },
	{ .name = "Joystick 1 Left Y -", .value = "CONTROLLER_2_UP" },
	{ .name = "Joystick 1 Left Y +", .value = "CONTROLLER_2_DOWN" },
	{ .name = "Joystick 1 Left X -", .value = "CONTROLLER_2_LEFT" },
	{ .name = "Joystick 1 Left X +", .value = "CONTROLLER_2_RIGHT" },
	{ .name = "Joystick 2 D-Pad Up", .value = "CONTROLLER_3_UP" },
	{ .name = "Joystick 2 D-Pad Down", .value = "CONTROLLER_3_DOWN" },
	{ .name = "Joystick 2 D-Pad Left", .value = "CONTROLLER_3_LEFT" },
	{ .name = "Joystick 2 D-Pad Right", .value = "CONTROLLER_3_RIGHT" },
	{ .name = "Joystick 2 Start", .value = "CONTROLLER_3_START" },
	{ .name = "Joystick 2 Back", .value = "CONTROLLER_3_SELECT" },
	{ .name = "Joystick 2 X", .value = "CONTROLLER_3_B" },
	{ .name = "Joystick 2 A", .value = "CONTROLLER_3_A" },
	{ .name = "Joystick 2 Y", .value = "CONTROLLER_3_SNES_X" },
	{ .name = "Joystick 2 B", .value = "CONTROLLER_3_SNES_A" },
	{ .name = "Joystick 2 Left Shoulder", .value = "CONTROLLER_3_SNES_L" },
	{ .name = "Joystick 2 Right Shoulder", .value = "CONTROLLER_3_SNES_R" },
	{ .name = "Joystick 2 Left Y -", .value = "CONTROLLER_3_UP" },
	{ .name = "Joystick 2 Left Y +", .value = "CONTROLLER_3_DOWN" },
	{ .name = "Joystick 2 Left X -", .value = "CONTROLLER_3_LEFT" },
	{ .name = "Joystick 2 Left X +", .value = "CONTROLLER_3_RIGHT" },
	{ .name = "Joystick 3 D-Pad Up", .value = "CONTROLLER_4_UP" },
	{ .name = "Joystick 3 D-Pad Down", .value = "CONTROLLER_4_DOWN" },
	{ .name = "Joystick 3 D-Pad Left", .value = "CONTROLLER_4_LEFT" },
	{ .name = "Joystick 3 D-Pad Right", .value = "CONTROLLER_4_RIGHT" },
	{ .name = "Joystick 3 Start", .value = "CONTROLLER_4_START" },
	{ .name = "Joystick 3 Back", .value = "CONTROLLER_4_SELECT" },
	{ .name = "Joystick 3 X", .value = "CONTROLLER_4_B" },
	{ .name = "Joystick 3 A", .value = "CONTROLLER_4_A" },
	{ .name = "Joystick 3 Y", .value = "CONTROLLER_4_SNES_X" },
	{ .name = "Joystick 3 B", .value = "CONTROLLER_4_SNES_A" },
	{ .name = "Joystick 3 Left Shoulder", .value = "CONTROLLER_4_SNES_L" },
	{ .name = "Joystick 3 Right Shoulder", .value = "CONTROLLER_4_SNES_R" },
	{ .name = "Joystick 3 Left Y -", .value = "CONTROLLER_4_UP" },
	{ .name = "Joystick 3 Left Y +", .value = "CONTROLLER_4_DOWN" },
	{ .name = "Joystick 3 Left X -", .value = "CONTROLLER_4_LEFT" },
	{ .name = "Joystick 3 Left X +", .value = "CONTROLLER_4_RIGHT" },

/* Famicom Keyboard Input Bindings */
	{ .name = "Keyboard F1", .value = "KEYBOARD_f1" },
	{ .name = "Keyboard F2", .value = "KEYBOARD_f2" },
	{ .name = "Keyboard F3", .value = "KEYBOARD_f3" },
	{ .name = "Keyboard F4", .value = "KEYBOARD_f4" },
	{ .name = "Keyboard F5", .value = "KEYBOARD_f5" },
	{ .name = "Keyboard F6", .value = "KEYBOARD_f6" },
	{ .name = "Keyboard F7", .value = "KEYBOARD_f7" },
	{ .name = "Keyboard F8", .value = "KEYBOARD_f8" },
	{ .name = "Keyboard 1", .value = "KEYBOARD_1" },
	{ .name = "Keyboard 2", .value = "KEYBOARD_2" },
	{ .name = "Keyboard 3", .value = "KEYBOARD_3" },
	{ .name = "Keyboard 4", .value = "KEYBOARD_4" },
	{ .name = "Keyboard 5", .value = "KEYBOARD_5" },
	{ .name = "Keyboard 6", .value = "KEYBOARD_6" },
	{ .name = "Keyboard 7", .value = "KEYBOARD_7" },
	{ .name = "Keyboard 8", .value = "KEYBOARD_8" },
	{ .name = "Keyboard 9", .value = "KEYBOARD_9" },
	{ .name = "Keyboard 0", .value = "KEYBOARD_0" },
	{ .name = "Keyboard -", .value = "KEYBOARD_minus" },
	{ .name = "Keyboard Equals", .value = "KEYBOARD_equals, KEYBOARD_CARET" },
	{ .name = "Keyboard End", .value = "KEYBOARD_stop" },
	{ .name = "Keyboard Escape", .value = "KEYBOARD_ESCAPE" },
	{ .name = "Keyboard q", .value = "KEYBOARD_q" },
	{ .name = "Keyboard w", .value = "KEYBOARD_w" },
	{ .name = "Keyboard e", .value = "KEYBOARD_e" },
	{ .name = "Keyboard r", .value = "KEYBOARD_r" },
	{ .name = "Keyboard t", .value = "KEYBOARD_t" },
	{ .name = "Keyboard y", .value = "KEYBOARD_y" },
	{ .name = "Keyboard u", .value = "KEYBOARD_u" },
	{ .name = "Keyboard i", .value = "KEYBOARD_i" },
	{ .name = "Keyboard o", .value = "KEYBOARD_o" },
	{ .name = "Keyboard p", .value = "KEYBOARD_p" },
	{ .name = "Keyboard Backslash", .value = "KEYBOARD_BACKSLASH, KEYBOARD_YEN" },
	{ .name = "Keyboard [", .value = "KEYBOARD_leftbracket" },
	{ .name = "Keyboard Return", .value = "KEYBOARD_enter" },
	{ .name = "Keyboard Left Ctrl", .value = "KEYBOARD_lctrl" },
	{ .name = "Keyboard Right Ctrl", .value = "KEYBOARD_rctrl" },
	{ .name = "Keyboard a", .value = "KEYBOARD_a" },
	{ .name = "Keyboard s", .value = "KEYBOARD_s" },
	{ .name = "Keyboard d", .value = "KEYBOARD_d" },
	{ .name = "Keyboard f", .value = "KEYBOARD_f" },
	{ .name = "Keyboard g", .value = "KEYBOARD_g" },
	{ .name = "Keyboard h", .value = "KEYBOARD_h" },
	{ .name = "Keyboard j", .value = "KEYBOARD_j" },
	{ .name = "Keyboard k", .value = "KEYBOARD_k" },
	{ .name = "Keyboard l", .value = "KEYBOARD_l" },
	{ .name = "Keyboard ;", .value = "KEYBOARD_semicolon" },
	{ .name = "Keyboard '", .value = "KEYBOARD_colon" },
	{ .name = "Keyboard ]", .value = "KEYBOARD_rightbracket" },
	{ .name = "Keyboard Left Alt", .value = "KEYBOARD_lalt, KEYBOARD_GRAPH" },
	{ .name = "Keyboard Left Shift", .value = "KEYBOARD_lshift" },
	{ .name = "Keyboard z", .value = "KEYBOARD_z" },
	{ .name = "Keyboard x", .value = "KEYBOARD_x" },
	{ .name = "Keyboard c", .value = "KEYBOARD_c" },
	{ .name = "Keyboard v", .value = "KEYBOARD_v" },
	{ .name = "Keyboard b", .value = "KEYBOARD_b" },
	{ .name = "Keyboard n", .value = "KEYBOARD_n" },
	{ .name = "Keyboard m", .value = "KEYBOARD_m" },
	{ .name = "Keyboard ,", .value = "KEYBOARD_comma" },
	{ .name = "Keyboard .", .value = "KEYBOARD_period" },
	{ .name = "Keyboard /", .value = "KEYBOARD_slash" },
	{ .name = "Keyboard Right Shift", .value = "KEYBOARD_rshift" },
	{ .name = "Keyboard Right Alt", .value = "KEYBOARD_ralt, KEYBOARD_KANA" },
	{ .name = "Keyboard Space", .value = "KEYBOARD_space" },
	{ .name = "Keyboard Delete", .value = "KEYBOARD_del" },
	{ .name = "Keyboard Insert", .value = "KEYBOARD_ins" },
	{ .name = "Keyboard Backspace", .value = "KEYBOARD_bs" },
	{ .name = "Keyboard Tab", .value = "KEYBOARD_tab, KEYBOARD_UNDERSCORE" },
	{ .name = "Keyboard Up", .value = "KEYBOARD_up" },
	{ .name = "Keyboard Down", .value = "KEYBOARD_down" },
	{ .name = "Keyboard Left", .value = "KEYBOARD_left" },
	{ .name = "Keyboard Right", .value = "KEYBOARD_right" },
	{ .name = "Keyboard PageUp", .value = "KEYBOARD_PGUP" },
	{ .name = "Keyboard PageDown", .value = "KEYBOARD_PGDN" },
	{ .name = "Keyboard CapsLock", .value = "KEYBOARD_CAPS" },
	{ .name = "Keyboard Home", .value = "KEYBOARD_HOME" },
	{ .name = "Keyboard End", .value = "KEYBOARD_END" },
	{ .name = "Keyboard F9", .value = "KEYBOARD_F9" },
	{ .name = "Keyboard F10", .value = "KEYBOARD_F10" },
	{ .name = "Keyboard F11", .value = "KEYBOARD_F11" },
	{ .name = "Keyboard F12", .value = "KEYBOARD_F12" },
	{ .name = "Keyboard '", .value = "KEYBOARD_APOSTROPHE, KEYBOARD_COLON" },
	{ .name = "Keyboard `", .value = "KEYBOARD_BACKQUOTE, KEYBOARD_AT" },
	{ .name = "Keyboard Numlock", .value = "KEYBOARD_NUMLOCK" },
	{ .name = "Keyboard Pause", .value = "KEYBOARD_PAUSE" },

/* Mat (Power Pad, Family Trainer) Input Bindings */
	{ .name = "Keyboard u", .value = "MAT_2_1" },
	{ .name = "Keyboard i", .value = "MAT_2_2" },
	{ .name = "Keyboard o", .value = "MAT_2_3" },
	{ .name = "Keyboard p", .value = "MAT_2_4" },
	{ .name = "Keyboard j", .value = "MAT_2_5" },
	{ .name = "Keyboard k", .value = "MAT_2_6" },
	{ .name = "Keyboard l", .value = "MAT_2_7" },
	{ .name = "Keyboard ;", .value = "MAT_2_8" },
	{ .name = "Keyboard m", .value = "MAT_2_9" },
	{ .name = "Keyboard ,", .value = "MAT_2_10" },
	{ .name = "Keyboard .", .value = "MAT_2_11" },
	{ .name = "Keyboard /", .value = "MAT_2_12" },

	/* Konami Hyper Shot bindings */
	{ .name = "Keyboard d", .value = "KONAMI_HYPER_SHOT_P1_JUMP" },
	{ .name = "Keyboard f", .value = "KONAMI_HYPER_SHOT_P1_RUN" },
	{ .name = "Joystick 0 X", .value = "KONAMI_HYPER_SHOT_P1_JUMP" },
	{ .name = "Joystick 0 A", .value = "KONAMI_HYPER_SHOT_P1_RUN" },
	{ .name = "Joystick 1 X", .value = "KONAMI_HYPER_SHOT_P2_JUMP" },
	{ .name = "Joystick 1 A", .value = "KONAMI_HYPER_SHOT_P2_RUN" },

	{ .name = "Mouse",
	  .value = "ZAPPER_2_UPDATE_LOCATION,MOUSE_2_UPDATE_LOCATION,"
	  "ARKANOID_2_DIAL_MOUSE" },
	{ .name = "Mouse Button 1",
	  .value = "ZAPPER_2_TRIGGER,MOUSE_2_LEFTBUTTON,ARKANOID_2" },
	{ .name = "Mouse Button 3",
	  .value = "ZAPPER_2_TRIGGER_OFFSCREEN,MOUSE_2_RIGHTBUTTON" },

	{ .name = NULL },
};

struct path_list {
	char **paths;
	int count;
	int max_length;
};

static struct path_list *data_path_list;

struct path_list *path_list_new(void)
{
	struct path_list *list;

	list = malloc(sizeof(*list));

	if (list) {
		list->paths = NULL;
		list->count = 0;
		list->max_length = 0;
	}

	return list;
}

void path_list_free(struct path_list *list)
{
	int i;

	if (!list)
		return;

	for (i = 0; i < list->count; i++)
		free(list->paths[i]);

	free(list);
}

int path_list_add_entry(struct path_list *list, char *path)
{
	char **tmp;
	int count;
	int length;

	if (!list || !path)
		return -1;

	count = list->count + 1;

	tmp = realloc(list->paths, count * sizeof(*list->paths));

	if (!tmp)
		return -1;

	list->paths = tmp;
	list->paths[list->count] = path;
	list->count = count;

	length = strlen(path);

	if (length > list->max_length)
		list->max_length = length;

	return 0;
}


static int init_path_lists(void)
{
	char *path;
	int length;
	char *t;
#if __unix__
	char *c;
	char *xdg_data_dirs;
#endif

	data_path_list = path_list_new();

	if (!data_path_list)
		return -1;

	path = get_user_data_path();
	if (!path)
		return -1;
	
	if (path_list_add_entry(data_path_list, path)) {
		free(path);
		return -1;
	}

#if defined __unix__
	xdg_data_dirs = getenv("XDG_DATA_DIRS");
	if (xdg_data_dirs)
		xdg_data_dirs = strdup(xdg_data_dirs);
	else
		xdg_data_dirs = strdup(DATADIR);

	if (!xdg_data_dirs)
		return 0;
	
	t = xdg_data_dirs;

	do {
		c = strchr(t, ':');
		if (c)
			*c = '\0';

		if (strlen(t) && t[0] == '/') {
			length = strlen(t) + 1 + strlen("/" PACKAGE_NAME);
			path = malloc(length);
			if (!path)
				return -1;

			snprintf(path, length, "%s/%s", t, PACKAGE_NAME);
			if (path) {
				path_list_add_entry(data_path_list,
						    path);
			}
		}

		if (c) {
			*c = ':';
			t = c + 1;
		}
	} while (c);

	if (xdg_data_dirs)
		free(xdg_data_dirs);

#elif defined _WIN32
	path = get_base_path();
	if (!path)
		return -1;

	length = strlen(path) + strlen(PATHSEP "data") + 1;
	t = malloc(length);
	if (!t) {
		free(path);
		return -1;
	}

	snprintf(t, length, "%s" PATHSEP "data", path);
	free(path);

	if (path_list_add_entry(data_path_list, t)) {
		free(t);
		return -1;
	}
#endif

	return 0;
}

static void free_path_lists(void)
{
	if (data_path_list)
		path_list_free(data_path_list);
}

char *config_get_path(struct config *config, int which, const char* filename, int user)
{
	const char *config_value;
	const char *default_path;
	char *buffer;
	int length;
	int limit;
	int i;

	config_value = NULL;
	default_path = "";

	switch (which) {
	case CONFIG_DATA_DIR_CONFIG:
		config_value = config->romcfg_path;
		default_path = DEFAULT_ROMCFG_PATH;
		break;
	case CONFIG_DATA_FILE_MAIN_CFG:
		config_value = config->maincfg_path;
		default_path = "";
		filename = DEFAULT_MAIN_CONFIG;
		break;
	case CONFIG_DATA_FILE_OSD_FONT:
		config_value = config->osd_font_path;
		default_path = "";
		filename = DEFAULT_OSD_FONT;
		break;
	case CONFIG_DATA_DIR_SAVE:
		config_value = config->save_path;
		default_path = DEFAULT_SAVE_PATH;
		break;
	case CONFIG_DATA_DIR_FDS_SAVE:
		config_value = config->fds_save_path;
		default_path = DEFAULT_FDS_SAVE_PATH;
		break;
	case CONFIG_DATA_DIR_PATCH:
		config_value = config->patch_path;
		default_path = DEFAULT_PATCH_PATH;
		break;
	case CONFIG_DATA_DIR_STATE:
		config_value = config->state_path;
		default_path = DEFAULT_STATE_PATH;
		break;
	case CONFIG_DATA_DIR_CHEAT:
		config_value = config->cheat_path;
		default_path = DEFAULT_CHEAT_PATH;
		break;
	case CONFIG_DATA_DIR_CHEAT_DB:
		default_path = DEFAULT_CHEAT_DB;
		break;
	case CONFIG_DATA_DIR_SCREENSHOT:
		config_value = config->screenshot_path;
		default_path = DEFAULT_SCREENSHOT_PATH;
		break;
	case CONFIG_DATA_FILE_FDS_BIOS:
		config_value = config->fds_bios_path;
		filename = DEFAULT_FDS_BIOS;
		break;
	case CONFIG_DATA_FILE_NSF_ROM:
		config_value = config->nsf_rom_path;
		filename = DEFAULT_NSF_ROM;
		break;
	case CONFIG_DATA_FILE_GAMECONTROLLER_DB:
		filename = DEFAULT_GAMECONTROLLER_DB;
		break;
	case CONFIG_DATA_FILE_ROM_DB:
		filename = DEFAULT_ROM_DB;
		break;
	default:
		return NULL;
	}

	if (config_value) {
		length = strlen(config_value);
	} else {
		length  = data_path_list->max_length;
		if (default_path)
			length += strlen(default_path) + 1;
	}

	if (filename)
		length += 1 + strlen(filename);
	else
		filename = "";

	length += 1; /* NUL terminator */

	buffer = malloc(length);
	if (!buffer)
		return NULL;

	if (config_value) {
		snprintf(buffer, length, "%s%s%s",
			 config_value, filename[0] ? "/" : "", filename);

		return buffer;
	}

	limit = data_path_list->count;
	if (user > 0 && limit)
		limit = 1;

	for (i = (user ? 0 : 1); i < limit; i++) {
		int len = strlen(data_path_list->paths[i]);
		snprintf(buffer, length, "%s%s%s%s%s",
			 data_path_list->paths[i],
			 (data_path_list->paths[i][len-1] == PATHSEP[0]) ? "" : PATHSEP,
			 default_path,
			 filename[0] ? PATHSEP: "", filename);

		if ((user > 0) || check_file_exists(buffer) ||
		    check_directory_exists(buffer)) {
			break;
		}
	}

	if (i == limit) {
		free(buffer);
		buffer = NULL;
	}

	return buffer;
}

char *config_get_fds_bios(struct config *config)
{
	char *filename;

	filename = config_get_path(config, CONFIG_DATA_FILE_FDS_BIOS,
					       NULL, -1);

	return filename;
}

char *config_get_nsf_rom(struct config *config)
{
	char *filename;

	filename = config_get_path(config, CONFIG_DATA_FILE_NSF_ROM,
					       NULL, -1);

	return filename;
}

void config_print_current_config(struct config *config)
{
}

static int config_set_item(struct config *config,
			   struct config_parameter *parameters,
			   const char *name, const char *value)
{
	struct config_parameter *parameter;
	char *end;
	long svalue;
	unsigned long uvalue;
	double fvalue;
	char *strvalue;
	int rc;
	void *ptr;

	if (value && (value[0] == '\0'))
		value = NULL;

	parameter = parameters;
	while (parameter->name) {
		if (!strcasecmp(name, parameter->name))
			break;

		parameter++;
	}

	if (!parameter->name && value) {
		log_warn("unknown configuration parameter \"%s\"\n",
			name);
		return -1;
	}

	ptr = (char *)config + parameter->offset;
	switch(parameter->type) {
	case CONFIG_TYPE_INTEGER:
		*((int *)ptr) = parameter->default_value.integer;
		if (!value)
			break;

		errno = 0;
		svalue = strtol(value, &end, 0);
		if (errno || *end) {
			log_warn("%s must be an integer value\n",
				 name);
			return -1;
		}

		if (parameter->validator) {
			rc = parameter->validator(parameter, &uvalue);
			if (rc < 0)
				return rc;
		} else if (parameter->valid_values.integer_list) {
			int i;
			for (i = 0; i < parameter->valid_value_count; i++) {
				int c;
				c = parameter->valid_values.integer_list[i];
				if (svalue == c)
					break;
			}

			if (i == parameter->valid_value_count) {
				log_warn("invalid value '%lu' for %s\n",
					svalue, parameter->name);
				return -1;
			}
		} else if ((svalue < parameter->min.svalue) ||
			   (svalue > parameter->max.svalue)) {
			log_warn("%s must be a value from %d to %d\n",
				name, parameter->min.svalue,
				parameter->max.svalue);
			return -1;
		}

		*((int *)ptr) = svalue;
		break;
	case CONFIG_TYPE_UNSIGNED_INTEGER:
		*((unsigned int *)ptr) = parameter->default_value.unsigned_int;
		if (!value)
			break;

		errno = 0;
		uvalue = strtoul(value, &end, 0);
		if (errno || *end) {
			log_warn("%s must be an unsigned integer value\n",
				 name);
			return -1;
		}

		if (parameter->validator) {
			rc = parameter->validator(parameter, &uvalue);
			if (rc < 0)
				return rc;
		} else if (parameter->valid_values.unsigned_int_list) {
			int i;
			for (i = 0; i < parameter->valid_value_count; i++) {
				int c;
				c = parameter->valid_values.unsigned_int_list[i];
				if (uvalue == c)
					break;
			}

			if (i == parameter->valid_value_count) {
				log_warn("invalid value '%lu' for %s\n",
					uvalue, parameter->name);
				return -1;
			}
		} else if ((uvalue < parameter->min.uvalue) ||
			   (uvalue > parameter->max.uvalue)) {
			log_warn("%s must be a value from %u to %u\n",
				name, parameter->min.uvalue,
				parameter->max.uvalue);
			return -1;
		}

		*((unsigned int *)ptr) = uvalue;
		break;
	case CONFIG_TYPE_BOOLEAN:
	case CONFIG_TYPE_BOOLEAN_NOSAVE:
		*((int *)ptr) = parameter->default_value.bool;
		if (!value)
			break;

		svalue = -1;

		if (strcasecmp(value, "true") == 0 ||
		    strcasecmp(value, "yes") == 0 ||
		    strcasecmp(value, "enabled") == 0 ||
		    strcasecmp(value, "on") == 0) {
			svalue = 1;
		} else if (strcasecmp(value, "false") == 0 ||
			   strcasecmp(value, "no") == 0 ||
			   strcasecmp(value, "disabled") == 0 ||
			   strcasecmp(value, "off") == 0) {
			svalue = 0;
		} else {
			errno = 0;
			svalue = strtol(value, &end, 10);
			if (errno || *end || svalue < 0 || svalue > 1)
				svalue = -1;
		}

		if (svalue < 0) {
			log_warn("%s must be true or false\n",
				name);

			return -1;
		}

		*((int *)ptr) = svalue;
		break;
	case CONFIG_TYPE_FLOAT:
		*((double *)ptr) = parameter->default_value.fpoint;
		if (!value)
			break;

		errno = 0;
		fvalue = strtod(value, &end);
		if (errno || *end) {
			log_warn("%s must be a floating-point value\n",
				name);
			return -1;
		}

		if (parameter->validator) {
			rc = parameter->validator(parameter, &fvalue);
			if (rc < 0)
				return rc;
		} else if (parameter->valid_values.float_list) {
			int i;
			for (i = 0; i < parameter->valid_value_count; i++) {
				double c;
				c = parameter->valid_values.float_list[i];
				if (fvalue == c)
					break;
			}

			if (i == parameter->valid_value_count) {
				log_warn("invalid value '%f' for %s\n",
					fvalue, parameter->name);
				return -1;
			}
		} else if ((fvalue < parameter->min.fvalue) ||
			   (fvalue > parameter->max.fvalue)) {
			log_warn("%s must be a value from %f to %f\n",
				name, parameter->min.fvalue,
				parameter->max.fvalue);
			return -1;
		}

		*((double *)ptr) = fvalue;
		break;
	case CONFIG_TYPE_STRING:
		strvalue = parameter->default_value.string;

		if (value && parameter->validator) {
			rc = parameter->validator(parameter, (char *)value);
			if (rc == 0)
				strvalue = (char *)value;
		} else if (value && parameter->valid_values.string_list) {
			int i;
			const char *v;
			for (i = 0; i < parameter->valid_value_count; i++) {
				v = parameter->valid_values.string_list[i];
				if (!strcasecmp(value, v))
					break;
			}

			if (i == parameter->valid_value_count) {
				log_warn("invalid value '%s' for %s\n",
					value, name);
				return -1;
			}

			strvalue = (char *)parameter->valid_values.string_list[i];
		} else if (value) {
			strvalue = (char *)value;
		}

		if (strvalue)
			strvalue = strdup(strvalue);

		if (*(char **)ptr)
			free(*(char **)ptr);

		*((char **)ptr) =  strvalue;
		break;
	}

	return 0;
}

void config_set(struct config *config,
	        struct config_parameter *parameters,
	        const char *name, const char *value)
{
	int offset;

	offset = 0;
	sscanf(name, " binding %n", &offset);
	if (offset) {
		input_bind(name + offset, value);
	} else {
		config_set_item(config, parameters, name, value);
	}
}

void main_config_set(struct config *config, const char *name, const char *value)
{
	config_set(config, config_parameters, name, value);
}

void rom_config_set(struct config *config, const char *name, const char *value)
{
	config_set(config, rom_config_parameters, name, value);
}

void rom_config_reset(struct config *config)
{
	struct config_parameter *parameter;

	parameter = rom_config_parameters;
	while (parameter->name) {
		config_set(config, rom_config_parameters,
			   parameter->name, NULL);
		parameter++;
	}
}

void config_reset_value(struct config *config, const char *name)
{
	config_set(config, config_parameters, name, NULL);
	config_set(config, rom_config_parameters, name, NULL);
}

static struct config_parameter *get_config_parameter(const char *name)
{
	struct config_parameter *parameter;
	
	parameter = config_parameters;
	while (parameter->name) {
		if (!strcasecmp(name, parameter->name))
			break;

		parameter++;
	}

	if (!parameter->name) {
		parameter = rom_config_parameters;
		while (parameter->name) {
			if (!strcasecmp(name, parameter->name))
				break;

			parameter++;
		}

		if (!parameter->name) {
			log_warn("unknown configuration parameter \"%s\"\n",
				name);
			return NULL;
		}
	}

	return parameter;
}

int config_is_rom_config(const char *name)
{
	struct config_parameter *parameter;
	int count;

	parameter = get_config_parameter(name);
	if (!parameter)
		return -1;

	count = sizeof(rom_config_parameters) /
		sizeof(rom_config_parameters[0]);

	if ((parameter >= rom_config_parameters) &&
	    (parameter < rom_config_parameters + count)) {
		return 1;
	}

	return 0;
}

int config_get_int_min(const char *name)
{
	struct config_parameter *param;

	param = get_config_parameter(name);

	return param->min.svalue;
}

int config_get_int_max(const char *name)
{
	struct config_parameter *param;

	param = get_config_parameter(name);

	return param->max.svalue;
}

unsigned int config_get_unsigned_min(const char *name)
{
	struct config_parameter *param;

	param = get_config_parameter(name);

	return param->min.uvalue;
}

unsigned int config_get_unsigned_max(const char *name)
{
	struct config_parameter *param;

	param = get_config_parameter(name);

	return param->max.uvalue;
}

double config_get_float_min(const char *name)
{
	struct config_parameter *param;

	param = get_config_parameter(name);

	return param->min.fvalue;
}

double config_get_float_max(const char *name)
{
	struct config_parameter *param;

	param = get_config_parameter(name);

	return param->max.fvalue;
}

const void *config_get_valid_values(const char *name, int *count,
				    const char ***value_names)
{
	struct config_parameter *param;
	const void *list;

	list = NULL;

	param = get_config_parameter(name);
	if (!param)
		return NULL;

	switch (param->type) {
	case CONFIG_TYPE_INTEGER:
		list = param->valid_values.integer_list;
		break;
	case CONFIG_TYPE_UNSIGNED_INTEGER:
		list = param->valid_values.unsigned_int_list;
		break;
	case CONFIG_TYPE_FLOAT:
		list = param->valid_values.float_list;
		break;
	case CONFIG_TYPE_STRING:
		list = param->valid_values.string_list;
		break;
	}

	if (count)
		*count = param->valid_value_count;

	if (value_names)
		*value_names = param->valid_value_names;

	return list;
}

void *config_get_data_ptr(struct config *config, const char *name)
{
	struct config_parameter *param;
	char *ptr;

	param = get_config_parameter(name);
	if (!param)
		return NULL;

	ptr = (char *)config;
	ptr += param->offset;
	
	return ptr;
}

struct config *config_init(struct emu *emu)
{
	struct config *configptr;
	struct config_parameter *parameter;

	if (init_path_lists())
		return NULL;

	configptr = malloc(sizeof *configptr);

	if (configptr)
		memset(configptr, 0, sizeof(*configptr));

	configptr->emu = emu;
	emu->config = configptr;

	/* Initialize with default values */
	parameter = config_parameters;
	while (parameter->name) {
		config_set(configptr, config_parameters,
			   parameter->name, NULL);
		parameter++;
	}

	parameter = rom_config_parameters;
	while (parameter->name) {
		config_set(configptr, rom_config_parameters,
			   parameter->name, NULL);
		parameter++;
	}

	return configptr;
}

void config_shutdown(void)
{
	free_path_lists();
}

void config_replace(struct config *dest, struct config *src)
{
	int i;

	/* Free all string values from dest struct */
	for (i = 0; config_parameters[i].type != CONFIG_NONE; i++) {
		char **ptr;
		size_t offset;

		if (config_parameters[i].type != CONFIG_TYPE_STRING)
			continue;

		offset = config_parameters[i].offset;
		ptr = (char **)((char *)dest + offset);

		if (!(*ptr))
			continue;

		free(*ptr);
	}

	/* Copy shallow copy of src onto dest. They now both point
           to the same strings.  */
	memcpy(dest, src, sizeof(*dest));

	/* Strdup each string value in dest and set it to use the
           newly-duped string */
	for (i = 0; config_parameters[i].type != CONFIG_NONE; i++) {
		char *tmp, **ptr;
		size_t offset;

		if (config_parameters[i].type != CONFIG_TYPE_STRING)
			continue;

		offset = config_parameters[i].offset;
		ptr = (char **)((char *)dest + offset);

		if (!(*ptr))
			continue;

		tmp = strdup(*ptr);
		*ptr = tmp;
	}
}

struct config *config_copy(struct config *config)
{
	struct config *new_config;
	int i;

	new_config = malloc(sizeof(*new_config));
	if (!new_config)
		return NULL;

	/* Do shallow copy */
	memcpy(new_config, config, sizeof(*new_config));

	/* Copy all string variables */
	for (i = 0; config_parameters[i].type != CONFIG_NONE; i++) {
		char *tmp, **ptr;
		size_t offset;

		if (config_parameters[i].type != CONFIG_TYPE_STRING)
			continue;

		offset = config_parameters[i].offset;
		ptr = (char **)((char *)config + offset);

		if (!(*ptr))
			continue;

		tmp = strdup(*ptr);
		ptr = (char **)((char *)new_config + offset);
		*ptr = tmp;
	}

	new_config->data_path_list = NULL;

	return new_config;
}

void config_cleanup(struct config *config)
{
	struct config_parameter *parameter;
	char *ptr;

	if (!config)
		return;

	for (parameter = config_parameters; parameter->name; parameter++) {
		if (parameter->type != CONFIG_TYPE_STRING)
			continue;

		ptr = (char *)config + parameter->offset;
		if (*(char **)ptr)
			free (*(char **)ptr);
	}

	free(config);
}

int config_check_changed(struct config *config,
			 struct config_parameter *parameters)
{
	int i;
	struct config_parameter *param;
	char *ptr;
	int rc;

	rc = 0;
	for (i = 0; parameters[i].name; i++) {
		param = &parameters[i];
		ptr = ((char *)config) + param->offset;

		switch (param->type) {
		case CONFIG_TYPE_INTEGER:
			if (*((int *)ptr) != param->default_value.integer)
				rc++;
			break;
		case CONFIG_TYPE_UNSIGNED_INTEGER:
			if (*((unsigned int *)ptr) != param->default_value.unsigned_int)
				rc++;
			break;
		case CONFIG_TYPE_BOOLEAN:
		case CONFIG_TYPE_BOOLEAN_NOSAVE:
			if (*((int *)ptr) != param->default_value.bool)
				rc++;
			break;
		case CONFIG_TYPE_FLOAT:
			if (*((double *)ptr) != param->default_value.fpoint)
				rc++;
			break;
		case CONFIG_TYPE_STRING:
			if (ptr && param->default_value.string &&
			    strcmp(*((char **)ptr), param->default_value.string)) {
				rc++;
			}
			break;
		}
	}

	return rc;
}

#if _WIN32
int config_set_portable_mode(int portable)
{
	char *base_path, *tmp;
	int length;
	int rc;

	rc = 0;

	base_path = get_base_path();
	if (!base_path)
		return -1;

	length = strlen(base_path) + strlen("userdata") + 1;

	tmp = malloc(length);
	if (!tmp) {
		free(base_path);
		return -1;
	}

	snprintf(tmp, length, "%suserdata", base_path);

	rc = check_directory_exists(tmp);

	if (portable < 0) {
		if (rc > 0)
			portable = 1;
		else
			portable = 0;
	}

	if (portable) {
		path_list_free(data_path_list);
		data_path_list = path_list_new();

		if (data_path_list) {
			if (path_list_add_entry(data_path_list, tmp) == 0)
				tmp = NULL;

			if (path_list_add_entry(data_path_list, base_path) == 0)
				base_path = NULL;
		}

		if (tmp) {
			free(tmp);
			rc = -1;
		}

		if (base_path) {
			free(base_path);
			rc = -1;
		}
	}

	rc = 0;

	if (!rc && portable) {
		log_info("Enabling portable mode\n");
		printf("Enabling portable mode\n");
	}

	return rc;
}
#endif

