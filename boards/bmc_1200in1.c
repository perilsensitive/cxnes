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

static CPU_WRITE_HANDLER(m1200in1_write_handler);

static struct board_write_handler m1200in1_write_handlers[] = {
	{m1200in1_write_handler, 0x8000, SIZE_32K, 0},
	{NULL},
};

struct board_info board_1200in1 = {
	.board_type = BOARD_TYPE_BMC_1200_IN_1,
	.name = "BMC 1200/36-IN-1",
	.init_prg = std_prg_16k,
	.init_chr0 = std_chr_8k,
	.write_handlers = m1200in1_write_handlers,
	.max_prg_rom_size = SIZE_512K,
	.max_chr_rom_size = SIZE_8K,
	.max_wram_size = {SIZE_8K, 0},
	.flags = BOARD_INFO_FLAG_MIRROR_M,
};

static CPU_WRITE_HANDLER(m1200in1_write_handler)
{
	struct board *board = emu->board;

	board->prg_banks[1].bank = addr & 0x07;
	board->prg_banks[2].bank = addr & 0x07;
	board->chr_banks0[0].bank = addr & 0x07;
	board_set_ppu_mirroring(board, addr & 0x08 ? MIRROR_H : MIRROR_V);

	board_prg_sync(board);
	board_chr_sync(board, 0);
}
