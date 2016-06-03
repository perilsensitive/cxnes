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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <libgen.h>

#include "audio.h"

#include "emu.h"
#include "db.h"
#include "file_io.h"
#include "video.h"
#include "fds.h"

#define NS_PER_SEC 1000000000L

#define NTSC_PPU_CLOCKS_PER_FRAME (341 * 261 + 340.5)
#define PAL_PPU_CLOCKS_PER_FRAME (341 * 312)
#define DENDY_PPU_CLOCKS_PER_FRAME (341 * 312)

#define NTSC_MASTER_CLOCK_RATE 21477272.73
#define PAL_MASTER_CLOCK_RATE  26601712.5
#define DENDY_MASTER_CLOCK_RATE PAL_MASTER_CLOCK_RATE
#define NTSC_CPU_CLOCK_DIVIDER 12
#define PAL_CPU_CLOCK_DIVIDER  16
#define DENDY_CPU_CLOCK_DIVIDER 15
#define NTSC_PPU_CLOCK_DIVIDER  4
#define PAL_PPU_CLOCK_DIVIDER   5
#define DENDY_PPU_CLOCK_DIVIDER   PAL_PPU_CLOCK_DIVIDER

#define NTSC_CPU_CLOCK_RATE (NTSC_MASTER_CLOCK_RATE / NTSC_CPU_CLOCK_DIVIDER)
#define PAL_CPU_CLOCK_RATE (PAL_MASTER_CLOCK_RATE / PAL_CPU_CLOCK_DIVIDER)
#define DENDY_CPU_CLOCK_RATE (DENDY_MASTER_CLOCK_RATE / DENDY_CPU_CLOCK_DIVIDER)

#define NTSC_PPU_CLOCK_RATE (NTSC_MASTER_CLOCK_RATE / NTSC_PPU_CLOCK_DIVIDER)
#define PAL_PPU_CLOCK_RATE (PAL_MASTER_CLOCK_RATE / PAL_PPU_CLOCK_DIVIDER)
#define DENDY_PPU_CLOCK_RATE (DENDY_MASTER_CLOCK_RATE / DENDY_PPU_CLOCK_DIVIDER)

#define NTSC_FRAMERATE (NTSC_PPU_CLOCK_RATE / NTSC_PPU_CLOCKS_PER_FRAME)
#define PAL_FRAMERATE (PAL_PPU_CLOCK_RATE / PAL_PPU_CLOCKS_PER_FRAME)
#define DENDY_FRAMERATE (DENDY_PPU_CLOCK_RATE / DENDY_PPU_CLOCKS_PER_FRAME)

const struct system_type_info system_type_info[] = {
	{
		.type = EMU_SYSTEM_TYPE_AUTO,
		.value = "auto",
		.description = "Auto",
	},
	{
		.type = EMU_SYSTEM_TYPE_FAMICOM,
		.value = "famicom",
		.description = "Famicom (RP2C02 PPU)",
	},
	{
		.type = EMU_SYSTEM_TYPE_FAMICOM_RGB,
		.value = "famicom_rgb",
		.description = "Famicom RGB (RP2C03B PPU)",
	},
	{
		.type = EMU_SYSTEM_TYPE_NES,
		.value = "nes",
		.description = "NTSC NES (RP2C02 PPU)",
	},
	{
		.type = EMU_SYSTEM_TYPE_NES_RGB,
		.value = "nes_rgb",
		.description = "NTSC NES RGB (RP2C03B PPU)",
	},
	{
		.type = EMU_SYSTEM_TYPE_PAL_NES,
		.value = "pal_nes",
		.description = "PAL NES (RP2C07 PPU)",
	},
	{
		.type = EMU_SYSTEM_TYPE_DENDY,
		.value = "dendy",
		.description = "Dendy",
	},
	{
		.type = EMU_SYSTEM_TYPE_VS_RP2C03B,
		.value = "rp2c03b",
		.description = "VS. (RP2C03B PPU)",
	},
	{
		.type = EMU_SYSTEM_TYPE_VS_RP2C03G,
		.value = "rp2c03g",
		.description = "VS. (RP2C03G PPU)",
	},
	{
		.type = EMU_SYSTEM_TYPE_VS_RP2C04_0001,
		.value = "rp2c04-0001",
		.description = "VS. (RP2C04-0001 PPU)",
	},
	{
		.type = EMU_SYSTEM_TYPE_VS_RP2C04_0002,
		.value = "rp2c04-0002",
		.description = "VS. (RP2C04-0002 PPU)",
	},
	{
		.type = EMU_SYSTEM_TYPE_VS_RP2C04_0003,
		.value = "rp2c04-0003",
		.description = "VS. (RP2C04-0003 PPU)",
	},
	{
		.type = EMU_SYSTEM_TYPE_VS_RP2C04_0004,
		.value = "rp2c04-0004",
		.description = "VS. (RP2C04-0004 PPU)",
	},
	{
		.type = EMU_SYSTEM_TYPE_VS_RC2C03B,
		.value = "rc2c03b",
		.description = "VS. (RC2C03B PPU)",
	},
	{
		.type = EMU_SYSTEM_TYPE_VS_RC2C03C,
		.value = "rc2c03c",
		.description = "VS. (RC2C03C PPU)",
	},
	{
		.type = EMU_SYSTEM_TYPE_VS_RC2C05_01,
		.value = "rc2c05-01",
		.description = "VS. (RC2C05-01 PPU)",
	},
	{
		.type = EMU_SYSTEM_TYPE_VS_RC2C05_02,
		.value = "rc2c05-02",
		.description = "VS. (RC2C05-02 PPU)",
	},
	{
		.type = EMU_SYSTEM_TYPE_VS_RC2C05_03,
		.value = "rc2c05-03",
		.description = "VS. (RC2C05-03 PPU)",
	},
	{
		.type = EMU_SYSTEM_TYPE_VS_RC2C05_04,
		.value = "rc2c05-04",
		.description = "VS. (RC2C05-04 PPU)",
	},
	{
		.type = EMU_SYSTEM_TYPE_VS_RC2C05_05,
		.value = "rc2c05-05",
		.description = "VS. (RC2C05-05 PPU)",
	},
	{
		.type = EMU_SYSTEM_TYPE_PLAYCHOICE,
		.value = "playchoice",
		.description = "PlayChoice (RP2C03B PPU)",
	},
	{
		.type = EMU_SYSTEM_TYPE_UNDEFINED,
		.value = NULL,
		.description = NULL,
	}
};

