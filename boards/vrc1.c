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

static CPU_WRITE_HANDLER(vrc1_write_handler);

static struct board_write_handler vrc1_write_handlers[] = {
	{vrc1_write_handler, 0x8000, SIZE_32K, 0},
	{NULL},
};

struct board_info board_vrc1 = {
	.board_type = BOARD_TYPE_VRC1,
	.name = "KONAMI-VRC-1",
	.mirroring_values = std_mirroring_vh,
	.init_prg = std_prg_8k,
	.init_chr0 = std_chr_4k,
	.write_handlers = vrc1_write_handlers,
	.max_prg_rom_size = SIZE_128K,
	.max_chr_rom_size = SIZE_128K,
	.max_wram_size = {SIZE_8K, 0},
	.flags = BOARD_INFO_FLAG_MIRROR_M,
};

static CPU_WRITE_HANDLER(vrc1_write_handler)
{
	struct board *board = emu->board;

	addr &= 0xf000;
	value &= 0x0f;

	switch (addr) {
	case 0x8000:
	case 0xa000:
	case 0xc000:
		board->prg_banks[1 + ((addr - 0x8000) >> 13)].bank =
		    value;
		board_prg_sync(board);
		break;
	case 0x9000:
		board->chr_banks0[0].bank =
		    (board->chr_banks0[0].
		     bank & 0xf) | ((value & 0x02) << 3);
		board->chr_banks0[1].bank =
		    (board->chr_banks0[1].
		     bank & 0xf) | ((value & 0x04) << 2);
		board_chr_sync(board, 0);
		standard_mirroring_handler(emu, 0, value, cycles);
		break;
	case 0xe000:
	case 0xf000:
		addr = (addr & 0x1000) >> 12;
		board->chr_banks0[addr].bank =
		    (board->chr_banks0[addr].bank & 0x10) | value;
		board_chr_sync(board, 0);
		break;
	}
}
