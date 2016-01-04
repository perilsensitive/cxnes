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

/* The otherwise unused latch storage will hold the masks and
   shifts needed to deal with the special behavior of SNROM,
   SXROM, SUROM and SOROM boards.
*/
#define _load_data board->data[0]
#define _load_count board->data[1]
#define _wram_disable board->data[2]
#define _wram_always_enabled board->data[3]
#define _wram_banks_used board->data[4]

static int mmc1_init(struct board *board);
static void mmc1_reset(struct board *board, int);
static void mmc1_end_frame(struct board *board, uint32_t cycles);
static CPU_WRITE_HANDLER(mmc1_write_handler);

static void mmc1_write_control(struct board *board, uint8_t data,
			       uint32_t cycles);
static void mmc1_write_chr(struct board *board, int bank, uint8_t data,
			   uint32_t cycles);
static void mmc1_write_prg(struct board *board, uint8_t data, uint32_t cycles);
static void sxrom_compat_nvram_post_load(struct board *board);
static void sxrom_compat_nvram_pre_save(struct board *board);

static struct board_funcs mmc1_funcs = {
	.init = mmc1_init,
	.reset = mmc1_reset,
	.end_frame = mmc1_end_frame,
};

static struct board_funcs sxrom_compat_funcs = {
	.init = mmc1_init,
	.reset = mmc1_reset,
	.end_frame = mmc1_end_frame,
	.nvram_post_load = sxrom_compat_nvram_post_load,
	.nvram_pre_save = sxrom_compat_nvram_pre_save,
};

