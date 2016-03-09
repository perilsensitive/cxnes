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
#include "vrc7_audio.h"
#include "m2_timer.h"

static CPU_WRITE_HANDLER(vrc7_write_handler);

static int vrc7_init(struct board *board);
static void vrc7_reset(struct board *board, int);
static void vrc7_cleanup(struct board *board);
static void vrc7_end_frame(struct board *board, uint32_t cycles);
static void vrc7_run(struct board *board, uint32_t cycles);
static int vrc7_load_state(struct board *board, struct save_state *state);
static int vrc7_save_state(struct board *board, struct save_state *state);

static struct board_funcs vrc7_funcs = {
	.init = vrc7_init,
	.reset = vrc7_reset,
	.cleanup = vrc7_cleanup,
	.end_frame = vrc7_end_frame,
	.run = vrc7_run,
	.load_state = vrc7_load_state,
	.save_state = vrc7_save_state,
};

struct board_write_handler vrc7_compat_write_handlers[] = {
	{vrc7_write_handler, 0x8000, SIZE_4K, 0xf018},
	{vrc7_write_handler, 0x8008, SIZE_4K, 0xf018},
	{vrc7_write_handler, 0x8010, SIZE_4K, 0xf018},
	{vrc7_write_handler, 0x9000, SIZE_4K, 0xf018},
	{vrc7_write_handler, 0xa000, SIZE_4K, 0xf018},
	{vrc7_write_handler, 0xa008, SIZE_4K, 0xf018},
	{vrc7_write_handler, 0xa010, SIZE_4K, 0xf018},
	{vrc7_write_handler, 0xb000, SIZE_4K, 0xf018},
	{vrc7_write_handler, 0xb008, SIZE_4K, 0xf018},
	{vrc7_write_handler, 0xb010, SIZE_4K, 0xf018},
	{vrc7_write_handler, 0xc000, SIZE_4K, 0xf018},
	{vrc7_write_handler, 0xc008, SIZE_4K, 0xf018},
	{vrc7_write_handler, 0xc010, SIZE_4K, 0xf018},
	{vrc7_write_handler, 0xd000, SIZE_4K, 0xf018},
	{vrc7_write_handler, 0xd008, SIZE_4K, 0xf018},
	{vrc7_write_handler, 0xd010, SIZE_4K, 0xf018},
	{vrc7_write_handler, 0xe000, SIZE_4K, 0xf018},
	{vrc7_write_handler, 0xe008, SIZE_4K, 0xf018},
	{vrc7_write_handler, 0xe010, SIZE_4K, 0xf018},
	{vrc7_write_handler, 0xf000, SIZE_4K, 0xf018},
	{vrc7_write_handler, 0xf008, SIZE_4K, 0xf018},
	{vrc7_write_handler, 0xf010, SIZE_4K, 0xf018},
	{NULL},
};

struct board_write_handler vrc7a_write_handlers[] = {
	{vrc7_write_handler, 0x8000, SIZE_4K, 0xf010},
	{vrc7_write_handler, 0x8010, SIZE_4K, 0xf010},
	{vrc7_write_handler, 0x9000, SIZE_4K, 0xf010},
	{vrc7_write_handler, 0xa000, SIZE_4K, 0xf010},
	{vrc7_write_handler, 0xa010, SIZE_4K, 0xf010},
	{vrc7_write_handler, 0xb000, SIZE_4K, 0xf010},
	{vrc7_write_handler, 0xb010, SIZE_4K, 0xf010},
	{vrc7_write_handler, 0xc000, SIZE_4K, 0xf010},
	{vrc7_write_handler, 0xc010, SIZE_4K, 0xf010},
	{vrc7_write_handler, 0xd000, SIZE_4K, 0xf010},
	{vrc7_write_handler, 0xd010, SIZE_4K, 0xf010},
	{vrc7_write_handler, 0xe000, SIZE_4K, 0xf010},
	{vrc7_write_handler, 0xe010, SIZE_4K, 0xf010},
	{vrc7_write_handler, 0xf000, SIZE_4K, 0xf010},
	{vrc7_write_handler, 0xf010, SIZE_4K, 0xf010},
	{NULL},
};

