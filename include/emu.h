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

#ifndef __EMU_H__
#define __EMU_H__

struct save_state;
struct config;
struct cpu_state;
struct ppu_state;
struct apu_state;
struct io_state;
struct cheat_state;
struct audio_state;
struct board;
struct emu;
struct rom;
struct vrc_timer;
struct a12_timer;
struct m2_timer;

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "cpu.h"
#include "ppu.h"
#include "apu.h"
#include "board.h"
#include "io.h"
#include "board_types.h"
#include "config.h"
#include "cheat.h"
#include "state.h"
#include "rom.h"
#include "sizes.h"
#include "log.h"

#define INES_HEADER_SIZE 16

enum system_type {
	EMU_SYSTEM_TYPE_FAMICOM,
	EMU_SYSTEM_TYPE_FAMICOM_RGB,
	EMU_SYSTEM_TYPE_NES,
	EMU_SYSTEM_TYPE_NES_RGB,
	EMU_SYSTEM_TYPE_PAL_NES,
	EMU_SYSTEM_TYPE_DENDY,
	EMU_SYSTEM_TYPE_PREFERRED = 0x0f,
	EMU_SYSTEM_TYPE_VS_RP2C03B = 0x10,
	EMU_SYSTEM_TYPE_VS_RP2C03G = 0x11,
	EMU_SYSTEM_TYPE_VS_RP2C04_0001 = 0x12,
	EMU_SYSTEM_TYPE_VS_RP2C04_0002 = 0x13,
	EMU_SYSTEM_TYPE_VS_RP2C04_0003 = 0x14,
	EMU_SYSTEM_TYPE_VS_RP2C04_0004 = 0x15,
	EMU_SYSTEM_TYPE_VS_RC2C03B = 0x16,
	EMU_SYSTEM_TYPE_VS_RC2C03C = 0x17,
	EMU_SYSTEM_TYPE_VS_RC2C05_01 = 0x18,
	EMU_SYSTEM_TYPE_VS_RC2C05_02 = 0x19,
	EMU_SYSTEM_TYPE_VS_RC2C05_03 = 0x1a,
	EMU_SYSTEM_TYPE_VS_RC2C05_04 = 0x1b,
	EMU_SYSTEM_TYPE_VS_RC2C05_05 = 0x1c,
	EMU_SYSTEM_TYPE_PLAYCHOICE = 0x30,
	EMU_SYSTEM_TYPE_AUTO = 0x40,
	EMU_SYSTEM_TYPE_UNDEFINED = 0xff,
	EMU_SYSTEM_TYPE_MASK = 0xf0,
	EMU_SYSTEM_TYPE_VS_SUPER_SKYKID = 0x20,
	EMU_SYSTEM_TYPE_VS_RAID_ON_BUNGELING_BAY = 0x21,
	EMU_SYSTEM_TYPE_VS_ICE_CLIMBER = 0x22,
	EMU_SYSTEM_TYPE_VS_SUPER_MARIO_BROS = 0x23,
	EMU_SYSTEM_TYPE_VS_PINBALL_JAPAN = 0x24,
	EMU_SYSTEM_TYPE_VS_PINBALL_USA = 0x25,
};

struct system_type_info {
	enum system_type type;
	const char *value;
	const char *description;
};

extern const struct system_type_info system_type_info[];

#define system_type_is_vs(x) ((((x) & EMU_SYSTEM_TYPE_MASK) == 0x10) || \
                              (((x) & EMU_SYSTEM_TYPE_MASK) == 0x20))
#define vs_system_type_to_ppu_type(x) ((x) & 0x0f)
#define vs_ppu_type_to_system_type(x) ((x) | 0x10)

struct vrc6_audio_state;
struct vrc7_audio_state;
struct fds_audio_state;
struct namcot163_audio_state;
struct mmc5_audio_state;
struct sunsoft5b_audio_state;

struct emu {
	struct cpu_state *cpu;
	struct ppu_state *ppu;
	struct apu_state *apu;
	struct io_state *io;
	struct board *board;
	struct config *config;
	struct cheat_state *cheats;
	struct audio_state *audio;

	struct vrc6_audio_state *vrc6_audio;
	struct vrc7_audio_state *vrc7_audio;
	struct fds_audio_state *fds_audio;
	struct namco163_audio_state *namco163_audio;
	struct mmc5_audio_state *mmc5_audio;
	struct sunsoft5b_audio_state *sunsoft5b_audio;
	struct vrc_timer *vrc_timer;
	struct a12_timer *a12_timer;
	struct m2_timer *m2_timer;

	/* Nothing below here should be modified directly; any
	   changes should be done with the appropriate
	   function call.  Reading these directly is OK.
	*/
	/* Timing variables */
	int cpu_clock_divider;
	int apu_clock_divider;
	int ppu_clock_divider;
	int draw_frame;
	int frame_timer;
	int frame_timer_reload;
	int frame_timer_mode;
	int current_framerate;
	int display_framerate;
	int nes_framerate;
	int user_framerate;
	long delay_ns;
	long current_delay_ns;
	double clock_rate;
	double current_clock_rate;

	int blargg_reset_timer;
	int resetting;
	int loaded;
	int paused;
	uint8_t *ram;
	enum system_type system_type;
	enum system_type guessed_system_type;

	int quick_save_slot;

	int overclocking;

	struct rom *rom;
	uint8_t *bios;
	size_t bios_size;
	char *rom_path;
	char *rom_file;
	char *save_file;
	char *cfg_file;
	char *cheat_file;
	char *state_file;
};

#define emu_paused(_e) (_e->paused)
#define emu_resetting(_e) (_e->resetting)
#define emu_loaded(_e) (_e->loaded)

int emu_init(struct emu *emu);
void emu_deinit(struct emu *emu);
struct emu *emu_new(void);
int emu_patch_rom(struct emu *emu, char *patch_filename);
int emu_load_rom(struct emu *emu, char *, int, char **);
int emu_run_frame(struct emu *emu);
int emu_reset(struct emu *, int hard);
int emu_apply_config(struct emu *);
void emu_cleanup(struct emu *);
void emu_toggle_sprite_limit(struct emu *emu);
void emu_toggle_scanline_renderer(struct emu *emu);
void emu_toggle_sprites(struct emu *emu);
void emu_toggle_bg(struct emu *emu);
void emu_toggle_cheats(struct emu *emu);
void emu_pause(struct emu *emu, int pause);
int emu_load_state(struct emu *emu, const char *filename);
int emu_save_state(struct emu *emu, const char *filename);
void emu_set_quick_save_slot(struct emu *emu, int slot, int display);
int emu_get_quick_save_slot(struct emu *emu);
int emu_quick_load_state(struct emu *emu, int quick_save_index, int display);
int emu_quick_save_state(struct emu *emu, int quick_save_index, int display);
int emu_set_system_type(struct emu *emu, enum system_type system_type);
const char *emu_get_system_type_name(enum system_type type);
int emu_select_next_system_type(struct emu *emu);
int emu_system_is_vs(struct emu *emu);

void emu_load_cheat(struct emu *emu);

int emu_set_framerate(struct emu *emu, int framerate);

void emu_set_remember_overclock_mode(struct emu *emu, int enabled);

enum system_type emu_find_system_type_by_config_value(const char *value);
/* FIXME not sure where to put this */
int osdprintf(const char *format, ...);
void emu_overclock(struct emu *emu, uint32_t cycles, int enabled);
char *emu_generate_rom_config_path(struct emu *emu, int save);

#endif				/* __EMU_H__ */
