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
#include "m2_timer.h"

static CPU_WRITE_HANDLER(btl_smb2c_write_handler);
static void btl_smb2c_reset(struct board *board, int);

static struct bank btl_smb2c_init_prg[] = {
	{16, 0, SIZE_4K, 0x5000, MAP_PERM_READ, MAP_TYPE_ROM},
	{ 2, 0, SIZE_8K, 0x6000, MAP_PERM_READ, MAP_TYPE_ROM},
	{ 1, 0, SIZE_8K, 0x8000, MAP_PERM_READ, MAP_TYPE_ROM},
	{ 0, 0, SIZE_8K, 0xa000, MAP_PERM_READ, MAP_TYPE_ROM},
	{ 0, 0, SIZE_8K, 0xc000, MAP_PERM_READ, MAP_TYPE_ROM},
	{ 9, 0, SIZE_8K, 0xe000, MAP_PERM_READ, MAP_TYPE_ROM},
	{.type = MAP_TYPE_END},
};

static struct board_funcs btl_smb2c_funcs = {
	.reset = btl_smb2c_reset,
};

static struct board_write_handler btl_smb2c_write_handlers[] = {
	{btl_smb2c_write_handler, 0x4020, 0x6000 - 0x4020, 0},
	{btl_smb2c_write_handler, 0x8122, 1, 0},
	{NULL},
};

struct board_info board_btl_smb2c = {
	.board_type = BOARD_TYPE_BTL_SMB2C,
	.name = "BTL-SMB2C",
	.funcs = &btl_smb2c_funcs,
	.init_prg = btl_smb2c_init_prg,
	.init_chr0 = std_chr_8k,
	.write_handlers = btl_smb2c_write_handlers,
	.max_prg_rom_size = SIZE_64K + SIZE_16K,
	.max_chr_rom_size = SIZE_8K,
	.flags = BOARD_INFO_FLAG_M2_TIMER,
};

static CPU_WRITE_HANDLER(btl_smb2c_write_handler)
{
	struct board *board = emu->board;
	int bank_translate[] = { 4, 3, 5, 3, 6, 3, 7, 3 };

	addr &= 0x4122;

	switch (addr) {
	case 0x4022:
		update_prg_bank(board, 4, bank_translate[value & 0x07]);
		break;
	case 0x4120:
		update_prg_bank(board, 1, (value & 0x01) ? 0 : 2);
		update_prg_bank(board, 5, (value & 0x01) ? 8 : 9);
		break;
	case 0x8122:
	case 0x4122:
		if (value & 0x03) {
			m2_timer_set_enabled(emu->m2_timer, 1, cycles);
			m2_timer_set_counter(emu->m2_timer, 4096, cycles);
		} else {
			m2_timer_set_enabled(emu->m2_timer, 0, cycles);
		}
		break;
	}
}

static void btl_smb2c_reset(struct board *board, int hard)
{
	if (hard) {
		m2_timer_set_flags(board->emu->m2_timer,
		                   M2_TIMER_FLAG_ONE_SHOT |
 		                   M2_TIMER_FLAG_AUTO_IRQ_DISABLE, 0);
		m2_timer_set_enabled(board->emu->m2_timer, 0, 0);
	}

}
