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

static CPU_WRITE_HANDLER(d74x161_161_32_write_handler)
{
	if (emu->rom->info.mirroring == MIRROR_M) {
		update_prg_bank(emu->board, 1, (value & 0x70) >> 4);
		board_set_ppu_mirroring(emu->board, (value & 0x80) ?
					MIRROR_1B : MIRROR_1A);
	} else {
		update_prg_bank(emu->board, 1, (value & 0xf0) >> 4);
	}
	update_chr0_bank(emu->board, 0, value & 0x0f);
}

static struct board_write_handler d74x161_161_32_write_handlers[] = {
	{d74x161_161_32_write_handler, 0x8000, SIZE_32K, 0},
	{NULL},
};

struct board_info board_74x161_161_32 = {
	.board_type = BOARD_TYPE_74x161_161_32,
	.name = "74*161/161/32",
	.init_prg = std_prg_16k,
	.init_chr0 = std_chr_8k,
	.write_handlers = d74x161_161_32_write_handlers,
	.max_prg_rom_size = SIZE_256K,
	.max_chr_rom_size = SIZE_128K,
	.max_wram_size = {SIZE_8K, 0},
};
