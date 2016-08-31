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

static CPU_WRITE_HANDLER(mmc6_bank_select);
static CPU_WRITE_HANDLER(mmc6_wram_protect);
static CPU_READ_HANDLER(mmc6_wram_read_handler);
static CPU_WRITE_HANDLER(mmc6_wram_write_handler);

static struct board_write_handler mmc6_write_handlers[] = {
	{mmc6_bank_select, 0x8000, SIZE_8K, 0x8001},
	{mmc3_bank_data, 0x8001, SIZE_8K, 0x8001},
	{mmc6_wram_write_handler, 0x7000, SIZE_4K, 0},
	{standard_mirroring_handler, 0xa000, SIZE_8K, 0xa001},
	{mmc6_wram_protect, 0xa001, SIZE_8K, 0xa001},
	{a12_timer_irq_latch, 0xc000, SIZE_8K, 0xc001},
	{a12_timer_irq_reload, 0xc001, SIZE_8K, 0xc001},
	{a12_timer_irq_disable, 0xe000, SIZE_8K, 0xe001},
	{a12_timer_irq_enable, 0xe001, SIZE_8K, 0xe001},
	{NULL},
};

static struct board_read_handler mmc6_read_handlers[] = {
	{mmc6_wram_read_handler, 0x7000, SIZE_4K, 0},
	{NULL},
};

struct board_info board_hkrom = {
	.board_type = BOARD_TYPE_HKROM,
	.name = "NES-HKROM",
	.mapper_name = "MMC6",
	.funcs = &mmc3_funcs,
	.init_prg = mmc3_init_prg,
	.init_chr0 = mmc3_init_chr0,
	.read_handlers = mmc6_read_handlers,
	.write_handlers = mmc6_write_handlers,
	.max_prg_rom_size = SIZE_512K,
	.max_chr_rom_size = SIZE_256K,
	.min_wram_size = {0, 0},
	.max_wram_size = {0, 0},
	.flags = BOARD_INFO_FLAG_MIRROR_M | BOARD_INFO_FLAG_MAPPER_NV,
	.mapper_ram_size = SIZE_1K,
	.mirroring_values = std_mirroring_vh,
};

static CPU_WRITE_HANDLER(mmc6_bank_select)
{
	struct board *board;
	board = emu->board;

	mmc3_bank_select(emu, addr, value, cycles);

	if (!(_bank_select & 0x20))
		_wram_protect = 0;
}

static CPU_WRITE_HANDLER(mmc6_wram_protect)
{
	struct board *board = emu->board;

	if (_bank_select & 0x20)
		_wram_protect = value;
}

static CPU_READ_HANDLER(mmc6_wram_read_handler)
{
	struct board *board = emu->board;
	uint8_t rc;

	/* Reads are open-bus if neither 512-byte page is readable */

	addr &= 0x3ff;

	/* Note that there will always be battery-backed PRG-RAM; the RAM
	   itself is built into the MMC6, and all HKROM boards in existence
	   have a battery */

	if (!(_wram_protect & 0xa0))
		rc = value;
	if ((addr < 0x200 && ((_wram_protect & 0xa0) == 0x80)) ||
	    (addr >= 0x200 && ((_wram_protect & 0xa0) == 0x20)))
		rc = 0;
	else
		rc = board->mapper_ram.data[addr];

	return rc;
}

static CPU_WRITE_HANDLER(mmc6_wram_write_handler)
{
	struct board *board = emu->board;

	addr &= 0x3ff;

	if ((addr < 0x200 && ((_wram_protect & 0x30) == 0x30)) ||
	    (addr >= 0x200 && ((_wram_protect & 0xc0) == 0xc0)))
		board->mapper_ram.data[addr] = value;
}

