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

#define _outer_bank_reg_set board->data[0]

static CPU_READ_HANDLER(maxi15_read_handler);
static void maxi15_reset(struct board *board, int);

static struct board_funcs maxi15_funcs = {
	.reset = maxi15_reset,
};

static struct board_read_handler maxi15_read_handlers[] = {
	{maxi15_read_handler, 0xff80, 32, 0},
	{maxi15_read_handler, 0xffe7, 16, 0},
	{NULL},
};

struct board_info board_maxi15 = {
	.board_type = BOARD_TYPE_MAXI_15,
	.name = "MLT-MAXI15",
	.funcs = &maxi15_funcs,
	.init_prg = std_prg_32k,
	.init_chr0 = std_chr_8k,
	.read_handlers = maxi15_read_handlers,
	.max_prg_rom_size = SIZE_1024K,
	.max_chr_rom_size = SIZE_1024K,
	.flags = BOARD_INFO_FLAG_MIRROR_M,
};

static CPU_READ_HANDLER(maxi15_read_handler)
{
	struct board *board = emu->board;

	if (addr >= 0xffe8) {
		update_prg_bank(board, 1, value);
		update_chr0_bank(board, 0, value >> 4);
	} else if ((addr >= 0xff80) && (!_outer_bank_reg_set)) {
		int bank;

		_outer_bank_reg_set = 1;

		/* FIXME: bit 4 unimplemented */
		bank  = value & 0x0f;
		bank |= value & 0x20 >> 1;
		board->prg_and = 0x00;
		board->chr_and = 0x03;

		board_set_ppu_mirroring(board, (value & 0x80) ?
					MIRROR_H : MIRROR_V);

		if (value & 0x40) {
			bank &= 0x1e;
			board->prg_and |= 0x01;
			board->chr_and |= 0x04;
		}

		board->prg_or = bank; 
		board->chr_or = bank << 2;

		board_prg_sync(board);
		board_chr_sync(board, 0);
	}

	return value;
}

static void maxi15_reset(struct board *board, int hard)
{
	if (hard) {
		_outer_bank_reg_set = 0;
	}
}
