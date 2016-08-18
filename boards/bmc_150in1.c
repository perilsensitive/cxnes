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

static CPU_WRITE_HANDLER(bmc_150in1_write_handler);
static void bmc_150in1_reset(struct board *board, int);

static struct board_write_handler bmc_150in1_write_handlers[] = {
	{bmc_150in1_write_handler, 0x8000, SIZE_32K, 0},
	{NULL},
};

static struct board_funcs bmc_150in1_funcs = {
	.reset = bmc_150in1_reset,
};

struct board_info board_bmc_150in1 = {
	.board_type = BOARD_TYPE_BMC_150_IN_1,
	.name = "BMC-150IN1",
	.funcs = &bmc_150in1_funcs,
	.init_prg = std_prg_16k,
	.init_chr0 = std_chr_8k,
	.write_handlers = bmc_150in1_write_handlers,
	.max_prg_rom_size = SIZE_256K,
	.max_chr_rom_size = SIZE_64K,
	.flags = BOARD_INFO_FLAG_MIRROR_M,
	.mirroring_values = std_mirroring_vh,
};

static CPU_WRITE_HANDLER(bmc_150in1_write_handler)
{
	struct board *board = emu->board;
	int bank;

	bank = (addr >> 1) & 0x07;

	printf("value: %x\n", value);

	board->prg_banks[1].bank = bank;
	board->prg_banks[2].bank = bank;

	if ((addr & 0x0C) == 0x0C) {
		board->prg_banks[1].bank &= ~0x01;
		board->prg_banks[2].bank |=  0x01;
	}

	board_prg_sync(board);
	update_chr0_bank(board, 0, bank);
	standard_mirroring_handler(emu, addr, addr, cycles);
}

static void bmc_150in1_reset(struct board *board, int hard)
{
	if (hard)
		bmc_150in1_write_handler(board->emu, 0x8000, 0, 0);
}