#if GUI_ENABLED
extern void gui_enable_event_timer(void);
#endif

void emu_deinit(struct emu *emu);
static char *emu_generate_rom_config_path(struct emu *emu, int save);
static int emu_set_rom_file(struct emu *emu, const char *rom_file);
static int emu_load_rom_common(struct emu *emu, struct rom *rom,
			       int patch_count, char **patchfiles);

struct emu *emu_new(void)
{
	struct emu *emu;

	emu = malloc(sizeof(*emu));
	if (emu)
		memset(emu, 0, sizeof(*emu));

	cheat_init(emu);

	return emu;
}

int emu_init(struct emu *emu)
{
	emu->system_type = EMU_SYSTEM_TYPE_UNDEFINED;
	emu->ram = malloc(SIZE_2K);
	if (!emu->ram) {
		free(emu);
		return -1;
	}

	emu->quick_save_slot = 0;

	/* Configuration should be initialized first, then CPU, then PPU.
	   Otherwise the I/O handlers and timing information won't be set
	   up correctly.  The rest can be initialized in any order. */
	cpu_init(emu);
	ppu_init(emu);

	apu_init(emu);
	io_init(emu);

	cpu_set_pagetable_entry(emu->cpu, 0x0000, SIZE_2K, emu->ram,
				CPU_PAGE_READWRITE);
	cpu_set_pagetable_entry(emu->cpu, 0x0800, SIZE_2K, emu->ram,
				CPU_PAGE_READWRITE);
	cpu_set_pagetable_entry(emu->cpu, 0x1000, SIZE_2K, emu->ram,
				CPU_PAGE_READWRITE);
	cpu_set_pagetable_entry(emu->cpu, 0x1800, SIZE_2K, emu->ram,
				CPU_PAGE_READWRITE);
	cpu_set_pagetable_entry(emu->cpu, 0x2000, SIZE_64K - SIZE_8K, NULL,
				CPU_PAGE_READWRITE);

	return 0;
}

static enum system_type find_system_type_by_config_value(const char *value)
{
	int i;

	for (i = 0; system_type_info[i].type != EMU_SYSTEM_TYPE_UNDEFINED;
	     i++) {
		if (value && system_type_info[i].value &&
		    strcasecmp(value, system_type_info[i].value) == 0) {
			break;
		}
	}

	return system_type_info[i].type;
}

static const char *find_config_value_by_system_type(enum system_type type)
{
	int i;

	for (i = 0; system_type_info[i].type != EMU_SYSTEM_TYPE_UNDEFINED;
	     i++) {
		if (type == system_type_info[i].type)
			break;
	}

	return system_type_info[i].value;
}

const char *emu_get_system_type_name(enum system_type type)
{
	int name_index;
	int i;

//	auto_index = 0;
	name_index = -1;

	for (i = 0; system_type_info[i].type != EMU_SYSTEM_TYPE_UNDEFINED; i++) {
		if (type == system_type_info[i].type) {
			name_index = i;
			break;
		}
	}

	if (name_index < 0)
		return NULL;

	return system_type_info[name_index].description;
}

