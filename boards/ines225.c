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

static CPU_WRITE_HANDLER(ines225_write_handler);
static void ines225_reset(struct board *board, int);

static struct board_funcs ines225_funcs = {
	.reset = ines225_reset,
};

static struct board_write_handler ines225_write_handlers[] = {
	{ines225_write_handler, 0x8000, SIZE_32K, 0},
	{NULL},
};

struct board_info board_ines225 = {
	.board_type = BOARD_TYPE_INES225,
	.name = "BMC 58/64/72-IN-1",
	.funcs = &ines225_funcs,
	.init_prg = std_prg_16k,
	.init_chr0 = std_chr_8k,
	.write_handlers = ines225_write_handlers,
	.max_prg_rom_size = SIZE_2048K,
	.max_chr_rom_size = SIZE_512K,
	.flags = BOARD_INFO_FLAG_MIRROR_M,
};

static CPU_WRITE_HANDLER(ines225_write_handler)
{
	struct board *board;
	int chr_bank;
	int prg_bank;

	board = emu->board;

	chr_bank  = addr & 0x3f;
	chr_bank |= (addr & 0x4000) >> 8;

	board->chr_banks0[0].bank = chr_bank;

	board_set_ppu_mirroring(board, (addr & 0x2000) ?
				MIRROR_H : MIRROR_V);

	prg_bank  = (addr & 0x0fc0) >> 6;
	prg_bank |= (addr & 0x4000) >> 8;

	if (!(addr & 0x1000)) {
		board->prg_banks[1].bank = prg_bank & 0x1e;
		board->prg_banks[2].bank = prg_bank | 0x01;
	} else {
		board->prg_banks[1].bank = prg_bank;
		board->prg_banks[2].bank = prg_bank;
	}

	board_prg_sync(board);
	board_chr_sync(board, 0);
}

static void ines225_reset(struct board *board, int hard)
{
	if (hard) {
		/* Act as if 0x00 was written to $8000 */
	}
}
