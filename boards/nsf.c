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

#include "fds_audio.h"
#include "mmc5_audio.h"
#include "namco163_audio.h"
#include "sunsoft5b_audio.h"
#include "vrc6_audio.h"
#include "vrc7_audio.h"
#include "m2_timer.h"

#define _expansion_sound_reg board->data[0]
#define _counter_status board->data[8]
#define _timer_delay board->timestamps[0]
#define _multiplier board->data[1]
#define _multiplicand board->data[2]
#define _product board->timestamps[1]
#define _clock_rate board->timestamps[2]
#define _cpu_multiplier board->timestamps[3]
#define _multi_chip board->data[3]

#define NSF_EXP_VRC6     0x01
#define NSF_EXP_VRC7     0x02
#define NSF_EXP_FDS      0x04
#define NSF_EXP_MMC5     0x08
#define NSF_EXP_NAMCO    0x10
#define NSF_EXP_SUNSOFT  0x20

#define NSF_PLAYER_SIZE SIZE_4K

#define EXP_HW_OFFSET (0x180 + 0x7b)

static CPU_WRITE_HANDLER(nsf_write_handler);
static CPU_READ_HANDLER(nsf_read_handler);
static int nsf_init(struct board *board);
static void nsf_reset(struct board *board, int);
static void nsf_cleanup(struct board *board);
static void nsf_end_frame(struct board *board, uint32_t);
static void nsf_run(struct board *board, uint32_t);

static struct board_funcs nsf_funcs = {
	.init = nsf_init,
	.reset = nsf_reset,
	.cleanup = nsf_cleanup,
	.end_frame = nsf_end_frame,
	.run = nsf_run,
};

static struct bank nsf_init_prg[] = {
	{0, 0, SIZE_4K, 0x4000, MAP_PERM_READ, MAP_TYPE_ROM},
	{0, 0, SIZE_4K, 0x8000, MAP_PERM_READ, MAP_TYPE_RAM1},
	{1, 0, SIZE_4K, 0x9000, MAP_PERM_READ, MAP_TYPE_RAM1},
	{2, 0, SIZE_4K, 0xa000, MAP_PERM_READ, MAP_TYPE_RAM1},
	{3, 0, SIZE_4K, 0xb000, MAP_PERM_READ, MAP_TYPE_RAM1},
	{4, 0, SIZE_4K, 0xc000, MAP_PERM_READ, MAP_TYPE_RAM1},
	{5, 0, SIZE_4K, 0xd000, MAP_PERM_READ, MAP_TYPE_RAM1},
	{6, 0, SIZE_4K, 0xe000, MAP_PERM_READ, MAP_TYPE_RAM1},
	{7, 0, SIZE_4K, 0xf000, MAP_PERM_READ, MAP_TYPE_RAM1},
	{0, 0, SIZE_4K, 0x6000, MAP_PERM_READWRITE, MAP_TYPE_RAM0},
	{1, 0, SIZE_4K, 0x7000, MAP_PERM_READWRITE, MAP_TYPE_RAM0},
	{.type = MAP_TYPE_END}
};

struct board_info board_nsf = {
	.board_type = BOARD_TYPE_NSF,
	.name = "NES Sound Format",
	.funcs = &nsf_funcs,
	.init_prg = nsf_init_prg,
	.init_chr0 = std_chr_8k,
	.max_prg_rom_size = SIZE_1024K + SIZE_4K + 128,
	.max_chr_rom_size = 0,
	.min_wram_size = {SIZE_8K, SIZE_32K},
	.max_wram_size = {SIZE_8K, SIZE_1024K},
	.min_vram_size = {SIZE_8K, 0},
	.max_vram_size = {SIZE_8K, 0},
	.flags = BOARD_INFO_FLAG_M2_TIMER,
};

