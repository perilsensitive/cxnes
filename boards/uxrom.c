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

static CPU_WRITE_HANDLER(uxrom_pc_prowrestling_write_handler);

struct bank un1rom_init_prg[] = {
	{0, 0, SIZE_8K, 0x6000, MAP_PERM_READWRITE, MAP_TYPE_RAM0},
	{0, 2, SIZE_16K, 0x8000, MAP_PERM_READ, MAP_TYPE_ROM},
	{-1, 2, SIZE_16K, 0xc000, MAP_PERM_READ, MAP_TYPE_ROM},
	{.type = MAP_TYPE_END},
};

struct bank unrom_74hc08_init_prg[] = {
	{0, 0, SIZE_8K, 0x6000, MAP_PERM_READWRITE, MAP_TYPE_RAM0},
	{0, 0, SIZE_16K, 0xc000, MAP_PERM_READ, MAP_TYPE_ROM},
	{0, 0, SIZE_16K, 0x8000, MAP_PERM_READ, MAP_TYPE_ROM},
	{.type = MAP_TYPE_END},
};

static struct board_write_handler uxrom_write_handlers[] = {
	{simple_prg_write_handler, 0x8000, SIZE_32K, 0},
	{NULL},
};

static struct board_write_handler uxrom_pc_prowrestling_write_handlers[] = {
	{uxrom_pc_prowrestling_write_handler, 0x8000, SIZE_32K, 0},
	{NULL},
};

static struct board_write_handler uxrom_no_conflict_write_handlers[] = {
	{simple_prg_no_conflict_write_handler, 0x8000, SIZE_32K, 0},
	{NULL},
};

struct board_info board_uxrom = {
	.board_type = BOARD_TYPE_UxROM,
	.name = "UxROM",
	.init_prg = std_prg_16k,
	.init_chr0 = std_chr_8k,
	.write_handlers = uxrom_write_handlers,
	.max_prg_rom_size = SIZE_4096K,
	.max_chr_rom_size = SIZE_8K,
	.max_wram_size = {SIZE_8K, 0},
};

struct board_info board_uxrom_pc_prowrestling = {
	.board_type = BOARD_TYPE_UxROM_PC_PROWRESTLING,
	.name = "UxROM-PLAYCHOICE-PROWRESTLING",
	.init_prg = std_prg_16k,
	.init_chr0 = std_chr_8k,
	.write_handlers = uxrom_pc_prowrestling_write_handlers,
	.max_prg_rom_size = SIZE_32K + SIZE_64K,
	.max_chr_rom_size = SIZE_8K,
	.max_wram_size = {SIZE_8K, 0},
};

struct board_info board_uxrom_no_conflict = {
	.board_type = BOARD_TYPE_UxROM_NO_CONFLICT,
	.name = "UxROM-NO-CONFLICT",
	.init_prg = std_prg_16k,
	.init_chr0 = std_chr_8k,
	.write_handlers = uxrom_no_conflict_write_handlers,
	.max_prg_rom_size = SIZE_4096K,
	.max_chr_rom_size = SIZE_8K,
	.max_wram_size = {SIZE_8K, 0},
};

struct board_info board_un1rom = {
	.board_type = BOARD_TYPE_UN1ROM,
	.name = "HVC-UN1ROM",
	.init_prg = un1rom_init_prg,
	.init_chr0 = std_chr_8k,
	.write_handlers = uxrom_write_handlers,
	.max_prg_rom_size = SIZE_1024K,
	.max_chr_rom_size = SIZE_8K,
	.max_wram_size = {SIZE_8K, 0},
};

struct board_info board_unrom_74hc08 = {
	.board_type = BOARD_TYPE_UNROM_74HC08,
	.name = "HVC-UNROM+74HC08",
	.init_prg = unrom_74hc08_init_prg,
	.init_chr0 = std_chr_8k,
	.write_handlers = uxrom_write_handlers,
	.max_prg_rom_size = SIZE_4096K,
	.max_chr_rom_size = SIZE_8K,
	.max_wram_size = {SIZE_8K, 0},
};

static CPU_WRITE_HANDLER(uxrom_pc_prowrestling_write_handler)
{
	if ((value & 0x07) < 4)
		value &= 0x01;
	else
		value -= 2;

	emu->board->prg_banks[1].bank = value;
	board_prg_sync(emu->board);
}