int emu_select_next_system_type(struct emu *emu)
{
	int type;
	int index;
	int auto_index;
	int i;
	int is_vs;

	type = emu->system_type;
	auto_index = 0;

	if ((type == EMU_SYSTEM_TYPE_AUTO) && (emu->guessed_system_type == EMU_SYSTEM_TYPE_PLAYCHOICE))
		return 0;
	else if (emu->system_type == EMU_SYSTEM_TYPE_PLAYCHOICE)
		return 0;

	if (type == EMU_SYSTEM_TYPE_AUTO)
		is_vs = system_type_is_vs(emu->guessed_system_type);
	else
		is_vs = system_type_is_vs(type);

	index = -1;

	for (i = 0; system_type_info[i].type != EMU_SYSTEM_TYPE_UNDEFINED; i++) {
		if (system_type_info[i].type == type)
			index = i;
	}

	if (index == -1)
		return -1;

	index++;
	for (i = index; system_type_info[i].type != EMU_SYSTEM_TYPE_UNDEFINED; i++) {
		if (system_type_is_vs(system_type_info[i].type) == is_vs) {
			break;
		}
	}
	index = i;

	if (system_type_info[index].type == EMU_SYSTEM_TYPE_UNDEFINED)
		index = 0;

	if (system_type_info[index].type == EMU_SYSTEM_TYPE_AUTO) {
		auto_index = -1;
		for (i = 0; system_type_info[i].type !=
			     EMU_SYSTEM_TYPE_UNDEFINED; i++) {
			if (system_type_info[i].type ==
			    emu->guessed_system_type) {
				auto_index = i;
			}
		}

		if (auto_index == -1)
			return -1;
	}


	emu_set_system_type(emu, system_type_info[index].type);
	if (system_type_info[index].type == EMU_SYSTEM_TYPE_AUTO) {
		osdprintf("System type: Auto [%s]\n",
			  system_type_info[auto_index].description);

	} else {
		osdprintf("System type: %s\n",
			  system_type_info[index].description);
	}

	return 0;
}

int emu_set_framerate(struct emu *emu, int framerate)
{
	int old_framerate;

	if (!emu)
		return -1;

	old_framerate = emu->user_framerate;

	if (framerate < 1)
		framerate = emu->nes_framerate;

	emu->user_framerate = framerate;
	emu->current_clock_rate = emu->clock_rate * emu->user_framerate /
		emu->nes_framerate;
	emu->current_delay_ns = NS_PER_SEC / emu->user_framerate;

	if (emu->config->vsync)
		emu->current_framerate = emu->display_framerate;
	else
		emu->current_framerate = emu->user_framerate;

	emu->frame_timer_reload = 0;
	if (emu->user_framerate != emu->display_framerate) {
		emu->frame_timer_reload = emu->user_framerate -
			emu->display_framerate;

		if (emu->frame_timer_reload < 0)
			emu->frame_timer_reload *= -1;

		emu->frame_timer = emu->frame_timer_reload;
	}

	if (old_framerate != framerate) {
		if (emu->config->alternate_speed_mute)
			audio_mute(framerate != emu->nes_framerate);
		video_apply_config(emu);
		audio_apply_config(emu);
		emu_apply_config(emu);
	}

	return 0;
}

void emu_set_remember_system_type(struct emu *emu, int enabled)
{
	struct config *config;
	const char *rom_type;
	const char *rom_type_name;
	enum system_type system_type;

	rom_type = NULL;
	rom_type_name = NULL;

	if (enabled)
		system_type = emu->system_type;
	else
		system_type = EMU_SYSTEM_TYPE_AUTO;

	config = emu->config;

	if (system_type_is_vs(emu->system_type)) {
		rom_type_name = "rom_vs_ppu_type";
		rom_type = find_config_value_by_system_type(system_type);
	} else if (system_type != EMU_SYSTEM_TYPE_PLAYCHOICE) {
		rom_type_name = "rom_console_type";
		if (enabled)
			rom_type = find_config_value_by_system_type(system_type);
		else
			rom_type = "preferred";
	}

	if (emu->loaded && (enabled != config->remember_system_type)) {
		char *path;

		config->remember_system_type = enabled;

		if (rom_type_name && rom_type) {
			rom_config_set(emu->config, rom_type_name, rom_type);

		}

		path = emu_generate_rom_config_path(emu, 1);
		if (path) {
			config_save_rom_config(emu->config, path);
			free(path);
		}
	}

}

