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
#include "a12_timer.h"
#include "m2_timer.h"
#include "mmc3.h"

static struct board_write_handler btl_superbros11_write_handlers[] = {
	{mmc3_bank_select, 0x8000, SIZE_8K, 0x8004},
	{mmc3_bank_data, 0x8004, SIZE_8K, 0x8004},
	{standard_mirroring_handler, 0xa000, SIZE_8K, 0xa004},
	{mmc3_wram_protect, 0xa004, SIZE_8K, 0xa004},
	{a12_timer_irq_latch, 0xc000, SIZE_8K, 0xc004},
	{a12_timer_irq_reload, 0xc004, SIZE_8K, 0xc004},
	{a12_timer_irq_disable, 0xe000, SIZE_8K, 0xe004},
	{a12_timer_irq_enable, 0xe004, SIZE_8K, 0xe004},
	{NULL}
};

struct board_info board_btl_superbros11 = {
	.board_type = BOARD_TYPE_BTL_SUPERBROS11,
	.name = "BTL-SUPERBROS11",
	.funcs = &mmc3_funcs,
	.init_prg = mmc3_init_prg,
	.init_chr0 = mmc3_init_chr0,
	.write_handlers = btl_superbros11_write_handlers,
	.max_prg_rom_size = SIZE_2048K,
	.max_chr_rom_size = SIZE_256K,
	.max_wram_size = {SIZE_8K, 0},
	.flags = BOARD_INFO_FLAG_MIRROR_M,
	.mirroring_values = std_mirroring_vh,
};
