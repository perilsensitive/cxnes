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

struct bank ntdec_193_init_prg[] = {
	{0, 0, SIZE_8K, 0x8000, MAP_PERM_READ, MAP_TYPE_ROM},
	{-3, 0, SIZE_8K, 0xa000, MAP_PERM_READ, MAP_TYPE_ROM},
	{-2, 0, SIZE_8K, 0xc000, MAP_PERM_READ, MAP_TYPE_ROM},
	{-1, 0, SIZE_8K, 0xe000, MAP_PERM_READ, MAP_TYPE_ROM},
	{.type = MAP_TYPE_END},
};

struct bank ntdec_193_init_chr0[] = {
	{0, 2, SIZE_4K, 0x0000, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{0, 1, SIZE_2K, 0x1000, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{0, 1, SIZE_2K, 0x1800, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{.type = MAP_TYPE_END},
};

static CPU_WRITE_HANDLER(ntdec_193_write_handler)
{
	switch (addr & 3) {
	case 0:
	case 1:
	case 2:
		update_chr0_bank(emu->board, addr & 0x03, value);
		break;
	case 3:
		update_prg_bank(emu->board, 0, value);
		break;
	}
}

static struct board_write_handler ntdec_193_write_handlers[] = {
	{ntdec_193_write_handler, 0x6000, SIZE_8K, 0},
	{NULL},
};

struct board_info board_ntdec_193 = {
	.board_type = BOARD_TYPE_NTDEC_193,
	.name = "NTDEC-193",
	.init_prg = ntdec_193_init_prg,
	.init_chr0 = ntdec_193_init_chr0,
	.write_handlers = ntdec_193_write_handlers,
	.max_prg_rom_size = SIZE_2048K,
	.max_chr_rom_size = SIZE_256K,
};