static int nsf_init(struct board *board)
{
	uint8_t expansion_chips;
	struct emu *emu;

	_multi_chip = 0;
	emu = board->emu;
	board->mapper_ram.size = 0;

	cpu_set_write_handler(emu->cpu, 0x4027, 4, 0, nsf_write_handler);
	cpu_set_write_handler(emu->cpu, 0x5ff6, 10, 0, nsf_write_handler);
	cpu_set_read_handler(emu->cpu, 0x4029, 1, 0, nsf_read_handler);
	cpu_set_read_handler(emu->cpu, 0xfffa, 6, 0, nsf_read_handler);
	board->mirroring = MIRROR_V;

	expansion_chips = board->prg_rom.data[EXP_HW_OFFSET];
	if (expansion_chips & NSF_EXP_VRC6) {
		vrc6_audio_init(emu);
		_multi_chip++;
	}
	if (expansion_chips & NSF_EXP_VRC7) {
		vrc7_audio_init(emu);
		_multi_chip++;
	}
	if (expansion_chips & NSF_EXP_FDS) {
		int i;

		fds_audio_init(emu);
		for (i = 0; i < 8; i++) {
			nsf_init_prg[i + 1].perms = MAP_PERM_READWRITE;
		}
		nsf_init_prg[9].type = MAP_TYPE_RAM1;
		nsf_init_prg[10].type = MAP_TYPE_RAM1;
		_multi_chip++;
	}
	if (expansion_chips & NSF_EXP_MMC5) {
		mmc5_audio_init(emu);
		/* Need to emulate exram (should be from 5c00 to 5ff5
		   inclusive, but Loopy's player uses 5ff0 - 5ff5).
		 */
		cpu_set_write_handler(emu->cpu, 0x5205, 2, 0,
				      nsf_write_handler);
		cpu_set_read_handler(emu->cpu, 0x5205, 2, 0, nsf_read_handler);
		cpu_set_write_handler(emu->cpu, 0x5c00, 0xff5 - 0xc00, 0,
				      nsf_write_handler);
		cpu_set_read_handler(emu->cpu, 0x5c00, 0xff5 - 0xc00, 0,
				     nsf_read_handler);

		board->mapper_ram.size += SIZE_1K;

		_multi_chip++;
	}
	if (expansion_chips & NSF_EXP_NAMCO) {
		board->mapper_ram.size += 128;
		namco163_audio_init(emu);
		_multi_chip++;
	}
	if (expansion_chips & NSF_EXP_SUNSOFT) {
		sunsoft5b_audio_init(emu);
		_multi_chip++;
	}

	if (_multi_chip > 1)
		_multi_chip = 1;
	else
		_multi_chip = 0;

	if (board->mapper_ram.size) {
		board->mapper_ram.type = CHIP_TYPE_MAPPER_RAM;
		board->mapper_ram.data = malloc(board->mapper_ram.size);
		if (!board->mapper_ram.data)
			return 1;
	}
	return 0;
}

static void nsf_cleanup(struct board *board)
{
	int expansion_chips;

	expansion_chips = board->prg_rom.data[EXP_HW_OFFSET];
	if (expansion_chips & NSF_EXP_VRC6)
		vrc6_audio_cleanup(board->emu);
	if (expansion_chips & NSF_EXP_VRC7)
		vrc7_audio_cleanup(board->emu);
	if (expansion_chips & NSF_EXP_FDS)
		fds_audio_cleanup(board->emu);
	if (expansion_chips & NSF_EXP_MMC5)
		mmc5_audio_cleanup(board->emu);
	if (expansion_chips & NSF_EXP_NAMCO)
		namco163_audio_cleanup(board->emu);
	if (expansion_chips & NSF_EXP_SUNSOFT)
		sunsoft5b_audio_init(board->emu);
}

static void nsf_reset(struct board *board, int hard)
{
	uint8_t expansion_chips;
	struct emu *emu;

	emu = board->emu;

	if (hard) {
		memset(board->wram[0].data, 0, board->wram[0].size);
		memset(board->mapper_ram.data, 0, board->mapper_ram.size);
		memcpy(board->wram[1].data,
		       board->prg_rom.data + NSF_PLAYER_SIZE + 0x80,
		       board->wram[1].size);

		m2_timer_set_flags(emu->m2_timer, M2_TIMER_FLAG_RELOAD, 0);
		m2_timer_set_irq_enabled(emu->m2_timer, 0, 0);

		if (cpu_get_type(emu->cpu) == CPU_TYPE_RP2A07)
			_clock_rate = 1662607;
		else
			_clock_rate = 1789773;

		_cpu_multiplier = emu->cpu_clock_divider;
	}
	_timer_delay = 0;
	board->wram_and = 0xff;
	board->wram_or = 0;

	expansion_chips = board->prg_rom.data[EXP_HW_OFFSET];
	if (expansion_chips & NSF_EXP_VRC6)
		vrc6_audio_reset(emu->vrc6_audio, hard);
	if (expansion_chips & NSF_EXP_VRC7)
		vrc7_audio_reset(emu->vrc7_audio, hard);
	if (expansion_chips & NSF_EXP_FDS)
		fds_audio_reset(emu->fds_audio, hard);
	if (expansion_chips & NSF_EXP_MMC5)
		mmc5_audio_reset(emu->mmc5_audio, hard);
	if (expansion_chips & NSF_EXP_NAMCO)
		namco163_audio_reset(emu->namco163_audio, hard);
	if (expansion_chips & NSF_EXP_SUNSOFT)
	        sunsoft5b_audio_reset(emu->sunsoft5b_audio, hard);
}

static void nsf_end_frame(struct board *board, uint32_t cycles)
{
	uint8_t expansion_chips;
	struct emu *emu;

	emu = board->emu;

	expansion_chips = board->prg_rom.data[EXP_HW_OFFSET];
	if (expansion_chips & NSF_EXP_VRC6)
		vrc6_audio_end_frame(emu->vrc6_audio, cycles);
	if (expansion_chips & NSF_EXP_VRC7)
		vrc7_audio_end_frame(emu->vrc7_audio, cycles);
	if (expansion_chips & NSF_EXP_FDS)
		fds_audio_end_frame(emu->fds_audio, cycles);
	if (expansion_chips & NSF_EXP_MMC5)
		mmc5_audio_end_frame(emu->mmc5_audio, cycles);
	if (expansion_chips & NSF_EXP_NAMCO)
		namco163_audio_end_frame(emu->namco163_audio, cycles);
	if (expansion_chips & NSF_EXP_SUNSOFT)
	        sunsoft5b_audio_end_frame(emu->sunsoft5b_audio, cycles);
}

