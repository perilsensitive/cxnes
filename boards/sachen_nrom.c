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

static CPU_READ_HANDLER(sachen_nrom_read_handler);

static void sachen_nrom_reset(struct board *board, int hard)
{
	if (hard) {
		int i;

		for (i = 0; i < SIZE_2K; i++) {
			cpu_poke(board->emu->cpu, i,
				 i & 4 ? 0x7f : 0x00);
		}
	}
}

struct board_funcs sachen_nrom_funcs = {
	.reset = sachen_nrom_reset,
};

static struct board_read_handler sachen_nrom_read_handlers[] = {
	{sachen_nrom_read_handler, 0x4100, SIZE_8K, 0x4100},
	{NULL},
};

struct board_info board_sachen_nrom = {
	.board_type = BOARD_TYPE_SACHEN_NROM,
	.name = "UNL-SA-NROM",
	.init_prg = std_prg_32k,
	.init_chr0 = std_chr_8k,
	.funcs = &sachen_nrom_funcs,
	.read_handlers = sachen_nrom_read_handlers,
	.max_prg_rom_size = SIZE_32K,
	.max_chr_rom_size = SIZE_8K,
};

static CPU_READ_HANDLER(sachen_nrom_read_handler)
{
	value &= 0xc0;
	value |= (~addr) & 0x3f;

	return value;
}
