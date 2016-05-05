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

static CPU_WRITE_HANDLER(sunsoft1_write_handler)
{
	struct board *board;
	int low, high;

	board = emu->board;
	high = (value & 0xf0) >> 4;
	low = value & 0x0f;

	if ((board->chr_banks0[0].bank != low) ||
	    (board->chr_banks0[1].bank != high)) {
		board->chr_banks0[0].bank = low;
		board->chr_banks0[1].bank = high;
		board_chr_sync(board, 0);
	}
}

static struct board_write_handler sunsoft1_write_handlers[] = {
	{sunsoft1_write_handler, 0x6000, SIZE_8K, 0},
	{NULL},
};

struct board_info board_sunsoft1 = {
	.board_type = BOARD_TYPE_SUNSOFT1,
	.name = "SUNSOFT-1",
	.init_prg = std_prg_32k,
	.init_chr0 = std_chr_4k,
	.write_handlers = sunsoft1_write_handlers,
	.max_prg_rom_size = SIZE_32K,
	.max_chr_rom_size = SIZE_128K,
};
