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

static struct bank unl_lh32_init_prg[] = {
	{-4, 0, SIZE_8K, 0x8000, MAP_PERM_READ, MAP_TYPE_ROM},
	{ 0, 0, SIZE_8K, 0x6000, MAP_PERM_READ, MAP_TYPE_ROM},
	{-3, 0, SIZE_8K, 0xa000, MAP_PERM_READ, MAP_TYPE_ROM},
	{ 0, 0, SIZE_8K, 0xc000, MAP_PERM_READWRITE, MAP_TYPE_RAM0},
	{-1, 0, SIZE_8K, 0xe000, MAP_PERM_READ, MAP_TYPE_ROM},
	{.type = MAP_TYPE_END},
};

static struct board_write_handler unl_lh32_write_handlers[] = {
	{simple_prg_no_conflict_write_handler, 0x6000, 1, 0},
	{NULL},
};

struct board_info board_unl_lh32 = {
	.board_type = BOARD_TYPE_UNL_LH32,
	.name = "UNL-LH32",
	.init_prg = unl_lh32_init_prg,
	.init_chr0 = std_chr_8k,
	.write_handlers = unl_lh32_write_handlers,
	.max_prg_rom_size = SIZE_64K,
	.min_wram_size = {SIZE_8K, 0},
	.max_wram_size = {SIZE_8K, 0},
	.max_chr_rom_size = SIZE_8K,
};
