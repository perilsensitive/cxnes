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

static CPU_WRITE_HANDLER(taito_tc0190fmc_write_handler);
static CPU_WRITE_HANDLER(taito_tc0190fmc_timer_latch);

static int taito_tc0190fmc_init(struct board *board);
static void taito_tc0190fmc_reset(struct board *board, int);
static void taito_tc0190fmc_end_frame(struct board *board, uint32_t cycles);
static void taito_tc0190fmc_cleanup(struct board *board);
static int taito_tc0190fmc_load_state(struct board *board, struct save_state *state);
static int taito_tc0190fmc_save_state(struct board *board, struct save_state *state);

/* MMC3-ish CHR bank mappings.  The default bank values map the first
   8K in, and appears to be required by at least one demo.  MMC3-like
   chips tend to use this mapping (although the default values may not
   be the same).
*/
static struct bank taito_tc0190fmc_init_chr[] = {
	{0, 0, SIZE_2K, 0x0000, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{0, 0, SIZE_2K, 0x0800, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{0, 0, SIZE_1K, 0x1000, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{0, 0, SIZE_1K, 0x1400, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{0, 0, SIZE_1K, 0x1800, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{0, 0, SIZE_1K, 0x1c00, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{.type = MAP_TYPE_END},
};

static struct board_funcs taito_tc0190fmc_funcs = {
	.init = taito_tc0190fmc_init,
	.reset = taito_tc0190fmc_reset,
	.end_frame = taito_tc0190fmc_end_frame,
	.cleanup = taito_tc0190fmc_cleanup,
	.load_state = taito_tc0190fmc_load_state,
	.save_state = taito_tc0190fmc_save_state,
};

static struct board_write_handler taito_tc0190fmc_pal16r4_write_handlers[] = {
	{taito_tc0190fmc_write_handler, 0x8000, SIZE_16K, 0},
	{standard_mirroring_handler, 0xe000, SIZE_8K, 0xe003},
	{taito_tc0190fmc_timer_latch, 0xc000, SIZE_8K, 0xe003,},
	{a12_timer_irq_reload, 0xc001, SIZE_8K, 0xe003,},
	{a12_timer_irq_enable, 0xc002, SIZE_8K, 0xe003},
	{a12_timer_irq_disable, 0xc003, SIZE_8K, 0xe003},
	{NULL},
};

static struct board_write_handler taito_tc0190fmc_write_handlers[] = {
	{taito_tc0190fmc_write_handler, 0x8000, SIZE_16K, 0},
	{NULL},
};

struct board_info board_taito_tc0190fmc_pal16r4 = {
	.board_type = BOARD_TYPE_TAITO_TC0190FMC_PAL16R4,
	.name = "TAITO-TC0190FMC+PAL16R4",
	.funcs = &taito_tc0190fmc_funcs,
	.init_prg = std_prg_8k,
	.init_chr0 = taito_tc0190fmc_init_chr,
	.write_handlers = taito_tc0190fmc_pal16r4_write_handlers,
	.max_prg_rom_size = SIZE_256K,
	.max_chr_rom_size = SIZE_256K,
	.flags = BOARD_INFO_FLAG_MIRROR_M,
	.mirroring_values = std_mirroring_vh,
	.mirroring_shift = 6,
};

struct board_info board_taito_tc0190fmc = {
	.board_type = BOARD_TYPE_TAITO_TC0190FMC,
	.name = "TAITO-TC0190FMC/TC0350FMR",
	.funcs = &taito_tc0190fmc_funcs,
	.init_prg = std_prg_8k,
	.init_chr0 = taito_tc0190fmc_init_chr,
	.write_handlers = taito_tc0190fmc_write_handlers,
	.max_prg_rom_size = SIZE_256K,
	.max_chr_rom_size = SIZE_256K,
	.flags = BOARD_INFO_FLAG_MIRROR_M,
	.mirroring_values = std_mirroring_vh,
	.mirroring_shift = 6,
};

static CPU_WRITE_HANDLER(taito_tc0190fmc_timer_latch)
{
	value ^= 0xff;
	a12_timer_irq_latch(emu, addr, value, cycles);
}

static CPU_WRITE_HANDLER(taito_tc0190fmc_write_handler)
{
	struct board *board = emu->board;

	switch (addr & 0xe003) {
	case 0x8000:
		if (board->info->board_type == BOARD_TYPE_TAITO_TC0190FMC)
			standard_mirroring_handler(emu, addr, value, cycles);
	case 0x8001:
		update_prg_bank(board, (addr & 0x01) + 1, value);
		break;
	case 0x8002:
	case 0x8003:
		update_chr0_bank(board, (addr & 0x01), value);
		break;
	case 0xa000:
	case 0xa001:
	case 0xa002:
	case 0xa003:
		update_chr0_bank(board, ((addr & 0x03) + 2), value);
		break;
	}
}

static int taito_tc0190fmc_init(struct board *board)
{
	return a12_timer_init(board->emu, A12_TIMER_VARIANT_TAITO_TC0190FMC);
}

static void taito_tc0190fmc_reset(struct board *board, int hard)
{
	if (hard)
		a12_timer_set_counter_enabled(board->emu->a12_timer, 1, 0);

	a12_timer_reset(board->emu->a12_timer, hard);
}

static void taito_tc0190fmc_end_frame(struct board *board, uint32_t cycles)
{
	a12_timer_end_frame(board->emu->a12_timer, cycles);
}

static void taito_tc0190fmc_cleanup(struct board *board)
{
	a12_timer_cleanup(board->emu);
}

static int taito_tc0190fmc_load_state(struct board *board, struct save_state *state)
{
	return a12_timer_load_state(board->emu, state);
}

static int taito_tc0190fmc_save_state(struct board *board, struct save_state *state)
{
	return a12_timer_save_state(board->emu, state);
}