int emu_set_system_type(struct emu *emu, enum system_type system_type)
{
	int old_system_type;
	struct config *config;
	const char *preferred_type;
	const char *rom_type;
	const char *rom_type_name;

	rom_type = NULL;
	rom_type_name = NULL;

	if (system_type == EMU_SYSTEM_TYPE_UNDEFINED)
		return -1;

	config = emu->config;

	old_system_type = emu->system_type;
	if (old_system_type == system_type)
		return 0;

	/* If this function is being called for the first
	   time, initialize the system type according to
	   user preferences; if the user has not indicated
	   a preference, use the type passed as an argument.
	*/

	if (old_system_type == EMU_SYSTEM_TYPE_UNDEFINED) {
		emu->guessed_system_type = system_type;

		if (system_type_is_vs(system_type)) {
			rom_type = config->rom_vs_ppu_type;
			rom_type_name = "rom_vs_ppu_type";
		} else if (system_type != EMU_SYSTEM_TYPE_PLAYCHOICE) {
			rom_type = config->rom_console_type;
			rom_type_name = "rom_console_type";
			preferred_type = config->preferred_console_type;

			if (strcasecmp(rom_type, "preferred") == 0)
				rom_type = preferred_type;
		}

		if (rom_type)
			system_type = find_system_type_by_config_value(rom_type);
	} else {
		if (system_type_is_vs(system_type) || system_type_is_vs(emu->system_type)) {
			rom_type_name = "rom_vs_ppu_type";
			rom_type = find_config_value_by_system_type(system_type);
		} else if (system_type != EMU_SYSTEM_TYPE_PLAYCHOICE) {
			preferred_type = config->preferred_console_type;
			rom_type_name = "rom_console_type";
			rom_type = find_config_value_by_system_type(system_type);

			if (strcasecmp(rom_type, "preferred") == 0)
				rom_type = preferred_type;
		}
	}

	emu->system_type = system_type;
	emu->nes_framerate = NTSC_FRAMERATE;
	emu->clock_rate = NTSC_MASTER_CLOCK_RATE;
	ppu_set_reset_connected(emu->ppu, 1);

	if (system_type == EMU_SYSTEM_TYPE_AUTO)
		system_type = emu->guessed_system_type;

	switch (system_type) {
	case EMU_SYSTEM_TYPE_FAMICOM:
		cpu_set_type(emu->cpu, CPU_TYPE_RP2A03);
		apu_set_type(emu->apu, APU_TYPE_RP2A03);
		ppu_set_type(emu->ppu, PPU_TYPE_RP2C02);
		ppu_set_reset_connected(emu->ppu, 0);
		break;
	case EMU_SYSTEM_TYPE_FAMICOM_RGB:
		cpu_set_type(emu->cpu, CPU_TYPE_RP2A03);
		apu_set_type(emu->apu, APU_TYPE_RP2A03);
		ppu_set_type(emu->ppu, PPU_TYPE_RP2C03B);
		ppu_set_reset_connected(emu->ppu, 0);
		break;
	case EMU_SYSTEM_TYPE_NES:
		cpu_set_type(emu->cpu, CPU_TYPE_RP2A03);
		apu_set_type(emu->apu, APU_TYPE_RP2A03);
		ppu_set_type(emu->ppu, PPU_TYPE_RP2C02);
		break;
	case EMU_SYSTEM_TYPE_NES_RGB:
		cpu_set_type(emu->cpu, CPU_TYPE_RP2A03);
		apu_set_type(emu->apu, APU_TYPE_RP2A03);
		ppu_set_type(emu->ppu, PPU_TYPE_RP2C03B);
		break;
	case EMU_SYSTEM_TYPE_PAL_NES:
		cpu_set_type(emu->cpu, CPU_TYPE_RP2A07);
		apu_set_type(emu->apu, APU_TYPE_RP2A07);
		ppu_set_type(emu->ppu, PPU_TYPE_RP2C07);
		emu->nes_framerate = PAL_FRAMERATE;
		emu->clock_rate = PAL_MASTER_CLOCK_RATE;
		break;
	case EMU_SYSTEM_TYPE_DENDY:
		cpu_set_type(emu->cpu, CPU_TYPE_DENDY);
		apu_set_type(emu->apu, APU_TYPE_DENDY);
		ppu_set_type(emu->ppu, PPU_TYPE_DENDY);
		emu->nes_framerate = DENDY_FRAMERATE;
		emu->clock_rate = DENDY_MASTER_CLOCK_RATE;
		break;
	case EMU_SYSTEM_TYPE_VS_RP2C03B:
	case EMU_SYSTEM_TYPE_VS_RP2C03G:
	case EMU_SYSTEM_TYPE_VS_RP2C04_0001:
	case EMU_SYSTEM_TYPE_VS_RP2C04_0002:
	case EMU_SYSTEM_TYPE_VS_RP2C04_0003:
	case EMU_SYSTEM_TYPE_VS_RP2C04_0004:
	case EMU_SYSTEM_TYPE_VS_RC2C03B:
	case EMU_SYSTEM_TYPE_VS_RC2C03C:
	case EMU_SYSTEM_TYPE_VS_RC2C05_01:
	case EMU_SYSTEM_TYPE_VS_RC2C05_02:
	case EMU_SYSTEM_TYPE_VS_RC2C05_03:
	case EMU_SYSTEM_TYPE_VS_RC2C05_04:
	case EMU_SYSTEM_TYPE_VS_RC2C05_05:
		/* System type for VS. systems is the same as the NES 2.0 PPU
		   type.
		*/
		cpu_set_type(emu->cpu, CPU_TYPE_RP2A03);
		apu_set_type(emu->apu, APU_TYPE_RP2A03);
		ppu_set_type(emu->ppu, vs_system_type_to_ppu_type(system_type));
		break;
	case EMU_SYSTEM_TYPE_PLAYCHOICE:
		cpu_set_type(emu->cpu, CPU_TYPE_RP2A03);
		apu_set_type(emu->apu, APU_TYPE_RP2A03);
		ppu_set_type(emu->ppu, PPU_TYPE_RP2C03B);
		break;
	default:
		err_message("Unknown system type 0x%02x\n", system_type);
		return 1;
	}

	emu_set_framerate(emu, emu->nes_framerate);

	if (emu->loaded && (system_type != old_system_type)) {
		char *path;

		video_apply_config(emu);
		audio_apply_config(emu);
		emu_apply_config(emu);

		if (!emu->config->remember_system_type)
			rom_type = "auto";

		if (rom_type_name && rom_type) {
			rom_config_set(emu->config, rom_type_name, rom_type);

			emu->loaded = 1;

			emu_reset(emu, 1);

			path = emu_generate_rom_config_path(emu, 1);
			if (path) {
				config_save_rom_config(emu->config, path);
				free(path);
			}
		}
	}

	return 0;
}

