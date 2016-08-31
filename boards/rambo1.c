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

static int rambo1_init(struct board *board);
static void rambo1_reset(struct board *board, int hard);

static CPU_WRITE_HANDLER(rambo1_bank_select);
static CPU_WRITE_HANDLER(rambo1_bank_data);
static CPU_WRITE_HANDLER(rambo1_irq_latch);
static CPU_WRITE_HANDLER(rambo1_irq_reload);
static CPU_WRITE_HANDLER(rambo1_irq_disable);
static CPU_WRITE_HANDLER(rambo1_irq_enable);

static struct board_funcs rambo1_funcs = {
	.init = rambo1_init,
	.reset = rambo1_reset,
	.cleanup = mmc3_cleanup,
	.end_frame = mmc3_end_frame,
	.load_state = mmc3_load_state,
	.save_state = mmc3_save_state,
};

static struct board_write_handler rambo1_write_handlers[] = {
	{rambo1_bank_select, 0x8000, SIZE_8K, 0x8001},
	{rambo1_bank_data, 0x8001, SIZE_8K, 0x8001},
	{standard_mirroring_handler, 0xa000, SIZE_8K, 0xa001},
	{mmc3_wram_protect, 0xa001, SIZE_8K, 0xa001},
	{rambo1_irq_latch, 0xc000, SIZE_8K, 0xc001},
	{rambo1_irq_reload, 0xc001, SIZE_8K, 0xc001},
	{rambo1_irq_disable, 0xe000, SIZE_8K, 0xe001},
	{rambo1_irq_enable, 0xe001, SIZE_8K, 0xe001},
	{NULL}
};

static struct board_write_handler tengen800037_write_handlers[] = {
	{rambo1_bank_select, 0x8000, SIZE_8K, 0x8001},
	{rambo1_bank_data, 0x8001, SIZE_8K, 0x8001},
	{standard_mirroring_handler, 0xa000, SIZE_8K, 0xa001},
	{mmc3_wram_protect, 0xa001, SIZE_8K, 0xa001},
	{rambo1_irq_latch, 0xc000, SIZE_8K, 0xc001},
	{rambo1_irq_reload, 0xc001, SIZE_8K, 0xc001},
	{rambo1_irq_disable, 0xe000, SIZE_8K, 0xe001},
	{rambo1_irq_enable, 0xe001, SIZE_8K, 0xe001},
	{NULL},
};

struct board_info board_tengen800032 = {
	.board_type = BOARD_TYPE_TENGEN_800032,
	.name = "TENGEN-800032",
	.mapper_name = "RAMBO-1",
	.funcs = &rambo1_funcs,
	.init_prg = mmc3_init_prg,
	.init_chr0 = mmc3_init_chr0,
	.write_handlers = rambo1_write_handlers,
	.max_prg_rom_size = SIZE_512K,
	.max_chr_rom_size = SIZE_256K,
	.max_wram_size = {SIZE_8K, 0},
	.flags = BOARD_INFO_FLAG_MIRROR_M |
	         BOARD_INFO_FLAG_M2_TIMER,
	.mirroring_values = std_mirroring_vh,
};

struct board_info board_tengen800037 = {
	.board_type = BOARD_TYPE_TENGEN_800037,
	.name = "TENGEN-800037",
	.mapper_name = "RAMBO-1",
	.funcs = &rambo1_funcs,
	.init_prg = mmc3_init_prg,
	.init_chr0 = mmc3_init_chr0,
	.write_handlers = tengen800037_write_handlers,
	.max_prg_rom_size = SIZE_512K,
	.max_chr_rom_size = SIZE_256K,
	.max_wram_size = {SIZE_8K, 0},
	.flags = BOARD_INFO_FLAG_MIRROR_M |
	         BOARD_INFO_FLAG_M2_TIMER,
	.mirroring_values = std_mirroring_vh,
};

static int rambo1_init(struct board *board)
{
	if (a12_timer_init(board->emu, A12_TIMER_VARIANT_RAMBO1))
		return 1;

	return 0;
}

static void rambo1_reset(struct board *board, int hard)
{
	mmc3_reset(board, hard);

	if (hard) {
		m2_timer_set_enabled(board->emu->m2_timer, 0, 0);
		m2_timer_set_prescaler(board->emu->m2_timer, 3, 0);
		m2_timer_set_prescaler_reload(board->emu->m2_timer, 3, 0);
		m2_timer_set_irq_delay(board->emu->m2_timer, 2, 0);
		m2_timer_set_size(board->emu->m2_timer, 8, 0);
		m2_timer_set_flags(board->emu->m2_timer,
				   M2_TIMER_FLAG_RELOAD |
				   M2_TIMER_FLAG_DELAYED_RELOAD |
				   M2_TIMER_FLAG_PRESCALER |
				   M2_TIMER_FLAG_PRESCALER_RELOAD,
				   0);
	}
}

