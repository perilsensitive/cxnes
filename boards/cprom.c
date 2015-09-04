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

static CPU_WRITE_HANDLER(cprom_write_handler)
{
	int old_bank = emu->board->chr_banks0[1].bank;

	if (value != old_bank) {
		int type;

		type = (value & 0x2) ? MAP_TYPE_RAM1 : MAP_TYPE_RAM0;
		emu->board->chr_banks0[1].bank = value & 0x01;
		emu->board->chr_banks0[1].type = type;

		board_chr_sync(emu->board, 0);
	}
}

static struct board_write_handler cprom_write_handlers[] = {
	{cprom_write_handler, 0x8000, SIZE_32K, 0},
	{NULL},
};

struct board_info board_cprom = {
	.board_type = BOARD_TYPE_CPROM,
	.name = "NES-CPROM",
	.init_prg = std_prg_32k,
	.init_chr0 = std_chr_4k,
	.write_handlers = cprom_write_handlers,
	.max_prg_rom_size = SIZE_32K,
	.min_vram_size = {SIZE_8K, SIZE_8K},
	.max_vram_size = {SIZE_8K, SIZE_8K},
	.max_wram_size = {SIZE_8K, 0},
};
