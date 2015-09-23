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

static int singlechip_init(struct board *board);

static struct board_funcs singlechip_funcs = {
	.init = singlechip_init,
};

/* FIXME not sure what the default bank values should be */
static struct bank singlechip_init_chr0[] = {
	{0, 0, SIZE_1K, 0x0000, MAP_PERM_READWRITE, MAP_TYPE_CIRAM},
	{0, 0, SIZE_1K, 0x0400, MAP_PERM_READWRITE, MAP_TYPE_CIRAM},
	{0, 0, SIZE_1K, 0x0800, MAP_PERM_READWRITE, MAP_TYPE_CIRAM},
	{0, 0, SIZE_1K, 0x0c00, MAP_PERM_READWRITE, MAP_TYPE_CIRAM},
	{1, 0, SIZE_1K, 0x1000, MAP_PERM_READWRITE, MAP_TYPE_CIRAM},
	{1, 0, SIZE_1K, 0x1400, MAP_PERM_READWRITE, MAP_TYPE_CIRAM},
	{1, 0, SIZE_1K, 0x1800, MAP_PERM_READWRITE, MAP_TYPE_CIRAM},
	{1, 0, SIZE_1K, 0x1c00, MAP_PERM_READWRITE, MAP_TYPE_CIRAM},
	{.type = MAP_TYPE_END},
};

struct board_info board_singlechip = {
	.board_type = BOARD_TYPE_SINGLECHIP,
	.name = "Single Chip",
	.funcs = &singlechip_funcs,
	.init_prg = std_prg_32k,
	.init_chr0 = singlechip_init_chr0,
	.max_prg_rom_size = SIZE_32K,
};

static int singlechip_init(struct board *board)
{
	switch (board->mirroring) {
	case MIRROR_H:
	case MIRROR_V:
		/* Default setup is correct for both of these */
		break;
	case MIRROR_1A:
		break;
	case MIRROR_1B:
		singlechip_init_chr0[4].bank = 0;
		singlechip_init_chr0[5].bank = 0;
		singlechip_init_chr0[6].bank = 0;
		singlechip_init_chr0[7].bank = 0;
		break;
	}

	return 0;
}
