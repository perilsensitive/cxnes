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

#define _x1_005_ram_protect board->data[0]

static CPU_WRITE_HANDLER(taito_x1_005_write_handler);
static CPU_READ_HANDLER(taito_x1_005_read_handler);

struct bank taito_x1_005_prg[] = {
	{0, 0, SIZE_8K, 0x8000, MAP_PERM_READ, MAP_TYPE_ROM},
	{0, 0, SIZE_8K, 0xa000, MAP_PERM_READ, MAP_TYPE_ROM},
	{0, 0, SIZE_8K, 0xc000, MAP_PERM_READ, MAP_TYPE_ROM},
	{-1, 0, SIZE_8K, 0xe000, MAP_PERM_READ, MAP_TYPE_ROM},
	{.type = MAP_TYPE_END},
};

static struct board_write_handler taito_x1_005_alt_write_handlers[] = {
	{taito_x1_005_write_handler, 0x7ef0, 16 + 256, 0},
	{NULL},
};

static struct board_write_handler taito_x1_005_write_handlers[] = {
	{taito_x1_005_write_handler, 0x7ef0, 6, 0},
	{standard_mirroring_handler, 0x7ef6, 2, 0},
	{taito_x1_005_write_handler, 0x7ef8, 8 + 256, 0},
	{NULL},
};

static struct board_read_handler taito_x1_005_read_handlers[] = {
	{taito_x1_005_read_handler, 0x7f00, 256, 0},
	{NULL},
};

struct board_info board_taito_x1_005 = {
	.board_type = BOARD_TYPE_TAITO_X1_005,
	.name = "TAITO-X1-005",
	.init_prg = taito_x1_005_prg,
	.init_chr0 = std_chr_2k_1k,
	.read_handlers = taito_x1_005_read_handlers,
	.write_handlers = taito_x1_005_write_handlers,
	.max_prg_rom_size = SIZE_512K,
	.max_chr_rom_size = SIZE_256K,
	.flags = BOARD_INFO_FLAG_MIRROR_M,
	.mapper_ram_size = 128,
	.mirroring_values = std_mirroring_hv,
};

/* FIXME: the games that use this have the board type TAITO-X1-005, so
   obviously they conflict with the above board.
*/
   
struct board_info board_taito_x1_005_alt = {
	.board_type = BOARD_TYPE_TAITO_X1_005_ALT,
	.name = "TAITO-X1-005-ALT-MIRRORING",
	.init_prg = taito_x1_005_prg,
	.init_chr0 = std_chr_2k_1k,
	.read_handlers = taito_x1_005_read_handlers,
	.write_handlers = taito_x1_005_alt_write_handlers,
	.max_prg_rom_size = SIZE_512K,
	.max_chr_rom_size = SIZE_256K,
	.flags = BOARD_INFO_FLAG_MIRROR_M,
	.mapper_ram_size = 128,
};

static CPU_READ_HANDLER(taito_x1_005_read_handler)
{
	struct board *board = emu->board;

      if ((addr >= 0x7f00) && (_x1_005_ram_protect == 0xa3)) {
	      addr &= 0x7f;
	      value = board->mapper_ram.data[addr];
      }

	return value;
}

static CPU_WRITE_HANDLER(taito_x1_005_write_handler)
{
	struct board *board = emu->board;

	if ((addr >= 0x7f00) && (_x1_005_ram_protect == 0xa3) ) {
		addr &= 0x7f;
		board->mapper_ram.data[addr] = value;
		return;
	}

	switch (addr) {
	case 0x7ef0:
	case 0x7ef1:
		if (board->info->board_type == BOARD_TYPE_TAITO_X1_005_ALT) {
			int bank = (addr & 1) << 1;
			board->nmt_banks[bank].bank =
			    (value >> 7) & 1;
			board->nmt_banks[bank + 1].bank =
			    (value >> 7) & 1;
			board_nmt_sync(board);
		}
	case 0x7ef2:
	case 0x7ef3:
	case 0x7ef4:
	case 0x7ef5:
		addr &= 7;
		//if (board->chr_banks0[addr].bank != value) {
		board->chr_banks0[addr].bank = value;
		board_chr_sync(board, 0);
		//}
		break;
	case 0x7ef8:
	case 0x7ef9:
		//printf("protect: %x\n", value);
		_x1_005_ram_protect = value;
		break;
	case 0x7efa:
	case 0x7efb:
	case 0x7efc:
	case 0x7efd:
	case 0x7efe:
	case 0x7eff:
		addr -= 0x7efa;
		addr >>= 1;
		if (board->prg_banks[addr].bank != value) {
			board->prg_banks[addr].bank = value;
			board_prg_sync(board);
		}
		break;
	}
}
