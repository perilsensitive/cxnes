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
#include "a12_timer.h"

static CPU_WRITE_HANDLER(mortalkombat2_write_handler);
static int mortalkombat2_init(struct board *board);
static void mortalkombat2_reset(struct board *board, int hard);
static void mortalkombat2_cleanup(struct board *board);
static int mortalkombat2_load_state(struct board *board, struct save_state *state);
static int mortalkombat2_save_state(struct board *board, struct save_state *state);
static void mortalkombat2_end_frame(struct board *board, uint32_t cycles);

static struct board_write_handler mortalkombat2_write_handlers[] = {
	{mortalkombat2_write_handler, 0x6000, SIZE_8K, 0},
	{NULL}
};

static struct board_funcs mortalkombat2_funcs = {
	.init = mortalkombat2_init,
	.reset = mortalkombat2_reset,
	.cleanup = mortalkombat2_cleanup,
	.load_state = mortalkombat2_load_state,
	.save_state = mortalkombat2_save_state,
	.end_frame = mortalkombat2_end_frame,
};

struct board_info board_unl_mortalkombat2 = {
	.board_type = BOARD_TYPE_UNL_MORTALKOMBAT2,
	.name = "UNL MK2/SF3/SMKR",
	.funcs = &mortalkombat2_funcs,
	.init_prg = std_prg_8k,
	.init_chr0 = std_chr_2k,
	.write_handlers = mortalkombat2_write_handlers,
	.max_prg_rom_size = SIZE_512K,
	.max_chr_rom_size = SIZE_512K,
};

static int mortalkombat2_init(struct board *board)
{
	struct emu *emu;

	emu = board->emu;
	if (a12_timer_init(emu, A12_TIMER_VARIANT_MMC3_STD))
		return 1;

	return 0;
}

static void mortalkombat2_reset(struct board *board, int hard)
{
	a12_timer_reset(board->emu->a12_timer, hard);
}

static void mortalkombat2_cleanup(struct board *board)
{
	a12_timer_cleanup(board->emu);
}

static int mortalkombat2_load_state(struct board *board, struct save_state *state)
{
	return a12_timer_load_state(board->emu, state);
}

static int mortalkombat2_save_state(struct board *board, struct save_state *state)
{
	return a12_timer_save_state(board->emu, state);
}

static void mortalkombat2_end_frame(struct board *board, uint32_t cycles)
{
	a12_timer_end_frame(board->emu->a12_timer, cycles);
}

static CPU_WRITE_HANDLER(mortalkombat2_write_handler)
{
	struct board *board;

	board = emu->board;

	addr &= 0x7003;

	switch (addr) {
	case 0x6000:
	case 0x6001:
	case 0x6002:
	case 0x6003:
		update_chr0_bank(board, addr & 0x03, value);
		break;
	case 0x7000:
	case 0x7001:
		update_prg_bank(board, (addr & 0x01) + 1, value);
		break;
	case 0x7002:
		a12_timer_set_irq_enabled(emu->a12_timer, 0, cycles);
		break;
	case 0x7003:
		a12_timer_set_reload(emu->a12_timer, 0x07, cycles);
		a12_timer_force_reload(emu->a12_timer, cycles);
		a12_timer_set_irq_enabled(emu->a12_timer, 1, cycles);
		break;
	}
}
