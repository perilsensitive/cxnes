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

static CPU_WRITE_HANDLER(unl_ks7057_write_handler);

static struct bank unl_ks7057_init_prg[] = {
	{0x00, 0, SIZE_2K,  0x8000, MAP_PERM_READ, MAP_TYPE_ROM},
	{0x00, 0, SIZE_2K,  0x8800, MAP_PERM_READ, MAP_TYPE_ROM},
	{0x00, 0, SIZE_2K,  0x9000, MAP_PERM_READ, MAP_TYPE_ROM},
	{0x00, 0, SIZE_2K,  0x9800, MAP_PERM_READ, MAP_TYPE_ROM},
	{0x00, 0, SIZE_2K,  0x6000, MAP_PERM_READ, MAP_TYPE_ROM},
	{0x00, 0, SIZE_2K,  0x6800, MAP_PERM_READ, MAP_TYPE_ROM},
	{0x00, 0, SIZE_2K,  0x7000, MAP_PERM_READ, MAP_TYPE_ROM},
	{0x00, 0, SIZE_2K,  0x7800, MAP_PERM_READ, MAP_TYPE_ROM},
	{0x0d, 0, SIZE_8K,  0xa000, MAP_PERM_READ, MAP_TYPE_ROM},
	{0x07, 0, SIZE_16K, 0xc000, MAP_PERM_READ, MAP_TYPE_ROM},
	{.type = MAP_TYPE_END},
};

static struct board_write_handler unl_ks7057_write_handlers[] = {
	{standard_mirroring_handler, 0x8003, SIZE_8K, 0x8003},
	{unl_ks7057_write_handler, 0xb000, SIZE_16K + SIZE_4K, 0},
	{NULL},
};

struct board_info board_unl_ks7057 = {
	.board_type = BOARD_TYPE_UNL_KS7057,
	.name = "UNL-KS7057",
	.init_prg = unl_ks7057_init_prg,
	.init_chr0 = std_chr_8k,
	.write_handlers = unl_ks7057_write_handlers,
	.max_prg_rom_size = SIZE_128K,
	.max_chr_rom_size = SIZE_8K,
	.flags = BOARD_INFO_FLAG_MIRROR_M,
	.mirroring_shift = 2,
	.mirroring_values = std_mirroring_hv,
};

static CPU_WRITE_HANDLER(unl_ks7057_write_handler)
{
	struct board *board = emu->board;
	int slot;
	int bank;

	slot = (((addr & 0xf000) - 0xb000) >> 11) | ((addr & 0x02) >> 1);

	if (addr & 1) {
		bank = board->prg_banks[slot].bank & 0x0f;
		bank |= value << 4;
	} else {
		bank = board->prg_banks[slot].bank & 0xf0;
		bank |= (value & 0x0f);
	}

	update_prg_bank(board, slot, bank);
}
