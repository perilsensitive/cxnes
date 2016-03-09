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

#define _latch0 board->data[0]
#define _latch1 board->data[1]

static int mmc4_init(struct board *board);
static void mmc4_reset(struct board *board, int);
static CPU_WRITE_HANDLER(mmc4_write_handler);
static void mmc4_ppu_hook(struct board *board, int addr);

static struct board_funcs mmc4_funcs =
    { .init = mmc4_init, .reset = mmc4_reset, };

static struct bank mmc2_init_prg[] = {
	{0, 0, SIZE_8K, 0x6000, MAP_PERM_READWRITE, MAP_TYPE_RAM0},
	{0, 0, SIZE_8K, 0x8000, MAP_PERM_READWRITE, MAP_TYPE_ROM},
	{-3, 0, SIZE_8K, 0xa000, MAP_PERM_READWRITE, MAP_TYPE_ROM},
	{-2, 0, SIZE_8K, 0xc000, MAP_PERM_READWRITE, MAP_TYPE_ROM},
	{-1, 0, SIZE_8K, 0xe000, MAP_PERM_READWRITE, MAP_TYPE_ROM},
	{.type = MAP_TYPE_END},
};

static struct bank mmc4_init_chr[] = {
	{0, 0, 0, 0x0000, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{0, 0, SIZE_4K, 0x0000, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{0, 0, 0, 0x1000, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{0, 0, SIZE_4K, 0x1000, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{.type = MAP_TYPE_END},
};

static struct board_write_handler mmc4_write_handlers[] = {
	{mmc4_write_handler, 0xa000, 5 * SIZE_4K, 0},
	{standard_mirroring_handler, 0xf000, SIZE_4K, 0},
	{NULL},
};

struct board_info board_fxrom = {
	.board_type = BOARD_TYPE_FxROM,
	.name = "FxROM",
	.mapper_name = "MMC4",
	.mirroring_values = std_mirroring_vh,
	.funcs = &mmc4_funcs,
	.init_prg = std_prg_16k,
	.init_chr0 = mmc4_init_chr,
	.write_handlers = mmc4_write_handlers,
	.max_prg_rom_size = SIZE_256K,
	.max_chr_rom_size = SIZE_128K,
	.max_wram_size = {SIZE_8K, 0},
	.flags = BOARD_INFO_FLAG_MIRROR_M,
};

struct board_info board_pxrom = {
	.board_type = BOARD_TYPE_PxROM,
	.name = "PxROM",
	.mapper_name = "MMC2",
	.mirroring_values = std_mirroring_vh,
	.funcs = &mmc4_funcs,
	.init_prg = mmc2_init_prg,
	.init_chr0 = mmc4_init_chr,
	.write_handlers = mmc4_write_handlers,
	.max_prg_rom_size = SIZE_128K,
	.max_chr_rom_size = SIZE_128K,
	.max_wram_size = {SIZE_8K, 0},
	.flags = BOARD_INFO_FLAG_MIRROR_M,
};

static int mmc4_init(struct board *board)
{
	ppu_set_read_hook(mmc4_ppu_hook);

	return 0;
}

static void mmc4_reset(struct board *board, int hard)
{
	if (hard) {
		_latch0 = 0xfe;
		_latch1 = 0xfe;
	}
}

static void mmc4_ppu_hook(struct board *board, int ppu_addr)
{
	int bank;

	if (ppu_addr >= 0x2000)
		return;

	bank = (ppu_addr >> 11) & 2;
	switch (ppu_addr & 0xff8) {
	case 0xfd8:
		break;
	case 0xfe8:
		bank++;
		break;
	default:
		return;
	}

	if (bank >> 1)
		_latch1 = 0xfd + (bank & 1);
	else
		_latch0 = 0xfd + (bank & 1);

	board->chr_banks0[bank].size = SIZE_4K;
	board->chr_banks0[bank ^ 1].size = 0;
	board_chr_sync(board, 0);
}

static CPU_WRITE_HANDLER(mmc4_write_handler)
{
	struct board *board = emu->board;

	uint8_t data;
	int bank;

	addr &= 0xf000;

	switch (addr) {
	case 0xa000:
		board->prg_banks[1].bank = value;
		board_prg_sync(board);
		break;
	case 0xb000:
	case 0xc000:
	case 0xd000:
	case 0xe000:
		ppu_run(emu->ppu, cycles);
		bank = (addr - 0xb000) >> 12;
		data = (bank >> 1 ? _latch1 : _latch0);
		board->chr_banks0[bank].bank = value;
		if (data == (0xfd + (bank & 1))) {
			board->chr_banks0[bank].size = SIZE_4K;
			board->chr_banks0[bank ^ 1].size = 0;
			board_chr_sync(board, 0);
		}
		break;
	}
}
