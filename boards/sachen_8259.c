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

#define _register_select board->data[0]

static CPU_WRITE_HANDLER(sachen_8259_write_handler);
static void sachen_8259_reset(struct board *board, int);

static struct board_funcs sachen_8259_funcs = {
	.reset = sachen_8259_reset,
};

static struct bank sachen_8259d_init_chr[] = {
	{0, 0, SIZE_1K, 0x0000, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{0, 0, SIZE_1K, 0x0400, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{0, 0, SIZE_1K, 0x0800, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{0, 0, SIZE_1K, 0x0c00, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{-1, 0, SIZE_4K, 0x1000, MAP_PERM_READWRITE, MAP_TYPE_AUTO,},
	{.type = MAP_TYPE_END},
};

static struct bank sachen_8259_init_chr[] = {
	{0, 0, SIZE_2K, 0x0000, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{1, 0, SIZE_2K, 0x0800, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{2, 0, SIZE_2K, 0x1000, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{3, 0, SIZE_2K, 0x1800, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{0, 0, 0, 0x0800, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{0, 0, 0, 0x1000, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{0, 0, 0, 0x1800, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{.type = MAP_TYPE_END},
};

static struct board_write_handler sachen_8259d_write_handlers[] = {
	{sachen_8259_write_handler, 0x4000, SIZE_2K, 0xc101},
	{sachen_8259_write_handler, 0x4001, SIZE_2K, 0xc101},
	{sachen_8259_write_handler, 0x4100, SIZE_2K, 0xc101},
	{sachen_8259_write_handler, 0x4101, SIZE_2K, 0xc101},
	{NULL},
};

static struct board_write_handler sachen_8259_write_handlers[] = {
	{sachen_8259_write_handler, 0x4100, SIZE_2K, 0xc101},
	{sachen_8259_write_handler, 0x4101, SIZE_2K, 0xc101},
	{NULL},
};

struct board_info board_sachen_8259d = {
	.board_type = BOARD_TYPE_SACHEN_8259D,
	.name = "SACHEN-8259D",
	.init_prg = std_prg_32k,
	.init_chr0 = sachen_8259d_init_chr,
	.write_handlers = sachen_8259d_write_handlers,
	.max_prg_rom_size = SIZE_256K,
	.max_chr_rom_size = SIZE_32K,
	.flags = BOARD_INFO_FLAG_MIRROR_M,
};

struct board_info board_sachen_8259b = {
	.board_type = BOARD_TYPE_SACHEN_8259B,
	.name = "SACHEN-8259B",
	.funcs = &sachen_8259_funcs,
	.init_prg = std_prg_32k,
	.init_chr0 = sachen_8259_init_chr,
	.write_handlers = sachen_8259_write_handlers,
	.max_prg_rom_size = SIZE_256K,
	.max_chr_rom_size = SIZE_128K,
	.flags = BOARD_INFO_FLAG_MIRROR_M,
};

struct board_info board_sachen_8259a = {
	.board_type = BOARD_TYPE_SACHEN_8259A,
	.name = "SACHEN-8259A",
	.funcs = &sachen_8259_funcs,
	.init_prg = std_prg_32k,
	.init_chr0 = sachen_8259_init_chr,
	.write_handlers = sachen_8259_write_handlers,
	.max_prg_rom_size = SIZE_256K,
	.max_chr_rom_size = SIZE_256K,
	.flags = BOARD_INFO_FLAG_MIRROR_M,
};

struct board_info board_sachen_8259c = {
	.board_type = BOARD_TYPE_SACHEN_8259C,
	.name = "SACHEN-8259C",
	.funcs = &sachen_8259_funcs,
	.init_prg = std_prg_32k,
	.init_chr0 = sachen_8259_init_chr,
	.write_handlers = sachen_8259_write_handlers,
	.max_prg_rom_size = SIZE_256K,
	.max_chr_rom_size = SIZE_512K,
	.flags = BOARD_INFO_FLAG_MIRROR_M,
};

static void sachen_8259d_set_mirroring(struct board *board, int value)
{
	int mirroring;

	if (board->chr_mode)
		value = 0x00;

	if (value == 0x02) {
		board->nmt_banks[0].bank = 0;
		board->nmt_banks[1].bank = 1;
		board->nmt_banks[2].bank = 1;
		board->nmt_banks[3].bank = 1;
		board_nmt_sync(board);
		return;
	}

	if (value == 0x00) {
		mirroring = MIRROR_V;
	} else if (value == 0x01) {
		mirroring = MIRROR_H;
	} else if (value == 0x02) {
		mirroring = 0x54;
	} else {
		mirroring = MIRROR_1A;
	}

	board_set_ppu_mirroring(board, mirroring);
}

static CPU_WRITE_HANDLER(sachen_8259_write_handler)
{
	struct board *board = emu->board;
	int mask;
	int shift;
	int or;

	if (board->info->board_type ==
	    BOARD_TYPE_SACHEN_8259A) {
		mask = 0x0f;
		shift = 1;
		or = 0x01;
	} else if (board->info->board_type ==
		   BOARD_TYPE_SACHEN_8259C) {
		mask = 0x1f;
		shift = 2;
		or = 0x03;
	} else {
		mask = 0x07;
		shift = 0;
		or = 0x00;
	}

	switch (addr) {
	case 0x4100:
		_register_select = value;
		break;
	case 0x4000:
	case 0x4001:
	case 0x4101:
		value &= 0x07;

		switch (_register_select) {
		case 0:
		case 1:
		case 2:
		case 3:
			board->chr_banks0[_register_select].bank &= ~mask;
			board->chr_banks0[_register_select].bank |= (value << shift);
			board->chr_banks0[_register_select].bank |= (_register_select & or);
			if (_register_select == 4)
				board->chr_banks0[1].bank = 0;

			if ((_register_select == 0) &&
			    (board->info->board_type != BOARD_TYPE_SACHEN_8259D)) {
				int bank = board->chr_banks0[0].bank & ~or;
				board->chr_banks0[4].bank = bank | (1 & or);
				board->chr_banks0[5].bank = bank | (2 & or);
				board->chr_banks0[6].bank = bank | (3 & or);
			}
			
			board_chr_sync(board, 0);
			break;
		case 4:
			board->chr_banks0[1].bank &= mask;
			board->chr_banks0[2].bank &= mask;
			board->chr_banks0[3].bank &= mask;

			if (board->info->board_type ==
			    BOARD_TYPE_SACHEN_8259D) {
				value <<= 2;
				board->chr_banks0[1].bank |= (value & 0x10);
				value <<= 1;
				board->chr_banks0[2].bank |= (value & 0x10);
				value <<= 1;
				board->chr_banks0[3].bank |= (value & 0x10);
			} else {
				int simple_bank;

				value <<= 3 + shift;

				board->chr_banks0[0].bank &= mask;
				board->chr_banks0[0].bank |= value;
				board->chr_banks0[1].bank |= value;
				board->chr_banks0[2].bank |= value;
				board->chr_banks0[3].bank |= value;

				simple_bank = board->chr_banks0[0].bank & ~or;
				board->chr_banks0[4].bank = simple_bank |
					(1 & or);
				board->chr_banks0[5].bank = simple_bank |
					(2 & or);
				board->chr_banks0[6].bank = simple_bank |
					(3 & or);
			}
			board_chr_sync(board, 0);
			break;
		case 5:
			board->prg_banks[1].bank = value;
			board_prg_sync(board);
			break;
		case 6:
			if (board->info->board_type ==
			    BOARD_TYPE_SACHEN_8259D) {
				value &= 0x01;
				board->chr_banks0[3].bank &= 0x17;
				board->chr_banks0[3].bank |= value << 3;
				board_chr_sync(board, 0);
			}
			break;
		case 7:
			board->chr_mode = value & 0x01;
			if (board->info->board_type !=
			    BOARD_TYPE_SACHEN_8259D) {
				if (board->chr_mode) {
					board->chr_banks0[1].size = 0;
					board->chr_banks0[2].size = 0;
					board->chr_banks0[3].size = 0;
					board->chr_banks0[4].size = SIZE_2K;
					board->chr_banks0[5].size = SIZE_2K;
					board->chr_banks0[6].size = SIZE_2K;
				} else {
					board->chr_banks0[1].size = SIZE_2K;
					board->chr_banks0[2].size = SIZE_2K;
					board->chr_banks0[3].size = SIZE_2K;
					board->chr_banks0[4].size = 0;
					board->chr_banks0[5].size = 0;
					board->chr_banks0[6].size = 0;
				}
				board_chr_sync(board, 0);
			}
			value >>= 1;
			sachen_8259d_set_mirroring(board, value);
			break;
		}
		break;
	}
}

static void sachen_8259_reset(struct board *board, int hard)
{
}
