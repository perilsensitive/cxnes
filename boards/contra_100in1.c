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

static CPU_WRITE_HANDLER(contra_100in1_write_handler);

static struct bank contra_100in1_init_prg[] = {
	{0, 0, SIZE_8K, 0x6000, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{0, 0, SIZE_8K, 0x8000, MAP_PERM_READ, MAP_TYPE_ROM},
	{1, 0, SIZE_8K, 0xa000, MAP_PERM_READ, MAP_TYPE_ROM},
	{2, 0, SIZE_8K, 0xc000, MAP_PERM_READ, MAP_TYPE_ROM},
	{3, 0, SIZE_8K, 0xe000, MAP_PERM_READ, MAP_TYPE_ROM},
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
	.mirroring_shift = 6,
	.mirroring_values = std_mirroring_vh,
};

static CPU_WRITE_HANDLER(contra_100in1_write_handler)
{
	struct board *board = emu->board;

	int bank = (value & 0x3f) << 1;
	int half = value >> 7;

	switch (addr & 0xfff) {
	case 0:
		board->prg_banks[1].bank = bank ^ half;
		board->prg_banks[2].bank = (bank + 1) ^ half;
		board->prg_banks[3].bank = (bank + 2) ^ half;
		board->prg_banks[4].bank = (bank + 3) ^ half;
		break;
	case 1:
		bank |= half;
		board->prg_banks[1].bank = bank;
		board->prg_banks[2].bank = bank + 1;
		board->prg_banks[3].bank = 0xfe;
		board->prg_banks[4].bank = 0xff;
		break;
	case 2:
		bank |= half;
		board->prg_banks[1].bank = bank;
		board->prg_banks[2].bank = bank;
		board->prg_banks[3].bank = bank;
		board->prg_banks[4].bank = bank;
		break;
	case 3:
		bank |= half;
		board->prg_banks[1].bank = bank;
		board->prg_banks[2].bank = bank + 1;
		board->prg_banks[3].bank = bank;
		board->prg_banks[4].bank = bank + 1;
		break;
	}

	board_prg_sync(board);

	standard_mirroring_handler(emu, addr, value, cycles);
}
