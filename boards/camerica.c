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

static CPU_WRITE_HANDLER(bf9096_write_handler)
{
	value = (value >> 1) & 0x0c;

	if (value != emu->board->prg_or) {
		emu->board->prg_or = value;
		board_prg_sync(emu->board);
	}
}

static CPU_WRITE_HANDLER(aladdin_write_handler)
{
	value = (value >> 1) & 0x0c;

	if (value != emu->board->prg_or) {
		emu->board->prg_or = value;
		board_prg_sync(emu->board);
	}
}

static CPU_WRITE_HANDLER(goldenfive_write_handler)
{
	if (addr < 0xc000) {
		if (value & 0x08) {
			emu->board->prg_or = (value << 4) & 0x70;
			board_prg_sync(emu->board);
		}
	} else {
		update_prg_bank(emu->board, 1, value);
	}
}

static void goldenfive_reset(struct board *board, int hard)
{
	if (hard) {
		board->prg_and = 0x0f;
		board->prg_or = 0x00;
	}
}

static void bf9096_reset(struct board *board, int hard)
{
	if (hard) {
		board->prg_and = 0x03;
		board->prg_or = 0x0c;
	}
}

static struct board_funcs camerica_bf9096_funcs = {
	.reset = bf9096_reset,
};

static struct board_funcs camerica_goldenfive_funcs = {
	.reset = goldenfive_reset,
};

static struct board_write_handler camerica_bf9093_write_handlers[] = {
	{simple_prg_no_conflict_write_handler, 0xc000, SIZE_16K, 0},
	{NULL},
};

static struct board_write_handler camerica_goldenfive_write_handlers[] = {
	{goldenfive_write_handler, 0x8000, SIZE_32K, 0},
	{NULL},
};

static struct board_write_handler camerica_bf9096_write_handlers[] = {
	{bf9096_write_handler, 0x8000, SIZE_16K, 0},
	{simple_prg_no_conflict_write_handler, 0xc000, SIZE_16K, 0},
	{NULL},
};

static struct board_write_handler camerica_bf9097_write_handlers[] = {
	{standard_mirroring_handler, 0x8000, SIZE_8K, 0},
	{simple_prg_no_conflict_write_handler, 0xc000, SIZE_16K, 0},
	{NULL},
};

static struct board_write_handler camerica_aladdin_write_handlers[] = {
	{aladdin_write_handler, 0x8000, SIZE_16K, 0},
	{simple_prg_no_conflict_write_handler, 0xc000, SIZE_16K, 0},
	{NULL},
};

struct board_info board_camerica_goldenfive = {
	.board_type = BOARD_TYPE_CAMERICA_GOLDENFIVE,
	.name = "CAMERICA-GOLDENFIVE",
	.funcs = &camerica_goldenfive_funcs,
	.init_prg = std_prg_16k,
	.init_chr0 = std_chr_8k,
	.write_handlers = camerica_goldenfive_write_handlers,
	.max_prg_rom_size = SIZE_2048K,
	.max_chr_rom_size = SIZE_8K,
	.max_wram_size = {SIZE_8K, 0},
};

struct board_info board_camerica_bf9093 = {
	.board_type = BOARD_TYPE_CAMERICA_BF9093,
	.name = "CAMERICA-BF9093",
	.init_prg = std_prg_16k,
	.init_chr0 = std_chr_8k,
	.write_handlers = camerica_bf9093_write_handlers,
	.max_prg_rom_size = SIZE_256K,
	.max_chr_rom_size = SIZE_8K,
	.max_wram_size = {SIZE_8K, 0},
};

struct board_info board_camerica_bf9096 = {
	.board_type = BOARD_TYPE_CAMERICA_BF9096,
	.name = "CAMERICA-BF9096",
	.funcs = &camerica_bf9096_funcs,
	.init_prg = std_prg_16k,
	.init_chr0 = std_chr_8k,
	.write_handlers = camerica_bf9096_write_handlers,
	.max_prg_rom_size = SIZE_256K,
	.max_chr_rom_size = SIZE_8K,
	.max_wram_size = {SIZE_8K, 0},
	.flags = BOARD_INFO_FLAG_MIRROR_M,
};

struct board_info board_camerica_bf9097 = {
	.board_type = BOARD_TYPE_CAMERICA_BF9097,
	.name = "CAMERICA-BF9097",
	.init_prg = std_prg_16k,
	.init_chr0 = std_chr_8k,
	.write_handlers = camerica_bf9097_write_handlers,
	.max_prg_rom_size = SIZE_128K,
	.max_chr_rom_size = SIZE_8K,
	.max_wram_size = {SIZE_8K, 0},
	.flags = BOARD_INFO_FLAG_MIRROR_M,
	.mirroring_values = std_mirroring_01,
	.mirroring_shift = 4,
};

struct board_info board_camerica_aladdin = {
	.board_type = BOARD_TYPE_CAMERICA_ALADDIN,
	.name = "CAMERICA-ALGQ",
	.funcs = &camerica_bf9096_funcs,
	.init_prg = std_prg_16k,
	.init_chr0 = std_chr_8k,
	.write_handlers = camerica_aladdin_write_handlers,
	.max_prg_rom_size = SIZE_256K,
	.max_chr_rom_size = SIZE_8K,
	.max_wram_size = {SIZE_8K, 0},
	.flags = BOARD_INFO_FLAG_MIRROR_M,
};
