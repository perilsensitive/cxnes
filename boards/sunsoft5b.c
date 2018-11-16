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
#include "sunsoft5b_audio.h"
#include "m2_timer.h"

#define SUNSOFT5B_IRQ_ENABLE         0x01
#define SUNSOFT5B_IRQ_COUNTER_ENABLE 0x80

#define _command_reg board->data[0]
#define _parameter_reg board->data[1]

static int sunsoft5b_init(struct board *board);
static void sunsoft5b_reset(struct board *board, int);
static CPU_WRITE_HANDLER(sunsoft5b_write_handler);
static void sunsoft5b_end_frame(struct board *board, uint32_t cycles);
static void sunsoft5b_run(struct board *board, uint32_t cycles);
static int sunsoft5b_load_state(struct board *board, struct save_state *state);
static int sunsoft5b_save_state(struct board *board, struct save_state *state);

static struct board_funcs sunsoft5b_funcs = {
	.init = sunsoft5b_init,
	.reset = sunsoft5b_reset,
	.end_frame = sunsoft5b_end_frame,
	.run = sunsoft5b_run,
	.load_state = sunsoft5b_load_state,
	.save_state = sunsoft5b_save_state,
};

static struct board_write_handler sunsoft5b_write_handlers[] = {
	{sunsoft5b_write_handler, 0x8000, SIZE_32K, 0},
	{NULL},
};
/*
  NES-BTR  // one has 8k wram, the other doesn't
  NES-JLROM
  NES-JSROM // has 8k wram
  SUNSOFT-5A
  SUNSOFT-5B
  SUNSOFT-FME-7 // some games with this have wram some with batt, some not
  
 */

struct board_info board_sunsoft5b = {
	.board_type = BOARD_TYPE_SUNSOFT5B,
	.name = "SUNSOFT-5B",
	.mapper_name = "SUNSOFT-5B",
	.funcs = &sunsoft5b_funcs,
	.mirroring_values = std_mirroring_vh01,
	.init_prg = std_prg_8k,
	.init_chr0 = std_chr_1k,
	.write_handlers = sunsoft5b_write_handlers,
	.max_prg_rom_size = SIZE_2048K,
	.max_chr_rom_size = SIZE_256K,
	.max_wram_size = {SIZE_256K, 0},
	.flags = BOARD_INFO_FLAG_MIRROR_M |
	         BOARD_INFO_FLAG_M2_TIMER,
};

static int sunsoft5b_init(struct board *board)
{
	int rc;

	rc = sunsoft5b_audio_init(board->emu);
	sunsoft5b_audio_install_handlers(board->emu, 0);

	return rc;
}

static void sunsoft5b_run(struct board *board, uint32_t cycles)
{
	sunsoft5b_audio_run(board->emu->sunsoft5b_audio, cycles);
}

static void sunsoft5b_process_command(struct board *board, uint32_t cycles)
{
	int type;
	uint8_t value = _parameter_reg;
	struct emu *emu;

	emu = board->emu;

	switch (_command_reg) {
	case 0x00:
	case 0x01:
	case 0x02:
	case 0x03:
	case 0x04:
	case 0x05:
	case 0x06:
	case 0x07:
		board->chr_banks0[_command_reg].bank = value;
		board_chr_sync(board, 0);
		break;
	case 0x08:
		type = MAP_TYPE_ROM;
		if (value & 0x40) {
			type = MAP_TYPE_AUTO;
			if (!(value & 0x80))
				type = MAP_TYPE_NONE;
		}

		board->prg_banks[0].bank = value & 0x3f;
		board->prg_banks[0].type = type;
		board_prg_sync(board);
		break;
	case 0x09:
	case 0x0a:
	case 0x0b:
		board->prg_banks[_command_reg - 0x09 + 1].bank =
		    value;
		board_prg_sync(board);
		break;
	case 0x0c:
		standard_mirroring_handler(emu, 0, value, cycles);
		break;
	case 0x0d:
		value &= 0x81;
		m2_timer_ack(emu->m2_timer, cycles);
		m2_timer_set_irq_enabled(emu->m2_timer,
					 value & SUNSOFT5B_IRQ_ENABLE,
					 cycles);
		m2_timer_set_counter_enabled(emu->m2_timer,
					     value & SUNSOFT5B_IRQ_COUNTER_ENABLE,
					     cycles);
					     
		
		break;
	case 0x0e:
		m2_timer_set_counter_lo(emu->m2_timer, value, cycles);
		break;
	case 0x0f:
		m2_timer_set_counter_hi(emu->m2_timer, value, cycles);
		break;
	}
}

static CPU_WRITE_HANDLER(sunsoft5b_write_handler)
{
	struct board *board = emu->board;

	addr &= 0xe000;

	switch (addr) {
	case 0x8000:
		_command_reg = value & 0xf;
		break;
	case 0xa000:
		_parameter_reg = value;
		sunsoft5b_process_command(board, cycles);
		break;
	case 0xc000:
	case 0xe000:
		/* Sound not yet supported */
		break;
	}
}

static void sunsoft5b_reset(struct board *board, int hard)
{
	if (hard) {
		m2_timer_set_counter_enabled(board->emu->m2_timer, 0, 0);
		m2_timer_set_irq_enabled(board->emu->m2_timer, 0, 0);
	}
	sunsoft5b_audio_reset(board->emu->sunsoft5b_audio, hard);
}

static void sunsoft5b_end_frame(struct board *board, uint32_t cycles)
{
	sunsoft5b_audio_end_frame(board->emu->sunsoft5b_audio, cycles);
}

static int sunsoft5b_load_state(struct board *board, struct save_state *state)
{
	return sunsoft5b_audio_load_state(board->emu, state);
}

static int sunsoft5b_save_state(struct board *board, struct save_state *state)
{
	return sunsoft5b_audio_save_state(board->emu, state);
}
