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

static CPU_WRITE_HANDLER(btl_smb2a_write_handler);
static void btl_smb2a_reset(struct board *board, int);

static struct bank btl_smb2a_init_prg[] = {
	{6, 0, SIZE_8K, 0x6000, MAP_PERM_READ, MAP_TYPE_ROM},
	{4, 0, SIZE_8K, 0x8000, MAP_PERM_READ, MAP_TYPE_ROM},
	{5, 0, SIZE_8K, 0xa000, MAP_PERM_READ, MAP_TYPE_ROM},
	{0, 0, SIZE_8K, 0xc000, MAP_PERM_READ, MAP_TYPE_ROM},
	{7, 0, SIZE_8K, 0xe000, MAP_PERM_READ, MAP_TYPE_ROM},
	{.type = MAP_TYPE_END},
};

static struct board_funcs btl_smb2a_funcs = {
	.reset = btl_smb2a_reset,
};

static struct board_write_handler btl_smb2a_write_handlers[] = {
	{btl_smb2a_write_handler, 0x8000, SIZE_32K, 0},
	{NULL},
};

struct board_info board_btl_smb2a = {
	.board_type = BOARD_TYPE_BTL_SMB2A,
	.name = "BTL-SMB2A",
	.funcs = &btl_smb2a_funcs,
	.init_prg = btl_smb2a_init_prg,
	.init_chr0 = std_chr_8k,
	.write_handlers = btl_smb2a_write_handlers,
	.max_prg_rom_size = SIZE_64K,
	.max_chr_rom_size = SIZE_8K,
	.flags = BOARD_INFO_FLAG_M2_TIMER,
};

static CPU_WRITE_HANDLER(btl_smb2a_write_handler)
{
	struct board *board = emu->board;

	addr &= 0xe000;

	switch (addr) {
	case 0x8000:
		m2_timer_ack(emu->m2_timer, cycles);
		m2_timer_set_counter_enabled(emu->m2_timer, 0, cycles);
		m2_timer_set_irq_enabled(emu->m2_timer, 0, cycles);
		break;
	case 0xa000:
		m2_timer_set_counter_enabled(emu->m2_timer, 1, cycles);
		m2_timer_set_irq_enabled(emu->m2_timer, 1, cycles);
		m2_timer_set_counter(emu->m2_timer, 4096, cycles);
		break;
	case 0xe000:
		update_prg_bank(board, 3, value);
		break;
	}
}

static void btl_smb2a_reset(struct board *board, int hard)
{
	if (hard) {
		m2_timer_set_flags(board->emu->m2_timer,
		                   M2_TIMER_FLAG_ONE_SHOT |
 		                   M2_TIMER_FLAG_AUTO_IRQ_DISABLE, 0);
		m2_timer_set_enabled(board->emu->m2_timer, 0, 0);
	}

}
