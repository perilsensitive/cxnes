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

static CPU_WRITE_HANDLER(cnrom_security_write_handler);

static struct board_write_handler cnrom_write_handlers[] = {
	{simple_chr_write_handler, 0x8000, SIZE_32K, 0},
	{NULL},
};

static struct board_write_handler cnrom_no_conflict_write_handlers[] = {
	{simple_chr_no_conflict_write_handler, 0x8000, SIZE_32K, 0},
	{NULL},
};

static struct board_write_handler cnrom_security_write_handlers[] = {
	{cnrom_security_write_handler, 0x8000, SIZE_32K, 0},
	{NULL},
};

struct board_info board_namco_cnrom_wram = {
	.board_type = BOARD_TYPE_NAMCO_CNROM_WRAM,
	.name = "NAMCOT-CNROM+WRAM",
	.init_prg = std_prg_32k,
	.init_chr0 = std_chr_8k,
	.write_handlers = cnrom_write_handlers,
	.max_prg_rom_size = SIZE_32K,
	.max_chr_rom_size = SIZE_2048K,
	.min_wram_size = {SIZE_8K, 0},
	.max_wram_size = {SIZE_8K, 0},
};

struct board_info board_cnrom = {
	.board_type = BOARD_TYPE_CNROM,
	.name = "CNROM",
	.init_prg = std_prg_32k,
	.init_chr0 = std_chr_8k,
	.write_handlers = cnrom_write_handlers,
	.max_prg_rom_size = SIZE_32K,
	.max_chr_rom_size = SIZE_2048K,
	.max_wram_size = {SIZE_8K, 0},
};

struct board_info board_cnrom_no_conflict = {
	.board_type = BOARD_TYPE_CNROM_NO_CONFLICT,
	.name = "CNROM-NO-CONFLICT",
	.init_prg = std_prg_32k,
	.init_chr0 = std_chr_8k,
	.write_handlers = cnrom_no_conflict_write_handlers,
	.max_prg_rom_size = SIZE_32K,
	.max_chr_rom_size = SIZE_2048K,
	.max_wram_size = {SIZE_8K, 0},
};

struct board_info board_cnrom_security = {
	.board_type = BOARD_TYPE_SECURITY_CNROM,
	.name = "CNROM+SECURITY",
	.init_prg = std_prg_32k,
	.init_chr0 = std_chr_8k,
	.write_handlers = cnrom_security_write_handlers,
	.max_prg_rom_size = SIZE_32K,
	.max_chr_rom_size = SIZE_8K,
	.max_wram_size = {SIZE_8K, 0},
};

struct board_info board_cnrom_security_bank0 = {
	.board_type = BOARD_TYPE_SECURITY_CNROM_BANK0,
	.name = "CNROM+SECURITY-BANK0",
	.init_prg = std_prg_32k,
	.init_chr0 = std_chr_8k,
	.write_handlers = cnrom_security_write_handlers,
	.max_prg_rom_size = SIZE_32K,
	.max_chr_rom_size = SIZE_8K,
	.max_wram_size = {SIZE_8K, 0},
};

struct board_info board_cnrom_security_bank1 = {
	.board_type = BOARD_TYPE_SECURITY_CNROM_BANK1,
	.name = "CNROM+SECURITY-BANK1",
	.init_prg = std_prg_32k,
	.init_chr0 = std_chr_8k,
	.write_handlers = cnrom_security_write_handlers,
	.max_prg_rom_size = SIZE_32K,
	.max_chr_rom_size = SIZE_8K,
	.max_wram_size = {SIZE_8K, 0},
};

struct board_info board_cnrom_security_bank2 = {
	.board_type = BOARD_TYPE_SECURITY_CNROM_BANK2,
	.name = "CNROM+SECURITY-BANK2",
	.init_prg = std_prg_32k,
	.init_chr0 = std_chr_8k,
	.write_handlers = cnrom_security_write_handlers,
	.max_prg_rom_size = SIZE_32K,
	.max_chr_rom_size = SIZE_8K,
	.max_wram_size = {SIZE_8K, 0},
};

struct board_info board_cnrom_security_bank3 = {
	.board_type = BOARD_TYPE_SECURITY_CNROM_BANK3,
	.name = "CNROM+SECURITY-BANK3",
	.init_prg = std_prg_32k,
	.init_chr0 = std_chr_8k,
	.write_handlers = cnrom_security_write_handlers,
	.max_prg_rom_size = SIZE_32K,
	.max_chr_rom_size = SIZE_8K,
	.max_wram_size = {SIZE_8K, 0},
};

static CPU_WRITE_HANDLER(cnrom_security_write_handler)
{
	struct board *board = emu->board;
	int submapper;

	submapper = BOARD_TYPE_TO_INES_SUBMAPPER(emu->board->info->board_type);
	if (!submapper) {
		value &= 0x33;
		if ((value & 0x0f) && value != 0x13) {
			board->chr_banks0[0].type = MAP_TYPE_AUTO;
			board->chr_banks0[0].bank = value;
			board_chr_sync(board, 0);
		} else {
			board->chr_banks0[0].type = MAP_TYPE_NONE;
			board_chr_sync(board, 0);
		}
	} else {
		submapper &= 0x03;
		if ((value & 0x03) == submapper) {
			board->chr_banks0[0].type = MAP_TYPE_AUTO;
			board->chr_banks0[0].bank = value;
			board_chr_sync(board, 0);
		} else {
			board->chr_banks0[0].type = MAP_TYPE_NONE;
			board_chr_sync(board, 0);
		}
	}
}