int emu_patch_rom(struct emu *emu, char *patch_filename)
{
	struct rom *rom;
	char *args[2];
	int rc;

	if (!emu->loaded)
		return -1;
	
	if (!emu->rom)
		return -1;

	rom = emu->rom;

	emu->rom = NULL;

	emu_deinit(emu);
	emu_init(emu);

	args[0] = patch_filename;
	args[1] = NULL;

	rc = emu_load_rom_common(emu, rom, 1, args);

	video_apply_config(emu);
	audio_apply_config(emu);
	emu_apply_config(emu);

	emu_reset(emu, 1);

	return rc;
}

int emu_load_rom(struct emu *emu, char *filename, int patch_count,
		 char **patchfiles)
{
	char *db_path;
	char **autopatches;
	int rc;
	struct rom *rom;

	if (!check_file_exists(filename))
		return 1;

	db_path = NULL;
	if (emu->config->db_enabled) {
		db_path = config_get_path(emu->config,
					      CONFIG_DATA_FILE_ROM_DB,
					      NULL, -1);

		if (check_file_exists(db_path))
			db_load_file(emu, db_path);

		free(db_path);
		db_path = NULL;
	}

	rom = rom_load_file(emu, filename);
	if (!rom)
		return 1;

	autopatches = NULL;

	if (!autopatches)
		autopatches = rom_find_autopatches(emu->config, rom);

	if (!patch_count) {
		patchfiles = autopatches;
		patch_count = -1;
	}

	rc = emu_load_rom_common(emu, rom, patch_count, patchfiles);

	if (autopatches)
		free(autopatches);

	db_cleanup();

	if (rc == 0) {
		print_rom_info(rom);

		video_set_window_title(rom->filename);
	}

	return rc;
}

void emu_deinit(struct emu *emu)
{
	if (emu->board)
		board_cleanup(emu->board);

	if (emu->apu)
		apu_cleanup(emu->apu);

	if (emu->ppu)
		ppu_cleanup(emu->ppu);

	if (emu->cpu)
		cpu_cleanup(emu->cpu);

	if (emu->io)
		io_cleanup(emu->io);

	emu->board = NULL;
	emu->apu = NULL;
	emu->ppu = NULL;
	emu->cpu = NULL;
	emu->io = NULL;

	if (emu->cheats)
		cheat_deinit(emu->cheats);

	emu->loaded = 0;
	free(emu->ram);
	emu->ram = NULL;

	if (emu->rom_path)
		free(emu->rom_path);

	if (emu->rom_file)
		free(emu->rom_file);

	if (emu->cheat_file)
		free(emu->cheat_file);

	if (emu->save_file)
		free(emu->save_file);

	if (emu->state_file)
		free(emu->state_file);

	if (emu->rom)
		rom_free(emu->rom);

	emu->rom = NULL;
	emu->rom_path = NULL;
	emu->rom_file = NULL;
	emu->cheat_file = NULL;
	emu->save_file = NULL;
	emu->state_file = NULL;
	video_set_window_title(NULL);
}

int emu_apply_config(struct emu *emu)
{
	int rc;

	rc = 0;

	if (!emu->loaded)
		return 0;

	rc |= cpu_apply_config(emu->cpu);
	rc |= ppu_apply_config(emu->ppu);
	rc |= apu_apply_config(emu->apu);
	rc |= board_apply_config(emu->board);
	rc |= io_apply_config(emu->io);
	rc |= cheat_apply_config(emu->cheats);

	return rc;
}

int emu_reset(struct emu *emu, int hard)
{
	if (!emu->loaded)
		return 1;

	emu->resetting = 1;

	if (hard)
		memset(emu->ram, 0xff, SIZE_2K);

	cpu_reset(emu->cpu, hard);
	apu_reset(emu->apu, hard);
	ppu_reset(emu->ppu, hard);
	board_reset(emu->board, hard);
	cheat_reset(emu, hard);
	io_reset(emu->io, hard);
	emu->resetting = 0;

	emu->blargg_reset_timer = -1;

	return 1;
}

