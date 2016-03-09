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

#ifndef __BOARD_H__
#define __BOARD_H__

#include "emu.h"

struct range_list;
struct board_info;
struct board;

enum mirroring_types {
	MIRROR_1A = 0x00,
	MIRROR_1B = 0x55,
	MIRROR_H  = 0x50,
	MIRROR_V  = 0x44,
	MIRROR_4  = 0xe4,
	MIRROR_M = 0x100,
	MIRROR_CUSTOM = 0x101,
	MIRROR_UNDEF = 0x102,
};

enum map_type {
	MAP_TYPE_SHIFT = 13,
	MAP_TYPE_NONE = 0,
	MAP_TYPE_ROM,
	MAP_TYPE_RAM0,
	MAP_TYPE_RAM1,
	MAP_TYPE_CIRAM,
	MAP_TYPE_MAPPER_RAM,
	MAP_TYPE_FILL,
	MAP_TYPE_ZERO,
	MAP_TYPE_AUTO,
	MAP_TYPE_END = -1
};

enum board_info_flag {
	BOARD_INFO_FLAG_PRG_IPS = 0x01,
	BOARD_INFO_FLAG_MIRROR_M = 0x02,
	BOARD_INFO_FLAG_WRAM0_NV = 0x04,
	BOARD_INFO_FLAG_WRAM1_NV = 0x08,
	BOARD_INFO_FLAG_VRAM0_NV = 0x10,
	BOARD_INFO_FLAG_VRAM1_NV = 0x20,
	BOARD_INFO_FLAG_MAPPER_NV = 0x40,
	BOARD_INFO_FLAG_M2_TIMER = 0x80,
};

#define MAP_PRG_SLOT_SIZE 2048
#define MAP_PRG_START 0x6000
#define MAP_PRG_END   0x10000
#define MAP_PRG_SLOTS ((MAP_PRG_END - MAP_PRG_START) / MAP_PRG_SLOT_SIZE)

#define MAP_CHR_SLOT_SIZE 1024
#define MAP_CHR_START 0x0000
#define MAP_CHR_END   0x3000
#define MAP_CHR_SLOTS ((MAP_CHR_END - MAP_CHR_START) / MAP_CHR_SLOT_SIZE)

#define MAP_PERM_NONE      0
#define MAP_PERM_READ      1
#define MAP_PERM_WRITE     2
#define MAP_PERM_READWRITE 3

#define VS_PROTECT_NONE 0
#define VS_PROTECT_RBI_BASEBALL 1
#define VS_PROTECT_TKO_BOXING 2
#define VS_PROTECT_SUPER_XEVIOUS 3

#define board_get_prg_bank(x) (((x) - MAP_PRG_START) / MAP_PRG_SLOT_SIZE)
#define board_get_chr_bank(x) ((x) / MAP_CHR_SLOT_SIZE)

int board_set_rom_board_type(struct rom *rom, uint32_t type, int battery);
int board_set_rom_board_type_by_name(struct rom *rom, const char *name, int battery);
uint32_t board_info_get_type(struct board_info *info);
const char *board_info_get_name(struct board_info *info);
size_t board_info_get_mapper_ram_size(struct board_info *info);
int board_info_get_flags(struct board_info *info);
struct board_info *board_info_find_by_type(uint32_t type);
uint32_t board_find_type_by_name(const char *name);

int board_set_type(struct board *, uint32_t board_type);
int board_set_rom_file(struct board *, const char *filename);
void board_set_rom_data(struct board *board, uint8_t * data,
			int prg_rom_offset, int prg_rom_size,
			int chr_rom_offset, int chr_rom_size);

void board_init_mapper_ram(struct board *board, int nv);

int board_set_mirroring(struct board *, int type);

uint8_t board_get_dip_switches(struct board *);
void board_toggle_dip_switch(struct board *, int sw);
int board_set_num_dip_switches(struct board *, int num);
int board_get_num_dip_switches(struct board *);
void board_set_dip_switch(struct board *, int sw, int on);

void board_set_ppu_mirroring(struct board *, int type);

void board_end_frame(struct board *, uint32_t cycles);

int board_init(struct emu *, struct rom *rom);
void board_reset(struct board *, int hard);
int board_apply_config(struct board *);
void board_run(struct board *, uint32_t cycles);
void board_cleanup(struct board *);

void board_prg_sync(struct board *);
void board_chr_sync(struct board *, int set);
void board_nmt_sync(struct board *);

void board_get_chr_rom(struct board *, uint8_t ** romptr, size_t *sizeptr);
void board_get_mapper_ram(struct board *, uint8_t ** ramptr, size_t *sizeptr);

uint32_t board_get_type(struct board *);
int board_save_state(struct board *board, struct save_state *state);
int board_load_state(struct board *board, struct save_state *state);
void board_write_ips_save(struct board *board, struct range_list *range_list);
int board_type_has_mapper_controlled_mirroring(uint32_t board_type);

#endif				/* __BOARD_H__ */
