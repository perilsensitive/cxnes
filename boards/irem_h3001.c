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

static void irem_h3001_reset(struct board *board, int);
static CPU_WRITE_HANDLER(irem_h3001_write_handler);

static struct board_funcs irem_h3001_funcs = {
	.reset = irem_h3001_reset,
};

static struct board_write_handler irem_h3001_write_handlers[] = {
	{irem_h3001_write_handler, 0x8000, SIZE_32K, 0},
	{standard_mirroring_handler, 0x9001, SIZE_4K, 0xf007},
	{NULL},
};

struct board_info board_irem_h3001 = {
	.board_type = BOARD_TYPE_IREM_H3001,
	.name = "IREM-H3001",
	.funcs = &irem_h3001_funcs,
	.init_prg = std_prg_8k,
	.init_chr0 = std_chr_1k,
	.write_handlers = irem_h3001_write_handlers,
	.max_prg_rom_size = SIZE_256K,
	.max_chr_rom_size = SIZE_256K,
	.max_wram_size = {SIZE_8K, 0},
	.mirroring_values = std_mirroring_vh,
	.mirroring_shift = 7,
	.flags = BOARD_INFO_FLAG_MIRROR_M |
	         BOARD_INFO_FLAG_M2_TIMER,
};

static CPU_WRITE_HANDLER(irem_h3001_write_handler)
{
	struct board *board = emu->board;

	switch (addr & 0xf007) {
	case 0x8000:
	case 0x8001:
	case 0x8002:
	case 0x8003:
	case 0x8004:
	case 0x8005:
	case 0x8006:
	case 0x8007:
	case 0xa000:
	case 0xa001:
	case 0xa002:
	case 0xa003:
	case 0xa004:
	case 0xa005:
	case 0xa006:
	case 0xa007:
	case 0xc000:
	case 0xc001:
	case 0xc002:
	case 0xc003:
	case 0xc004:
	case 0xc005:
	case 0xc006:
	case 0xc007:
		update_prg_bank(board, ((addr >> 13) & 0x03) + 1, value);
		break;
	case 0xb000:
	case 0xb001:
	case 0xb002:
	case 0xb003:
	case 0xb004:
	case 0xb005:
	case 0xb006:
	case 0xb007:
		update_chr0_bank(board, addr & 0x07, value);
		break;
	case 0x9003:
		value &= 0x80;
		m2_timer_ack(emu->m2_timer, cycles);
		m2_timer_set_irq_enabled(emu->m2_timer, value, cycles);
		break;
	case 0x9004:
		m2_timer_force_reload(emu->m2_timer, cycles);
		break;
	case 0x9005:
		m2_timer_set_reload_hi(emu->m2_timer, value, cycles);
		break;
	case 0x9006:
		m2_timer_set_reload_lo(emu->m2_timer, value, cycles);
		break;
	}
}

static void irem_h3001_reset(struct board *board, int hard)
{
	if (hard) {
		m2_timer_set_flags(board->emu->m2_timer,
				   M2_TIMER_FLAG_ONE_SHOT, 0);
		m2_timer_set_irq_enabled(board->emu->m2_timer, 0, 0);
		board->prg_banks[1].bank = 0x00;
		board->prg_banks[2].bank = 0x01;
		board->prg_banks[3].bank = 0xfe;
	}
}

