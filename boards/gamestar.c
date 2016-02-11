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

static CPU_WRITE_HANDLER(gamestar_b_write_handler);
static void gamestar_b_reset(struct board *board, int);

static struct board_write_handler gamestar_b_write_handlers[] = {
	{gamestar_b_write_handler, 0x8000, SIZE_32K, 0},
	{NULL},
};

static struct board_funcs gamestar_b_funcs = {
	.reset = gamestar_b_reset,
};

struct board_info board_gamestar_b = {
	.board_type = BOARD_TYPE_GAMESTAR_B,
	.name = "BMC GAMESTAR (b)",
	.funcs = &gamestar_b_funcs,
	.init_prg = std_prg_16k,
	.init_chr0 = std_chr_8k,
	.write_handlers = gamestar_b_write_handlers,
	.max_prg_rom_size = SIZE_256K,
	.max_chr_rom_size = SIZE_128K,
	.flags = BOARD_INFO_FLAG_MIRROR_M,
	.mirroring_shift = 7,
	.mirroring_values = std_mirroring_vh,
};

static CPU_WRITE_HANDLER(gamestar_b_write_handler)
{
	struct board *board = emu->board;

	board->prg_banks[1].bank = addr;
	board->prg_banks[2].bank = addr;

	if (!(addr & 0x40)) {
		board->prg_banks[1].bank &= ~0x01;
		board->prg_banks[2].bank |=  0x01;
	}

	board_prg_sync(board);
	update_chr0_bank(board, 0, addr >> 3);
	standard_mirroring_handler(emu, addr, addr, cycles);
}

static void gamestar_b_reset(struct board *board, int hard)
{
	if (hard)
		gamestar_b_write_handler(board->emu, 0x8000, 0, 0);
}
