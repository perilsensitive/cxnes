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

#define _reg1 board->data[0]
#define _reg2 board->data[1]

static CPU_WRITE_HANDLER(waixing_sgzlz_write_handler);

static struct board_write_handler waixing_sgzlz_write_handlers[] = {
	{waixing_sgzlz_write_handler, 0x4801, 2, 0},
	{standard_mirroring_handler, 0x4800, 1, 0},
	{NULL},
};

struct board_info board_waixing_sgzlz = {
	.board_type = BOARD_TYPE_WAIXING_SGZLZ,
	.name = "WAIXING SAN GUO ZHONG LIE ZHUAN",
	.init_prg = std_prg_32k,
	.init_chr0 = std_chr_8k,
	.write_handlers = waixing_sgzlz_write_handlers,
	.max_prg_rom_size = SIZE_8192K,
	.max_chr_rom_size = SIZE_8K,
	.max_wram_size = {SIZE_8K, 0},
	.flags = BOARD_INFO_FLAG_MIRROR_M,
	.mirroring_values = std_mirroring_vh,
};

static CPU_WRITE_HANDLER(waixing_sgzlz_write_handler)
{
	struct board *board = emu->board;
	int bank;

	switch (addr) {
	case 0x4801:
		_reg1 = value;
		bank = (_reg1 >> 1);
		bank = bank | (_reg2 << 2);
		update_prg_bank(board, 1, bank);
		break;
	case 0x4802:
		_reg2 = value;
		break;
	}
}
