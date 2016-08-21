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

static CPU_WRITE_HANDLER(unl_ks7037_write_handler);

static struct bank unl_ks7037_init_prg[] = {
	{0, 0, SIZE_4K,  0x6000, MAP_PERM_READWRITE, MAP_TYPE_RAM0},
	{15, 0, SIZE_4K,  0x7000, MAP_PERM_READ, MAP_TYPE_ROM},
	{0, 0, SIZE_8K,  0x8000, MAP_PERM_READ, MAP_TYPE_ROM},
	{-4, 0, SIZE_4K,  0xa000, MAP_PERM_READ, MAP_TYPE_ROM},
	{1, 0, SIZE_4K,  0xb000, MAP_PERM_READWRITE, MAP_TYPE_RAM0},
	{0, 0, SIZE_8K,  0xc000, MAP_PERM_READ, MAP_TYPE_ROM},
	{-1, 0, SIZE_8K,  0xe000, MAP_PERM_READ, MAP_TYPE_ROM},
	{.type = MAP_TYPE_END},
};

static struct board_write_handler unl_ks7037_write_handlers[] = {
	{unl_ks7037_write_handler, 0x8000, SIZE_8K, 0},
	{unl_ks7037_write_handler, 0xc000, SIZE_16K, 0},
	{NULL},
};

struct board_info board_unl_ks7037 = {
	.board_type = BOARD_TYPE_UNL_KS7037,
	.name = "UNL-KS7037",
	.init_prg = unl_ks7037_init_prg,
	.init_chr0 = std_chr_8k,
	.write_handlers = unl_ks7037_write_handlers,
	.max_wram_size = { SIZE_8K, 0 },
	.min_wram_size = { SIZE_8K, 0 },
	.max_prg_rom_size = SIZE_128K,
	.max_chr_rom_size = SIZE_8K,
	.flags = BOARD_INFO_FLAG_MIRROR_M,
};

static CPU_WRITE_HANDLER(unl_ks7037_write_handler)
{
	struct board *board = emu->board;

	addr &= 0xe001;

	if (addr == 0x8000) {
		_cmd = value & 0x07;
	} else if (addr == 0x8001) {
		int old_mirroring = _mirroring;

		switch(_cmd) {
		case 2:
			_mirroring &= 0xfc;
			_mirroring |= value & 0x01;
			break;
		case 3:
			_mirroring &= 0xcf;
			_mirroring |= (value & 0x01) << 4;
			break;
		case 4:
			_mirroring &= 0xf3;
			_mirroring |= (value & 0x01) << 2;
			break;
		case 5:
			_mirroring &= 0x3f;
			_mirroring |= (value & 0x01) << 6;
			break;
		case 6:
			update_prg_bank(board, 2, value);
			break;
		case 7:
			update_prg_bank(board, 5, value);
			break;
		}

		if (_mirroring != old_mirroring)
			board_set_ppu_mirroring(board, _mirroring);
	}
}
