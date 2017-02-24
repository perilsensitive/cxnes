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

static CPU_WRITE_HANDLER(magic_kid_googoo_write_handler);

static struct board_write_handler magic_kid_googoo_write_handlers[] = {
	{magic_kid_googoo_write_handler, 0x8000, SIZE_8K, 0},
	{magic_kid_googoo_write_handler, 0xa000, SIZE_4K, 0},
	{magic_kid_googoo_write_handler, 0xc000, SIZE_8K, 0},
	{NULL},
};

static struct bank magic_kid_googoo_init_prg[] = {
	{0, 0, SIZE_8K, 0x6000, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{0, 0, SIZE_16K, 0x8000, MAP_PERM_READ, MAP_TYPE_ROM},
	{0, 0, SIZE_16K, 0xc000, MAP_PERM_READ, MAP_TYPE_ROM},
	{.type = MAP_TYPE_END},
};

struct board_info board_unl_magic_kid_googoo = {
	.board_type = BOARD_TYPE_MAGIC_KID_GOOGOO,
	.name = "UNL-MAGIC_KID_GOOGOO",
	.init_prg = magic_kid_googoo_init_prg,
	.init_chr0 = std_chr_2k,
	.write_handlers = magic_kid_googoo_write_handlers,
	.max_prg_rom_size = SIZE_512K,
	.max_chr_rom_size = SIZE_128K,
	.min_wram_size = {SIZE_8K, 0},
	.max_wram_size = {SIZE_8K, 0},
};

static CPU_WRITE_HANDLER(magic_kid_googoo_write_handler)
{
	struct board *board = emu->board;

	switch (addr & 0xe000) {
	case 0x8000:
	case 0xc000:
		update_prg_bank(board, 1, ((addr & 0x4000) >> 11) |
		                          (value & 0x07));
		break;
	case 0xa000:
		update_chr0_bank(board, addr & 0x03, value);
		break;
	}
}
