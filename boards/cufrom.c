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

static CPU_WRITE_HANDLER(cufrom_write_handler);

static struct board_write_handler cufrom_write_handlers[] = {
	{cufrom_write_handler, 0x8000, SIZE_32K, 0},
	{NULL},
};

struct board_info board_cufrom = {
	.board_type = BOARD_TYPE_RETROUSB_CUFROM,
	.name = "RET-CUFROM",
	.init_prg = std_prg_16k,
	.init_chr0 = std_chr_8k,
	.write_handlers = cufrom_write_handlers,
	.max_prg_rom_size = SIZE_128K,
	.max_wram_size = {SIZE_8K, 0},
	.min_vram_size = {SIZE_32K, 0},
	.max_vram_size = {SIZE_32K, 0},
};

static CPU_WRITE_HANDLER(cufrom_write_handler)
{
	struct board *board = emu->board;

	update_prg_bank(board, 1, value >> 2);
	update_chr0_bank(board, 0, value & 0x03);
}
