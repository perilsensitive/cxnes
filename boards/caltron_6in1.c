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

static CPU_WRITE_HANDLER(caltron_6in1_write_handler);
static void caltron_6in1_reset(struct board *board, int);

static struct board_funcs caltron6in1_funcs = {
	.reset = caltron_6in1_reset,
};

static struct board_write_handler caltron_6in1_write_handlers[] = {
	{caltron_6in1_write_handler, 0x6000, SIZE_2K, 0},
	{caltron_6in1_write_handler, 0x8000, SIZE_32K, 0},
	{NULL},
};

struct board_info board_caltron_6in1 = {
	.board_type = BOARD_TYPE_CALTRON_6_IN_1,
	.name = "MLT-CALTRON6IN1",
	.funcs = &caltron6in1_funcs,
	.init_prg = std_prg_32k,
	.init_chr0 = std_chr_8k,
	.write_handlers = caltron_6in1_write_handlers,
	.max_prg_rom_size = SIZE_256K,
	.max_chr_rom_size = SIZE_128K,
	.flags = BOARD_INFO_FLAG_MIRROR_M,
	.mirroring_values = std_mirroring_vh,
	.mirroring_shift = 6,
};

static CPU_WRITE_HANDLER(caltron_6in1_write_handler)
{
	struct board *board = emu->board;

	if (addr >= 0x8000) {
		if (board->prg_banks[1].bank >= 4) {
			if ((value & 3) !=
			    board->chr_banks0[0].bank) {
				board->chr_banks0[0].bank =
				    value & 0x03;
				board_chr_sync(board, 0);
			}
		}
	} else {
		int prg_bank = addr & 0x07;
		int chr_or = (addr & 0x18) >> 1;

		if (prg_bank != board->prg_banks[1].bank) {
			board->prg_banks[1].bank = prg_bank;
			board_prg_sync(board);
		}

		if (chr_or != board->chr_or) {
			board->chr_or = chr_or;
			board_chr_sync(board, 0);
		}

		standard_mirroring_handler(emu, addr, value, cycles);
	}
}

static void caltron_6in1_reset(struct board *board, int hard)
{
	board->chr_or = 0;
	board->prg_banks[1].bank = 0;
	board->chr_banks0[0].bank = 0;
	board_prg_sync(board);
	board_chr_sync(board, 0);
}
