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

static CPU_WRITE_HANDLER(rcm_gs2015_write_handler);
static CPU_WRITE_HANDLER(rcm_tetrisfamily_write_handler);

static struct board_write_handler rcm_gs2015_write_handlers[] = {
	{rcm_gs2015_write_handler, 0x8000, SIZE_32K, 0},
	{NULL},
};

static struct board_write_handler rcm_tetrisfamily_write_handlers[] = {
	{rcm_tetrisfamily_write_handler, 0x8000, SIZE_32K, 0},
	{NULL},
};

struct board_info board_rcm_gs2015 = {
	.board_type = BOARD_TYPE_RCM_GS2015,
	.name = "RCM-GS2015",
	.init_prg = std_prg_32k,
	.init_chr0 = std_chr_8k,
	.write_handlers = rcm_gs2015_write_handlers,
	.max_prg_rom_size = SIZE_2048K,
	.max_chr_rom_size = SIZE_1024K,
};

struct board_info board_rcm_tetrisfamily = {
	.board_type = BOARD_TYPE_RCM_TETRISFAMILY,
	.name = "RCM-TETRISFAMILY",
	.init_prg = std_prg_16k,
	.init_chr0 = std_chr_8k,
	.write_handlers = rcm_tetrisfamily_write_handlers,
	.max_prg_rom_size = SIZE_512K,
	.max_chr_rom_size = SIZE_8K,
	.max_wram_size = {SIZE_8K, 0},
	.flags = BOARD_INFO_FLAG_MIRROR_M,
	.mirroring_values = std_mirroring_vh,
	.mirroring_shift = 7,
};

static CPU_WRITE_HANDLER(rcm_gs2015_write_handler)
{
	struct board *board;

	board = emu->board;

	update_prg_bank(board, 1, addr);
	update_chr0_bank(board, 0, addr >> 1);
}

static CPU_WRITE_HANDLER(rcm_tetrisfamily_write_handler)
{
	struct board *board = emu->board;

	board->prg_banks[1].bank = ((addr & 0x0f) << 1) |
		((addr & 0x20) >> 5);
	board->prg_banks[2].bank = board->prg_banks[1].bank;

	if (addr & 0x10) {
		board->prg_banks[1].size = SIZE_16K;
		board->prg_banks[2].size = SIZE_16K;
		board->prg_banks[1].shift = 0;
	} else {
		board->prg_banks[1].size = SIZE_32K;
		board->prg_banks[2].size = 0;
		board->prg_banks[1].shift = 1;
	}

	standard_mirroring_handler(emu, addr, value, cycles);

	board_prg_sync(board);
}
