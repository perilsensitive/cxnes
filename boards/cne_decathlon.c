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

static CPU_WRITE_HANDLER(cne_decathlon_write_handler);

static struct bank cne_decathlon_init_prg[] = {
	{ 0, 0, SIZE_8K, 0x6000, MAP_PERM_READ, MAP_TYPE_ROM},
	{ 0, 0, SIZE_32K, 0x8000, MAP_PERM_READ, MAP_TYPE_ROM},
	{.type = MAP_TYPE_END},
};

static struct board_write_handler cne_decathlon_write_handlers[] = {
	{cne_decathlon_write_handler, 0x8065, 0x80e5 - 0x8065, 0},
	{NULL},
};

struct board_info board_cne_decathlon = {
	.board_type = BOARD_TYPE_CNE_DECATHLON,
	.name = "CNE-DECATHLON",
	.init_prg = cne_decathlon_init_prg,
	.init_chr0 = std_chr_8k,
	.write_handlers = cne_decathlon_write_handlers,
	.max_prg_rom_size = SIZE_128K,
	.max_wram_size = { SIZE_8K, 0 },
	.max_chr_rom_size = SIZE_64K,
};

static CPU_WRITE_HANDLER(cne_decathlon_write_handler)
{
	struct board *board = emu->board;

	if (addr < 0x80a5) {
		update_prg_bank(board, 1, (addr - 0x8065) & 0x03);
	} else {
		update_chr0_bank(board, 0, (addr - 0x80a5) & 0x07);
	}
}
