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
#include "mmc5_audio.h"

#define _exram_mode board->data[0]
#define _mirroring  board->data[1]
#define _wram_protect board->data[2]
#define _ppu_rendering board->data[3]
#define _ppu_large_sprites board->data[4]
#define _fill_mode_tile board->data[5]
#define _fill_mode_color board->data[6]
#define _last_chr_regs_written board->data[7]
#define _wram_banks_used board->data[8]
#define _target_irq_scanline board->data[9]
#define _counter_reset_scanline board->data[10]
#define _irq_flags board->data[11]
#define _multiplier board->timestamps[0]
#define _multiplicand board->timestamps[1]
#define _product board->timestamps[2]

#define _vblank_target board->timestamps[3]

static int mmc5_init(struct board *board);
static void mmc5_run(struct board *board, uint32_t cycles);
static void mmc5_cleanup(struct board *board);
static void mmc5_reset(struct board *board, int);
static void mmc5_end_frame(struct board *board, uint32_t cycles);
static void mmc5_set_prg_mode(struct board *board, int);
static void mmc5_set_chr_mode(struct board *board, int);
static void mmc5_nvram_post_load(struct board *board);
static void mmc5_nvram_pre_save(struct board *board);
static int mmc5_load_state(struct board *board, struct save_state *state);
static int mmc5_save_state(struct board *board, struct save_state *state);

extern CPU_WRITE_HANDLER(ppu_ctrl_reg_write_handler);
extern CPU_WRITE_HANDLER(ppu_mask_reg_write_handler);
static CPU_WRITE_HANDLER(mmc5_write_handler);
static CPU_WRITE_HANDLER(mmc5_ppu_handler);
static CPU_READ_HANDLER(mmc5_read_handler);

static struct board_funcs mmc5_funcs = {
	.init = mmc5_init,
	.reset = mmc5_reset,
	.cleanup = mmc5_cleanup,
	.end_frame = mmc5_end_frame,
	.run = mmc5_run,
	.load_state = mmc5_load_state,
	.save_state = mmc5_save_state,
};

static struct board_funcs mmc5_compat_funcs = {
	.init = mmc5_init,
	.reset = mmc5_reset,
	.cleanup = mmc5_cleanup,
	.end_frame = mmc5_end_frame,
	.run = mmc5_run,
	.nvram_post_load = mmc5_nvram_post_load,
	.nvram_pre_save = mmc5_nvram_pre_save,
	.load_state = mmc5_load_state,
	.save_state = mmc5_save_state,
};

/* Note that all banks except $E000 - $FFFF may be PRG-RAM instead
   of PRG-ROM */
static struct bank mmc5_init_prg[] = {
	{0xff, 0, SIZE_8K, 0x6000, MAP_PERM_READ, MAP_TYPE_RAM0},
	{0x80, 0, SIZE_8K, 0x8000, MAP_PERM_READ, MAP_TYPE_ROM},
	{0x80, 0, SIZE_8K, 0xa000, MAP_PERM_READ, MAP_TYPE_ROM},
	{0x80, 0, SIZE_8K, 0xc000, MAP_PERM_READ, MAP_TYPE_ROM},
	{0xff, 0, SIZE_8K, 0xe000, MAP_PERM_READ, MAP_TYPE_ROM},
	{.type = MAP_TYPE_END},
};

