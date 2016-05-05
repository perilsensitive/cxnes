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

static CPU_WRITE_HANDLER(colordreams_write_handler)
{
	update_prg_bank(emu->board, 1, (value & 0x03));
	update_chr0_bank(emu->board, 0, (value & 0xf0) >> 4);

}

static CPU_WRITE_HANDLER(rumblestation_write_handler)
{
	int new_prg_or, new_chr_or;
	
	new_prg_or = (value & 0x0f) << 1;
	new_chr_or = (value & 0xf0) >> 1;

	if (new_prg_or != emu->board->prg_or) {
		emu->board->prg_or = new_prg_or;
		board_prg_sync(emu->board);
	}

	if (new_chr_or != emu->board->chr_or) {
		emu->board->chr_or = new_chr_or;
		board_chr_sync(emu->board, 0);
	}
}

static void rumblestation_reset(struct board *board, int hard)
{
	if (!hard)
		return;

	board->prg_and = 0x01;
	board->chr_and = 0x07;

	board->prg_or = 0x00;
	board->chr_or = 0x00;

	board_prg_sync(board);
	board_chr_sync(board, 0);
}

static struct board_write_handler colordreams_write_handlers[] = {
	{colordreams_write_handler, 0x8000, SIZE_32K, 0},
	{NULL},
};

static struct board_write_handler rumblestation_write_handlers[] = {
	{rumblestation_write_handler, 0x6000, SIZE_8K, 0},
	{colordreams_write_handler, 0x8000, SIZE_32K, 0},
	{NULL},
};

static struct board_funcs rumblestation_funcs = {
	.reset = rumblestation_reset,
};

struct board_info board_colordreams = {
	.board_type = BOARD_TYPE_COLORDREAMS,
	.name = "COLORDREAMS-74*377",
	.init_prg = std_prg_32k,
	.init_chr0 = std_chr_8k,
	.write_handlers = colordreams_write_handlers,
	.max_prg_rom_size = SIZE_512K,
	.max_chr_rom_size = SIZE_128K,
	.max_wram_size = {SIZE_8K, 0},
};

struct board_info board_rumblestation_15_in_1 = {
	.board_type = BOARD_TYPE_RUMBLESTATION_15_IN_1,
	.name = "RUMBLESTATION 15-IN-1",
	.funcs = &rumblestation_funcs,
	.init_prg = std_prg_32k,
	.init_chr0 = std_chr_8k,
	.write_handlers = rumblestation_write_handlers,
	.max_prg_rom_size = SIZE_1024K,
	.max_chr_rom_size = SIZE_1024K,
};