int emu_run_frame(struct emu *emu, uint32_t *buffer, uint32_t *nes_buffer)
{
	int cycles;

	if (emu->config->vsync && emu->frame_timer_reload) {
		int old_frame_timer = emu->frame_timer;

		if (emu->user_framerate > emu->display_framerate) {
			if (emu->frame_timer > 0)
				emu->frame_timer -= emu->display_framerate;
			else
				emu->frame_timer += emu->frame_timer_reload;

			if (old_frame_timer > 0)
				emu->draw_frame = 0;
		} else if (emu->user_framerate < emu->display_framerate) {
			if (emu->frame_timer > 0)
				emu->frame_timer -= emu->user_framerate;
			else
				emu->frame_timer += emu->frame_timer_reload;

			if (old_frame_timer > 0)
				return 0;
		}
	}

	ppu_begin_frame(emu->ppu, buffer, nes_buffer);
	cycles = cpu_run(emu->cpu);
	apu_run(emu->apu, cycles);
	board_run(emu->board, cycles);
	io_run(emu->io, cycles);

	cycles = ppu_end_frame(emu->ppu, cycles);
	cpu_end_frame(emu->cpu, cycles);
	apu_end_frame(emu->apu, cycles);
	board_end_frame(emu->board, cycles);
	io_end_frame(emu->io, cycles);

	return cycles;
}

void emu_cleanup(struct emu *emu)
{
	emu_deinit(emu);
	config_cleanup(emu->config);
	cheat_cleanup(emu->cheats);
	free(emu);
}

void emu_toggle_scanline_renderer(struct emu *emu)
{
	if (!emu->loaded)
		return;
	
	ppu_set_scanline_renderer(emu->ppu, -1);
}

void emu_toggle_sprite_limit(struct emu *emu)
{
	if (!emu->loaded)
		return;
	
	ppu_set_sprite_limit(emu->ppu, -1);
}

void emu_toggle_sprites(struct emu *emu)
{
	if (!emu->loaded)
		return;
	
	ppu_toggle_sprites(emu->ppu);
}

void emu_toggle_bg(struct emu *emu)
{
	if (!emu->loaded)
		return;
	
	ppu_toggle_bg(emu->ppu);
}

void blargg_reset_request(struct emu *emu)
{
	log_dbg("blargg test rom requested reset\n");
	emu->blargg_reset_timer = 10;
}

void emu_toggle_cheats(struct emu *emu)
{
	int enabled;
	if (!emu->loaded)
		return;
	
	enabled = cheat_enable_cheats(emu->cheats, -1);
	osdprintf("Cheats %s", enabled ? "enabled" : "disabled");
}

void emu_pause(struct emu *emu, int pause)
{
	if (emu->loaded) {
	emu->paused = pause;
	audio_pause(pause);
#if GUI_ENABLED
	if (pause)
		gui_enable_event_timer();
#endif
	video_set_screensaver_enabled(pause);
	video_show_cursor(emu->paused);
	}
}

static int find_oldest_save_state(struct emu *emu, char *path)
{
	int oldest = -1;
	int i;
	int len;
	int64_t oldest_sec;
	int32_t oldest_nsec;

	len = strlen(path);

	for (i = 0; i < 10; i++) {
		int64_t sec;
		int32_t nsec;
		int rc;

		path[len - 1] = i + '0';
		rc = get_file_mtime(path, &sec, &nsec);

		if (rc < 0) {
			oldest = i;
			break;
		}

		if (oldest == -1) {
			oldest_sec = sec;
			oldest_nsec = nsec;
			oldest = i;
		} else if ((sec < oldest_sec) || ((sec == oldest_sec) &&
			    (nsec < oldest_nsec))) {
			oldest_sec = sec;
			oldest_nsec = nsec;
			oldest = i;
		}
	}

	return oldest;
}

static int find_newest_save_state(struct emu *emu, char *path)
{
	int newest = -1;
	int i;
	int len;
	int64_t newest_sec;
	int32_t newest_nsec;

	len = strlen(path);

	for (i = 0; i < 10; i++) {
		int64_t sec;
		int32_t nsec;
		int rc;

		path[len - 1] = i + '0';
		rc = get_file_mtime(path, &sec, &nsec);

		if (rc < 0)
			continue;

		if (newest == -1) {
			newest_sec = sec;
			newest_nsec = nsec;
			newest = i;
		} else if ((sec > newest_sec) || ((sec == newest_sec) &&
			    (nsec > newest_nsec))) {
			newest_sec = sec;
			newest_nsec = nsec;
			newest = i;
		}
	}

	return newest;
}

int emu_get_quick_save_slot(struct emu *emu)
{
	return emu->quick_save_slot;
}

void emu_set_quick_save_slot(struct emu *emu, int slot, int display)
{
	if (slot < 0 || slot > 9)
		return;

	emu->quick_save_slot = slot;
	
	if (display)
		osdprintf("State %d selected\n", slot);
}

int emu_quick_save_state(struct emu *emu, int quick_save_index, int display)
{
	const char *state_file;
	char *path;
	int len;
	int rc;

	if (quick_save_index < 0)
		quick_save_index = emu->quick_save_slot;

	if (quick_save_index < 0 || quick_save_index > 10)
		return -1;

	state_file = emu->state_file;

	path = config_get_path(emu->config, CONFIG_DATA_DIR_STATE,
				   state_file, 1);
	len = strlen(path);

	if (quick_save_index == 10)
		quick_save_index = find_oldest_save_state(emu, path);

	/* FIXME not sure how portable this really is */
	path[len - 1] = '0' + quick_save_index;

	rc = emu_save_state(emu, path);
	if (display) {
		if (rc == 0)
			osdprintf("State %d saved\n", quick_save_index);
		else
			osdprintf("State %d save failed\n", quick_save_index);
	}
	free(path);

	return rc;
}

