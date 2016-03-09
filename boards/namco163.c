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
#include "namco163_audio.h"
#include "m2_timer.h"

#define _prg_select2 board->data[0]

static int namco163_init(struct board *board);
static void namco163_reset(struct board *board, int hard);
static void namco163_cleanup(struct board *board);
static void namco163_end_frame(struct board *board, uint32_t cycles);
static void namco163_run(struct board *board, uint32_t cycles);
static CPU_WRITE_HANDLER(namco163_write_handler);
static CPU_WRITE_HANDLER(namco175_write_handler);
static CPU_READ_HANDLER(namco163_read_handler);
static int namco163_load_state(struct board *board, struct save_state *state);
static int namco163_save_state(struct board *board, struct save_state *state);

static struct bank namco163_init_prg[] = {
	{0, 0, SIZE_2K, 0x6000, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{1, 0, SIZE_2K, 0x6800, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{2, 0, SIZE_2K, 0x7000, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{3, 0, SIZE_2K, 0x7800, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{0, 0, SIZE_8K, 0x8000, MAP_PERM_READ, MAP_TYPE_ROM},
	{0, 0, SIZE_8K, 0xa000, MAP_PERM_READ, MAP_TYPE_ROM},
	{-2, 0, SIZE_8K, 0xc000, MAP_PERM_READ, MAP_TYPE_ROM},
	{-1, 0, SIZE_8K, 0xe000, MAP_PERM_READ, MAP_TYPE_ROM},
	{.type = MAP_TYPE_END},
};

static struct board_funcs namco163_funcs = {
	.init = namco163_init,
	.reset = namco163_reset,
	.cleanup = namco163_cleanup,
	.end_frame = namco163_end_frame,
	.run = namco163_run,
	.load_state = namco163_load_state,
	.save_state = namco163_save_state,
};

static struct board_write_handler namco163_write_handlers[] = {
	{namco163_write_handler, 0x5000, SIZE_4K, 0},
	{namco163_write_handler, 0x8000, SIZE_32K, 0},
	{NULL},
};

static struct board_write_handler namco340_write_handlers[] = {
	{namco163_write_handler, 0x8000, SIZE_32K - SIZE_2K, 0},
	{NULL},
};

static struct board_write_handler namco175_write_handlers[] = {
       {namco163_write_handler, 0x8000, SIZE_16K, 0},
       {namco175_write_handler, 0xc000, SIZE_2K, 0},
       {namco163_write_handler, 0xe000, SIZE_4K + SIZE_2K, 0},
       {NULL},
};

static struct board_read_handler namco163_read_handlers[] = {
	{namco163_read_handler, 0x5000, SIZE_4K, 0},
	{NULL},
};

struct board_info board_namco163 = {
	.board_type = BOARD_TYPE_NAMCO_163,
	.name = "NAMCOT-163",
	.mapper_name = "Namco 163",
	.funcs = &namco163_funcs,
	.init_prg = namco163_init_prg,
	.init_chr0 = std_chr_1k,
	.write_handlers = namco163_write_handlers,
	.read_handlers = namco163_read_handlers,
	.max_prg_rom_size = SIZE_512K,
	.max_chr_rom_size = SIZE_256K,
	.max_wram_size = {SIZE_8K, 0},
	.max_vram_size = {SIZE_32K, 0},
	.flags = BOARD_INFO_FLAG_MIRROR_M |
	         BOARD_INFO_FLAG_M2_TIMER,
	.mapper_ram_size = 128,
};

struct board_info board_namco340 = {
	.board_type = BOARD_TYPE_NAMCO_340,
	.name = "NAMCOT-340",
	.init_prg = namco163_init_prg,
	.init_chr0 = std_chr_1k,
	.write_handlers = namco340_write_handlers,
	.max_prg_rom_size = SIZE_512K,
	.max_chr_rom_size = SIZE_256K,
	.max_wram_size = {SIZE_8K, 0},
	.max_vram_size = {SIZE_32K, 0},
	.flags = BOARD_INFO_FLAG_MIRROR_M |
	         BOARD_INFO_FLAG_M2_TIMER,
};

struct board_info board_namco340_compat = {
	.board_type = BOARD_TYPE_NAMCO_340_COMPAT,
	.name = "NAMCOT-340-COMPAT",
	.init_prg = namco163_init_prg,
	.init_chr0 = std_chr_1k,
	.write_handlers = namco340_write_handlers,
	.max_prg_rom_size = SIZE_512K,
	.max_chr_rom_size = SIZE_256K,
	.max_wram_size = {SIZE_8K, 0},
	.max_vram_size = {SIZE_32K, 0},
	.flags = BOARD_INFO_FLAG_MIRROR_M |
	         BOARD_INFO_FLAG_M2_TIMER,
};

struct board_info board_namco175 = {
	.board_type = BOARD_TYPE_NAMCO_175,
	.name = "NAMCOT-175",
	.init_prg = namco163_init_prg,
	.init_chr0 = std_chr_1k,
	.write_handlers = namco175_write_handlers,
	.max_prg_rom_size = SIZE_512K,
	.max_chr_rom_size = SIZE_256K,
	.max_wram_size = {SIZE_8K, 0},
	.max_vram_size = {SIZE_32K, 0},
	.flags = BOARD_INFO_FLAG_M2_TIMER,
};

static int namco163_init(struct board *board)
{
	namco163_audio_init(board->emu);
	cpu_set_write_handler(board->emu->cpu, 0xf800, 2048, 0,
			      namco163_audio_write_handler);
	cpu_set_write_handler(board->emu->cpu, 0x4800, 2048, 0,
			      namco163_audio_write_handler);
	cpu_set_read_handler(board->emu->cpu, 0x4800, 2048, 0,
			     namco163_audio_read_handler);

	return 0;
}

static void namco163_cleanup(struct board *board)
{
	namco163_audio_cleanup(board->emu);
}

static void namco163_reset(struct board *board, int hard)
{
	if (hard) {
		m2_timer_set_size(board->emu->m2_timer,
				  15, 0);
		m2_timer_set_flags(board->emu->m2_timer,
				   M2_TIMER_FLAG_COUNT_UP |
				   M2_TIMER_FLAG_ONE_SHOT, 0);
		m2_timer_set_enabled(board->emu->m2_timer, 0, 0);
	}

	namco163_audio_reset(board->emu->namco163_audio, hard);
}

static void namco163_end_frame(struct board *board, uint32_t cycles)
{
	namco163_audio_end_frame(board->emu->namco163_audio, cycles);
}

static void namco163_map_chr(struct board *board, int addr, uint8_t value)
{
	int bank;
	int type;

	type = MAP_TYPE_ROM;
	bank = (addr - 0x8000) >> 11;

	if ((board->info->board_type == BOARD_TYPE_NAMCO_163) &&
	    (value >= 0xe0)) {
		if ((bank < 4 && !(_prg_select2 & 0x40)) ||
		    (bank >= 4 && !(_prg_select2 & 0x80)))
			type = MAP_TYPE_CIRAM;
	}

	board->chr_banks0[bank].type = type;
	board->chr_banks0[bank].bank = value;
	board_chr_sync(board, 0);
}

static void namco163_run(struct board *board, uint32_t cycles)
{
	namco163_audio_run(board->emu->namco163_audio, cycles);
}

static CPU_READ_HANDLER(namco163_read_handler)
{
//	printf("read %x\n", addr);

	if (addr < 0x5800) {
		value = m2_timer_get_counter(emu->m2_timer, cycles) & 0xff;
	} else {
		value = m2_timer_get_counter(emu->m2_timer, cycles) >> 8;
		value &= 0x7f;

		if (m2_timer_get_irq_enabled(emu->m2_timer))
			value |= 0x80;
	}

	return value;
}

static CPU_WRITE_HANDLER(namco175_write_handler)
{
	struct board *board;

	board = emu->board;

	if (addr >= 0xc000 && addr <= 0xc800) {
		value &= 0x01;
		value = (value ? MAP_PERM_READWRITE : MAP_PERM_NONE);

		if (value != board->prg_banks[0].perms) {
			board->prg_banks[0].perms = value;
			board->prg_banks[1].perms = value;
			board->prg_banks[2].perms = value;
			board->prg_banks[3].perms = value;
			board_prg_sync(board);
		}
	}
}

static CPU_WRITE_HANDLER(namco163_write_handler)
{
	struct board *board = emu->board;

//	printf("wrote %x to %x\n", value, addr);

	if (addr < 0x5800) {
		m2_timer_set_counter_lo(emu->m2_timer, value, cycles);
	} else if (addr < 0x6000) {
		m2_timer_set_counter_hi(emu->m2_timer, value, cycles);
		m2_timer_set_enabled(emu->m2_timer, value & 0x80, cycles);
	} else if (addr < 0xc000) {
		namco163_map_chr(board, addr, value);
		return;
	} else if (addr < 0xe000) {
		int type;

		type = MAP_TYPE_ROM;
		addr -= 0xc000;
		addr >>= 11;

		if (value >= 0xe0) {
			value &= 0x01;
			type = MAP_TYPE_CIRAM;
		}

		if (board->info->board_type == BOARD_TYPE_NAMCO_163) {
			board->nmt_banks[addr].bank = value;
			board->nmt_banks[addr].type = type;
			board_nmt_sync(board);
		}
	} else if (addr < 0xe800) {
		int mirroring;

		board->prg_banks[4].bank = value;
		board_prg_sync(board);
		switch (board->info->board_type) {
		case BOARD_TYPE_NAMCO_163:
			namco163_audio_write_handler(emu, addr, value, cycles);
			break;
		case BOARD_TYPE_NAMCO_340:
		case BOARD_TYPE_NAMCO_340_COMPAT:
			switch ((value >> 6) & 0x03) {
			case 0:
				mirroring = MIRROR_1A;
				break;
			case 1:
				mirroring = MIRROR_V;
				break;
			case 2:
				mirroring = MIRROR_H;
				break;
			case 3:
				mirroring = MIRROR_1B;
				break;
			default:
				mirroring = MIRROR_UNDEF;
			}
			board_set_ppu_mirroring(board, mirroring);
		}
	} else if (addr < 0xf000) {
		board->prg_banks[5].bank = value & 0x3f;
		if ((value & 0xc0) != _prg_select2) {
			int type_l, type_h;
			int i;

			type_l = (value & 0x40) ? MAP_TYPE_ROM : MAP_TYPE_RAM0;
			type_h = (value & 0x40) ? MAP_TYPE_ROM : MAP_TYPE_RAM0;

			for (i = 0; i < 4; i++) {
				if (board->chr_banks0[i].bank >=
				    0xe0)
					board->chr_banks0[i].type =
					    type_l;

			}

			for (i = 4; i < 8; i++) {
				if (board->chr_banks0[i].bank >=
				    0xe0)
					board->chr_banks0[i].type =
					    type_h;

			}

			board_chr_sync(board, 0);
		}
		board_prg_sync(board);
		_prg_select2 = value & 0xc0;
	} else if (addr < 0xf800) {
		board->prg_banks[6].bank = value;
		board_prg_sync(board);
	} else {
		if ((value < 0x40) || (value > 0x4e))
			value = 0x0f;

		if (value & 0x01)
			board->prg_banks[0].perms = MAP_PERM_READ;
		if (value & 0x02)
			board->prg_banks[1].perms = MAP_PERM_READ;
		if (value & 0x04)
			board->prg_banks[2].perms = MAP_PERM_READ;
		if (value & 0x08)
			board->prg_banks[3].perms = MAP_PERM_READ;
		if ((value & 0xf1) == 0x40)
			board->prg_banks[0].perms = MAP_PERM_READWRITE;
		if ((value & 0xf2) == 0x40)
			board->prg_banks[1].perms = MAP_PERM_READWRITE;
		if ((value & 0xf4) == 0x40)
			board->prg_banks[2].perms = MAP_PERM_READWRITE;
		if ((value & 0xf8) == 0x40)
			board->prg_banks[3].perms = MAP_PERM_READWRITE;
		board_prg_sync(board);
		namco163_audio_write_handler(emu, addr, value, cycles);
	}

}

static int namco163_load_state(struct board *board, struct save_state *state)
{
	int rc;
	rc = namco163_audio_load_state(board->emu, state);

	return rc;
}

static int namco163_save_state(struct board *board, struct save_state *state)
{
	int rc;

	rc = namco163_audio_save_state(board->emu, state);

	return rc;
}
