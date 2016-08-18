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

#include "board_private.h"

static CPU_WRITE_HANDLER(unl_cc_21_write_handler);

static struct bank unl_cc_21_init_prg[] = {
	{-1, 0, SIZE_32K, 0x8000, MAP_PERM_READ, MAP_TYPE_ROM},
	{.type = MAP_TYPE_END},
};

static struct board_write_handler unl_cc_21_write_handlers[] = {
	{unl_cc_21_write_handler, 0x8000, SIZE_32K, 0},
	{NULL},
};

struct board_info board_unl_cc_21 = {
	.board_type = BOARD_TYPE_UNL_CC_21,
	.name = "UNL-CC-21",
	.init_prg = unl_cc_21_init_prg,
	.init_chr0 = std_chr_4k,
	.write_handlers = unl_cc_21_write_handlers,
	.max_prg_rom_size = SIZE_32K,
	.max_chr_rom_size = SIZE_16K,
	.flags = BOARD_INFO_FLAG_MIRROR_M,
	.mirroring_values = std_mirroring_01,
	.mirroring_shift = 1,
};

static CPU_WRITE_HANDLER(unl_cc_21_write_handler)
{
	struct board *board = emu->board;

	standard_mirroring_handler(emu, addr, addr, cycles);
	update_chr0_bank(board, 0, addr);
}
