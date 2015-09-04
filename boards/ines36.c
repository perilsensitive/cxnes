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

static CPU_WRITE_HANDLER(ines36_write_handler)
{
	update_prg_bank(emu->board, 1, (value & 0xf) >> 4);
	update_chr0_bank(emu->board, 0, value & 0xf);

}

static struct board_write_handler ines36_write_handlers[] = {
	{ines36_write_handler, 0x8400, 0xffff - 0x8400, 0},
	{NULL},
};

struct board_info board_ines36 = {
	.board_type = BOARD_TYPE_INES36,
	.name = "iNES mapper 36",
	.init_prg = std_prg_32k,
	.init_chr0 = std_chr_8k,
	.write_handlers = ines36_write_handlers,
	.max_prg_rom_size = SIZE_512K,
	.max_chr_rom_size = SIZE_256K,
	.max_wram_size = {SIZE_8K, 0},
};