int emu_quick_load_state(struct emu *emu, int quick_save_index, int display)
{
	const char *state_file;
	char *path;
	int len;
	int rc;

	if (quick_save_index < 0)
		quick_save_index = emu->quick_save_slot;

	if (quick_save_index < 0 || quick_save_index > 10)
		return -1;

	rc = 0;

	state_file = emu->state_file;

	path = config_get_path(emu->config, CONFIG_DATA_DIR_STATE,
				   state_file, 1);
	len = strlen(path);

	if (quick_save_index == 10) {
		quick_save_index = find_newest_save_state(emu, path);
		if (quick_save_index < 0)
			return 0;
	}

	/* FIXME not sure how portable this really is */
	path[len - 1] = '0' + quick_save_index;

	rc = emu_load_state(emu, path);
	if (display) {
		if (rc == 0)
			osdprintf("State %d loaded\n", quick_save_index);
		else
			osdprintf("State %d load failed\n", quick_save_index);
	}

	free(path);

	return rc;
}

int emu_save_state(struct emu *emu, const char *filename)
{
	struct save_state *state;
	int rc;

	if (!emu->loaded)
		return -1;

	state = create_save_state();
	if (!state)
		return -1;

	cpu_save_state(emu->cpu, state);
	ppu_save_state(emu->ppu, state);
	apu_save_state(emu->apu, state);
	board_save_state(emu->board, state);
	save_state_add_chunk(state, "RAM ", emu->ram, 2048);
	io_save_state(emu->io, state);
	rc = save_state_write(state, filename);

	destroy_save_state(state);

	return rc;
}

int emu_load_state(struct emu *emu, const char *filename)
{
	struct save_state *state;
	uint8_t *buf;
	size_t size;

	if (!emu->loaded)
		return -1;

	state = create_save_state();
	if (!state)
		return -1;

	if (save_state_read(state, filename) < 0)
		return -1;

	cpu_load_state(emu->cpu, state);
	ppu_load_state(emu->ppu, state);
	apu_load_state(emu->apu, state);

	if (save_state_find_chunk(state, "RAM ", &buf, &size) < 0)
		return -1;

	if (size < 2048)
		return -1;

	memcpy(emu->ram, buf, 2048);
	board_load_state(emu->board, state);
	io_load_state(emu->io, state);
	
	destroy_save_state(state);

	return 0;
}

int emu_system_is_vs(struct emu *emu)
{
	int type;

	if (emu->system_type == EMU_SYSTEM_TYPE_AUTO)
		type = emu->guessed_system_type;
	else
		type = emu->system_type;

	if ((type & EMU_SYSTEM_TYPE_MASK) == 0x10)
		return 1;

	return 0;
}

static char *emu_generate_rom_config_path(struct emu *emu, int save)
{
	char *romdir_cfg_file, *default_cfg_file, *buffer;
	int len;

	buffer = NULL;

	len = strlen(emu->rom_path) + 1 + strlen(emu->cfg_file) + 1;
	romdir_cfg_file = malloc(len);

	if (romdir_cfg_file) {
		snprintf(romdir_cfg_file, len, "%s%c%s",
			 emu->rom_path, PATHSEP[0],
			 emu->cfg_file);
	} else {
		return NULL;
	}

	default_cfg_file = config_get_path(emu->config,
					   CONFIG_DATA_DIR_CONFIG,
					   emu->cfg_file, save ? 1 : -1);

	if (!default_cfg_file) {
		free(romdir_cfg_file);
		return NULL;
	}

	buffer = romdir_cfg_file;
	if (!check_file_exists(buffer)) {
		if (!emu->config->config_uses_romdir) {
			buffer = default_cfg_file;
			free(romdir_cfg_file);
		} else {
			free(default_cfg_file);
		}
	} else {
		free(default_cfg_file);
	}

	log_dbg("rom config file: %s\n", buffer);
	
	return buffer;
}

