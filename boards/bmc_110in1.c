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

static CPU_WRITE_HANDLER(bmc_110in1_write_handler);
static void bmc_110in1_reset(struct board *board, int);

static struct board_funcs bmc_110in1_funcs = {
	.reset = bmc_110in1_reset,
};

static struct board_write_handler bmc_110in1_write_handlers[] = {
	{bmc_110in1_write_handler, 0x8000, SIZE_32K, 0},
	{NULL},
};

struct board_info board_bmc_110in1 = {
	.board_type = BOARD_TYPE_BMC_110_IN_1,
	.name = "BMC 58/64/72/110-IN-1",
	.funcs = &bmc_110in1_funcs,
	.init_prg = std_prg_16k,
	.init_chr0 = std_chr_8k,
	.write_handlers = bmc_110in1_write_handlers,
	.max_prg_rom_size = SIZE_2048K,
	.max_chr_rom_size = SIZE_1024K,
	.flags = BOARD_INFO_FLAG_MIRROR_M,
	.mirroring_values = std_mirroring_hv,
};

static CPU_WRITE_HANDLER(bmc_110in1_write_handler)
{
	struct board *board;
	int chr_bank;
	int prg_bank;
	int mode;

	board = emu->board;

	chr_bank  = addr & 0x3f;
	chr_bank |= (addr & 0x4000) >> 8;

	board->chr_banks0[0].bank = chr_bank;

	standard_mirroring_handler(emu, addr, (addr >> 13) & 1, cycles);

	prg_bank  = (addr & 0x0fc0) >> 6;
	prg_bank |= (addr & 0x4000) >> 8;

	mode = (~addr >> 12) & 0x01;

	board->prg_banks[1].bank = prg_bank & ~mode;
	board->prg_banks[2].bank = prg_bank | mode;

	board_prg_sync(board);
	board_chr_sync(board, 0);
}

static void bmc_110in1_reset(struct board *board, int hard)
{
	if (hard) {
		bmc_110in1_write_handler(board->emu, 0x8000, 0x00, 0);
	}
}
