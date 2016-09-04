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

#define _bank board->data[0]

static CPU_WRITE_HANDLER(bmc_n625092_write_handler);
static void bmc_n625092_reset(struct board *board, int);

static struct board_funcs bmc_n625092_funcs = {
	.reset = bmc_n625092_reset,
};

static struct bank bmc_n625092_init_prg[] = {
	{0, 0, SIZE_8K, 0x6000, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{0, 0, SIZE_16K, 0x8000, MAP_PERM_READ, MAP_TYPE_ROM},
	{0, 0, SIZE_16K, 0xc000, MAP_PERM_READ, MAP_TYPE_ROM},
	{.type = MAP_TYPE_END},
};

static struct board_write_handler bmc_n625092_write_handlers[] = {
	{bmc_n625092_write_handler, 0x8000, SIZE_32K, 0},
	{NULL},
};

struct board_info board_bmc_n625092 = {
	.board_type = BOARD_TYPE_BMC_N625092,
	.name = "BMC-N625092",
	.funcs = &bmc_n625092_funcs,
	.init_prg = bmc_n625092_init_prg,
	.init_chr0 = std_chr_8k,
	.write_handlers = bmc_n625092_write_handlers,
	.max_prg_rom_size = SIZE_1024K,
	.max_chr_rom_size = SIZE_8K,
	.flags = BOARD_INFO_FLAG_MIRROR_M,
	.max_wram_size = { SIZE_8K, 0 },
	.mirroring_values = std_mirroring_vh,
};

static CPU_WRITE_HANDLER(bmc_n625092_write_handler)
{
	struct board *board = emu->board;
	int b0, b1;

	switch (addr & 0xc000) {
	case 0x8000:
		standard_mirroring_handler(emu, addr, addr & 0x01, cycles);
		board->prg_mode = (addr & 0x102) >> 1;
		board->prg_or = (addr & 0xe0) >> 2;
		break;
	case 0xc000:
		_bank = addr & 0x07;
		break;
	}

	if (board->prg_mode & 0x01) {
		if (board->prg_mode & 0x80) {
			b0 = _bank;
			b1 = 0x07;
		} else {
			b0 = _bank & 0x06;
			b1 = b0 | 0x01;
		}
	} else {
		b0 = _bank;
		b1 = _bank;
	}

	update_prg_bank(board, 1, b0);
	update_prg_bank(board, 2, b1);
}

static void bmc_n625092_reset(struct board *board, int hard)
{
	if (hard) {
		board->prg_mode = 0;
		board->prg_or = 0;
		board->prg_and = 0x07;
		_bank = 0;
	}

}
