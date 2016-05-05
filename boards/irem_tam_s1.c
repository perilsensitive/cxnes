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

static CPU_WRITE_HANDLER(irem_tam_s1_write_handler)
{
	int mirroring = 0;

	update_prg_bank(emu->board, 2, (value & 0x0f));
	switch (value & 0xc0) {
	case 0x00:
		mirroring = MIRROR_1A;
		break;
	case 0x40:
		mirroring = MIRROR_H;
		break;
	case 0x80:
		mirroring = MIRROR_V;
		break;
	case 0xc0:
		mirroring = MIRROR_1B;
		break;
	}

	board_set_ppu_mirroring(emu->board, mirroring);
}

static struct bank irem_tam_s1_init_prg[] = {
	{0, 0, SIZE_8K, 0x6000, MAP_PERM_READWRITE, MAP_TYPE_RAM0},
	{-1, 0, SIZE_16K, 0x8000, MAP_PERM_READ, MAP_TYPE_ROM},
	{0, 0, SIZE_16K, 0xc000, MAP_PERM_READ, MAP_TYPE_ROM},
	{.type = MAP_TYPE_END},
};

static struct board_write_handler irem_tam_s1_write_handlers[] = {
	{irem_tam_s1_write_handler, 0x8000, SIZE_32K, 0},
	{NULL},
};

struct board_info board_irem_tam_s1 = {
	.board_type = BOARD_TYPE_IREM_TAM_S1,
	.name = "IREM-TAM-S1",
	.mapper_name = "TAM-S1",
	.init_prg = irem_tam_s1_init_prg,
	.init_chr0 = std_chr_8k,
	.write_handlers = irem_tam_s1_write_handlers,
	.max_prg_rom_size = SIZE_256K,
	.max_chr_rom_size = SIZE_8K,
	.max_wram_size = {SIZE_8K, 0},
	.flags = BOARD_INFO_FLAG_MIRROR_M,
};