static CPU_WRITE_HANDLER(rambo1_bank_select)
{
	struct board *board;
	int old;

	board = emu->board;
	old = _bank_select & 0xc0;
	_bank_select = value;

	board->prg_mode = value & 0x40;
	board->chr_mode = value & 0xa0;

	if ((value & 0xa0) != (old & 0xa0)) {
		if (!(board->chr_mode & 0x20)) {
			board->chr_banks0[0].bank = _ext_regs[0] & 0xfe;
			board->chr_banks0[1].bank = _ext_regs[0] | 0x01;
			board->chr_banks0[2].bank = _ext_regs[1] & 0xfe;
			board->chr_banks0[3].bank = _ext_regs[1] | 0x01;
		} else {
			board->chr_banks0[0].bank = _ext_regs[0];
			board->chr_banks0[1].bank = _ext_regs[1];
			board->chr_banks0[2].bank = _ext_regs[2];
			board->chr_banks0[3].bank = _ext_regs[3];
		}

		if (!(board->chr_mode & 0x80)) {
			board->chr_banks0[0].address &= 0x0fff;
			board->chr_banks0[1].address &= 0x0fff;
			board->chr_banks0[2].address &= 0x0fff;
			board->chr_banks0[3].address &= 0x0fff;
			board->chr_banks0[4].address |= 0x1000;
			board->chr_banks0[5].address |= 0x1000;
			board->chr_banks0[6].address |= 0x1000;
			board->chr_banks0[7].address |= 0x1000;
		} else {
			board->chr_banks0[0].address |= 0x1000;
			board->chr_banks0[1].address |= 0x1000;
			board->chr_banks0[2].address |= 0x1000;
			board->chr_banks0[3].address |= 0x1000;
			board->chr_banks0[4].address &= 0x0fff;
			board->chr_banks0[5].address &= 0x0fff;
			board->chr_banks0[6].address &= 0x0fff;
			board->chr_banks0[7].address &= 0x0fff;
		}

	    	if (board->info->board_type == BOARD_TYPE_TENGEN_800037)
			mmc3_txsrom_mirroring(board);

		board_chr_sync(board, 0);
	}

	if ((value & 0x40) != (old & 0x40)) {
		if (!board->prg_mode) {
			board->prg_banks[1].address = 0x8000;
			board->prg_banks[2].address = 0xa000;
			board->prg_banks[3].address = 0xc000;
		} else {
			board->prg_banks[1].address = 0xa000;
			board->prg_banks[2].address = 0xc000;
			board->prg_banks[3].address = 0x8000;
		}

		board_prg_sync(board);
	}
}

static CPU_WRITE_HANDLER(rambo1_bank_data)
{
	struct board *board;
	int bank;

	board = emu->board;

	bank = _bank_select & 0x0f;

	switch (bank) {
	case 2:
	case 3:
	case 4:
	case 5:
	case 6:
	case 7:
		mmc3_bank_data(emu, addr, value, cycles);
		break;
	case 0:
	case 1:
	case 8:
	case 9:
		bank = ((bank & 1) << 1) | ((bank & 0x08) >> 3);
		_ext_regs[bank] = value;
		board_chr_sync(board, 0);
		break;
	case 15:
		update_prg_bank(board, 3, value);
		break;
	}

	if ((bank < 6) &&
	    (board->info->board_type == BOARD_TYPE_TENGEN_800037)) {
		mmc3_txsrom_mirroring(board);
	}
}

static CPU_WRITE_HANDLER(rambo1_irq_latch)
{
	emu->board->irq_counter_reload = value;
	m2_timer_set_reload(emu->m2_timer, value, cycles);
	a12_timer_set_reload(emu->a12_timer, value, cycles);
}

static CPU_WRITE_HANDLER(rambo1_irq_reload)
{
	struct board *board;
	int delay;

	board = emu->board;
	
	if (board->irq_control != (value & 1)) {
		board->irq_control = value & 1;
		m2_timer_set_counter_enabled(emu->m2_timer, value & 0x01, cycles);
		a12_timer_set_counter_enabled(emu->a12_timer, !(value & 0x01), cycles);
	}

	if (board->irq_counter_reload == 0)
		delay = 0;
	else
		delay = 1;

	if (board->irq_control) {
		m2_timer_force_reload(emu->m2_timer, cycles);
		m2_timer_set_prescaler(emu->m2_timer, 3, cycles);
		m2_timer_set_force_reload_delay(emu->m2_timer, delay ? 2 : 0, cycles);
	} else {
		//a12_timer_set_force_reload_delay(emu->a12_timer, delay, cycles);
		a12_timer_force_reload(emu->a12_timer, cycles);
	}
}

static CPU_WRITE_HANDLER(rambo1_irq_disable)
{
	struct board *board;

	board = emu->board;
	
	if (board->irq_control)
		m2_timer_set_irq_enabled(emu->m2_timer, 0, cycles);
	else
		a12_timer_set_irq_enabled(emu->a12_timer, 0, cycles);
}

static CPU_WRITE_HANDLER(rambo1_irq_enable)
{
	struct board *board;

	board = emu->board;

	if (board->irq_control)
		m2_timer_set_irq_enabled(emu->m2_timer, 1, cycles);
	else
		a12_timer_set_irq_enabled(emu->a12_timer, 1, cycles);
}
