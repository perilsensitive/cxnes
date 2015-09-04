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

static void jaleco_ss88006_reset(struct board *board, int);
static CPU_WRITE_HANDLER(jaleco_ss88006_write_handler);

static struct board_funcs jaleco_ss88006_funcs = {
	.reset = jaleco_ss88006_reset,
};

static struct board_write_handler jaleco_ss88006_write_handlers[] = {
	{jaleco_ss88006_write_handler, 0x8000, SIZE_32K, 0},
	{NULL},
};

struct board_info board_jaleco_ss88006 = {
	.board_type = BOARD_TYPE_JALECO_SS88006,
	.name = "JALECO-JF-23/24/25/27/29/37/40",
	.mapper_name = "Jaleco SS88006",
	.funcs = &jaleco_ss88006_funcs,
	.init_prg = std_prg_8k,
	.init_chr0 = std_chr_1k,
	.write_handlers = jaleco_ss88006_write_handlers,
	.max_prg_rom_size = SIZE_256K,
	.max_chr_rom_size = SIZE_256K,
	.max_wram_size = {SIZE_8K, 0},
	.flags = BOARD_INFO_FLAG_MIRROR_M |
	         BOARD_INFO_FLAG_M2_TIMER,
};

static CPU_WRITE_HANDLER(jaleco_ss88006_write_handler)
{
	uint8_t blah;
	int low;
	struct board *board = emu->board;
	int size;
	int irq_reload;

	low = !(addr & 1);

	
	switch (addr) {
	case 0x8000:
	case 0x8001:
	case 0x8002:
	case 0x8003:
	case 0x9000:
	case 0x9001:
		addr = (((addr & 2) >> 1) | ((addr & 0x1000) >> 11)) + 1;
		blah = board->prg_banks[addr].bank;
		if (low)
			blah = (blah & 0xf0) | (value & 0x0f);
		else
			blah = (blah & 0x0f) | ((value & 0x0f) << 4);
		update_prg_bank(board, addr, blah);
		break;
	case 0xa000:
	case 0xa001:
	case 0xa002:
	case 0xa003:
	case 0xb000:
	case 0xb001:
	case 0xb002:
	case 0xb003:
		addr = ((addr & 2) >> 1) | ((addr & 0x1000) >> 11);
		blah = board->chr_banks0[addr].bank;
		if (low)
			blah = (blah & 0xf0) | (value & 0x0f);
		else
			blah = (blah & 0x0f) | ((value & 0x0f) << 4);
		update_chr0_bank(board, addr, blah);
		break;
	case 0xc000:
	case 0xc001:
	case 0xc002:
	case 0xc003:
		addr = ((addr & 2) >> 1) | ((addr & 0x4000) >> 12);
		blah = board->chr_banks0[addr].bank;
		if (low)
			blah = (blah & 0xf0) | (value & 0x0f);
		else
			blah = (blah & 0x0f) | ((value & 0x0f) << 4);
		update_chr0_bank(board, addr, blah);
		break;
	case 0xd000:
	case 0xd001:
	case 0xd002:
	case 0xd003:
		addr = ((addr & 2) >> 1) | ((addr & 0xc000) >> 13);
		blah = board->chr_banks0[addr].bank;
		if (low)
			blah = (blah & 0xf0) | (value & 0x0f);
		else
			blah = (blah & 0x0f) | ((value & 0x0f) << 4);
		update_chr0_bank(board, addr, blah);
		break;
	case 0xe000:
	case 0xe001:
	case 0xe002:
	case 0xe003:
		addr &= 3;
		irq_reload = m2_timer_get_reload(emu->m2_timer);
		irq_reload &= ~(0x0f << (4 * addr));
		irq_reload |= (value & 0x0f) << (4 * addr);
		m2_timer_set_reload(emu->m2_timer, irq_reload, cycles);
		break;
	case 0xf000:
		m2_timer_force_reload(emu->m2_timer, cycles);
		break;
	case 0xf001:
		size = 16;
		if (value & 0x02)
			size = 12;
		else if (value & 0x04)
			size = 8;
		else if (value & 0x08)
			size = 4;

		m2_timer_ack(emu->m2_timer, cycles);
		m2_timer_set_size(emu->m2_timer, size, cycles);
		m2_timer_set_enabled(emu->m2_timer, value & 0x01,
				     cycles);
		break;
	case 0xf002:
		switch (value & 0x03) {
			case 0x00: value = MIRROR_H; break;
			case 0x01: value = MIRROR_V; break;
			case 0x02: value = MIRROR_1A; break;
			case 0x03: value = MIRROR_1B; break;
		}
		board_set_ppu_mirroring(board, value);
		break;
	}
}

static void jaleco_ss88006_reset(struct board *board, int hard)
{
	if (hard) {
		m2_timer_set_flags(board->emu->m2_timer,
				   M2_TIMER_FLAG_IRQ_ON_RELOAD,
				   0);
		m2_timer_set_enabled(board->emu->m2_timer, 0, 0);
	}
}
