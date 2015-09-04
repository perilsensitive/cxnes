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

#define _register_select board->data[0]
#define _prg_bank board->data[1]
#define _mirror_mode board->data[2]
#define _chr_a10 board->data[3]

static void streemerz_bundle_reset(struct board *board, int);
static CPU_WRITE_HANDLER(streemerz_bundle_write_handler);

static struct board_funcs streemerz_bundle_funcs = {
	.reset = streemerz_bundle_reset,
};

static struct board_write_handler streemerz_write_handlers[] = {
	{streemerz_bundle_write_handler, 0x5000, SIZE_4K, 0},
	{streemerz_bundle_write_handler, 0x8000, SIZE_32K, 0},
	{NULL},
};

struct bank streemerz_bundle_init_prg[] = {
	{0, 0, SIZE_16K, 0x8000, MAP_PERM_READ, MAP_TYPE_ROM},
	{-1, 0, SIZE_16K, 0xc000, MAP_PERM_READ, MAP_TYPE_ROM},
	{.type = MAP_TYPE_END},
};

static uint8_t streemerz_bundle_mirroring[4] = { MIRROR_1A, MIRROR_1B,
						 MIRROR_V,  MIRROR_H };

struct board_info board_streemerz_bundle = {
	.board_type = BOARD_TYPE_STREEMERZ_BUNDLE,
	.name = "Streemerz Bundle",
	.funcs = &streemerz_bundle_funcs,
	.init_prg = streemerz_bundle_init_prg,
	.init_chr0 = std_chr_8k,
	.write_handlers = streemerz_write_handlers,
	.max_prg_rom_size = SIZE_8192K,
	.max_chr_rom_size = 0,
	.min_vram_size = {SIZE_32K, 0},
	.max_vram_size = {SIZE_32K, 0},
	.flags = BOARD_INFO_FLAG_MIRROR_M,
	.mirroring_values = streemerz_bundle_mirroring,
};

static void streemerz_bundle_reset(struct board *board, int hard)
{
/* XXX move this into board_info now that I'm not tied down to using the macro */
	if (hard)
		board->prg_or = 0x1fe;
}

static CPU_WRITE_HANDLER(streemerz_bundle_write_handler)
{
	struct board *board = emu->board;

	int new_prg_and;
	int prg_updated = 0;

	if ((addr & 0xf000) == 0x5000) {
		_register_select = value;
		return;
	}

	switch (_register_select & 0x81) {
	case 0x00:
		board->chr_banks0[0].bank = value;
		_chr_a10 = value & 0x10;
		board_chr_sync(board, 0);
		if (_mirror_mode < 2) {
			standard_mirroring_handler(emu, addr, !!_chr_a10,
						   cycles);
		}
		break;
	case 0x01:
		_prg_bank = value;
		prg_updated = 1;
		_chr_a10 = value & 0x10;
		if (_mirror_mode < 2) {
			standard_mirroring_handler(emu, addr, !!_chr_a10,
						   cycles);
		}
		break;
	case 0x80:
		_mirror_mode = value & 0x03;
		standard_mirroring_handler(emu, addr, value, cycles);

		if ((value & 0x0c) != board->prg_mode) {
			board->prg_mode = value & 0x0c;
			prg_updated = 1;
		}

		new_prg_and = 0;
		switch (value & 0x30) {
		case 0x00:
			new_prg_and = 0x01;
			break;
		case 0x10:
			new_prg_and = 0x03;
			break;
		case 0x20:
			new_prg_and = 0x07;
			break;
		case 0x30:
			new_prg_and = 0x0f;
			break;
		}

		if (new_prg_and != board->prg_and) {
			board->prg_and = new_prg_and;
			prg_updated = 1;
		}
		break;
	case 0x81:
		board->prg_or = (uint32_t) value << 1;
		prg_updated = 1;
		break;
	}

	if (prg_updated) {
		switch (board->prg_mode) {
		case 0x00:
		case 0x04:
			board->prg_banks[0].address = 0x8000;
			board->prg_banks[1].address = 0xc000;
			board->prg_banks[0].bank = _prg_bank << 1;
			board->prg_banks[1].bank =
			    (_prg_bank << 1) | 1;
			break;
		case 0x08:
			board->prg_banks[0].address = 0xc000;
			board->prg_banks[1].address = 0x8000;
			board->prg_banks[0].bank = _prg_bank;
			board->prg_banks[1].bank =
			    board->prg_or;
			break;
		case 0x0c:
			board->prg_banks[0].address = 0x8000;
			board->prg_banks[1].address = 0xc000;
			board->prg_banks[0].bank = _prg_bank;
			board->prg_banks[1].bank =
			    board->prg_or | 1;
			break;
		}

		board_prg_sync(board);
	}

}
