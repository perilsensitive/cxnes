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

static CPU_WRITE_HANDLER(m76in1_write_handler);

static struct board_write_handler m76in1_write_handlers[] = {
	{m76in1_write_handler, 0x8000, SIZE_32K, 0},
	{NULL},
};

struct board_info board_76in1 = {
	.board_type = BOARD_TYPE_76_IN_1,
	.name = "BMC 76-IN-1 / BMC SUPER 42-IN-1",
	.init_prg = std_prg_16k,
	.init_chr0 = std_chr_8k,
	.write_handlers = m76in1_write_handlers,
	.max_prg_rom_size = SIZE_2048K,
	.max_chr_rom_size = SIZE_8K,
	.max_wram_size = {SIZE_8K, 0},
	.mirroring_values = std_mirroring_hv,
	.mirroring_shift = 6,
};

static CPU_WRITE_HANDLER(m76in1_write_handler)
{
	struct board *board = emu->board;
	int prg;

	switch (addr & 0x8001) {
	case 0x8000:
		prg = ((value & 0x80) >> 2) | (value & 0x1f);
		board->prg_banks[1].bank = prg;
		board->prg_banks[2].bank = prg;

		if (!(value & 0x20)) {
			board->prg_banks[1].bank &= ~0x01;
			board->prg_banks[2].bank |= 0x01;
		} 

		standard_mirroring_handler(emu, addr, value, cycles);
		break;
	case 0x8001:
		board->prg_or = (value & 0x01) << 6;
		break;
	}

	board_prg_sync(board);
}