struct board_write_handler vrc7b_write_handlers[] = {
	{vrc7_write_handler, 0x8000, SIZE_4K, 0xf008},
	{vrc7_write_handler, 0x8008, SIZE_4K, 0xf008},
	{vrc7_write_handler, 0x9000, SIZE_4K, 0xf008},
	{vrc7_write_handler, 0xa000, SIZE_4K, 0xf008},
	{vrc7_write_handler, 0xa008, SIZE_4K, 0xf008},
	{vrc7_write_handler, 0xb000, SIZE_4K, 0xf008},
	{vrc7_write_handler, 0xb008, SIZE_4K, 0xf008},
	{vrc7_write_handler, 0xc000, SIZE_4K, 0xf008},
	{vrc7_write_handler, 0xc008, SIZE_4K, 0xf008},
	{vrc7_write_handler, 0xd000, SIZE_4K, 0xf008},
	{vrc7_write_handler, 0xd008, SIZE_4K, 0xf008},
	{vrc7_write_handler, 0xe000, SIZE_4K, 0xf008},
	{vrc7_write_handler, 0xe008, SIZE_4K, 0xf008},
	{vrc7_write_handler, 0xf000, SIZE_4K, 0xf008},
	{vrc7_write_handler, 0xf008, SIZE_4K, 0xf008},
	{NULL},
};

struct board_info board_vrc7_compat = {
	.board_type = BOARD_TYPE_VRC7_COMPAT,
	.name = "KONAMI-VRC-7",
	.funcs = &vrc7_funcs,
	.init_prg = std_prg_8k,
	.init_chr0 = std_chr_1k,
	.write_handlers = vrc7a_write_handlers,
	.max_prg_rom_size = SIZE_512K,
	.max_chr_rom_size = SIZE_256K,
	.max_wram_size = {SIZE_8K, 0},
	.flags = BOARD_INFO_FLAG_MIRROR_M |
	         BOARD_INFO_FLAG_M2_TIMER,
	.mirroring_values = std_mirroring_vh01,
};

struct board_info board_vrc7a = {
	.board_type = BOARD_TYPE_VRC7A,
	.name = "KONAMI-VRC-7A",
	.funcs = &vrc7_funcs,
	.init_prg = std_prg_8k,
	.init_chr0 = std_chr_1k,
	.write_handlers = vrc7a_write_handlers,
	.max_prg_rom_size = SIZE_512K,
	.max_chr_rom_size = SIZE_256K,
	.max_wram_size = {SIZE_8K, 0},
	.flags = BOARD_INFO_FLAG_MIRROR_M |
	         BOARD_INFO_FLAG_M2_TIMER,
	.mirroring_values = std_mirroring_vh01,
};

struct board_info board_vrc7b = {
	.board_type = BOARD_TYPE_VRC7B,
	.name = "KONAMI-VRC-7B",
	.funcs = &vrc7_funcs,
	.init_prg = std_prg_8k,
	.init_chr0 = std_chr_1k,
	.write_handlers = vrc7b_write_handlers,
	.max_prg_rom_size = SIZE_512K,
	.max_chr_rom_size = SIZE_256K,
	.max_wram_size = {SIZE_8K, 0},
	.flags = BOARD_INFO_FLAG_MIRROR_M |
	         BOARD_INFO_FLAG_M2_TIMER,
	.mirroring_values = std_mirroring_vh01,
};

static int vrc7_init(struct board *board)
{
	int rc;

	rc = vrc7_audio_init(board->emu);
	vrc7_audio_install_handlers(board->emu, 0);
	return rc;
}

static void vrc7_cleanup(struct board *board)
{
	vrc7_audio_cleanup(board->emu);
}

static void vrc7_end_frame(struct board *board, uint32_t cycles)
{
	vrc7_audio_end_frame(board->emu->vrc7_audio, cycles);
}

static void vrc7_run(struct board *board, uint32_t cycles)
{
	vrc7_audio_run(board->emu->vrc7_audio, cycles);
}

