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

static void bandai_reset(struct board *board, int);
static CPU_WRITE_HANDLER(bandai_write_handler);

static struct board_funcs bandai_funcs = {
	.reset = bandai_reset,
};

static struct board_write_handler bandai_fcg_write_handlers[] = {
	{bandai_write_handler, 0x6000, SIZE_8K, 0},
	{NULL},
};

static struct board_write_handler bandai_lz93d50_write_handlers[] = {
	{bandai_write_handler, 0x8000, SIZE_32K, 0},
	{NULL},
};

struct board_info board_bandai_fcg = {
	.board_type = BOARD_TYPE_BANDAI_FCG,
	.name = "BANDAI-FCG",
	.funcs = &bandai_funcs,
	.init_prg = std_prg_16k,
	.init_chr0 = std_chr_1k,
	.write_handlers = bandai_fcg_write_handlers,
	.max_prg_rom_size = SIZE_256K,
	.max_chr_rom_size = SIZE_256K,
	.flags = BOARD_INFO_FLAG_MIRROR_M |
	         BOARD_INFO_FLAG_M2_TIMER,
	.mirroring_values = std_mirroring_vh01,
};

struct board_info board_bandai_lz93d50 = {
	.board_type = BOARD_TYPE_BANDAI_LZ93D50,
	.name = "BANDAI-LZ93D50",
	.funcs = &bandai_funcs,
	.init_prg = std_prg_16k,
	.init_chr0 = std_chr_1k,
	.write_handlers = bandai_lz93d50_write_handlers,
	.max_prg_rom_size = SIZE_512K,
	.max_chr_rom_size = SIZE_256K,
	.max_wram_size = {256, 0},
	.flags = BOARD_INFO_FLAG_MIRROR_M |
	         BOARD_INFO_FLAG_M2_TIMER,
	.mirroring_values = std_mirroring_vh01,
};

struct board_info board_bandai_jump2 = {
	.board_type = BOARD_TYPE_BANDAI_JUMP2,
	.name = "BANDAI-JUMP2",
	.funcs = &bandai_funcs,
	.init_prg = std_prg_16k,
	.init_chr0 = std_chr_1k,
	.write_handlers = bandai_lz93d50_write_handlers,
	.max_prg_rom_size = SIZE_512K,
	.max_chr_rom_size = SIZE_256K,
	.max_wram_size = {SIZE_8K, 0},
	.flags = BOARD_INFO_FLAG_MIRROR_M |
	         BOARD_INFO_FLAG_M2_TIMER,
	.mirroring_values = std_mirroring_vh01,
};

static CPU_WRITE_HANDLER(bandai_write_handler)
{
	struct board *board = emu->board;

	addr &= 0x0f;

	switch (addr) {
	case 0x00:
	case 0x01:
	case 0x02:
	case 0x03:
		if (board->info->board_type == BOARD_TYPE_BANDAI_JUMP2) {
			value = value ? 0x10 : 0;
			if (value != board->prg_or) {
				board->prg_or = value;
				board_prg_sync(board);
			}
			break;
		}
	case 0x04:
	case 0x05:
	case 0x06:
	case 0x07:
		if (board->info->board_type != BOARD_TYPE_BANDAI_JUMP2) {
			update_chr0_bank(board, addr, value);
		}
		break;
	case 0x08:
		update_prg_bank(board, 1, value);
		break;
	case 0x09:
		standard_mirroring_handler(emu, addr, value, cycles);
		break;
	case 0x0a:
		m2_timer_force_reload(emu->m2_timer, cycles);
		m2_timer_set_irq_enabled(emu->m2_timer, value & 1, cycles);
		break;
	case 0x0b:
		m2_timer_set_reload_lo(emu->m2_timer, value, cycles);
		break;
	case 0x0c:
		m2_timer_set_reload_hi(emu->m2_timer, value, cycles);
		break;
	}
}

static void bandai_reset(struct board *board, int hard)
{
	if (hard) {
		int i;
		m2_timer_set_irq_enabled(board->emu->m2_timer, 0, 0);
		if (board->info->board_type == BOARD_TYPE_BANDAI_JUMP2) {
			board->prg_and = 0x0f;
			board->prg_or = 0x00;
			for (i = 0; i < 8; i++)
				board->chr_banks0[i].bank = i;
		}
	}
}
