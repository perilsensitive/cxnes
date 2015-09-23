/*
  cxNES - NES/Famicom Emulator
  Copyright (C) 2011-2015 Ryan Jackson

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

#define _temp_counter_latch board->timestamps[0]

static CPU_WRITE_HANDLER(vrc3_write_handler);

static void vrc3_reset(struct board *board, int);

static struct board_funcs vrc3_funcs = {
	.reset = vrc3_reset,
};

static struct board_write_handler vrc3_write_handlers[] = {
	{vrc3_write_handler, 0x8000, SIZE_32K, 0},
	{NULL},
};

struct board_info board_vrc3 = {
	.board_type = BOARD_TYPE_VRC3,
	.name = "KONAMI-VRC-3",
	.funcs = &vrc3_funcs,
	.init_prg = std_prg_16k,
	.init_chr0 = std_chr_8k,
	.write_handlers = vrc3_write_handlers,
	.max_prg_rom_size = SIZE_128K,
	.max_chr_rom_size = SIZE_8K,
	.max_wram_size = {SIZE_8K, 0},
	.flags = BOARD_INFO_FLAG_M2_TIMER,
};

static void vrc3_reset(struct board *board, int hard)
{
	if (hard) {
		board->prg_mode = 0;
		board->prg_and = 0xff;
		board->chr_and = 0xff;

		m2_timer_set_flags(board->emu->m2_timer,
				   M2_TIMER_FLAG_COUNT_UP|
				   M2_TIMER_FLAG_RELOAD|
				   M2_TIMER_FLAG_IRQ_ON_RELOAD, 0);
		m2_timer_set_size(board->emu->m2_timer, 16, 0);
		m2_timer_set_enabled(board->emu->m2_timer, 0, 0);
	}
}

static CPU_WRITE_HANDLER(vrc3_write_handler)
{
	struct board *board = emu->board;

	switch (addr & 0xf000) {
	case 0x8000:
		_temp_counter_latch &= 0xfff0;
		_temp_counter_latch |= (value & 0xf);
		m2_timer_set_reload(emu->m2_timer, _temp_counter_latch,
				    cycles);
		break;
	case 0x9000:
		_temp_counter_latch &= 0xff0f;
		_temp_counter_latch |= (value & 0xf) << 4;
		m2_timer_set_reload(emu->m2_timer, _temp_counter_latch,
				    cycles);
		break;
	case 0xa000:
		_temp_counter_latch &= 0xf0ff;
		_temp_counter_latch |= (value & 0xf) << 8;
		m2_timer_set_reload(emu->m2_timer, _temp_counter_latch,
				    cycles);
		break;
	case 0xb000:
		_temp_counter_latch &= 0x0fff;
		_temp_counter_latch |= (value & 0xf) << 12;
		m2_timer_set_reload(emu->m2_timer, _temp_counter_latch,
				    cycles);
		break;
	case 0xc000:
		emu->board->irq_control = value;
		m2_timer_set_enabled(emu->m2_timer, value & 0x02, cycles);
		m2_timer_set_size(emu->m2_timer, (value & 0x04) ? 8 : 16,
				  cycles);
		m2_timer_force_reload(emu->m2_timer, cycles);
		break;
	case 0xd000:		/* FIXME 0xe000? */
		m2_timer_ack(emu->m2_timer, cycles);
		if (emu->board->irq_control & 0x01) {
			m2_timer_set_enabled(emu->m2_timer, 1, cycles);
			m2_timer_schedule_irq(emu->m2_timer, cycles);
		} else {
			m2_timer_set_enabled(emu->m2_timer, 0, cycles);
		}
		break;
	case 0xf000:
		board->prg_banks[1].bank = value;
		board_prg_sync(board);
		break;
	}
}