static void vrc7_reset(struct board *board, int hard)
{
	vrc7_audio_reset(board->emu->vrc7_audio, hard);
	if (hard) {
		m2_timer_set_flags(board->emu->m2_timer,
				   M2_TIMER_FLAG_COUNT_UP|
				   M2_TIMER_FLAG_RELOAD|
				   M2_TIMER_FLAG_PRESCALER |
				   M2_TIMER_FLAG_PRESCALER_RELOAD|
				   M2_TIMER_FLAG_IRQ_ON_RELOAD, 0);
		m2_timer_set_prescaler_reload(board->emu->m2_timer, 340, 0);
		m2_timer_set_prescaler(board->emu->m2_timer, 340, 0);
		m2_timer_set_prescaler_decrement(board->emu->m2_timer, 3, 0);
		m2_timer_set_enabled(board->emu->m2_timer, 0, 0);
		m2_timer_set_size(board->emu->m2_timer, 8, 0);
		board->prg_banks[0].perms = MAP_PERM_NONE;
	}

	board->prg_mode = 0;
	board->prg_and = 0xff;
	board->chr_and = 0xff;
}

static CPU_WRITE_HANDLER(vrc7_write_handler)
{
	struct board *board = emu->board;
	int timer_flags;

	addr = (addr & 0xf010) | ((addr & 0x08) << 1);

	switch (addr & 0xf010) {
	case 0x8000:
	case 0x8010:
		board->prg_banks[((addr & 0x10) >> 4) + 1].bank =
		    value;
		board_prg_sync(board);
		break;
	case 0x9000:
		board->prg_banks[3].bank = value;
		board_prg_sync(board);
		break;
	case 0xa000:
	case 0xa010:
		board->chr_banks0[((addr & 0x10) >> 4)].bank =
		    value;
		board_chr_sync(board, 0);
		break;
	case 0xb000:
	case 0xb010:
		board->chr_banks0[((addr & 0x10) >> 4) | 0x02].
		    bank = value;
		board_chr_sync(board, 0);
		break;
	case 0xc000:
	case 0xc010:
		board->chr_banks0[((addr & 0x10) >> 4) | 0x04].
		    bank = value;
		board_chr_sync(board, 0);
		break;
	case 0xd000:
	case 0xd010:
		board->chr_banks0[((addr & 0x10) >> 4) | 0x06].
		    bank = value;
		board_chr_sync(board, 0);
		break;
	case 0xe000:
		standard_mirroring_handler(emu, addr, value, cycles);
		vrc7_audio_write_handler(emu, addr, value, cycles);
		if (value & 0x80) {
			board->prg_banks[0].perms = MAP_PERM_READWRITE;
		} else {
			board->prg_banks[0].perms = MAP_PERM_NONE;
		}
		board_prg_sync(board);
		break;
	case 0xe010:
		m2_timer_set_reload(emu->m2_timer, value, cycles);
		break;
	case 0xf000:
		emu->board->irq_control = value;

		timer_flags = M2_TIMER_FLAG_COUNT_UP |
			M2_TIMER_FLAG_RELOAD |
			M2_TIMER_FLAG_IRQ_ON_RELOAD;

		if (!(value & 0x02)) {
			m2_timer_set_enabled(emu->m2_timer,
					     0, cycles);
			m2_timer_set_flags(emu->m2_timer,
					   timer_flags, cycles);
			return;
		}

		if (!(value & 0x04)) {
			m2_timer_set_prescaler_reload(emu->m2_timer,
						      340, cycles);
			m2_timer_set_prescaler(emu->m2_timer, 340,
					       cycles);
			m2_timer_set_prescaler_decrement(emu->m2_timer,
							 3, cycles);
			timer_flags |= M2_TIMER_FLAG_PRESCALER |
				M2_TIMER_FLAG_PRESCALER_RELOAD;
		}

		m2_timer_set_flags(emu->m2_timer, timer_flags, cycles);
		m2_timer_force_reload(emu->m2_timer, cycles);
		m2_timer_set_enabled(emu->m2_timer, value & 0x02,
				     cycles);
		break;
	case 0xf010:
		m2_timer_ack(emu->m2_timer, cycles);
		if (emu->board->irq_control & 0x01) {
			m2_timer_set_enabled(emu->m2_timer, 1, cycles);
			m2_timer_schedule_irq(emu->m2_timer, cycles);
		} else {
			m2_timer_set_enabled(emu->m2_timer, 0, cycles);
		}
		break;
	}
}

static int vrc7_load_state(struct board *board, struct save_state *state)
{
	int rc;
	rc = vrc7_audio_load_state(board->emu, state);

	return rc;
}

static int vrc7_save_state(struct board *board, struct save_state *state)
{
	int rc;

	rc = vrc7_audio_save_state(board->emu, state);

	return rc;
}
