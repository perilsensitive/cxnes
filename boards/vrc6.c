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
#include "vrc6_audio.h"
#include "m2_timer.h"

static CPU_WRITE_HANDLER(vrc6_write_handler);
static CPU_WRITE_HANDLER(vrc6_even_chr_handler);
static CPU_WRITE_HANDLER(vrc6_odd_chr_handler);

static int vrc6_init(struct board *board);
static void vrc6_reset(struct board *board, int);
static void vrc6_cleanup(struct board *board);
static void vrc6_end_frame(struct board *board, uint32_t cycles);
static void vrc6_run(struct board *board, uint32_t cycles);
static int vrc6_load_state(struct board *board, struct save_state *state);
static int vrc6_save_state(struct board *board, struct save_state *state);

static struct bank vrc6_init_prg[] = {
	{0, 0, SIZE_8K, 0x6000, MAP_PERM_READWRITE, MAP_TYPE_RAM0},
	{0, 0, SIZE_16K, 0x8000, MAP_PERM_READ, MAP_TYPE_ROM},
	{0xfe, 0, SIZE_8K, 0xc000, MAP_PERM_READ, MAP_TYPE_ROM},
	{0xff, 0, SIZE_8K, 0xe000, MAP_PERM_READ, MAP_TYPE_ROM},
	{.type = MAP_TYPE_END},
};

static struct board_write_handler vrc6a_write_handlers[] = {
	{vrc6_write_handler, 0x8000, SIZE_4K, 0},
	{vrc6_write_handler, 0xb000, SIZE_4K, 0},
	{vrc6_write_handler, 0xc000, SIZE_4K, 0},
	{vrc6_even_chr_handler, 0xd000, SIZE_4K, 0xf003},
	{vrc6_even_chr_handler, 0xd002, SIZE_4K, 0xf003},
	{vrc6_even_chr_handler, 0xe000, SIZE_4K, 0xf003},
	{vrc6_even_chr_handler, 0xe002, SIZE_4K, 0xf003},
	{vrc6_odd_chr_handler, 0xd001, SIZE_4K, 0xf003},
	{vrc6_odd_chr_handler, 0xd003, SIZE_4K, 0xf003},
	{vrc6_odd_chr_handler, 0xe001, SIZE_4K, 0xf003},
	{vrc6_odd_chr_handler, 0xe003, SIZE_4K, 0xf003},
	{vrc6_write_handler, 0xf000, SIZE_4K, 0},
	{NULL},
};

static struct board_write_handler vrc6b_write_handlers[] = {
	{vrc6_write_handler, 0x8000, SIZE_4K, 0},
	{vrc6_write_handler, 0xb003, SIZE_4K, 0xf003},
	{vrc6_write_handler, 0xc000, SIZE_4K, 0},
	{vrc6_even_chr_handler, 0xd000, SIZE_4K, 0xf003},
	{vrc6_even_chr_handler, 0xd001, SIZE_4K, 0xf003},
	{vrc6_even_chr_handler, 0xe000, SIZE_4K, 0xf003},
	{vrc6_even_chr_handler, 0xe001, SIZE_4K, 0xf003},
	{vrc6_odd_chr_handler, 0xd002, SIZE_4K, 0xf003},
	{vrc6_odd_chr_handler, 0xd003, SIZE_4K, 0xf003},
	{vrc6_odd_chr_handler, 0xe002, SIZE_4K, 0xf003},
	{vrc6_odd_chr_handler, 0xe003, SIZE_4K, 0xf003},
	{vrc6_write_handler, 0xf000, SIZE_4K, 0},
	{NULL},
};

static struct board_funcs vrc6_funcs = {
	.init = vrc6_init,
	.reset = vrc6_reset,
	.cleanup = vrc6_cleanup,
	.end_frame = vrc6_end_frame,
	.run = vrc6_run,
	.load_state = vrc6_load_state,
	.save_state = vrc6_save_state,
};

struct board_info board_vrc6a = {
	.board_type = BOARD_TYPE_VRC6A,
	.name = "KONAMI-VRC-6A",
	.funcs = &vrc6_funcs,
	.init_prg = vrc6_init_prg,
	.init_chr0 = std_chr_1k,
	.write_handlers = vrc6a_write_handlers,
	.max_prg_rom_size = SIZE_256K,
	.max_chr_rom_size = SIZE_256K,
	.max_wram_size = {SIZE_8K, 0},
	.flags = BOARD_INFO_FLAG_MIRROR_M |
	         BOARD_INFO_FLAG_M2_TIMER,
};

