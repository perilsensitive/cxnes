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

static void irem_74x161_161_21_138_reset(struct board *board, int hard)
{
	if (hard) {
		board->nmt_banks[0].type = MAP_TYPE_RAM0;
		board->nmt_banks[0].bank = 0;
		board->nmt_banks[0].perms = MAP_PERM_READWRITE;
		board->nmt_banks[1].type = MAP_TYPE_RAM0;
		board->nmt_banks[1].bank = 1;
		board->nmt_banks[1].perms = MAP_PERM_READWRITE;
		board->nmt_banks[2].type = MAP_TYPE_CIRAM;
		board->nmt_banks[2].bank = 0;
		board->nmt_banks[2].perms = MAP_PERM_READWRITE;
		board->nmt_banks[3].type = MAP_TYPE_CIRAM;
		board->nmt_banks[3].bank = 1;
		board->nmt_banks[3].perms = MAP_PERM_READWRITE;
		board->mirroring = MIRROR_M;
		board_nmt_sync(board);
	}
}

static struct board_funcs irem_74x161_161_21_138_funcs = {
	.reset = irem_74x161_161_21_138_reset,
};

static CPU_WRITE_HANDLER(irem_74x161_161_21_138_write_handler)
{
	update_prg_bank(emu->board, 1, (value & 0x0f));
	update_chr0_bank(emu->board, 0, (value & 0xf0) >> 4);
}

static struct bank irem_74x161_161_21_138_init_chr0[] = {
	{0, 0, SIZE_2K, 0x0000, MAP_PERM_READ, MAP_TYPE_ROM},
	{2, 0, SIZE_1K, 0x0800, MAP_PERM_READWRITE, MAP_TYPE_RAM0},
	{3, 0, SIZE_1K, 0x0c00, MAP_PERM_READWRITE, MAP_TYPE_RAM0},
	{4, 0, SIZE_1K, 0x1000, MAP_PERM_READWRITE, MAP_TYPE_RAM0},
	{5, 0, SIZE_1K, 0x1400, MAP_PERM_READWRITE, MAP_TYPE_RAM0},
	{6, 0, SIZE_1K, 0x1800, MAP_PERM_READWRITE, MAP_TYPE_RAM0},
	{7, 0, SIZE_1K, 0x1c00, MAP_PERM_READWRITE, MAP_TYPE_RAM0},
	{.type = MAP_TYPE_END},
};

static struct board_write_handler
irem_74x161_161_21_138_write_handlers[] = {
	{irem_74x161_161_21_138_write_handler, 0x8000, SIZE_32K, 0},
	{NULL},
};

struct board_info board_irem_74x161_161_21_138 = {
	.board_type = BOARD_TYPE_IREM_74x161_161_21_138,
	.funcs = &irem_74x161_161_21_138_funcs,
	.name = "IREM-74*161/161/21/138",
	.init_prg = std_prg_32k,
	.init_chr0 = irem_74x161_161_21_138_init_chr0,
	.write_handlers = irem_74x161_161_21_138_write_handlers,
	.max_prg_rom_size = SIZE_512K,
	.max_chr_rom_size = SIZE_32K,
	.max_wram_size = {SIZE_8K, 0},
	.min_vram_size = {SIZE_8K, 0},
	.max_vram_size = {SIZE_8K, 0},
	.flags = BOARD_INFO_FLAG_MIRROR_M,
};
