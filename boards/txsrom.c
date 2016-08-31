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
#include "a12_timer.h"
#include "m2_timer.h"
#include "mmc3.h"

static CPU_WRITE_HANDLER(txsrom_bank_select);
static CPU_WRITE_HANDLER(txsrom_bank_data);

static struct board_write_handler txsrom_write_handlers[] = {
	{txsrom_bank_select, 0x8000, SIZE_8K, 0x8001},
	{txsrom_bank_data, 0x8001, SIZE_8K, 0x8001},
	{mmc3_wram_protect, 0xa001, SIZE_8K, 0xa001},
	{a12_timer_irq_latch, 0xc000, SIZE_8K, 0xc001},
	{a12_timer_irq_reload, 0xc001, SIZE_8K, 0xc001},
	{a12_timer_irq_disable, 0xe000, SIZE_8K, 0xe001},
	{a12_timer_irq_enable, 0xe001, SIZE_8K, 0xe001},
	{NULL},
};

struct board_info board_txsrom = {
	.board_type = BOARD_TYPE_TxSROM,
	.name = "TxSROM",
	.mapper_name = "MMC3",
	.funcs = &mmc3_funcs,
	.init_prg = mmc3_init_prg,
	.init_chr0 = mmc3_init_chr0,
	.write_handlers = txsrom_write_handlers,
	.max_prg_rom_size = SIZE_2048K,
	.max_chr_rom_size = SIZE_256K,
	.max_wram_size = {SIZE_8K, 0},
	.flags = BOARD_INFO_FLAG_MIRROR_M,
	.mirroring_values = std_mirroring_vh,
};

static CPU_WRITE_HANDLER(txsrom_bank_select)
{
	struct board *board;
	int old;

	board = emu->board;
	old = _bank_select & 0xc0;

	mmc3_bank_select(emu, addr, value, cycles);

	if ((value & 0x80) != (old & 0x80)) {
		mmc3_txsrom_mirroring(board);
	}
}

static CPU_WRITE_HANDLER(txsrom_bank_data)
{
	struct board *board;
	int bank;

	board = emu->board;

	bank = _bank_select & 0x07;

	mmc3_bank_data(emu, addr, value, cycles);

	if (bank < 6)
		mmc3_txsrom_mirroring(board);
}

