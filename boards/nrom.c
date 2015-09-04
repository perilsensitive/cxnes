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

#define _resetbased_game board->data[0]

static void resetbased_4_in_1_reset(struct board *board, int hard)
{
	if (hard)
		_resetbased_game = 0;
	else
		_resetbased_game++;

	board->prg_banks[1].bank = _resetbased_game;
	board->prg_banks[2].bank = _resetbased_game;
	board->chr_banks0[0].bank = _resetbased_game;
	board_prg_sync(board);
	board_chr_sync(board, 0);
}

struct board_funcs resetbased_4_in_1_funcs = {
	.reset = resetbased_4_in_1_reset,
};

struct board_info board_resetbased_4_in_1 = {
	.board_type = BOARD_TYPE_RESETBASED_4_IN_1,
	.name = "BMC RESETBASED 4-IN-1",
	.funcs = &resetbased_4_in_1_funcs,
	.init_prg = std_prg_16k,
	.init_chr0 = std_chr_8k,
	.max_prg_rom_size = SIZE_64K,
	.max_chr_rom_size = SIZE_32K,
	.max_wram_size = {SIZE_8K, 0},
};

struct board_info board_familybasic = {
	.board_type = BOARD_TYPE_FAMILYBASIC,
	.name = "HVC-FAMILYBASIC",
	.init_prg = std_prg_32k,
	.init_chr0 = std_chr_8k,
	.max_prg_rom_size = SIZE_32K,
	.max_chr_rom_size = SIZE_8K,
	.min_wram_size = {SIZE_2K, 0},
	.max_wram_size = {SIZE_4K, 0},
};

struct board_info board_nrom = {
	.board_type = BOARD_TYPE_NROM,
	.name = "NROM",
	.init_prg = std_prg_32k,
	.init_chr0 = std_chr_8k,
	.max_prg_rom_size = SIZE_32K,
	.max_chr_rom_size = SIZE_8K,
	.max_wram_size = {SIZE_8K, 0},
};

struct board_info board_nrom368 = {
	.board_type = BOARD_TYPE_NROM368,
	.name = "NROM-368",
	.init_prg = std_prg_46k,
	.init_chr0 = std_chr_8k,
	.max_prg_rom_size = SIZE_32K + SIZE_16K,
	.max_chr_rom_size = SIZE_8K,
};
