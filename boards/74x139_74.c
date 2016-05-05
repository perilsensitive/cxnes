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

static struct bank d74x139_74_prg_32k[] = {
	{0, 0, SIZE_32K, 0x8000, MAP_PERM_READ, MAP_TYPE_ROM},
	{.type = MAP_TYPE_END},
};

static CPU_WRITE_HANDLER(d74x139_74_write_handler)
{
	update_chr0_bank(emu->board, 0, ((value & 0x2) >> 1) |
			 ((value & 0x1) << 1));
}

static struct board_write_handler d74x139_74_write_handlers[] = {
	{d74x139_74_write_handler, 0x6000, SIZE_8K, 0},
	{NULL},
};

struct board_info board_74x139_74 = {
	.board_type = BOARD_TYPE_74x139_74,
	.name = "74*139/74",
	.init_prg = d74x139_74_prg_32k,
	.init_chr0 = std_chr_8k,
	.write_handlers = d74x139_74_write_handlers,
	.max_prg_rom_size = SIZE_32K,
	.max_chr_rom_size = SIZE_32K,
	.max_wram_size = {SIZE_8K, 0},
};
