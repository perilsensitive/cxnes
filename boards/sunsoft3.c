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

#define irq_counter_load_toggle board->data[0]

static void sunsoft3_reset(struct board *board, int);
static CPU_WRITE_HANDLER(sunsoft3_write_handler);

static struct board_funcs sunsoft3_funcs = {
	.reset = sunsoft3_reset,
};

static struct board_write_handler sunsoft3_write_handlers[] = {
	{sunsoft3_write_handler, 0x8000, SIZE_32K, 0},
	{standard_mirroring_handler, 0xe800, SIZE_4K, 0},
	{NULL},
};

struct board_info board_sunsoft3 = {
	.board_type = BOARD_TYPE_SUNSOFT3,
	.name = "SUNSOFT-3",
	.mapper_name = "SUNSOFT-3",
	.funcs = &sunsoft3_funcs,
	.init_prg = std_prg_16k,
	.init_chr0 = std_chr_2k,
	.write_handlers = sunsoft3_write_handlers,
	.max_prg_rom_size = SIZE_256K,
	.max_chr_rom_size = SIZE_256K,
	.max_wram_size = {SIZE_8K, 0},
	.flags = BOARD_INFO_FLAG_MIRROR_M |
	         BOARD_INFO_FLAG_M2_TIMER,
	.mirroring_values = std_mirroring_vh01,
};

static CPU_WRITE_HANDLER(sunsoft3_write_handler)
{
	struct board *board = emu->board;

	switch (addr & 0xf800) {
	case 0x8800:
	case 0x9800:
	case 0xa800:
	case 0xb800:
		update_chr0_bank(board, ((addr >> 12) & 3), value);
		break;
	case 0xc800:
		if (irq_counter_load_toggle) {
			m2_timer_set_counter_lo(emu->m2_timer, value,
						cycles);
		} else {
			m2_timer_set_counter_hi(emu->m2_timer, value,
						cycles);
		}
		irq_counter_load_toggle ^= 1;
		break;
	case 0xd800:
		value &= 0x10;
		m2_timer_ack(emu->m2_timer, cycles);
		m2_timer_set_counter_enabled(emu->m2_timer, value, cycles);
		m2_timer_set_irq_enabled(emu->m2_timer, value, cycles);
		irq_counter_load_toggle = 0;
		break;
	case 0xf800:
		update_prg_bank(board, 1, value);
		break;
	}
}

static void sunsoft3_reset(struct board *board, int hard)
{
	if (hard) {
		m2_timer_set_flags(board->emu->m2_timer,
				   M2_TIMER_FLAG_IRQ_ON_RELOAD, 0);
		m2_timer_set_counter_enabled(board->emu->m2_timer, 0, 0);
		m2_timer_set_irq_enabled(board->emu->m2_timer, 0, 0);
		irq_counter_load_toggle = 0;
	}
}
