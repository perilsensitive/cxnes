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

static CPU_WRITE_HANDLER(super700in1_write_handler);
static void super700in1_reset(struct board *board, int);

static struct board_write_handler super700in1_write_handlers[] = {
	{super700in1_write_handler, 0x8000, SIZE_32K, 0},
	{NULL},
};

static struct board_funcs super700in1_funcs = {
	.reset = super700in1_reset,
};

struct board_info board_super700in1 = {
	.board_type = BOARD_TYPE_SUPER700IN1,
	.name = "BMC SUPER 700-IN-1",
	.funcs = &super700in1_funcs,
	.init_prg = std_prg_16k,
	.init_chr0 = std_chr_8k,
	.write_handlers = super700in1_write_handlers,
	.max_prg_rom_size = SIZE_256K,
	.max_chr_rom_size = SIZE_128K,
	.flags = BOARD_INFO_FLAG_MIRROR_M,
	.mirroring_shift = 7,
	.mirroring_values = std_mirroring_vh,
};

static CPU_WRITE_HANDLER(super700in1_write_handler)
{
	struct board *board = emu->board;
	int prg, chr;
	
	prg = (addr & 0x40) | ((addr >> 8) & 0x3f);
	chr = ((addr & 0x1f) << 2) | (value & 0x03);

	board->prg_banks[1].bank = prg;
	board->prg_banks[2].bank = prg;

	if (!(addr & 0x20)) {
		board->prg_banks[1].bank &= ~0x01;
		board->prg_banks[2].bank |=  0x01;
	}

	board_prg_sync(board);
	update_chr0_bank(board, 0, chr);
	standard_mirroring_handler(emu, addr, addr, cycles);
}

static void super700in1_reset(struct board *board, int hard)
{
	if (hard)
		super700in1_write_handler(board->emu, 0x8000, 0, 0);
}
