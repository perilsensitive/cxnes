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

static CPU_WRITE_HANDLER(unl_smb2j_write_handler);
static CPU_READ_HANDLER(unl_smb2j_read_handler);
static void unl_smb2j_reset(struct board *board, int);

static struct bank unl_smb2j_init_prg[] = {
	{ 4, 0, SIZE_8K,  0x6000, MAP_PERM_READ, MAP_TYPE_ROM},
	{ 0, 0, SIZE_32K, 0x8000, MAP_PERM_READ, MAP_TYPE_ROM},
	{.type = MAP_TYPE_END},
};

static struct board_funcs unl_smb2j_funcs = {
	.reset = unl_smb2j_reset,
};

static struct board_write_handler unl_smb2j_write_handlers[] = {
	{unl_smb2j_write_handler, 0x4027, 1, 0},
	{unl_smb2j_write_handler, 0x4068, 1, 0},
	{NULL},
};

static struct board_read_handler unl_smb2j_read_handlers[] = {
	{unl_smb2j_read_handler, 0x4042, 0x4055 - 0x4042 + 1, 0},
	{NULL},
};

struct board_info board_unl_smb2j = {
	.board_type = BOARD_TYPE_UNL_SMB2J,
	.name = "UNL-SMB2J",
	.funcs = &unl_smb2j_funcs,
	.init_prg = unl_smb2j_init_prg,
	.init_chr0 = std_chr_8k,
	.write_handlers = unl_smb2j_write_handlers,
	.read_handlers = unl_smb2j_read_handlers,
	.max_prg_rom_size = SIZE_64K,
	.max_chr_rom_size = SIZE_8K,
	.flags = BOARD_INFO_FLAG_M2_TIMER,
};

static CPU_READ_HANDLER(unl_smb2j_read_handler)
{
	return 0xff;
}

static CPU_WRITE_HANDLER(unl_smb2j_write_handler)
{
	struct board *board = emu->board;

	switch (addr) {
	case 0x4068:
		value &= 0x01;
		m2_timer_ack(emu->m2_timer, cycles);
		if (value) {
			m2_timer_set_counter_enabled(emu->m2_timer, 1, cycles);
			m2_timer_set_irq_enabled(emu->m2_timer, 1, cycles);
			m2_timer_set_counter(emu->m2_timer, 5750, cycles);
		} else {
			m2_timer_ack(emu->m2_timer, cycles);
			m2_timer_set_counter_enabled(emu->m2_timer, 0, cycles);
			m2_timer_set_irq_enabled(emu->m2_timer, 0, cycles);
		}
		break;
	case 0x4027:
		update_prg_bank(board, 0, 4 + (value & 0x01));
		break;
	}
}

static void unl_smb2j_reset(struct board *board, int hard)
{
	if (hard) {
		m2_timer_set_flags(board->emu->m2_timer,
		                   M2_TIMER_FLAG_ONE_SHOT |
 		                   M2_TIMER_FLAG_AUTO_IRQ_DISABLE, 0);
		m2_timer_set_enabled(board->emu->m2_timer, 0, 0);
	}
}
