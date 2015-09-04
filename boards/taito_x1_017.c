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

static CPU_WRITE_HANDLER(taito_x1_017_write_handler);

static struct board_write_handler taito_x1_017_write_handlers[] = {
	{taito_x1_017_write_handler, 0x7ef0, 16, 0},
	{NULL},
};

static struct bank taito_x1_017_init_prg[] = {
	{0, 0, SIZE_8K, 0x8000, MAP_PERM_READ, MAP_TYPE_ROM},
	{0, 0, SIZE_8K, 0xa000, MAP_PERM_READ, MAP_TYPE_ROM},
	{0, 0, SIZE_8K, 0xc000, MAP_PERM_READ, MAP_TYPE_ROM},
	{-1, 0, SIZE_8K, 0xe000, MAP_PERM_READ, MAP_TYPE_ROM},
	{0, 0, SIZE_2K, 0x6000, MAP_PERM_NONE, MAP_TYPE_MAPPER_RAM},
	{1, 0, SIZE_2K, 0x6800, MAP_PERM_NONE, MAP_TYPE_MAPPER_RAM},
	{4, 0, SIZE_1K, 0x7000, MAP_PERM_NONE, MAP_TYPE_MAPPER_RAM},
	{.type = MAP_TYPE_END},
};

struct board_info board_taito_x1_017 = {
	.board_type = BOARD_TYPE_TAITO_X1_017,
	.name = "TAITO-X1-017",
	.init_prg = taito_x1_017_init_prg,
	.init_chr0 = std_chr_2k_1k,
	.write_handlers = taito_x1_017_write_handlers,
	.max_prg_rom_size = SIZE_512K,
	.max_chr_rom_size = SIZE_256K,
	.min_wram_size = { 0, 0 },
	.max_wram_size = { 0, 0 },
	.flags = BOARD_INFO_FLAG_MIRROR_M,
	.mapper_ram_size = SIZE_4K + SIZE_1K,
	.mirroring_values = std_mirroring_hv,
};

static CPU_WRITE_HANDLER(taito_x1_017_write_handler)
{
	struct board *board = emu->board;

	int wram_enable_magic[3] = { 0xca, 0x69, 0x84 };
	int wram_mode = MAP_PERM_NONE;

	switch (addr) {
	case 0x7ef0:
	case 0x7ef1:
	case 0x7ef2:
	case 0x7ef3:
	case 0x7ef4:
	case 0x7ef5:
		addr &= 7;
		if (board->chr_banks0[addr].bank != value) {
			board->chr_banks0[addr].bank = value;
			board_chr_sync(board, 0);
		}
		break;
	case 0x7ef6:
		standard_mirroring_handler(emu, addr, value, cycles);
		if (board->chr_mode != (value & 2)) {
			int i;

			board->chr_mode = value & 2;

			for (i = 0; i < 6; i++)
				board->chr_banks0[i].address ^=
				    0x1000;
		}
		break;
	case 0x7ef7:
	case 0x7ef8:
	case 0x7ef9:
		addr -= 0x7ef7;
		if (value == wram_enable_magic[addr])
			wram_mode = MAP_PERM_READWRITE;

		if (board->prg_banks[addr + 4].perms !=
		    wram_mode) {
			board->prg_banks[addr + 4].perms =
			    wram_mode;
			board_prg_sync(board);
		}
		break;
	case 0x7efa:
	case 0x7efb:
	case 0x7efc:
		addr -= 0x7efa;
		value >>= 2;
		if (board->prg_banks[addr].bank != value) {
			board->prg_banks[addr].bank = value;
			board_prg_sync(board);
		}
		break;
	case 0x7efd:
	case 0x7efe:
	case 0x7eff:
		break;
	}
}
