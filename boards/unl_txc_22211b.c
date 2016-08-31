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

#define _regs (board->data + 0)

static CPU_WRITE_HANDLER(txc_22211b_write_handler);
static CPU_READ_HANDLER(txc_22211b_read_handler);

static struct bank unl_22211b_init_prg[] = {
	{ 0, 0, SIZE_8K, 0x6000, MAP_PERM_READ, MAP_TYPE_ROM},
	{-1, 0, SIZE_32K, 0x8000, MAP_PERM_READ, MAP_TYPE_ROM},
	{.type = MAP_TYPE_END},
};

static struct board_write_handler unl_22211b_write_handlers[] = {
	{txc_22211b_write_handler, 0x4100, 4, 0},
	{txc_22211b_write_handler, 0x8000, SIZE_32K, 0},
	{NULL},
};

static struct board_read_handler unl_22211b_read_handlers[] = {
	{txc_22211b_read_handler, 0x4100, 1, 0},
	{NULL},
};

struct board_info board_unl_txc_22211b = {
	.board_type = BOARD_TYPE_UNL_TXC_22211B,
	.name = "UNL-22211-B",
	.init_prg = unl_22211b_init_prg,
	.init_chr0 = std_chr_8k,
	.write_handlers = unl_22211b_write_handlers,
	.read_handlers = unl_22211b_read_handlers,
	.max_prg_rom_size = SIZE_128K,
	.max_wram_size = {SIZE_8K, 0},
	.max_chr_rom_size = SIZE_128K,
};

static CPU_READ_HANDLER(txc_22211b_read_handler)
{
	struct board *board = emu->board;
	return (_regs[1] ^ _regs[2]) | 0x40;
}

static CPU_WRITE_HANDLER(txc_22211b_write_handler)
{
	struct board *board = emu->board;
	int chr_value;

	if (addr > 0x8000)
		addr = 0x8000;

	switch (addr) {
	case 0x4100:
	case 0x4101:
	case 0x4102:
	case 0x4103:
		_regs[addr & 0x03] = value;
		break;
	case 0x8000:
		chr_value  = ((value ^ _regs[2]) >> 3) & 0x02;
		chr_value |= ((value ^ _regs[2]) >> 5) & 0x01;
		update_chr0_bank(board, 0, chr_value);
		update_prg_bank(board, 1, _regs[2] >> 2);
		break;
	}
}
