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

static CPU_WRITE_HANDLER(unl_ac08_write_handler);

static struct bank unl_ac08_init_prg[] = {
	{ 0, 0, SIZE_8K,  0x6000, MAP_PERM_READ, MAP_TYPE_ROM},
	{-1, 0, SIZE_32K, 0x8000, MAP_PERM_READ, MAP_TYPE_ROM},
	{.type = MAP_TYPE_END},
};

static struct board_write_handler unl_ac08_write_handlers[] = {
	{standard_mirroring_handler, 0x4025, 1, 0},
	{unl_ac08_write_handler, 0x8000, SIZE_32K, 0},
	{NULL},
};

struct board_info board_unl_ac08 = {
	.board_type = BOARD_TYPE_UNL_AC08,
	.name = "UNL-AC08",
	.init_prg = unl_ac08_init_prg,
	.init_chr0 = std_chr_8k,
	.write_handlers = unl_ac08_write_handlers,
	.max_prg_rom_size = SIZE_128K + SIZE_64K,
	.max_chr_rom_size = SIZE_8K,
	.mirroring_values = std_mirroring_hv,
	.mirroring_shift = 3,
};

static CPU_WRITE_HANDLER(unl_ac08_write_handler)
{
	struct board *board = emu->board;

	if (addr == 0x8001)
		value >>= 1;

	update_prg_bank(board, 0, value & 0x0f);
}
