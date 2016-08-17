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

#define _cart_mode board->data[0]

static CPU_WRITE_HANDLER(btl_smb2b_write_handler);
static void btl_smb2b_reset(struct board *board, int);

static struct bank btl_smb2b_init_prg[] = {
	{0x0f, 0, SIZE_8K, 0x6000, MAP_PERM_READ, MAP_TYPE_ROM},
	{8, 0, SIZE_8K, 0x8000, MAP_PERM_READ, MAP_TYPE_ROM},
	{9, 0, SIZE_8K, 0xa000, MAP_PERM_READ, MAP_TYPE_ROM},
	{0, 0, SIZE_8K, 0xc000, MAP_PERM_READ, MAP_TYPE_ROM},
	{0x0b, 0, SIZE_8K, 0xe000, MAP_PERM_READ, MAP_TYPE_ROM},
	{.type = MAP_TYPE_END},
};

static struct board_funcs btl_smb2b_funcs = {
	.reset = btl_smb2b_reset,
};

static struct board_write_handler btl_smb2b_write_handlers[] = {
	{btl_smb2b_write_handler, 0x4020, 0x6000 - 0x4020, 0},
	{NULL},
};

struct board_info board_btl_smb2b = {
	.board_type = BOARD_TYPE_BTL_SMB2B,
	.name = "BTL-SMB2B",
	.funcs = &btl_smb2b_funcs,
	.init_prg = btl_smb2b_init_prg,
	.init_chr0 = std_chr_8k,
	.write_handlers = btl_smb2b_write_handlers,
	.max_prg_rom_size = SIZE_64K,
	.max_chr_rom_size = SIZE_8K,
	.flags = BOARD_INFO_FLAG_M2_TIMER,
};

static CPU_WRITE_HANDLER(btl_smb2b_write_handler)
{
	struct board *board = emu->board;
	int bank;

	addr &= 0x4120;

	switch (addr) {
	case 0x4020:
		bank  = (value & 0x06) >> 1;
		bank |= (value & 0x01) << 2;
		bank |= (value & 0x08);
		update_prg_bank(board, 3, bank);
		break;
	case 0x4120:
		if (value & 0x01) {
			m2_timer_set_enabled(emu->m2_timer, 1, cycles);
			m2_timer_set_counter(emu->m2_timer, 4096, cycles);
		} else {
			m2_timer_set_enabled(emu->m2_timer, 0, cycles);
		}
		break;
	}
}

static void btl_smb2b_reset(struct board *board, int hard)
{
	if (hard) {
		m2_timer_set_flags(board->emu->m2_timer,
		                   M2_TIMER_FLAG_ONE_SHOT |
 		                   M2_TIMER_FLAG_AUTO_IRQ_DISABLE, 0);
		m2_timer_set_enabled(board->emu->m2_timer, 0, 0);
	}

}
