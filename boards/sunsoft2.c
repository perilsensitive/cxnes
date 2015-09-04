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

#include "board_private.h"

static CPU_WRITE_HANDLER(sunsoft2_write_handler)
{
	update_prg_bank(emu->board, 1, (value & 0x70) >> 4);
	update_chr0_bank(emu->board, 0, (value & 0x07) | ((value & 0x80) >> 4));
	standard_mirroring_handler(emu, addr, value, cycles);
}

static CPU_WRITE_HANDLER(sunsoft3r_write_handler)
{
	int type = MAP_TYPE_RAM0;

	update_prg_bank(emu->board, 1, (value & 0x70) >> 4);

	if (!(value & 0x01)) {
		type = MAP_TYPE_NONE;
	}

	if (type != emu->board->chr_banks0[0].type) {
		emu->board->chr_banks0[0].type = type;
		board_chr_sync(emu->board, 0);
	}
}

static struct board_write_handler sunsoft2_write_handlers[] = {
	{sunsoft2_write_handler, 0x8000, SIZE_32K, 0},
	{NULL},
};

static struct board_write_handler sunsoft3r_write_handlers[] = {
	{sunsoft3r_write_handler, 0x8000, SIZE_32K, 0},
	{NULL},
};

struct board_info board_sunsoft2 = {
	.board_type = BOARD_TYPE_SUNSOFT2,
	.name = "SUNSOFT-2",
	.mapper_name = "SUNSOFT-2",
	.init_prg = std_prg_16k,
	.init_chr0 = std_chr_8k,
	.write_handlers = sunsoft2_write_handlers,
	.max_prg_rom_size = SIZE_128K,
	.max_chr_rom_size = SIZE_128K,
	.max_wram_size = {SIZE_8K, 0},
	.flags = BOARD_INFO_FLAG_MIRROR_M,
	.mirroring_values = std_mirroring_01,
	.mirroring_shift = 3,
};

struct board_info board_sunsoft3r = {
	.board_type = BOARD_TYPE_SUNSOFT3R,
	.name = "SUNSOFT-3R",
	.mapper_name = "SUNSOFT-2",
	.init_prg = std_prg_16k,
	.init_chr0 = std_chr_8k,
	.write_handlers = sunsoft3r_write_handlers,
	.max_prg_rom_size = SIZE_128K,
	.max_chr_rom_size = SIZE_128K,
	.max_wram_size = {SIZE_8K, 0},
};