static CPU_READ_HANDLER(nsf_read_handler)
{
	struct board *board = emu->board;

	/* MMC5 only */
	if (addr >= 0x5c00 && addr < 0x5ff5) {
		if (board->mapper_ram.data)
			value = board->mapper_ram.data[addr & 0x3ff];

		return value;
	}

	switch (addr) {
	case 0x5205:
		value = _product & 0xff;
		break;
	case 0x5206:
		value = (_product >> 8) & 0xff;
		break;
	case 0x4029:
		value = 0;
		if (m2_timer_get_irq_enabled(emu->m2_timer))
			value = 1;
		if (m2_timer_get_counter_status(emu->m2_timer, cycles)) {
			_counter_status = 0x80;
		}

		value |= _counter_status; 
		break;
	case 0xfffa:
	case 0xfffb:
	case 0xfffc:
	case 0xfffd:
	case 0xfffe:
	case 0xffff:
		value = board->prg_rom.data[addr & 0xfff];
		break;
	}

	return value;
}

static CPU_WRITE_HANDLER(nsf_write_handler)
{
	struct board *board = emu->board;

	/* MMC5 only */
	if (addr >= 0x5c00 && addr < 0x5ff0) {
		if (board->mapper_ram.data)
			board->mapper_ram.data[addr & 0x3ff] = value;
		return;
	}

	switch (addr) {
	case 0x5205:
		_multiplier = value;
		_product = _multiplier * _multiplicand;
		break;
	case 0x5206:
		_multiplicand = value;
		_product = _multiplier * _multiplicand;
		break;
	case 0x4027:
		_timer_delay &= 0xff00;
		_timer_delay |= value;
		m2_timer_set_reload(emu->m2_timer,
				    (uint64_t)_timer_delay * _clock_rate /
				    1000000, cycles);
		m2_timer_force_reload(emu->m2_timer, cycles);
		break;
	case 0x4028:
		_timer_delay &= 0x00ff;
		_timer_delay |= value << 8;
		m2_timer_set_reload(emu->m2_timer,
				    (uint64_t)_timer_delay * _clock_rate /
				    1000000, cycles);
		m2_timer_force_reload(emu->m2_timer, cycles);
		break;
	case 0x4029:
		m2_timer_force_reload(emu->m2_timer, cycles);
		if ((value & 1) && !m2_timer_get_irq_enabled(emu->m2_timer)) {
			m2_timer_set_irq_enabled(emu->m2_timer, value & 1, cycles);
		}
		_counter_status = 0x00;
		break;
	case 0x402a:
		_expansion_sound_reg = value;
		if (_expansion_sound_reg & NSF_EXP_VRC6)
			vrc6_audio_install_handlers(emu, _multi_chip);
		if (_expansion_sound_reg & NSF_EXP_VRC7)
			vrc7_audio_install_handlers(emu, _multi_chip);
		if (_expansion_sound_reg & NSF_EXP_FDS)
			fds_audio_install_handlers(emu, _multi_chip);
		if (_expansion_sound_reg & NSF_EXP_MMC5)
			mmc5_audio_install_handlers(emu, _multi_chip);
		if (_expansion_sound_reg & NSF_EXP_NAMCO)
			namco163_audio_install_handlers(emu, _multi_chip);
		if (_expansion_sound_reg & NSF_EXP_SUNSOFT)
			sunsoft5b_audio_install_handlers(emu, _multi_chip);
		break;
	case 0x5ff6:
	case 0x5ff7:
		board->prg_banks[9 + (addr & 0x01)].bank = value;
		board_prg_sync(board);
		break;
	case 0x5ff8:
	case 0x5ff9:
	case 0x5ffa:
	case 0x5ffb:
	case 0x5ffc:
	case 0x5ffd:
	case 0x5ffe:
	case 0x5fff:
		board->prg_banks[1 + (addr & 7)].bank = value;
		board_prg_sync(board);
		break;
	}
}

static void nsf_run(struct board *board, uint32_t cycles)
{
	struct emu *emu;
	int expansion_chips;

	emu = board->emu;
	
	expansion_chips = board->prg_rom.data[EXP_HW_OFFSET];

	if (expansion_chips & NSF_EXP_VRC6)
		vrc6_audio_run(emu->vrc6_audio, cycles);
	if (expansion_chips & NSF_EXP_VRC7)
		vrc7_audio_run(emu->vrc7_audio, cycles);
	if (expansion_chips & NSF_EXP_FDS)
		fds_audio_run(emu->fds_audio, cycles);
	if (expansion_chips & NSF_EXP_MMC5)
		mmc5_audio_run(emu->mmc5_audio, cycles);
	if (expansion_chips & NSF_EXP_NAMCO)
		namco163_audio_run(emu->namco163_audio, cycles);
	if (expansion_chips & NSF_EXP_SUNSOFT)
	        sunsoft5b_audio_run(emu->sunsoft5b_audio, cycles);
}
