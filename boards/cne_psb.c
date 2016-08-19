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

static CPU_WRITE_HANDLER(cne_psb_write_handler);

static struct board_write_handler cne_psb_write_handlers[] = {
	{cne_psb_write_handler, 0x6000, SIZE_2K, 0},
	{NULL},
};

struct board_info board_cne_psb = {
	.board_type = BOARD_TYPE_CNE_PSB,
	.name = "CNE-PSB",
	.init_prg = std_prg_8k,
	.init_chr0 = std_chr_2k,
	.write_handlers = cne_psb_write_handlers,
	.max_prg_rom_size = SIZE_2048K,
	.max_chr_rom_size = SIZE_512K,
	.max_wram_size = {SIZE_8K, 0},
};

static CPU_WRITE_HANDLER(cne_psb_write_handler)
{
	struct board *board = emu->board;

	switch (addr & 0x6007) {
	case 0x6000:
	case 0x6001:
	case 0x6002:
	case 0x6003:
		update_prg_bank(board, (addr & 0x03) + 1, value);
		break;
	case 0x6004:
	case 0x6005:
	case 0x6006:
	case 0x6007:
		update_chr0_bank(board, addr & 0x03, value);
		break;
	}
}
