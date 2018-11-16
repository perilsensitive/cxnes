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

#define _security  board->data[1]
#define _temp_irq_latch board->data[2]

static CPU_READ_HANDLER(vrc2_read_handler);
static CPU_READ_HANDLER(vrc2_read_handler);
static CPU_WRITE_HANDLER(vrc4_prg_handler);
static CPU_WRITE_HANDLER(vrc4_prg_mode_handler);
static CPU_WRITE_HANDLER(vrc4_chr_even_lo_handler);
static CPU_WRITE_HANDLER(vrc4_chr_even_hi_handler);
static CPU_WRITE_HANDLER(vrc4_chr_odd_lo_handler);
static CPU_WRITE_HANDLER(vrc4_chr_odd_hi_handler);
static int vrc4_init(struct board *board);
static void vrc4_reset(struct board *board, int);

static struct board_funcs vrc4_funcs = {
	.init = vrc4_init,
	.reset = vrc4_reset,
};

static struct board_funcs vrc2_funcs = {
	.init = vrc4_init,
};

struct bank vrc2a_init_chr[] = {
	{0, 1, SIZE_1K, 0x0000, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{0, 1, SIZE_1K, 0x0400, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{0, 1, SIZE_1K, 0x0800, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{0, 1, SIZE_1K, 0x0c00, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{0, 1, SIZE_1K, 0x1000, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{0, 1, SIZE_1K, 0x1400, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{0, 1, SIZE_1K, 0x1800, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{0, 1, SIZE_1K, 0x1c00, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{.type = MAP_TYPE_END},
};

struct board_info board_vrc2a = {
	.board_type = BOARD_TYPE_VRC2A,
	.name = "KONAMI-VRC-2A",
	.funcs = &vrc2_funcs,
	.mirroring_values = std_mirroring_vh,
	.init_prg = std_prg_8k,
	.init_chr0 = vrc2a_init_chr,
	.max_prg_rom_size = SIZE_256K,
	.max_chr_rom_size = SIZE_128K,
	.max_wram_size = {SIZE_8K, 0},
	.flags = BOARD_INFO_FLAG_MIRROR_M,
};

struct board_info board_vrc2b = {
	.board_type = BOARD_TYPE_VRC2B,
	.name = "KONAMI-VRC-2B",
	.funcs = &vrc2_funcs,
	.mirroring_values = std_mirroring_vh,
	.init_prg = std_prg_8k,
	.init_chr0 = std_chr_1k,
	.max_prg_rom_size = SIZE_256K,
	.max_chr_rom_size = SIZE_256K,
	.max_wram_size = {0, 0},
	.flags = BOARD_INFO_FLAG_MIRROR_M,
};

struct board_info board_vrc2c = {
	.board_type = BOARD_TYPE_VRC2C,
	.name = "KONAMI-VRC-2C",
	.funcs = &vrc2_funcs,
	.mirroring_values = std_mirroring_vh,
	.init_prg = std_prg_8k,
	.init_chr0 = std_chr_1k,
	.max_prg_rom_size = SIZE_256K,
	.max_chr_rom_size = SIZE_256K,
	.max_wram_size = {SIZE_8K, 0},
	.flags = BOARD_INFO_FLAG_MIRROR_M,
};

struct board_info board_vrc4a = {
	.board_type = BOARD_TYPE_VRC4A,
	.name = "KONAMI-VRC-4A",
	.funcs = &vrc4_funcs,
	.mirroring_values = std_mirroring_vh01,
	.init_prg = std_prg_8k,
	.init_chr0 = std_chr_1k,
	.max_prg_rom_size = SIZE_256K,
	.max_chr_rom_size = SIZE_512K,
	.max_wram_size = {SIZE_8K, 0},
	.flags = BOARD_INFO_FLAG_MIRROR_M |
	         BOARD_INFO_FLAG_M2_TIMER,
};

struct board_info board_vrc4b = {
	.board_type = BOARD_TYPE_VRC4B,
	.name = "KONAMI-VRC-4B",
	.funcs = &vrc4_funcs,
	.mirroring_values = std_mirroring_vh01,
	.init_prg = std_prg_8k,
	.init_chr0 = std_chr_1k,
	.max_prg_rom_size = SIZE_256K,
	.max_chr_rom_size = SIZE_512K,
	.max_wram_size = {SIZE_4K, 0},
	.flags = BOARD_INFO_FLAG_MIRROR_M |
	         BOARD_INFO_FLAG_M2_TIMER,
};

struct board_info board_vrc4c = {
	.board_type = BOARD_TYPE_VRC4C,
	.name = "KONAMI-VRC-4C",
	.funcs = &vrc4_funcs,
	.mirroring_values = std_mirroring_vh01,
	.init_prg = std_prg_8k,
	.init_chr0 = std_chr_1k,
	.max_prg_rom_size = SIZE_256K,
	.max_chr_rom_size = SIZE_512K,
	.max_wram_size = {SIZE_8K, 0},
	.flags = BOARD_INFO_FLAG_MIRROR_M |
	         BOARD_INFO_FLAG_M2_TIMER,
};

struct board_info board_vrc4d = {
	.board_type = BOARD_TYPE_VRC4D,
	.name = "KONAMI-VRC-4D",
	.funcs = &vrc4_funcs,
	.mirroring_values = std_mirroring_vh01,
	.init_prg = std_prg_8k,
	.init_chr0 = std_chr_1k,
	.max_prg_rom_size = SIZE_256K,
	.max_chr_rom_size = SIZE_512K,
	.max_wram_size = {SIZE_4K, 0},
	.flags = BOARD_INFO_FLAG_MIRROR_M |
	         BOARD_INFO_FLAG_M2_TIMER,
};

struct board_info board_vrc4e = {
	.board_type = BOARD_TYPE_VRC4E,
	.name = "KONAMI-VRC-4E",
	.funcs = &vrc4_funcs,
	.mirroring_values = std_mirroring_vh01,
	.init_prg = std_prg_8k,
	.init_chr0 = std_chr_1k,
	.max_prg_rom_size = SIZE_256K,
	.max_chr_rom_size = SIZE_512K,
	.max_wram_size = {SIZE_4K, 0},
	.flags = BOARD_INFO_FLAG_MIRROR_M |
	         BOARD_INFO_FLAG_M2_TIMER,
};

struct board_info board_vrc4f = {
	.board_type = BOARD_TYPE_VRC4F,
	.name = "KONAMI-VRC-4F",
	.funcs = &vrc4_funcs,
	.mirroring_values = std_mirroring_vh01,
	.init_prg = std_prg_8k,
	.init_chr0 = std_chr_1k,
	.max_prg_rom_size = SIZE_256K,
	.max_chr_rom_size = SIZE_512K,
	.max_wram_size = {SIZE_4K, 0},
	.flags = BOARD_INFO_FLAG_MIRROR_M |
	         BOARD_INFO_FLAG_M2_TIMER,
};

struct board_info board_vrc4e_compat = {
	.board_type = BOARD_TYPE_VRC4E_COMPAT,
	.name = "KONAMI-VRC-4E/2B-COMPAT",
	.funcs = &vrc4_funcs,
	.mirroring_values = std_mirroring_vh01,
	.init_prg = std_prg_8k,
	.init_chr0 = std_chr_1k,
	.max_prg_rom_size = SIZE_256K,
	.max_chr_rom_size = SIZE_512K,
	.max_wram_size = {SIZE_8K, 0},
	.flags = BOARD_INFO_FLAG_MIRROR_M |
	         BOARD_INFO_FLAG_M2_TIMER,
};

struct board_info board_vrc4ac = {
	.board_type = BOARD_TYPE_VRC4AC,
	.name = "KONAMI-VRC-4A/4C-COMPAT",
	.funcs = &vrc4_funcs,
	.mirroring_values = std_mirroring_vh01,
	.init_prg = std_prg_8k,
	.init_chr0 = std_chr_1k,
	.max_prg_rom_size = SIZE_256K,
	.max_chr_rom_size = SIZE_512K,
	.max_wram_size = {SIZE_8K, 0},
	.flags = BOARD_INFO_FLAG_MIRROR_M |
	         BOARD_INFO_FLAG_M2_TIMER,
};

struct board_info board_vrc4bd = {
	.board_type = BOARD_TYPE_VRC4BD,
	.name = "KONAMI-VRC-4B/4D-COMPAT",
	.funcs = &vrc4_funcs,
	.mirroring_values = std_mirroring_vh01,
	.init_prg = std_prg_8k,
	.init_chr0 = std_chr_1k,
	.max_prg_rom_size = SIZE_256K,
	.max_chr_rom_size = SIZE_512K,
	.max_wram_size = {SIZE_8K, 0},
	.flags = BOARD_INFO_FLAG_MIRROR_M |
	         BOARD_INFO_FLAG_M2_TIMER,
};

static CPU_READ_HANDLER(vrc2_read_handler)
{
	struct board *board = emu->board;

	/* Should onle be here when reading from
	   0x6000 - 0x6fff if no prg ram */
	return (value & 0xfe) | _security;
}

static CPU_WRITE_HANDLER(vrc2_write_handler)
{
	struct board *board = emu->board;

	_security = value & 0x01;
}

static CPU_WRITE_HANDLER(vrc4_irq_latch_lo)
{
	struct board *board;

	board = emu->board;
	_temp_irq_latch &= 0xf0;
	_temp_irq_latch |= value & 0x0f;

	m2_timer_set_reload(emu->m2_timer, _temp_irq_latch, cycles);
}

static CPU_WRITE_HANDLER(vrc4_irq_latch_hi)
{
	struct board *board;

	board = emu->board;
	_temp_irq_latch &= 0x0f;
	_temp_irq_latch |= (value & 0x0f) << 4;

	m2_timer_set_reload(emu->m2_timer, _temp_irq_latch, cycles);
}

static CPU_WRITE_HANDLER(vrc4_irq_acknowledge)
{
	m2_timer_ack(emu->m2_timer, cycles);
	if (emu->board->irq_control & 0x01) {
		m2_timer_set_enabled(emu->m2_timer, 1, cycles);
		m2_timer_schedule_irq(emu->m2_timer, cycles);
	} else {
		m2_timer_set_enabled(emu->m2_timer, 0, cycles);
	}
}

static CPU_WRITE_HANDLER(vrc4_irq_control)
{
	int timer_flags;

	emu->board->irq_control = value;

	m2_timer_ack(emu->m2_timer, cycles);

	timer_flags = M2_TIMER_FLAG_COUNT_UP |
		M2_TIMER_FLAG_RELOAD |
		M2_TIMER_FLAG_IRQ_ON_RELOAD;

	if (!(value & 0x02)) {
		m2_timer_set_enabled(emu->m2_timer, 0, cycles);
		m2_timer_set_flags(emu->m2_timer, timer_flags, cycles);
		return;
	}

	if (!(value & 0x04)) {
		m2_timer_set_prescaler_reload(emu->m2_timer, 340, cycles);
		m2_timer_set_prescaler(emu->m2_timer, 340, cycles);
		m2_timer_set_prescaler_decrement(emu->m2_timer, 3, cycles);
		timer_flags |= M2_TIMER_FLAG_PRESCALER |
			M2_TIMER_FLAG_PRESCALER_RELOAD;
	}

	m2_timer_set_flags(emu->m2_timer, timer_flags, cycles);
	m2_timer_force_reload(emu->m2_timer, cycles);
	m2_timer_set_enabled(emu->m2_timer, value & 0x02, cycles);
	
}

static CPU_WRITE_HANDLER(vrc4_prg_handler)
{
	struct board *board = emu->board;

	switch (addr & 0xf000) {
	case 0x8000:
		board->prg_banks[1].bank = value & 0x1f;
		board_prg_sync(board);
		break;
	case 0xa000:
		board->prg_banks[2].bank = value & 0x1f;
		board_prg_sync(board);
		break;
	}
}

static CPU_WRITE_HANDLER(vrc4_prg_mode_handler)
{
	struct board *board = emu->board;

	value &= 0x02;
	if (value != board->prg_mode) {
		board->prg_mode = value;

		if (board->prg_mode) {
			board->prg_banks[1].address = 0xc000;
			board->prg_banks[3].address = 0x8000;
		} else {
			board->prg_banks[1].address = 0x8000;
			board->prg_banks[3].address = 0xc000;
		}
		board_prg_sync(board);
	}
}

static CPU_WRITE_HANDLER(vrc4_chr_even_lo_handler)
{
	int slot;
	int new_bank;

	struct board *board = emu->board;

	slot = (((addr & 0xf000) >> 12) - 0x0b) * 2;

	new_bank = board->chr_banks0[slot].bank & 0x1f0;
	new_bank |= (value & 0x0f);

	if (new_bank != board->chr_banks0[slot].bank) {
		board->chr_banks0[slot].bank = new_bank;
		board_chr_sync(board, 0);
	}
}

static CPU_WRITE_HANDLER(vrc4_chr_even_hi_handler)
{
	int slot;
	int new_bank;

	struct board *board = emu->board;

	slot = (((addr & 0xf000) >> 12) - 0x0b) * 2;

	new_bank = board->chr_banks0[slot].bank & 0x0f;
	new_bank |= (value & 0x1f) << 4;

	if (new_bank != board->chr_banks0[slot].bank) {
		board->chr_banks0[slot].bank = new_bank;
		board_chr_sync(board, 0);
	}
}

static CPU_WRITE_HANDLER(vrc4_chr_odd_lo_handler)
{
	int slot;
	int new_bank;

	struct board *board = emu->board;

	slot = (((addr & 0xf000) >> 12) - 0x0b) * 2 + 1;

	new_bank = board->chr_banks0[slot].bank & 0x1f0;
	new_bank |= (value & 0x0f);

	if (new_bank != board->chr_banks0[slot].bank) {
		board->chr_banks0[slot].bank = new_bank;
		board_chr_sync(board, 0);
	}
}

static CPU_WRITE_HANDLER(vrc4_chr_odd_hi_handler)
{
	int slot;
	int new_bank;

	struct board *board = emu->board;

	slot = (((addr & 0xf000) >> 12) - 0x0b) * 2 + 1;

	new_bank = board->chr_banks0[slot].bank & 0x0f;
	new_bank |= (value & 0x1f) << 4;

	if (new_bank != board->chr_banks0[slot].bank) {
		board->chr_banks0[slot].bank = new_bank;
		board_chr_sync(board, 0);
	}
}

static void vrc4_reset(struct board *board, int hard)
{
	if (!hard)
		return;

	if (board->info->flags & BOARD_INFO_FLAG_M2_TIMER) {
		m2_timer_set_flags(board->emu->m2_timer,
				   M2_TIMER_FLAG_COUNT_UP|
				   M2_TIMER_FLAG_RELOAD|
				   M2_TIMER_FLAG_PRESCALER |
				   M2_TIMER_FLAG_PRESCALER_RELOAD|
				   M2_TIMER_FLAG_IRQ_ON_RELOAD, 0);
		m2_timer_set_prescaler_reload(board->emu->m2_timer, 340, 0);
		m2_timer_set_prescaler(board->emu->m2_timer, 340, 0);
		m2_timer_set_prescaler_decrement(board->emu->m2_timer, 3, 0);
		m2_timer_set_enabled(board->emu->m2_timer, 0, 0);
		m2_timer_set_size(board->emu->m2_timer, 8, 0);
	}

	board->prg_and = 0xff;
	board->chr_and = 0x1ff;
	board->prg_mode = 0;
}

static int vrc4_init(struct board *board)
{
	struct cpu_state *cpu;
	int addr_mask1;
	int addr_mask2;
	int compat_mask1;
	int compat_mask2;
	int addr;
	int mask;
	uint32_t board_type;
	int has_copy_protection;

	board_type = board->info->board_type;
	has_copy_protection = 0;

	/* Make GCC happy */
	addr_mask1 = addr_mask2 = 0;
	compat_mask1 = compat_mask2 = 0;

	switch (board_type) {
	case BOARD_TYPE_VRC2A:
		has_copy_protection = 1;
		addr_mask1 = 0x02;
		addr_mask2 = 0x01;
		break;
	case BOARD_TYPE_VRC2B:
		has_copy_protection = 1;
		addr_mask1 = 0x01;
		addr_mask2 = 0x02;
		break;
	case BOARD_TYPE_VRC2C:
		addr_mask1 = 0x02;
		addr_mask2 = 0x01;
		break;
	case BOARD_TYPE_VRC4A:
		addr_mask1 = 0x02;
		addr_mask2 = 0x04;
		break;
	case BOARD_TYPE_VRC4B:
		addr_mask1 = 0x02;
		addr_mask2 = 0x01;
		break;
	case BOARD_TYPE_VRC4C:
		addr_mask1 = 0x40;
		addr_mask2 = 0x80;
		break;
	case BOARD_TYPE_VRC4D:
		addr_mask1 = 0x08;
		addr_mask2 = 0x04;
		break;
	case BOARD_TYPE_VRC4E:
		addr_mask1 = 0x04;
		addr_mask2 = 0x08;
		break;
	case BOARD_TYPE_VRC4F:
		addr_mask1 = 0x01;
		addr_mask2 = 0x02;
		break;
	case BOARD_TYPE_VRC4AC:
		addr_mask1 = 0x02;
		addr_mask2 = 0x04;
		compat_mask1 = 0x40;
		compat_mask2 = 0x80;
		break;
	case BOARD_TYPE_VRC4BD:
		addr_mask1 = 0x02;
		addr_mask2 = 0x01;
		compat_mask1 = 0x08;
		compat_mask2 = 0x04;
		break;
	case BOARD_TYPE_VRC4E_COMPAT:
		has_copy_protection = 1;
		addr_mask1 = 0x04;
		addr_mask2 = 0x08;
		compat_mask1 = 0x01;
		compat_mask2 = 0x02;
		break;
	}

	cpu = board->emu->cpu;
	mask = 0xf000 | addr_mask1 | addr_mask2;

	/* Map write handler for PRG bank registers */
	cpu_set_write_handler(cpu, 0x8000, SIZE_4K, 0x8000, vrc4_prg_handler);
	cpu_set_write_handler(cpu, 0xa000, SIZE_4K, 0xa000, vrc4_prg_handler);

	/* Map write handler for mirroring */
	cpu_set_write_handler(cpu, 0x9000, SIZE_4K, mask,
			      standard_mirroring_handler);
	cpu_set_write_handler(cpu, 0x9000 | addr_mask1, SIZE_4K, mask,
			      standard_mirroring_handler);

	for (addr = 0xb000; addr < 0xf000; addr += 0x1000) {
		/* Map handler for lower half of CHR banks */
		cpu_set_write_handler(cpu, addr, SIZE_4K, mask,
				      vrc4_chr_even_lo_handler);
		cpu_set_write_handler(cpu, addr | addr_mask2, SIZE_4K, mask,
				      vrc4_chr_odd_lo_handler);

		/* Map handler for upper half of CHR banks */
		cpu_set_write_handler(cpu, addr | addr_mask1, SIZE_4K, mask,
				      vrc4_chr_even_hi_handler);
		cpu_set_write_handler(cpu, addr | addr_mask1 | addr_mask2,
				      SIZE_4K, mask, vrc4_chr_odd_hi_handler);
	}

	if (compat_mask1 && compat_mask2) {
		int compat_mask = 0xf000 | compat_mask1 | compat_mask2;
		/* vrc2b compat */
		for (addr = 0xb000; addr < 0xf000; addr += 0x1000) {
			/* Map handler for lower half of CHR banks */
			cpu_set_write_handler(cpu, addr | compat_mask2,
					      SIZE_4K, compat_mask,
					      vrc4_chr_odd_lo_handler);

			/* Map handler for upper half of CHR banks */
			cpu_set_write_handler(cpu, addr | compat_mask1,
					      SIZE_4K, compat_mask,
					      vrc4_chr_even_hi_handler);
			cpu_set_write_handler(cpu, addr | compat_mask1 |
					      compat_mask2,
					      SIZE_4K, compat_mask,
					      vrc4_chr_odd_hi_handler);
		}
	}

	/* Enable the copy protection for VRC2 if we have no PRG RAM */
	if (has_copy_protection && !board->wram[0].data) {
		cpu_set_write_handler(board->emu->cpu, 0x6000,
				      SIZE_4K, 0, vrc2_write_handler);
		cpu_set_read_handler(board->emu->cpu, 0x6000,
				     SIZE_4K, 0, vrc2_read_handler);
	}

	if ((board_type != BOARD_TYPE_VRC2A) &&
	    (board_type != BOARD_TYPE_VRC2B)) {
		/* /\* Map handlers for VRC timer *\/ */
		cpu_set_write_handler(cpu, 0xf000, SIZE_4K, mask,
				      vrc4_irq_latch_lo);
		cpu_set_write_handler(cpu, 0xf000 | addr_mask1, SIZE_4K, mask,
				      vrc4_irq_latch_hi);
		cpu_set_write_handler(cpu, 0xf000 | addr_mask2, SIZE_4K, mask,
				      vrc4_irq_control);
		cpu_set_write_handler(cpu, 0xf000 | addr_mask1 | addr_mask2,
				      SIZE_4K, mask, vrc4_irq_acknowledge);

		/* Map handler for PRG mode register */
		cpu_set_write_handler(cpu, 0x9000 | addr_mask2, SIZE_4K, mask,
				      vrc4_prg_mode_handler);
		cpu_set_write_handler(cpu, 0x9000 | addr_mask1 | addr_mask2,
				      SIZE_4K, mask, vrc4_prg_mode_handler);

		if (compat_mask1 && compat_mask2) {
			int compat_mask = 0xf000 | compat_mask1 |
				compat_mask2;

			cpu_set_write_handler(cpu, 0xf000 | compat_mask1,
					      SIZE_4K, compat_mask,
					      vrc4_irq_latch_hi);
			cpu_set_write_handler(cpu, 0xf000 | compat_mask2,
					      SIZE_4K, compat_mask,
					      vrc4_irq_control);
			cpu_set_write_handler(cpu, compat_mask,
					      SIZE_4K, compat_mask,
					      vrc4_irq_acknowledge);

			cpu_set_write_handler(cpu, 0x9000 | compat_mask2,
					      SIZE_4K, compat_mask,
					      vrc4_prg_mode_handler);
			cpu_set_write_handler(cpu, 0x9000 | compat_mask1 |
					      compat_mask2,
					      SIZE_4K, compat_mask,
					      vrc4_prg_mode_handler);
		}
	} else {
		/* Map handler for mirroring register */
		cpu_set_write_handler(cpu, 0x9000 | addr_mask2, SIZE_4K, mask,
				      standard_mirroring_handler);
		cpu_set_write_handler(cpu, 0x9000 | addr_mask1 | addr_mask2,
				      SIZE_4K, mask, standard_mirroring_handler);

	}

	return 0;
}

