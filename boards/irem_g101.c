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

static CPU_WRITE_HANDLER(irem_g101_write_handler);

static void irem_g101_reset(struct board *board, int);

static struct board_write_handler irem_g101_write_handlers[] = {
	{irem_g101_write_handler, 0x8000, SIZE_16K, 0},
	{NULL},
};

static struct board_write_handler irem_g101_b_write_handlers[] = {
	{irem_g101_write_handler, 0x8000, SIZE_4K, 0},
	{irem_g101_write_handler, 0xa000, SIZE_8K, 0},
	{NULL},
};

struct board_funcs irem_g101_funcs = {
	.reset = irem_g101_reset,
};

struct board_info board_irem_g101 = {
	.board_type = BOARD_TYPE_IREM_G101,
	.name = "IREM-G101",
	.mirroring_values = std_mirroring_vh,
	.init_prg = std_prg_8k,
	.init_chr0 = std_chr_1k,
	.write_handlers = irem_g101_write_handlers,
	.max_prg_rom_size = SIZE_256K,
	.max_chr_rom_size = SIZE_256K,
	.max_wram_size = {SIZE_8K, 0},
	.flags = BOARD_INFO_FLAG_MIRROR_M,
};

struct board_info board_irem_g101_b = {
	.board_type = BOARD_TYPE_IREM_G101_B,
	.name = "IREM-G101-B",
	.init_prg = std_prg_8k,
	.init_chr0 = std_chr_1k,
	.write_handlers = irem_g101_b_write_handlers,
	.max_prg_rom_size = SIZE_256K,
	.max_chr_rom_size = SIZE_256K,
	.max_wram_size = {0, 0},
};

static void irem_g101_switch_prg_mode(struct board *board)
{
	if (board->prg_mode) {
		board->prg_banks[1].address = 0xc000;
		board->prg_banks[3].address = 0x8000;
	} else {
		board->prg_banks[1].address = 0x8000;
		board->prg_banks[3].address = 0xc000;
	}

	board_prg_sync(board);
}

static CPU_WRITE_HANDLER(irem_g101_write_handler)
{
	struct board *board = emu->board;

	addr &= 0xf007;

	switch (addr) {
	case 0x8000:
	case 0x8001:
	case 0x8002:
	case 0x8003:
	case 0x8004:
	case 0x8005:
	case 0x8006:
	case 0x8007:
		update_prg_bank(board, 1, value);
		break;
	case 0xa000:
	case 0xa001:
	case 0xa002:
	case 0xa003:
	case 0xa004:
	case 0xa005:
	case 0xa006:
	case 0xa007:
		update_prg_bank(board, 2, value);
		break;
	case 0xb000:
	case 0xb001:
	case 0xb002:
	case 0xb003:
	case 0xb004:
	case 0xb005:
	case 0xb006:
	case 0xb007:
		update_chr0_bank(board, addr & 0x07, value);
		break;
	case 0x9000:
	case 0x9001:
	case 0x9002:
	case 0x9003:
	case 0x9004:
	case 0x9005:
	case 0x9006:
	case 0x9007:
		if ((value & 0x02) != board->prg_mode) {
			board->prg_mode = value & 0x02;
			irem_g101_switch_prg_mode(board);
		}

		standard_mirroring_handler(emu, 0, value, cycles);
		break;
	}
}

static void irem_g101_reset(struct board *board, int hard)
{
	if (hard) {
		board->prg_mode = 0;
		board->prg_banks[3].bank = -2;
		irem_g101_switch_prg_mode(board);
	}
}