static struct bank mmc5_init_chr0[] = {
	{0, 0, SIZE_1K, 0x0000, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{1, 0, SIZE_1K, 0x0400, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{2, 0, SIZE_1K, 0x0800, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{3, 0, SIZE_1K, 0x0c00, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{4, 0, SIZE_1K, 0x1000, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{5, 0, SIZE_1K, 0x1400, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{6, 0, SIZE_1K, 0x1800, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{7, 0, SIZE_1K, 0x1c00, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{.type = MAP_TYPE_END},
};

static struct bank mmc5_init_chr1[] = {
	{0, 0, SIZE_1K, 0x0000, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{1, 0, SIZE_1K, 0x0400, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{2, 0, SIZE_1K, 0x0800, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{3, 0, SIZE_1K, 0x0c00, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{4, 0, SIZE_1K, 0x1000, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{5, 0, SIZE_1K, 0x1400, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{6, 0, SIZE_1K, 0x1800, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{7, 0, SIZE_1K, 0x1c00, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{.type = MAP_TYPE_END},
};

static struct board_write_handler mmc5_write_handlers[] = {
	{mmc5_write_handler, 0x5100, 0xf00, 0},
	{mmc5_ppu_handler, 0x2000, 2, 0},
	{NULL},
};

static struct board_read_handler mmc5_read_handlers[] = {
	{mmc5_read_handler, 0x5100, 0xf00, 0},
	{NULL},
};

struct board_info board_exrom = {
	.board_type = BOARD_TYPE_ExROM,
	.name = "ExROM",
	.mapper_name = "MMC5",
	.funcs = &mmc5_funcs,
	.init_prg = mmc5_init_prg,
	.init_chr0 = mmc5_init_chr0,
	.init_chr1 = mmc5_init_chr1,
	.read_handlers = mmc5_read_handlers,
	.write_handlers = mmc5_write_handlers,
	.max_prg_rom_size = SIZE_1024K,
	.max_chr_rom_size = SIZE_1024K,
	.max_wram_size = {SIZE_32K, SIZE_32K},
	.flags = BOARD_INFO_FLAG_MIRROR_M,
	.mapper_ram_size = SIZE_1K,
};

struct board_info board_exrom_compat = {
	.board_type = BOARD_TYPE_ExROM_COMPAT,
	.name = "ExROM-COMPAT",
	.mapper_name = "MMC5",
	.funcs = &mmc5_compat_funcs,
	.init_prg = mmc5_init_prg,
	.init_chr0 = mmc5_init_chr0,
	.init_chr1 = mmc5_init_chr1,
	.read_handlers = mmc5_read_handlers,
	.write_handlers = mmc5_write_handlers,
	.max_prg_rom_size = SIZE_1024K,
	.max_chr_rom_size = SIZE_1024K,
	.max_wram_size = {SIZE_32K, SIZE_32K},
	.flags = BOARD_INFO_FLAG_MIRROR_M,
	.mapper_ram_size = SIZE_1K,
};

static int mmc5_init(struct board *board)
{
	struct emu *emu;
	int rc;

	emu = board->emu;

	board->prg_and = 0xff;
	board->prg_or = 0;
	board->chr_and = 0xff;
	board->chr_or = 0;
	board->wram_and = 0xff;
	board->fill_mode_nmt = malloc(SIZE_1K);

	memset(board->fill_mode_nmt, 0, SIZE_1K);

	rc = mmc5_audio_init(emu);
	mmc5_audio_install_handlers(emu, 0);

	return rc;
}

static void mmc5_cleanup(struct board *board)
{
	mmc5_audio_cleanup(board->emu);
}

static void mmc5_reset(struct board *board, int hard)
{
	if (hard) {
		board->prg_mode = 0x03;
		board->chr_mode = 0x03;
		//_exram_mode = 0;
		_wram_banks_used = 0x01;
	}

	_ppu_rendering = 0;
	_ppu_large_sprites = 0;
	_counter_reset_scanline = -1;
	board->irq_counter = ~0;
	board->irq_control = 0;

	mmc5_audio_reset(board->emu->mmc5_audio, hard);
}

static void mmc5_set_chr_mode(struct board *board, int value)
{
	int i;

	for (i = 0; i < 8; i++) {
		board->chr_banks0[i].size = 0;
		board->chr_banks0[i].address = 0;
		board->chr_banks1[i].size = 0;
		board->chr_banks1[i].address = 0;
	}

	switch (value) {
	case 0:
		board->chr_banks0[7].address = 0x0000;
		board->chr_banks0[7].size = SIZE_8K;
		board->chr_banks1[3].address = 0x0000;
		board->chr_banks1[3].size = SIZE_8K;
		break;
	case 1:
		board->chr_banks0[3].address = 0x0000;
		board->chr_banks0[3].size = SIZE_4K;
		board->chr_banks0[7].address = 0x1000;
		board->chr_banks0[7].size = SIZE_4K;
		board->chr_banks1[3].address = 0x0000;
		board->chr_banks1[3].size = SIZE_4K;
		board->chr_banks1[7].address = 0x1000;
		board->chr_banks1[7].size = SIZE_4K;
		break;
	case 2:
		for (i = 0; i < 4; i++) {
			board->chr_banks0[i * 2 + 1].address =
			    i << 11;
			board->chr_banks0[i * 2 + 1].size = SIZE_2K;
			board->chr_banks1[i * 2 + 1].address =
			    i << 11;
			board->chr_banks1[i * 2 + 1].size = SIZE_2K;
		}
		break;
	case 3:
		for (i = 0; i < 8; i++) {
			board->chr_banks0[i].address = i << 10;
			board->chr_banks0[i].size = SIZE_1K;
			board->chr_banks1[i].address = i << 10;
			board->chr_banks1[i].size = SIZE_1K;
		}
		break;
	}

	board->chr_mode = value;
	board_chr_sync(board, 0);
	board_chr_sync(board, 1);
}

static void mmc5_set_prg_mode(struct board *board, int value)
{
	int i;

	for (i = 1; i < 5; i++) {
		board->prg_banks[i].size = 0;
		board->prg_banks[i].address = 0;
	}

	switch (value) {
	case 0:
		board->prg_banks[4].address = 0x8000;
		board->prg_banks[4].size = SIZE_32K;
		board->prg_banks[4].shift = 2;
		break;
	case 1:
		board->prg_banks[2].address = 0x8000;
		board->prg_banks[2].size = SIZE_16K;
		board->prg_banks[2].shift = 1;
		board->prg_banks[4].address = 0xc000;
		board->prg_banks[4].size = SIZE_16K;
		board->prg_banks[4].shift = 1;
		break;
	case 2:
		board->prg_banks[2].address = 0x8000;
		board->prg_banks[2].size = SIZE_16K;
		board->prg_banks[2].shift = 1;
		board->prg_banks[3].address = 0xc000;
		board->prg_banks[3].size = SIZE_8K;
		board->prg_banks[4].address = 0xe000;
		board->prg_banks[4].size = SIZE_8K;
		break;
	case 3:
		board->prg_banks[1].address = 0x8000;
		board->prg_banks[1].size = SIZE_8K;
		board->prg_banks[2].address = 0xa000;
		board->prg_banks[2].size = SIZE_8K;
		board->prg_banks[3].address = 0xc000;
		board->prg_banks[3].size = SIZE_8K;
		board->prg_banks[4].address = 0xe000;
		board->prg_banks[4].size = SIZE_8K;
		break;
	}

	board->prg_mode = value;
	board_prg_sync(board);
}

static void mmc5_update_ram_perms(struct board *board, int value)
{
	int perms;
	int i;

	if (value == _wram_protect) {
		return;
	} else if (value != 3 && _wram_protect != 3) {
		_wram_protect = value;
		return;
	} else if (value != 3) {
		perms = MAP_PERM_READ;
	} else {
		perms = MAP_PERM_READ | MAP_PERM_WRITE;
	}

	_wram_protect = value;

	for (i = 0; i < 5; i++) {
		if (board->prg_banks[i].type == MAP_TYPE_RAM0 ||
		    board->prg_banks[i].type == MAP_TYPE_RAM1) {
			board->prg_banks[i].perms = perms;
		}
	}

	board_prg_sync(board);
}

void mmc5_update__mirroring(struct board *board, int value)
{
	int i;
	int type;
	int bank;

	for (i = 0; i < 4; i++) {
		switch (value & 0x03) {
		case 0:
		case 1:
			type = MAP_TYPE_CIRAM;
			bank = value & 0x03;
			break;
		case 2:
			if (_exram_mode == 0 || _exram_mode == 1)
				type = MAP_TYPE_MAPPER_RAM;
			else
				type = MAP_TYPE_ZERO;

			bank = 0;
			break;
		case 3:
			type = MAP_TYPE_FILL;
			bank = 0;
			break;

		}

		board->nmt_banks[i].type = type;
		board->nmt_banks[i].bank = bank;

		value >>= 2;
	}

	_mirroring = value;
	board_nmt_sync(board);
}

static CPU_READ_HANDLER(mmc5_read_handler)
{
	struct board *board = emu->board;

	if (addr >= 0x5c00 && addr < 0x6000) {
		if (_exram_mode > 1)
			value = board->mapper_ram.data[addr & 0x3ff];
	} else if (addr == 0x5204) {
		value = 0;
		if (_ppu_rendering && cycles < 247566)
			value = 0x40;
		if (_ppu_rendering && board->irq_counter < cycles) {
			value |= 0x80;
			cpu_interrupt_ack(emu->cpu, IRQ_MMC5_TIMER);
		}
	} else if (addr == 0x5205) {
		value = _product & 0xff;
	} else if (addr == 0x5206) {
		value = (_product >> 8) & 0xff;
	}

	return value;
}

static void mmc5_schedule_irq(struct emu *emu, uint32_t cycles)
{
	int scanline, scanline_cycle;
	int odd_frame, short_frame;
	int target, current, vblank;
	int difference;
	struct board *board;
	int value;

	board = emu->board;

	value = (int8_t)_counter_reset_scanline +
		_target_irq_scanline + 1;

	if (_target_irq_scanline == 0 || value > 240) {
		board->irq_counter = ~0;
		cpu_interrupt_cancel(emu->cpu, IRQ_MMC5_TIMER);
		cpu_board_run_cancel(emu->cpu);

		return;
	}

	ppu_run(emu->ppu, cycles);
	ppu_get_cycles(emu->ppu,  &scanline,
		       &scanline_cycle, &odd_frame,
		       &short_frame);

	target = (value * 341 + 339) * emu->ppu_clock_divider;
	vblank = (241 * 341) * emu->ppu_clock_divider;
	current = ((scanline + 1) * 341 + scanline_cycle) *
		emu->ppu_clock_divider;

	difference = target - current;
	board->irq_counter = cycles + difference;
	if (_ppu_rendering && board->irq_control &&
	    (board->irq_counter >= cycles)) {
		/* printf("scheduling irq for %d\n", board->irq_counter); */
		cpu_interrupt_cancel(emu->cpu, IRQ_MMC5_TIMER);
		cpu_board_run_cancel(emu->cpu);
		cpu_interrupt_schedule(emu->cpu, IRQ_MMC5_TIMER,
				       board->irq_counter);

		difference = vblank - current;
		/* printf("scheduling board run for %d\n", cycles + difference); */
		_vblank_target = cycles + difference;
		cpu_board_run_schedule(emu->cpu, _vblank_target);
	}
}

static CPU_WRITE_HANDLER(mmc5_write_handler)
{
	struct board *board = emu->board;

	int bank;

	if (addr >= 0x5c00 && addr < 0x6000) {
		addr &= 0x3ff;
		if (_exram_mode == 0 || _exram_mode == 1) {
			ppu_run(emu->ppu, cycles);
			if (_ppu_rendering && cycles < 412610) {
				board->mapper_ram.data[addr] = value;

			} else {
				board->mapper_ram.data[addr] = 0;

			}
		} else if (_exram_mode == 2) {
			board->mapper_ram.data[addr] = value;
		}
		return;
	}

	switch (addr) {
	case 0x5100:
		value &= 0x03;
		if (value != board->prg_mode) {
			mmc5_set_prg_mode(board, value);
		}
		break;
	case 0x5101:
		value &= 0x03;
		if (value != board->chr_mode) {
			board->chr_mode = value;
			mmc5_set_chr_mode(board, value);
		}
		break;
	case 0x5102:
		value &= 0x02;
		if (value != (_wram_protect & 0x02)) {
			value |= _wram_protect & 0x01;
			mmc5_update_ram_perms(board, value);
		}
		break;
	case 0x5103:
		value &= 0x01;
		if (value != (_wram_protect & 0x01)) {
			value |= _wram_protect & 0x02;
			mmc5_update_ram_perms(board, value);
		}
		break;
	case 0x5104:
		value &= 0x03;
		if (value != _exram_mode) {
			_exram_mode = value;
			ppu_use_exram(emu->ppu, _exram_mode, cycles);
			if (_exram_mode == 0 || _exram_mode == 1) {
				mmc5_update__mirroring(board, _mirroring);
			}
		}
		break;
	case 0x5105:
		mmc5_update__mirroring(board, value);
		break;
	case 0x5106:
		if (_fill_mode_tile != value) {
			ppu_run(emu->ppu, cycles);
			_fill_mode_tile = value;
			memset(board->fill_mode_nmt, value, 0x3c0);
		}
		break;
	case 0x5107:
		value &= 0x03;
		if (value != _fill_mode_color) {
			ppu_run(emu->ppu, cycles);
			value |= (value << 2) | (value << 4) | (value << 6);
			memset(board->fill_mode_nmt + 0x3c0, value, 0x40);
			_fill_mode_color = value & 0x03;
		}
		break;
	case 0x5113:
		value &= 0x7;
		if (value != board->prg_banks[0].bank) {
			board->prg_banks[0].bank = value;
			if (value & 0x04)
				board->prg_banks[0].type =
				    MAP_TYPE_RAM1;
			else
				board->prg_banks[0].type =
				    MAP_TYPE_RAM0;
			
			_wram_banks_used |=(1 << value);
			board->prg_banks[0].perms =
			    MAP_PERM_READWRITE;
			if (_wram_protect != 3)
				board->prg_banks[0].perms =
				    MAP_PERM_READ;

			board_prg_sync(board);
		}
		break;
	case 0x5114:
	case 0x5115:
	case 0x5116:
		bank = 1 + (addr & 0x03);
		if (value != board->prg_banks[bank].bank) {
			int perms = MAP_PERM_READ | MAP_PERM_WRITE;
			if (_wram_protect != 3)
				perms = MAP_PERM_READ;

			board->prg_banks[bank].bank = value;
			if (value & 0x80) {
				board->prg_banks[bank].type =
				    MAP_TYPE_ROM;
				board->prg_banks[bank].perms =
				    MAP_PERM_READ;
			} else if (value & 0x04) {
				board->prg_banks[bank].type =
				    MAP_TYPE_RAM1;
				board->prg_banks[bank].perms =
				    perms;
				_wram_banks_used |=
					(1 << board->prg_banks[bank].bank);
			} else {
				board->prg_banks[bank].type =
				    MAP_TYPE_RAM0;
				board->prg_banks[bank].perms =
				    perms;
				_wram_banks_used |=
					(1 << board->prg_banks[bank].bank);
			}
			board_prg_sync(board);
		}
		break;
	case 0x5117:
		/* type is always ROM, perm is always RO */
		if (value != board->prg_banks[4].bank) {
			board->prg_banks[4].bank = value;
			board_prg_sync(board);
		}
		break;
	case 0x5120:
	case 0x5121:
	case 0x5122:
	case 0x5123:
	case 0x5124:
	case 0x5125:
	case 0x5126:
	case 0x5127:
		board->chr_banks0[addr & 0x07].bank = value;
		_last_chr_regs_written = 0;
		board_chr_sync(board, 0);
		if (!_ppu_large_sprites) {
			ppu_select_bg_pagemap(emu->ppu, 0, cycles);
			ppu_select_spr_pagemap(emu->ppu, 0, cycles);
		}
		break;
	case 0x5128:
	case 0x5129:
	case 0x512a:
	case 0x512b:
		/* The second 4 banks always mirror the first 4 */
		board->chr_banks1[addr & 0x03].bank = value;
		board->chr_banks1[(addr & 0x03) | 0x04].bank =
		    value;
		_last_chr_regs_written = 1;
		board_chr_sync(board, 1);
		if (!_ppu_large_sprites) {
			ppu_select_bg_pagemap(emu->ppu, 1, cycles);
			ppu_select_spr_pagemap(emu->ppu, 1, cycles);
		}
		break;
	case 0x5130:
		if (board->chr_or != (value & 0x03) << 8) {
			board->chr_or = (value & 0x03) << 8;
			board_chr_sync(board, 0);
			board_chr_sync(board, 1);
		}
		break;
	case 0x5200:
		ppu_set_split_screen(emu->ppu, value & 0x80, value & 0x40,
				     value & 0x1f, cycles);

		break;
	case 0x5201:
		ppu_set_split_screen_scroll(emu->ppu, value, cycles);
		break;
	case 0x5202:
		ppu_set_split_screen_bank(emu->ppu, value, cycles);
		break;
	case 0x5203:
		_target_irq_scanline = value;
		mmc5_schedule_irq(emu, cycles);
		break;
	case 0x5204:
		value &= 0x80;
		if (!board->irq_control && value && _ppu_rendering) {
			board->irq_control = value;
			mmc5_schedule_irq(emu, cycles);
		} else if (board->irq_control && !value) {
			board->irq_control = value;
			cpu_interrupt_cancel(emu->cpu, IRQ_MMC5_TIMER);
			cpu_board_run_cancel(emu->cpu);
		}
		break;
	case 0x5205:
		_multiplicand = value;
		_product = _multiplier * _multiplicand;
		break;
	case 0x5206:
		_multiplier = value;
		_product = _multiplier * _multiplicand;
		break;
	}
}

void mmc5_end_frame(struct board *board, uint32_t cycles)
{
	if (_ppu_rendering && board->irq_control &&
	    board->irq_counter != ~0) {
		uint32_t current = cpu_get_cycles(board->emu->cpu);
		mmc5_schedule_irq(board->emu, current);
	}

	mmc5_audio_end_frame(board->emu->mmc5_audio, cycles);
}

static CPU_WRITE_HANDLER(mmc5_ppu_handler)
{
	struct board *board = emu->board;

	if (addr == 0x2000) {
		ppu_ctrl_reg_write_handler(emu, addr, value, cycles);
		if (value & 0x20 && !_ppu_large_sprites) {
			ppu_select_spr_pagemap(emu->ppu, 0, cycles);
			ppu_select_bg_pagemap(emu->ppu, 1, cycles);
		} else if (!(value & 0x20) && _ppu_large_sprites) {
			ppu_select_spr_pagemap(emu->ppu,
					       _last_chr_regs_written, cycles);
			ppu_select_bg_pagemap(emu->ppu,
					      _last_chr_regs_written, cycles);
		}
		_ppu_large_sprites = value & 0x20;
	} else if (addr == 0x2001) {
		ppu_mask_reg_write_handler(emu, addr, value, cycles);
		if (value & 0x18 && !_ppu_rendering) {
			if (board->irq_control &&
			    board->irq_counter != ~0) {
				mmc5_schedule_irq(emu, cycles);
			}
		} else if (!(value & 0x18) && _ppu_rendering) {
			if (board->irq_control) {
				cpu_interrupt_cancel(emu->cpu, IRQ_MMC5_TIMER);
				cpu_interrupt_ack(emu->cpu, IRQ_MMC5_TIMER);
			}
		}

		if (!_ppu_rendering && (value & 0x18)) {
			int scanline, scanline_cycle;
			int odd_frame, short_frame;

			ppu_get_cycles(emu->ppu,  &scanline,
				       &scanline_cycle, &odd_frame,
				       &short_frame);

			if (scanline > 239)
				scanline = -1;
			_counter_reset_scanline = scanline;
			if (scanline_cycle >= 339)
				_counter_reset_scanline = scanline + 1;
		}

		_ppu_rendering = value & 0x18;
	}
}

static void mmc5_run(struct board *board, uint32_t cycles)
{
	if (cycles >= _vblank_target) {
		cpu_interrupt_ack(board->emu->cpu, IRQ_MMC5_TIMER);
		_counter_reset_scanline = -1;
	}

	mmc5_audio_run(board->emu->mmc5_audio, cycles);
}

static void mmc5_nvram_post_load(struct board *board)
{
}

static void mmc5_nvram_pre_save(struct board *board)
{
	int ekrom_mask, etrom_mask;
	int size;

	ekrom_mask = 1 << 0;
	etrom_mask = (1 << 0) | (1 << 4);

	if ((_wram_banks_used == ekrom_mask) ||
	    (_wram_banks_used == etrom_mask)) {
		size = SIZE_8K;
	} else if (_wram_banks_used &&
		   !(_wram_banks_used & 0xf0)) {
		size = SIZE_32K;
	} else {
		size = SIZE_64K;
	}

	if (board->save_file_size < size)
		board->save_file_size = size;
}

static int mmc5_load_state(struct board *board, struct save_state *state)
{
	return mmc5_audio_load_state(board->emu, state);
}

static int mmc5_save_state(struct board *board, struct save_state *state)
{
	return mmc5_audio_save_state(board->emu, state);
}
