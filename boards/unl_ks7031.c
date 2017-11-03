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

#define _cmd board->data[0]
#define _mirroring board->data[1]

static CPU_WRITE_HANDLER(unl_ks7031_write_handler);

static struct bank unl_ks7031_init_prg[] = {
	{ 0, 0, SIZE_2K,  0x6000, MAP_PERM_READ, MAP_TYPE_ROM},
	{ 0, 0, SIZE_2K,  0x6800, MAP_PERM_READ, MAP_TYPE_ROM},
	{ 0, 0, SIZE_2K,  0x7000, MAP_PERM_READ, MAP_TYPE_ROM},
	{ 0, 0, SIZE_2K,  0x7800, MAP_PERM_READ, MAP_TYPE_ROM},
	{15, 0, SIZE_2K,  0x8000, MAP_PERM_READ, MAP_TYPE_ROM},
	{14, 0, SIZE_2K,  0x8800, MAP_PERM_READ, MAP_TYPE_ROM},
	{13, 0, SIZE_2K,  0x9000, MAP_PERM_READ, MAP_TYPE_ROM},
	{12, 0, SIZE_2K,  0x9800, MAP_PERM_READ, MAP_TYPE_ROM},
	{11, 0, SIZE_2K,  0xa000, MAP_PERM_READ, MAP_TYPE_ROM},
	{10, 0, SIZE_2K,  0xa800, MAP_PERM_READ, MAP_TYPE_ROM},
	{ 9, 0, SIZE_2K,  0xb000, MAP_PERM_READ, MAP_TYPE_ROM},
	{ 8, 0, SIZE_2K,  0xb800, MAP_PERM_READ, MAP_TYPE_ROM},
	{ 7, 0, SIZE_2K,  0xc000, MAP_PERM_READ, MAP_TYPE_ROM},
	{ 6, 0, SIZE_2K,  0xc800, MAP_PERM_READ, MAP_TYPE_ROM},
	{ 5, 0, SIZE_2K,  0xd000, MAP_PERM_READ, MAP_TYPE_ROM},
	{ 4, 0, SIZE_2K,  0xd800, MAP_PERM_READ, MAP_TYPE_ROM},
	{ 3, 0, SIZE_2K,  0xe000, MAP_PERM_READ, MAP_TYPE_ROM},
	{ 2, 0, SIZE_2K,  0xe800, MAP_PERM_READ, MAP_TYPE_ROM},
	{ 1, 0, SIZE_2K,  0xf000, MAP_PERM_READ, MAP_TYPE_ROM},
	{ 0, 0, SIZE_2K,  0xf800, MAP_PERM_READ, MAP_TYPE_ROM},
	{.type = MAP_TYPE_END},
};

static struct board_write_handler unl_ks7031_write_handlers[] = {
	{unl_ks7031_write_handler, 0x8000, SIZE_32K, 0},
	{NULL},
};

struct board_info board_unl_ks7031 = {
	.board_type = BOARD_TYPE_UNL_KS7031,
	.name = "UNL-KS7031",
	.init_prg = unl_ks7031_init_prg,
	.init_chr0 = std_chr_8k,
	.write_handlers = unl_ks7031_write_handlers,
	.max_wram_size = { SIZE_8K, 0 },
	.min_wram_size = { SIZE_8K, 0 },
	.max_prg_rom_size = SIZE_128K,
	.max_chr_rom_size = SIZE_8K,
};

static CPU_WRITE_HANDLER(unl_ks7031_write_handler)
{
	struct board *board = emu->board;
	int bank = (addr >> 11) & 0x03;

	update_prg_bank(board, bank, value);
}
