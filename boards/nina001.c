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

static CPU_WRITE_HANDLER(nina001_write_handler);

static struct board_write_handler nina001_write_handlers[] = {
	{nina001_write_handler, 0x7ffd, 3, 0},
	{NULL},
};

struct board_info board_nina001 = {
	.board_type = BOARD_TYPE_NINA_001,
	.name = "AVE-NINA-01",
	.init_prg = std_prg_32k,
	.init_chr0 = std_chr_4k,
	.write_handlers = nina001_write_handlers,
	.max_prg_rom_size = SIZE_64K,
	.max_chr_rom_size = SIZE_64K,
	.min_wram_size = {SIZE_8K, 0},
	.max_wram_size = {SIZE_8K, 0},
};

static CPU_WRITE_HANDLER(nina001_write_handler)
{
	struct board *board = emu->board;

	switch (addr) {
	case 0x7ffd:
		board->prg_banks[1].bank = value;
		board_prg_sync(board);
		break;
	case 0x7ffe:
	case 0x7fff:
		board->chr_banks0[addr & 1].bank = value;
		board_chr_sync(board, 0);
		break;
	}

	if (board->wram[0].size > 0)
		board->wram[0].data[addr - 0x6000] = value;
}
