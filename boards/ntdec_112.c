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

#define _bank_select board->data[0]

static CPU_WRITE_HANDLER(ntdec112_write_handler);

static void ntdec112_reset(struct board *board, int);

static struct board_funcs ntdec112_funcs = {
	.reset = ntdec112_reset,
};

static struct board_write_handler ntdec112_write_handlers[] = {
	{ntdec112_write_handler, 0x8000, SIZE_32K, 0},
	{NULL}
};

struct board_info board_ntdec_112 = {
	.board_type = BOARD_TYPE_NTDEC_112,
	.name = "NTDEC-112",
	.funcs = &ntdec112_funcs,
	.init_prg = std_prg_8k,
	.init_chr0 = std_chr_2k_1k,
	.write_handlers = ntdec112_write_handlers,
	.max_prg_rom_size = SIZE_256K,
	.max_chr_rom_size = SIZE_512K,
	.max_wram_size = {SIZE_8K, 0},
	.flags = BOARD_INFO_FLAG_MIRROR_M,
	.mirroring_values = std_mirroring_vh,
};

static void ntdec112_reset(struct board *board, int hard)
{
	if (hard)
		_bank_select = 0;
}

static CPU_WRITE_HANDLER(ntdec112_write_handler)
{
	struct board *board;
	int update_chr;
	int i;

	board = emu->board;
	update_chr = 0;

	switch (addr & 0xe000) {
	case 0x8000:
		_bank_select = value;
		break;
	case 0xa000:
		if (_bank_select < 2) {
			update_prg_bank(board, _bank_select + 1, value);
		} else {
			i = _bank_select - 2;

			board->chr_banks0[i].bank &= 0x100;
			board->chr_banks0[i].bank |= value;
			update_chr = 1;
		}
		break;
	case 0xc000:
		for (i = 0; i < 6; i++) {
			board->chr_banks0[i].bank &= 0xff;
			board->chr_banks0[i].bank |= (value << (6 - i)) & 0x100;
		}
		break;
	case 0xe000:
		if (value & 0x02)
			board->chr_and = 0x1ff;
		else
			board->chr_and = 0xff;

		update_chr = 1;
			
		standard_mirroring_handler(emu, addr, value, cycles);
		break;
	}

	if (update_chr) {
		board_chr_sync(board, 0);
	}
}

