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

static CPU_WRITE_HANDLER(action52_write_handler);
static void action52_reset(struct board *board, int);

static struct board_funcs action52_funcs = {
	.reset = action52_reset,
};

static struct board_write_handler action52_write_handlers[] = {
	{action52_write_handler, 0x8000, SIZE_32K, 0},
	{NULL},
};

struct board_info board_action52 = {
	.board_type = BOARD_TYPE_ACTION_52,
	.name = "MLT-ACTION52",
	.funcs = &action52_funcs,
	.init_prg = std_prg_16k,
	.init_chr0 = std_chr_8k,
	.write_handlers = action52_write_handlers,
	.max_prg_rom_size = SIZE_2048K,
	.max_chr_rom_size = SIZE_512K,
	.flags = BOARD_INFO_FLAG_MIRROR_M,
};

static CPU_WRITE_HANDLER(action52_write_handler)
{
	struct board *board;
	int chr_bank;
	int prg_bank;

	board = emu->board;

	chr_bank  = value & 0x03;
	chr_bank |= (addr & 0x0f) << 2;

	board->chr_banks0[0].bank = chr_bank;

	board_set_ppu_mirroring(board, (addr & 0x2000) ?
				MIRROR_H : MIRROR_V);

	prg_bank = (addr & 0x07c0) >> 6;

	if (!(addr & 0x0020)) {
		board->prg_banks[1].bank = prg_bank & 0x1e;
		board->prg_banks[2].bank = prg_bank | 0x01;
	} else {
		board->prg_banks[1].bank = prg_bank;
		board->prg_banks[2].bank = prg_bank;
	}

	board->prg_or = (addr & 0x1800) >> 6;

	/* Hack for Action 52/Cheetamen II: bank 2 is open bus,
	   everything else works as expected.  Swap banks 2 and
	   3 so that standard iNES roms work (bank 3 stored after
	   bank 1 in the rom).

	   Cheetamen II selects bank 1 right after boot, so this
	   needs to be a mirror of bank 0.  This setup allows
	   this to work.
	*/

	if (board->prg_or == 0x40) {
		board->prg_banks[1].type = MAP_TYPE_NONE;
		board->prg_banks[2].type = MAP_TYPE_NONE;
	} else {
		board->prg_banks[1].type = MAP_TYPE_ROM;
		board->prg_banks[2].type = MAP_TYPE_ROM;
	}

	if (board->prg_or & 0x40)
		board->prg_or ^= 0x20;

	board_prg_sync(board);
	board_chr_sync(board, 0);
}

static void action52_reset(struct board *board, int hard)
{
	if (hard) {
		/* Act as if 0x00 was written to $8000 */
		board->prg_and = 0x1f;
		board->prg_or = 0x00;
		board->prg_banks[1].bank = 0;
		board->prg_banks[2].bank = 1;
		board->chr_banks0[0].bank = 0;
	}
}
