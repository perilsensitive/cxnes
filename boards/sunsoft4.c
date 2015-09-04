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

#define _nametable_bank0 board->data[0]
#define _nametable_bank1 board->data[1]
#define _mirroring_reg board->data[2]

static void sunsoft4_reset(struct board *board, int);
static CPU_WRITE_HANDLER(sunsoft4_write_handler);

static struct board_funcs sunsoft4_funcs = {
	.reset = sunsoft4_reset,
};

static struct board_write_handler sunsoft4_write_handlers[] = {
	{sunsoft4_write_handler, 0x8000, SIZE_32K, 0},
	{NULL},
};

struct board_info board_sunsoft4 = {
	.board_type = BOARD_TYPE_SUNSOFT4,
	.name = "SUNSOFT-4",
	.funcs = &sunsoft4_funcs,
	.init_prg = std_prg_16k,
	.init_chr0 = std_chr_2k,
	.write_handlers = sunsoft4_write_handlers,
	.max_prg_rom_size = SIZE_4096K,
	.max_chr_rom_size = SIZE_2048K,
	.max_wram_size = {SIZE_8K, 0},
	.flags = BOARD_INFO_FLAG_MIRROR_M,
};

static void sunsoft4_reset(struct board *board, int hard)
{
	if (hard) {
		_nametable_bank0 = 0;
		_nametable_bank1 = 0;
		_mirroring_reg = 0;
	}
}

static CPU_WRITE_HANDLER(sunsoft4_write_handler)
{
	struct board *board = emu->board;

	addr &= 0xf000;

	switch (addr) {
	case 0x8000:
	case 0x9000:
	case 0xa000:
	case 0xb000:
		board->chr_banks0[(addr - 0x8000) >> 12].bank =
		    value;
		board_chr_sync(board, 0);
		break;
	case 0xc000:
		_nametable_bank0 = value | 0x80;
		if (_mirroring_reg & 0x11) {
			board->nmt_banks[0].type = MAP_TYPE_ROM;
			board->nmt_banks[0].bank = _nametable_bank0;
			board->nmt_banks[0].perms = MAP_PERM_READ;
			board->nmt_banks[1].type = MAP_TYPE_ROM;
			board->nmt_banks[1].bank = _nametable_bank0;
			board->nmt_banks[1].perms = MAP_PERM_READ;
		} else if (_mirroring_reg & 0x10) {
			board->nmt_banks[0].type = MAP_TYPE_ROM;
			board->nmt_banks[0].bank = _nametable_bank0;
			board->nmt_banks[0].perms = MAP_PERM_READ;
			board->nmt_banks[2].type = MAP_TYPE_ROM;
			board->nmt_banks[2].bank = _nametable_bank0;
			board->nmt_banks[2].perms = MAP_PERM_READ;
		}
		board_nmt_sync(board);
		break;
	case 0xd000:
		_nametable_bank1 = value | 0x80;
		if (_mirroring_reg & 0x11) {
			board->nmt_banks[2].type = MAP_TYPE_ROM;
			board->nmt_banks[2].bank = _nametable_bank1;
			board->nmt_banks[2].perms = MAP_PERM_READ;
			board->nmt_banks[3].type = MAP_TYPE_ROM;
			board->nmt_banks[3].bank = _nametable_bank1;
			board->nmt_banks[3].perms = MAP_PERM_READ;
		} else if (_mirroring_reg & 0x10) {
			board->nmt_banks[1].type = MAP_TYPE_ROM;
			board->nmt_banks[1].bank = _nametable_bank1;
			board->nmt_banks[1].perms = MAP_PERM_READ;
			board->nmt_banks[3].type = MAP_TYPE_ROM;
			board->nmt_banks[3].bank = _nametable_bank1;
			board->nmt_banks[3].perms = MAP_PERM_READ;
		}
		board_nmt_sync(board);
		break;
	case 0xe000:
		_mirroring_reg = value;
		switch (value & 0x11) {
		case 0x00:
			board_set_ppu_mirroring(board, MIRROR_V);
			break;
		case 0x01:
			board_set_ppu_mirroring(board, MIRROR_H);
			break;
		case 0x10:
			board->nmt_banks[0].type = MAP_TYPE_ROM;
			board->nmt_banks[0].bank = _nametable_bank0;
			board->nmt_banks[0].perms = MAP_PERM_READ;
			board->nmt_banks[1].type = MAP_TYPE_ROM;
			board->nmt_banks[1].bank = _nametable_bank1;
			board->nmt_banks[1].perms = MAP_PERM_READ;
			board->nmt_banks[2].type = MAP_TYPE_ROM;
			board->nmt_banks[2].bank = _nametable_bank0;
			board->nmt_banks[2].perms = MAP_PERM_READ;
			board->nmt_banks[3].type = MAP_TYPE_ROM;
			board->nmt_banks[3].bank = _nametable_bank1;
			board->nmt_banks[3].perms = MAP_PERM_READ;
			board_nmt_sync(board);
			break;
		case 0x11:
			board->nmt_banks[0].type = MAP_TYPE_ROM;
			board->nmt_banks[0].bank = _nametable_bank0;
			board->nmt_banks[0].perms = MAP_PERM_READ;
			board->nmt_banks[1].type = MAP_TYPE_ROM;
			board->nmt_banks[1].bank = _nametable_bank0;
			board->nmt_banks[1].perms = MAP_PERM_READ;
			board->nmt_banks[2].type = MAP_TYPE_ROM;
			board->nmt_banks[2].bank = _nametable_bank1;
			board->nmt_banks[2].perms = MAP_PERM_READ;
			board->nmt_banks[3].type = MAP_TYPE_ROM;
			board->nmt_banks[3].bank = _nametable_bank1;
			board->nmt_banks[3].perms = MAP_PERM_READ;
			board_nmt_sync(board);
			break;
		}
		break;
	case 0xf000:
		board->prg_banks[1].bank = value;
		board_prg_sync(board);
		break;
	}
}
