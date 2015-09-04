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

static CPU_WRITE_HANDLER(contra_100in1_write_handler);

static struct bank contra_100in1_init_prg[] = {
	{0, 0, SIZE_8K, 0x6000, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{0, 0, SIZE_16K, 0x8000, MAP_PERM_READ, MAP_TYPE_ROM},
	{0, 0, 0, 0xa000, MAP_PERM_READ, MAP_TYPE_ROM},
	{1, 0, SIZE_16K, 0xc000, MAP_PERM_READ, MAP_TYPE_ROM},
	{0, 0, 0, 0xe000, MAP_PERM_READ, MAP_TYPE_ROM},
	{.type = MAP_TYPE_END}
};

static struct board_write_handler contra_100in1_write_handlers[] = {
	{contra_100in1_write_handler, 0x8000, SIZE_32K, 0},
	{NULL},
};

struct board_info board_contra_100in1 = {
	.board_type = BOARD_TYPE_CONTRA_100_IN_1,
	.name = "WAIXING / BMC CONTRA 100-IN-1",
	.init_prg = contra_100in1_init_prg,
	.init_chr0 = std_chr_8k,
	.write_handlers = contra_100in1_write_handlers,
	.max_prg_rom_size = SIZE_512K,
	.max_chr_rom_size = SIZE_8K,
	.max_wram_size = {SIZE_8K, 0},
	.flags = BOARD_INFO_FLAG_MIRROR_M,
};

static CPU_WRITE_HANDLER(contra_100in1_write_handler)
{
	struct board *board = emu->board;

	board->prg_mode = addr & 3;

	if (board->prg_mode != 2) {
		board->prg_banks[1].bank = value & 0x3f;
		board->prg_banks[1].size = SIZE_16K;
		board->prg_banks[2].size = 0;
		board->prg_banks[3].bank = value & 0x3f;
		board->prg_banks[3].size = SIZE_16K;
		board->prg_banks[4].size = 0;

		if (board->prg_mode == 0)
			board->prg_banks[3].bank |= 1;
		else if (board->prg_mode == 1)
			board->prg_banks[3].bank = -1;
	} else {
		int i;

		for (i = 1; i < 5; i++) {
			board->prg_banks[i].bank =
			    ((value & 0x3f) << 1) | (value >> 7);
			board->prg_banks[i].size = SIZE_8K;
		}
	}

	board_prg_sync(board);

	board_set_ppu_mirroring(board, value & 0x40 ? MIRROR_H : MIRROR_V);
}
