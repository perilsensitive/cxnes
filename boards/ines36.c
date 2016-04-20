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

#define _selected_prg_bank board->data[0]
#define _latch board->data[1]

static CPU_READ_HANDLER(ines36_read_handler)
{
	struct board *board;

	board = emu->board;

	addr &= 0xe103;
	if (addr == 0x4100) {
		value = _selected_prg_bank;
	}

	return value;
}

static CPU_WRITE_HANDLER(ines36_write_handler)
{
	struct board *board;

	board = emu->board;

	if ((addr & 0xe200) == 0x4200) {
		update_chr0_bank(board, 0, value & 0xf);
		return;
	}

	addr &= 0xe103;
	if (addr == 0x4102) {
		_latch = 0;
		_selected_prg_bank = value >> 4;
	} else if (addr == 0x4100) {
		value = _selected_prg_bank;
		update_prg_bank(emu->board, 1, ~value);
		_latch = 1;
	} else if (addr >= 0x8000) {
		if (_latch) {
			value = _selected_prg_bank;
			update_prg_bank(emu->board, 1, value);
		} else {
			_selected_prg_bank = value >> 4;
		}
	}


}

static struct board_read_handler ines36_read_handlers[] = {
	{ines36_read_handler, 0x4100, 0x8000 - 0x4100, 0},
	{NULL},
};

static struct board_write_handler ines36_write_handlers[] = {
	{ines36_write_handler, 0x4100, 0xffff - 0x4100 + 1, 0},
	{NULL},
};

struct board_info board_ines36 = {
	.board_type = BOARD_TYPE_INES36,
	.name = "iNES mapper 36",
	.init_prg = std_prg_32k,
	.init_chr0 = std_chr_8k,
	.write_handlers = ines36_write_handlers,
	.read_handlers = ines36_read_handlers,
	.max_prg_rom_size = SIZE_512K,
	.max_chr_rom_size = SIZE_256K,
	.max_wram_size = {SIZE_8K, 0},
};
