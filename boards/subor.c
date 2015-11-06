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

#define outer_bank_1 board->data[0]
#define outer_bank_2 board->data[1]
#define inner_bank_1 board->data[2]
#define inner_bank_2 board->data[3]

static CPU_WRITE_HANDLER(subor_write_handler);

static struct board_write_handler subor_write_handlers[] = {
	{subor_write_handler, 0x8000, SIZE_32K, 0},
	{NULL},
};

static struct bank subor_b_init_prg[] = {
	{0, 0, SIZE_8K, 0x6000, MAP_PERM_READWRITE, MAP_TYPE_RAM0},
	{0, 0, SIZE_16K, 0x8000, MAP_PERM_READ, MAP_TYPE_ROM},
	{7, 0, SIZE_16K, 0xc000, MAP_PERM_READ, MAP_TYPE_ROM},
	{.type = MAP_TYPE_END},
};

static struct bank subor_a_init_prg[] = {
	{0, 0, SIZE_8K, 0x6000, MAP_PERM_READWRITE, MAP_TYPE_RAM0},
	{0, 0, SIZE_16K, 0x8000, MAP_PERM_READ, MAP_TYPE_ROM},
	{0x20, 0, SIZE_16K, 0xc000, MAP_PERM_READ, MAP_TYPE_ROM},
	{.type = MAP_TYPE_END},
};

struct board_info board_subor_b = {
	.board_type = BOARD_TYPE_SUBOR_B,
	.name = "SUBOR (b)",
	.init_prg = subor_b_init_prg,
	.init_chr0 = std_chr_8k,
	.write_handlers = subor_write_handlers,
	.max_prg_rom_size = SIZE_1024K,
	.max_chr_rom_size = SIZE_8K,
	.max_wram_size = {SIZE_8K, 0},
};

struct board_info board_subor_a = {
	.board_type = BOARD_TYPE_SUBOR_A,
	.name = "SUBOR (a)",
	.init_prg = subor_a_init_prg,
	.init_chr0 = std_chr_8k,
	.write_handlers = subor_write_handlers,
	.max_prg_rom_size = SIZE_1024K,
	.max_chr_rom_size = SIZE_8K,
	.max_wram_size = {SIZE_8K, 0},
};

static CPU_WRITE_HANDLER(subor_write_handler)
{
	struct board *board = emu->board;
	uint32_t type;
	int bank;

	type = board->info->board_type;

	switch (addr & 0xf000) {
	case 0x8000:
	case 0x9000:
		outer_bank_1 = (value & 0x10) << 1;
		break;
	case 0xa000:
	case 0xb000:
		outer_bank_2 = (value & 0x10) << 1;
		board->prg_mode = value & 0x0c;
		break;
	case 0xc000:
	case 0xd000:
		inner_bank_1 = value & 0x1f;
		break;
	case 0xe000:
	case 0xf000:
		inner_bank_2 = value & 0x1f;
		break;
	}

	bank = (outer_bank_1 ^ outer_bank_2) | (inner_bank_1 ^ inner_bank_2);

	switch (board->prg_mode & 0x0c) {
	case 0x00:
		board->prg_banks[1].bank = bank;
		if (type == BOARD_TYPE_SUBOR_B) {
			board->prg_banks[2].bank = 0x07;
		} else {
			board->prg_banks[2].bank = 0x20;
		}
		break;
	case 0x04:
		board->prg_banks[1].bank = 0x1f;
		board->prg_banks[2].bank = bank;
		break;
	case 0x08:
	case 0x0c:
		bank &= 0xfe;
		if (type == BOARD_TYPE_SUBOR_B) {
			board->prg_banks[1].bank = bank;
			board->prg_banks[2].bank = bank | 1;
		} else {
			board->prg_banks[1].bank = bank | 1;
			board->prg_banks[2].bank = bank;
		}
		break;
	}

	board_prg_sync(board);
}
