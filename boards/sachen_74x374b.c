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

#define _register_select board->data[0]
#define _counter board->data[1]

static CPU_READ_HANDLER(sachen_74ls374n_read_handler);
static CPU_WRITE_HANDLER(sachen_74ls374n_write_handler);
static void sachen_74ls374n_reset(struct board *board, int);

static struct board_funcs sachen_74ls374n_funcs = {
	.reset = sachen_74ls374n_reset,
};

static uint8_t mirroring_values[4] = { MIRROR_H, MIRROR_V, 0x54, MIRROR_1A };

static struct board_write_handler sachen_74ls374n_write_handlers[] = {
	{sachen_74ls374n_write_handler, 0x4100, 2048, 0xc101},
	{sachen_74ls374n_write_handler, 0x4101, 2048, 0xc101},
	{NULL},
};

static struct board_read_handler sachen_74ls374n_read_handlers[] = {
	{sachen_74ls374n_read_handler, 0x4100, 0x1e00, 0},
	{NULL},
};

struct board_info board_sachen_74ls374n = {
	.board_type = BOARD_TYPE_SACHEN_74LS374N,
	.name = "UNL-SACHEN-74LS374N",
	.init_prg = std_prg_32k,
	.init_chr0 = std_chr_8k,
	.funcs = &sachen_74ls374n_funcs,
	.read_handlers = sachen_74ls374n_read_handlers,
	.write_handlers = sachen_74ls374n_write_handlers,
	.max_prg_rom_size = SIZE_256K,
	.max_chr_rom_size = SIZE_128K,
	.min_wram_size = { SIZE_8K, 0 },
	.max_wram_size = { SIZE_8K, 0 },
	.flags = BOARD_INFO_FLAG_MIRROR_M,
	.mirroring_values = mirroring_values,
	.mirroring_shift = 1,
};

static CPU_READ_HANDLER(sachen_74ls374n_read_handler)
{
	struct board *board;

	board = emu->board;

	value = (~(_register_select & 0x07) & 0x3f) ^ 0x01;

	return value;
}

static CPU_WRITE_HANDLER(sachen_74ls374n_write_handler)
{
	struct board *board = emu->board;


	switch (addr & 0xc101) {
	case 0x4100:
		_register_select = value;
		break;
	case 0x4101:
		value &= 0x07;

		switch (_register_select) {
		case 2:
			board->chr_banks0[0].bank &= 0x07;
			board->chr_banks0[0].bank |= (value & 0x01) << 3;
			board_chr_sync(board, 0);
			board->prg_banks[1].bank = (value & 0x01);
			board_prg_sync(board);
			break;
		case 4:
			board->chr_banks0[0].bank &= 0x0b;
			board->chr_banks0[0].bank |= (value & 0x01) << 2;
			board_chr_sync(board, 0);
			break;
		case 5:
			board->prg_banks[1].bank = value & 0x07;
			board_prg_sync(board);
			break;
		case 6:
			board->chr_banks0[0].bank &= 0x0c;
			board->chr_banks0[0].bank |= (value & 0x03);
			board_chr_sync(board, 0);
			break;
		case 7:
			standard_mirroring_handler(emu, addr, value, cycles);
			break;
		}
		break;
	}
}

static void sachen_74ls374n_reset(struct board *board, int hard)
{
	/* printf("here reset\n"); */
	/* if (!hard) */
	/* 	_counter ^= 1; */
	/* else { */
	/* 	_counter = 0; */
	/* if (hard) { */
	/* 	board_set_ppu_mirroring(board, MIRROR_V); */
	/* } */
}