struct board_info board_vrc6b = {
	.board_type = BOARD_TYPE_VRC6B,
	.name = "KONAMI-VRC-6B",
	.funcs = &vrc6_funcs,
	.init_prg = vrc6_init_prg,
	.init_chr0 = std_chr_1k,
	.write_handlers = vrc6b_write_handlers,
	.max_prg_rom_size = SIZE_256K,
	.max_chr_rom_size = SIZE_256K,
	.max_wram_size = {SIZE_8K, 0},
	.flags = BOARD_INFO_FLAG_MIRROR_M |
	         BOARD_INFO_FLAG_M2_TIMER,
};

static int vrc6_init(struct board *board)
{
	int rc;

	rc = vrc6_audio_init(board->emu);
	vrc6_audio_install_handlers(board->emu, 0);
	return rc;
}

static void vrc6_cleanup(struct board *board)
{
	vrc6_audio_cleanup(board->emu);
}

static void vrc6_reset(struct board *board, int hard)
{
	vrc6_audio_reset(board->emu->vrc6_audio, hard);
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
	}
}

static void vrc6_end_frame(struct board *board, uint32_t cycles)
{
	vrc6_audio_end_frame(board->emu->vrc6_audio, cycles);
}

static void vrc6_run(struct board *board, uint32_t cycles)
{
	vrc6_audio_run(board->emu->vrc6_audio, cycles);
}

static CPU_WRITE_HANDLER(vrc6_write_handler)
{
	struct board *board = emu->board;
	int timer_flags;

	addr &= 0xf003;

	switch (addr) {
	case 0x8000:
	case 0x8001:
	case 0x8002:
	case 0x8003:
		board->prg_banks[1].bank = value;
		board_prg_sync(board);
		break;
	case 0xc000:
	case 0xc001:
	case 0xc002:
	case 0xc003:
		board->prg_banks[2].bank = value;
		board_prg_sync(board);
		break;
	case 0xb000:
	case 0xb001:
	case 0xb002:
	case 0xb003:
		switch (value & 0xc) {
		case 0x0:
			value = MIRROR_V;
			break;
		case 0x4:
			value = MIRROR_H;
			break;
		case 0x8:
			value = MIRROR_1A;
			break;
		case 0xc:
			value = MIRROR_1B;
			break;
		}

		board_set_ppu_mirroring(board, value);
		break;
	case 0xf000:
		m2_timer_set_reload(emu->m2_timer, value, cycles);
		break;
	case 0xf001:
	case 0xf002:
		if (board->info->board_type == BOARD_TYPE_VRC6B)
			addr ^= 0x03;

		if (addr == 0xf001) {
			emu->board->irq_control = value;

			m2_timer_ack(emu->m2_timer, cycles);

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
		} else {
			m2_timer_ack(emu->m2_timer, cycles);
			if (emu->board->irq_control & 0x01) {
				m2_timer_set_enabled(emu->m2_timer, 1, cycles);
				m2_timer_schedule_irq(emu->m2_timer, cycles);
			} else {
				m2_timer_set_enabled(emu->m2_timer, 0, cycles);
			}
		}
		break;
	}

}

static CPU_WRITE_HANDLER(vrc6_even_chr_handler)
{
	int slot;

	slot = 0;
	if (addr & 0x03)
		slot = 2;

	if ((addr & 0xf000) == 0xe000) {
		slot += 4;
	}

	emu->board->chr_banks0[slot].bank = value;
	board_chr_sync(emu->board, 0);
}

static CPU_WRITE_HANDLER(vrc6_odd_chr_handler)
{
	int slot;

	slot = 1;
	if ((addr & 0x03) == 3)
		slot = 3;

	if ((addr & 0xf000) == 0xe000) {
		slot += 4;
	}

	emu->board->chr_banks0[slot].bank = value;
	board_chr_sync(emu->board, 0);
}

static int vrc6_load_state(struct board *board, struct save_state *state)
{
	int rc;
	rc = vrc6_audio_load_state(board->emu, state);

	return rc;
}

static int vrc6_save_state(struct board *board, struct save_state *state)
{
	int rc;

	rc = vrc6_audio_save_state(board->emu, state);

	return rc;
}
