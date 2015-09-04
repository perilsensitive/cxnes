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

#define _cart_mode board->data[0]

static CPU_WRITE_HANDLER(m22_in_1_write_handler);
static void m22_in_1_reset(struct board *board, int);

static struct bank m22in1_init_prg[] = {
	{0, 0, SIZE_8K, 0x6000, MAP_PERM_READWRITE, MAP_TYPE_AUTO},

	/* Contra mode banks */
	{0, 0, SIZE_16K, 0x8000, MAP_PERM_READ, MAP_TYPE_ROM},
	{7, 0, SIZE_16K, 0xc000, MAP_PERM_READ, MAP_TYPE_ROM},

	/* Multicart mode 0 banks */
	{0, 1, 0, 0x8000, MAP_PERM_READ, MAP_TYPE_ROM},

	/* Multicart mode 1 banks */
	{0, 0, 0, 0x8000, MAP_PERM_READ, MAP_TYPE_ROM},
	{0, 0, 0, 0xc000, MAP_PERM_READ, MAP_TYPE_ROM},
	{.type = MAP_TYPE_END},
};

static struct board_funcs m22_in_1_funcs = {
	.reset = m22_in_1_reset,
};

static struct board_write_handler m22_in_1_write_handlers[] = {
	{m22_in_1_write_handler, 0x8000, SIZE_32K, 0},
	{NULL},
};

struct board_info board_22_in_1 = {
	.board_type = BOARD_TYPE_22_IN_1,
	.name = "BMC SUPER 22 GAMES / 20-IN-1",
	.funcs = &m22_in_1_funcs,
	.init_prg = m22in1_init_prg,
	.init_chr0 = std_chr_8k,
	.write_handlers = m22_in_1_write_handlers,
	.max_prg_rom_size = SIZE_128K + SIZE_512K,
	.max_chr_rom_size = SIZE_8K,
	.flags = BOARD_INFO_FLAG_MIRROR_M,
};

static CPU_WRITE_HANDLER(m22_in_1_write_handler)
{
	struct board *board = emu->board;

	board->prg_mode = value & 0x20;
	board->prg_banks[1].bank = value & 0x07;
	board->prg_banks[3].bank = (value & 0x1f) + 0x08;
	board->prg_banks[4].bank = (value & 0x1f) + 0x08;
	board->prg_banks[5].bank = (value & 0x1f) + 0x08;

	if (_cart_mode) {
		board_set_ppu_mirroring(board,
					(value & 0x40) ? MIRROR_V : MIRROR_H);

		if (!board->prg_mode) {
			board->prg_banks[3].size = SIZE_32K;
			board->prg_banks[4].size = 0;
			board->prg_banks[5].size = 0;
		} else {
			board->prg_banks[3].size = 0;
			board->prg_banks[4].size = SIZE_16K;
			board->prg_banks[5].size = SIZE_16K;
		}
	}

	board_prg_sync(board);
}

static void m22_in_1_reset(struct board *board, int hard)
{
	int mirroring;

	if (hard) {
		_cart_mode = 0;
	} else {
		_cart_mode ^= 1;
	}

	if (!_cart_mode) {
		board->prg_banks[1].size = SIZE_16K;
		board->prg_banks[2].size = SIZE_16K;
		board->prg_banks[3].size = 0;
		board->prg_banks[4].size = 0;
		board->prg_banks[5].size = 0;
		mirroring = MIRROR_V;
	} else {
		if (board->prg_banks[1].bank & 0x40)
			mirroring = MIRROR_V;
		else
			mirroring = MIRROR_H;
			
		if (!board->prg_mode) {
			board->prg_banks[1].size = 0;
			board->prg_banks[2].size = 0;
			board->prg_banks[3].size = SIZE_32K;
			board->prg_banks[4].size = 0;
			board->prg_banks[5].size = 0;
		} else {
			board->prg_banks[1].size = 0;
			board->prg_banks[2].size = 0;
			board->prg_banks[3].size = 0;
			board->prg_banks[4].size = SIZE_16K;
			board->prg_banks[5].size = SIZE_16K;
		}
	}

	board_prg_sync(board);
	board_set_ppu_mirroring(board, mirroring);
}