static int emu_set_rom_file(struct emu *emu, const char *rom_file)
{
	const char *rom_ext;
	char *tmp;
	int len;

	tmp = strdup(rom_file);
	if (!tmp)
		return -1;
	emu->rom_file = strdup(basename(tmp));
	free(tmp);
	if (!emu->rom_file)
		return -1;
	
	tmp = strdup(rom_file);
	if (!tmp)
		return -1;

	emu->rom_path = strdup(dirname(tmp));
	free(tmp);
	if (!emu->rom_path)
		return -1;

	len = strlen(emu->rom_file) + 1;
	
	rom_ext = strrchr(rom_file, '.');
	if (!rom_ext)
		len += 4;

	emu->save_file = malloc(len);
	emu->cfg_file = malloc(len);
	emu->cheat_file = malloc(len);
	emu->state_file = malloc(len);

	if (!emu->save_file  || !emu->cfg_file ||
	    !emu->cheat_file || !emu->state_file ) {
		return -1;
	}

	/* Store the standard names of the save, config,
	   cheat and state files for future use.  Replace
	   the ROM file's extension with the appropriate
	   one, or append the extension if the ROM didn't
	   have one.
	*/
	len = strlen(emu->rom_file) - (rom_ext ? strlen(rom_ext) : 0);
	strncpy(emu->save_file, emu->rom_file, len);
	strncpy(emu->cfg_file, emu->rom_file, len);
	strncpy(emu->cheat_file, emu->rom_file, len);
	strncpy(emu->state_file, emu->rom_file, len);
	strncpy(emu->save_file + len, ".sav", 5);
	strncpy(emu->cfg_file + len, ".cfg", 5);
	strncpy(emu->cheat_file + len, ".cht", 5);
	strncpy(emu->state_file + len, ".sta", 5);

	log_dbg("rom: %s\n", emu->rom_file);
	log_dbg("dir: %s\n", emu->rom_path);
	log_dbg("save: %s\n", emu->save_file);
	log_dbg("cfg: %s\n", emu->cfg_file);
	log_dbg("cheat: %s\n", emu->cheat_file);
	log_dbg("state: %s\n", emu->state_file);

	return 0;
}

void emu_load_rom_cfg(struct emu *emu)
{
	char *buffer;

	buffer = emu_generate_rom_config_path(emu, 0);
	
	if (check_file_exists(buffer)) {
		printf("Config file: %s\n", buffer);
		config_load_rom_config(emu->config, buffer);
	}

	free(buffer);
	return;
}


void emu_save_rom_config(struct emu *emu)
{
	char *path;

	path = emu_generate_rom_config_path(emu, 1);
	if (path) {
		config_save_rom_config(emu->config, path);
		free(path);
	}
}


void emu_load_cheat(struct emu *emu)
{
	char *buffer;

	if (!emu->config->autoload_cheats)
		return;

	buffer = config_get_path(emu->config, CONFIG_DATA_DIR_CHEAT,
				     emu->cheat_file, -1);

	if (check_file_exists(buffer)) {
		if (cheat_load_file(emu, buffer) != 0) {
			/* log_warn("failed to read cheat file \"%s\"\n", */
			/*      buffer); */
		}
	}

	free(buffer);
}


static int emu_load_rom_common(struct emu *emu, struct rom *rom,
			       int patch_count, char **patchfiles)
{
	int patches_applied = 0;
#if ZIP_ENABLED
	char **zip_patchfiles;
#endif
	int i;

	if (rom->info.board_type == BOARD_TYPE_FDS) {
		if (fds_convert_to_fds(rom)) {
			rom_free(rom);
			return -1;
		}

		/* Modify rom_buffer_size so that it doesn't
		   include padding or the size of the bios rom.
		*/
		rom->buffer_size /= 65500;
		rom->buffer_size *= 65500;
		rom->buffer_size += 16;
	}

#if ZIP_ENABLED
	zip_patchfiles = rom_find_zip_autopatches(emu->config, rom);
	if (zip_patchfiles)
		patchfiles = zip_patchfiles;
#endif	

	if (patchfiles) {
		patches_applied = rom_apply_patches(rom, patch_count,
						    patchfiles, zip_patchfiles != NULL);
		if (zip_patchfiles)
			free(zip_patchfiles);
	}

	if (patches_applied < 0) {
		rom_free(rom);
		return 1;
	} else if ((patches_applied > 0) &&
		   (rom->info.board_type != BOARD_TYPE_FDS) &&
		   (rom->info.board_type != BOARD_TYPE_NSF)) {
		   /* Patches were applied; metadata may have
		      changed.  Parse the header again and
		      update the rom structure's metadata.
		   */
		rom = rom_reload_file(emu, rom);
		if (!rom) {
			return 1;
		}
	}

	if (rom->info.board_type == BOARD_TYPE_FDS) {
		if (fds_load_bios(emu, rom)) {
			rom_free(rom);
			return 1;
		}
		rom->info.total_prg_size = rom->buffer_size - 16;
	}
	
	emu->rom = rom;
	if (emu_set_rom_file(emu, rom->filename)) {
		return 1;
	}

	emu_load_rom_cfg(emu);

	if (emu_set_system_type(emu, rom->info.system_type) < 0)
		return 1;

	if (board_init(emu, rom)) {
		emu->rom = NULL;
		rom_free(rom);
		emu_deinit(emu);
		return 1;
	}

	emu->loaded = 1;
	video_show_cursor(emu->paused);

	io_set_auto_four_player_mode(emu->io, rom->info.four_player_mode);
	io_set_auto_vs_controller_mode(emu->io, rom->info.vs_controller_mode);
	for (i = 0; i < 5; i++) {
		io_set_auto_device(emu->io, i, rom->info.auto_device_id[i]);
	}

	return 0;
}
