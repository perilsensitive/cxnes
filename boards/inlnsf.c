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

static CPU_WRITE_HANDLER(inlnsf_write_handler);

static struct board_write_handler inlnsf_write_handlers[] = {
	{inlnsf_write_handler, 0x5000, SIZE_4K, 0},
	{NULL},
};

struct board_info board_inlnsf = {
	.board_type = BOARD_TYPE_INLNSF,
	.name = "Infiniteneslives NSF",
	.init_prg = std_prg_4k,
	.init_chr0 = std_chr_8k,
	.write_handlers = inlnsf_write_handlers,
	.max_prg_rom_size = SIZE_1024K,
	.max_chr_rom_size = SIZE_8K,
	.max_wram_size = {SIZE_8K, 0},
};

static CPU_WRITE_HANDLER(inlnsf_write_handler)
{
	struct board *board = emu->board;

	if (value != board->prg_banks[(addr & 7) + 1].bank) {
		board->prg_banks[(addr & 7) + 1].bank = value;
		board_prg_sync(board);
	}
}