static struct bank event_init_prg[] = {
	{0, 0, SIZE_8K, 0x6000, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{0, 0, SIZE_16K, 0x8000, MAP_PERM_READ, MAP_TYPE_ROM},
	{0xf, 0, SIZE_16K, 0xc000, MAP_PERM_READ, MAP_TYPE_ROM},
	{0, 0, 0, 0x8000, MAP_PERM_READ, MAP_TYPE_ROM},	/* NWC cart mapper */
	{.type = MAP_TYPE_END},
};

static struct board_write_handler mmc1_write_handlers[] = {
	{mmc1_write_handler, 0x8000, SIZE_32K, 0},
	{NULL},
};

static uint8_t mmc1_mirroring[] = { MIRROR_1A, MIRROR_1B, MIRROR_V, MIRROR_H };

struct board_info board_serom_shrom = {
	.board_type = BOARD_TYPE_SEROM_SHROM,
	.name = "SEROM/SHROM/SH1ROM",
	.mapper_name = "MMC1",
	.mirroring_values = mmc1_mirroring,
	.funcs = &mmc1_funcs,
	.init_prg = event_init_prg,
	.init_chr0 = std_chr_4k,
	.write_handlers = mmc1_write_handlers,
	.max_prg_rom_size = SIZE_32K,
	.max_chr_rom_size = SIZE_128K,
	.max_wram_size = {SIZE_8K, 0},
	.flags = BOARD_INFO_FLAG_MIRROR_M,
};

struct board_info board_sxrom = {
	.board_type = BOARD_TYPE_SxROM,
	.name = "SxROM",
	.mapper_name = "MMC1",
	.mirroring_values = mmc1_mirroring,
	.funcs = &mmc1_funcs,
	.init_prg = std_prg_16k,
	.init_chr0 = std_chr_4k,
	.write_handlers = mmc1_write_handlers,
	.max_prg_rom_size = SIZE_256K,
	.max_chr_rom_size = SIZE_128K,
	.max_wram_size = {SIZE_8K, 0},
	.flags = BOARD_INFO_FLAG_MIRROR_M,
};

struct board_info board_sxrom_wram = {
	.board_type = BOARD_TYPE_SxROM_WRAM,
	.name = "SxROM",
	.mapper_name = "MMC1",
	.mirroring_values = mmc1_mirroring,
	.funcs = &mmc1_funcs,
	.init_prg = std_prg_16k,
	.init_chr0 = std_chr_4k,
	.write_handlers = mmc1_write_handlers,
	.max_prg_rom_size = SIZE_256K,
	.max_chr_rom_size = SIZE_128K,
	.min_wram_size = {SIZE_8K, 0},
	.max_wram_size = {SIZE_8K, 0},
	.flags = BOARD_INFO_FLAG_MIRROR_M,
};

struct board_info board_snrom = {
	.board_type = BOARD_TYPE_SNROM,
	.name = "SNROM",
	.mapper_name = "MMC1",
	.mirroring_values = mmc1_mirroring,
	.funcs = &mmc1_funcs,
	.init_prg = std_prg_16k,
	.init_chr0 = std_chr_4k,
	.write_handlers = mmc1_write_handlers,
	.max_prg_rom_size = SIZE_256K,
	.max_chr_rom_size = SIZE_8K,
	.min_wram_size = {SIZE_8K, 0},
	.max_wram_size = {SIZE_8K, 0},
	.flags = BOARD_INFO_FLAG_MIRROR_M,
};

struct board_info board_sorom = {
	.board_type = BOARD_TYPE_SOROM,
	.name = "SOROM",
	.mirroring_values = mmc1_mirroring,
	.mapper_name = "MMC1",
	.funcs = &mmc1_funcs,
	.init_prg = std_prg_16k,
	.init_chr0 = std_chr_4k,
	.write_handlers = mmc1_write_handlers,
	.max_prg_rom_size = SIZE_256K,
	.max_chr_rom_size = SIZE_8K,
	.min_wram_size = {SIZE_8K, SIZE_8K},
	.max_wram_size = {SIZE_8K, SIZE_8K},
	.flags = BOARD_INFO_FLAG_MIRROR_M |
	         BOARD_INFO_FLAG_WRAM1_NV,
};

struct board_info board_surom = {
	.board_type = BOARD_TYPE_SUROM,
	.name = "SUROM",
	.mirroring_values = mmc1_mirroring,
	.mapper_name = "MMC1",
	.funcs = &mmc1_funcs,
	.init_prg = std_prg_16k,
	.init_chr0 = std_chr_4k,
	.write_handlers = mmc1_write_handlers,
	.max_prg_rom_size = SIZE_512K,
	.max_chr_rom_size = SIZE_8K,
	.min_wram_size = {SIZE_8K, 0},
	.max_wram_size = {SIZE_8K, 0},
	.flags = BOARD_INFO_FLAG_MIRROR_M,
};

struct board_info board_sXrom = {
	.board_type = BOARD_TYPE_SXROM,
	.name = "SXROM",
	.mirroring_values = mmc1_mirroring,
	.mapper_name = "MMC1",
	.funcs = &mmc1_funcs,
	.init_prg = std_prg_16k,
	.init_chr0 = std_chr_4k,
	.write_handlers = mmc1_write_handlers,
	.max_prg_rom_size = SIZE_512K,
	.max_chr_rom_size = SIZE_8K,
	.min_wram_size = {SIZE_32K, 0},
	.max_wram_size = {SIZE_32K, 0},
	.flags = BOARD_INFO_FLAG_MIRROR_M,
};

struct board_info board_sxrom_compat = {
	.board_type = BOARD_TYPE_SxROM_COMPAT,
	.name = "SxROM-COMPAT",
	.mapper_name = "MMC1",
	.mirroring_values = mmc1_mirroring,
	.funcs = &sxrom_compat_funcs,
	.init_prg = std_prg_16k,
	.init_chr0 = std_chr_4k,
	.write_handlers = mmc1_write_handlers,
	.max_prg_rom_size = SIZE_512K,
	.max_chr_rom_size = SIZE_8K,
	.max_wram_size = {SIZE_32K, 0},
	.min_wram_size = {SIZE_32K, 0},
	.flags = BOARD_INFO_FLAG_MIRROR_M |
	         BOARD_INFO_FLAG_WRAM0_NV |
	         BOARD_INFO_FLAG_WRAM1_NV,
};

struct board_info board_sxrom_mmc1a = {
	.board_type = BOARD_TYPE_SxROM_MMC1A,
	.name = "SxROM-MMC1A",
	.mapper_name = "MMC1A",
	.mirroring_values = mmc1_mirroring,
	.funcs = &mmc1_funcs,
	.init_prg = std_prg_16k,
	.init_chr0 = std_chr_4k,
	.write_handlers = mmc1_write_handlers,
	.max_prg_rom_size = SIZE_256K,
	.max_chr_rom_size = SIZE_128K,
	.max_wram_size = {SIZE_8K, 0},
	.flags = BOARD_INFO_FLAG_MIRROR_M,
};

struct board_info board_event = {
	.board_type = BOARD_TYPE_EVENT,
	.name = "NES-EVENT",
	.mapper_name = "MMC1",
	.funcs = &mmc1_funcs,
	.mirroring_values = mmc1_mirroring,
	.init_prg = event_init_prg,
	.init_chr0 = std_chr_4k,
	.write_handlers = mmc1_write_handlers,
	.max_prg_rom_size = SIZE_256K,
	.max_chr_rom_size = 0,
	.min_wram_size = {SIZE_8K, 0},
	.max_wram_size = {SIZE_8K, 0},
	.min_vram_size = {SIZE_8K, 0},
	.max_vram_size = {SIZE_8K, 0},
	.flags = BOARD_INFO_FLAG_MIRROR_M,
};

static int mmc1_init(struct board *board)
{
	_wram_always_enabled = 0;

	switch (board->info->board_type) {
	case BOARD_TYPE_EVENT:
		board_set_num_dip_switches(board, 4);
		break;
	case BOARD_TYPE_SxROM_MMC1A:
		_wram_always_enabled = 1;
		break;
	}

	if (emu_system_is_vs(board->emu))
		_wram_always_enabled = 1;

	return 0;
}

static void mmc1_reset(struct board *board, int hard)
{
	if (hard) {
		board->timestamps[0] = 0;
		mmc1_write_control(board, 0xc, 0);
		mmc1_write_prg(board, 0, 0);
		mmc1_write_chr(board, 0, 0, 0);
		mmc1_write_chr(board, 1, 0, 0);
		board->prg_or = 0;
		board->prg_and = 0xf;
		board->chr_and = 0x1f;
		board->chr_or = 0;
	}

	board->irq_counter_timestamp = 0;
	board->irq_counter = 0;
	_wram_banks_used |= 1;

	switch (board->info->board_type) {
	case BOARD_TYPE_EVENT:
	case BOARD_TYPE_SEROM_SHROM:
		/* First 32K of first chip are mapped in
		   on reset or power up */
		board->prg_banks[3].size = SIZE_32K;
		board->prg_banks[3].bank = 0x00;
		board_prg_sync(board);
		break;
	}
}

static void mmc1_end_frame(struct board *board, uint32_t cycles)
{
	board->timestamps[0] = 0;
	if (board->irq_counter) {
		int elapsed;

		elapsed = (cycles - board->irq_counter_timestamp) /
		    board->emu->cpu_clock_divider;
		if (elapsed > board->irq_counter)
			elapsed = board->irq_counter;
		board->irq_counter -= elapsed;
		board->irq_counter_timestamp = 0;
		/* we don't schedule the IRQ until the
		   next-to-last frame before it is supposed to fire.
		   We have to do this because otherwise we would
		   overflow the cpu irq timestamp, which is only
		   32-bit, and we'd need 36 bits.  The choice of 35000
		   is arbitrary.  A) it's a nice, big even number and
		   B) it's a bit longer than one PAL frame.
		 */

		if (board->irq_counter
		    && board->irq_counter < 35000) {
			cpu_interrupt_schedule(board->emu->cpu, IRQ_M2_TIMER,
					       board->irq_counter *
					       board->emu->cpu_clock_divider);
		}
	}
}

static void mmc1_write_control(struct board *board, uint8_t value,
			       uint32_t cycles)
{
	/* Change PRG mode */
	board->prg_mode = value & 0x0c;
	switch (board->prg_mode) {
	case 0x00:
	case 0x04:
		board->prg_banks[1].size = SIZE_32K;
		board->prg_banks[1].shift = 1;
		board->prg_banks[1].address = 0x8000;
		board->prg_banks[2].size = 0;
		break;
	case 0x08:
		board->prg_banks[1].size = SIZE_16K;
		board->prg_banks[1].shift = 0;
		board->prg_banks[1].address = 0xc000;
		board->prg_banks[2].size = SIZE_16K;
		board->prg_banks[2].bank = 0;
		board->prg_banks[2].address = 0x8000;
		break;
	case 0x0c:
		board->prg_banks[1].size = SIZE_16K;
		board->prg_banks[1].shift = 0;
		board->prg_banks[1].address = 0x8000;
		board->prg_banks[2].size = SIZE_16K;
		board->prg_banks[2].bank = 0xf;
		board->prg_banks[2].address = 0xc000;
		break;
	}

	/* Change CHR mode */
	board->chr_mode = value & 0x10;
	if (board->chr_mode) {
		board->chr_banks0[0].size = SIZE_4K;
		board->chr_banks0[0].shift = 0;
		board->chr_banks0[1].size = SIZE_4K;
		board->chr_banks0[1].address = 0x1000;
		board->chr_banks0[1].perms = MAP_PERM_READWRITE;
		board->chr_banks0[1].type = MAP_TYPE_AUTO;
	} else {
		board->chr_banks0[0].size = SIZE_8K;
		board->chr_banks0[0].shift = 1;
		board->chr_banks0[1].size = 0;
		board->chr_banks0[1].address = 0x1000;
	}

	board_prg_sync(board);
	board_chr_sync(board, 0);

	standard_mirroring_handler(board->emu, 0, value, cycles);
}

void mmc1_write_prg(struct board *board, uint8_t data, uint32_t cycles)
{
	board->prg_banks[1].bank = data;

	if (!_wram_always_enabled) {
		_wram_disable =
		    (_wram_disable & 0x02) | ((data & 0x10) >> 4);

		board->prg_banks[0].perms =
		    (_wram_disable) ? MAP_PERM_NONE : MAP_PERM_READWRITE;
	}

	board_prg_sync(board);
}

void mmc1_write_chr(struct board *board, int bank, uint8_t data,
		    uint32_t cycles)
{
	board->chr_banks0[bank].bank = data;
	if (!board->chr_mode && (bank == 0)) {
		int do_prg_sync = 0;

		/* PRG RAM bank */
		if (((data >> 2) & 0x03) != board->prg_banks[0].bank) {
			int wram_bank = (data >> 2) & 0x03;
			int type = MAP_TYPE_RAM0;

			if (board->wram[1].size && (wram_bank & 0x02))
				type = MAP_TYPE_RAM1;

			board->prg_banks[0].bank = wram_bank;
			board->prg_banks[0].type = type;

			_wram_banks_used |= 1 << ((data >> 2) & 3);
			do_prg_sync = 1;
		}

		/* Select high or low 256K */
		if (board->prg_rom.size == SIZE_512K) {
			if (board->prg_or != (data & 0x10)) {
				board->prg_or = data & 0x10;
				do_prg_sync = 1;
			}
		}

		switch (board->info->board_type) {
		case BOARD_TYPE_SNROM:
			_wram_disable =
			    (_wram_disable & 0x01) | ((data & 0x10) >> 3);
			board->prg_banks[0].perms =
			    (_wram_disable) ? MAP_PERM_NONE :
			    MAP_PERM_READWRITE;
			do_prg_sync = 1;
			break;
		case BOARD_TYPE_EVENT:
			data >>= 1;
			board->prg_banks[3].bank = data & 0x03;
			if (data & 0x04) {
				board->prg_banks[3].size = 0;
				board->prg_or = 0x08;
				board->prg_and = 0x07;
			} else {
				board->prg_banks[3].size = SIZE_32K;
				board->prg_and = 0x03;
				board->prg_or = 0x00;
			}

			if (data & 0x08) {
				cpu_interrupt_ack(board->emu->cpu, IRQ_M2_TIMER);
				cpu_interrupt_cancel(board->emu->cpu,
						     IRQ_M2_TIMER);
				board->irq_counter = 0;
			} else if (!board->irq_counter) {
				board->irq_counter = 0x10 |
				    (board_get_dip_switches(board) & 0xf);
				board->irq_counter <<= 25;
				board->irq_counter_timestamp =
				    cycles;
			}

			do_prg_sync = 1;
			break;
		}

		if (do_prg_sync)
			board_prg_sync(board);

	}

	board_chr_sync(board, 0);
}

static CPU_WRITE_HANDLER(mmc1_write_handler)
{
	struct board *board = emu->board;

	/* MMC1 ignores writes that are too close together.  Adjacent
	   STA instructions work, for example, but Read/Modify/Write
	   ones only seem to get one write through instead of two. */
	if (cycles - board->timestamps[0] ==
	    emu->cpu_clock_divider) {
		return;
	}
	board->timestamps[0] = cycles;

	if (value & 0x80) {
		_load_data = 0;
		_load_count = 0;
		board->prg_mode = 0x0c;
		board_prg_sync(board);
		return;
	} else {
		_load_data |= (value & 1) << _load_count;
		if (++_load_count < 5)
			return;
	}

	_load_count = 0;
	board->timestamps[0] = 0;

	switch (addr & 0xe000) {
	case 0x8000:
		mmc1_write_control(board, _load_data, cycles);
		break;
	case 0xa000:
		mmc1_write_chr(board, 0, _load_data, cycles);
		break;
	case 0xc000:
		mmc1_write_chr(board, 1, _load_data, cycles);
		break;
	case 0xe000:
		mmc1_write_prg(board, _load_data, cycles);
		break;
	}

	_load_data = 0;
}

static void sxrom_compat_nvram_post_load(struct board *board)
{
	if (board->save_file_size == SIZE_8K) {
		int i;

		for (i = 1; i < 4; i++) {
			memcpy(board->nv_ram_data + (i * SIZE_8K),
			       board->nv_ram_data, SIZE_8K);
		}
	} else if (board->save_file_size == SIZE_16K) {
		memcpy(board->nv_ram_data + SIZE_16K,
		       board->nv_ram_data + SIZE_8K,
		       SIZE_8K);
	}
}

static void sxrom_compat_nvram_pre_save(struct board *board)
{
	int size;
	int sorom_mask;
	int snrom_mask;

	size = SIZE_32K;
	sorom_mask = (1 << 0) | (1 << 2);
	snrom_mask = (1 << 0);

	if (board->save_file_size == SIZE_16K) {
		if (board->prg_rom.size < SIZE_512K) {
			if ((_wram_banks_used == sorom_mask) ||
			    (_wram_banks_used == snrom_mask)) {
				size = SIZE_16K;
			}
		}
	} else if ((board->save_file_size == SIZE_8K) ||
		   (board->save_file_size == 0)) {
		if (board->prg_rom.size < SIZE_512K &&
		    _wram_banks_used == sorom_mask) {
			size = SIZE_16K;
		} else if (_wram_banks_used == snrom_mask) {
			size = SIZE_8K;
		}
	}

	if (size == SIZE_16K) {
		memcpy(board->nv_ram_data + SIZE_8K,
		       board->nv_ram_data + SIZE_16K,
		       SIZE_8K);
	}

	board->save_file_size = size;
}
