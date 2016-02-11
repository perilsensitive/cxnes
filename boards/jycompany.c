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
#include "a12_timer.h"

#define IRQ_SOURCE_MASK 0x03
#define IRQ_DIRECTION_MASK 0xc0
#define IRQ_SOURCE_CPU_CYCLES 0x00
#define IRQ_SOURCE_PPU_A12    0x01
#define IRQ_SOURCE_PPU_READS  0x02
#define IRQ_SOURCE_CPU_WRITES 0x03
#define IRQ_DIRECTION_DOWN    0x80
#define IRQ_DIRECTION_UP      0x40
#define IRQ_DIRECTION_HALT    0x00
#define IRQ_DIRECTION_HALT2   0xc0
#define IRQ_PRESCALER_SIZE    0x04

#define CHR_MODE_BLOCK  0x04
#define CHR_MODE_MIRROR 0x10
#define PRG_MODE_S      0x80
#define NMT_MODE_R      0x40
#define NMT_MODE_N      0x20

#define _chr_block  board->data[0]
#define _nmt_mode   board->data[1]
#define _mirroring  board->data[2]
#define _nmt_select board->data[3]
#define _mult1      board->data[4]
#define _mult2      board->data[5]
#define _irq_xor    board->data[6]
#define _chr_latch  (board->data + 7)
#define _ram        (board->data + 9)

#define _jy_nmt_banks board->timestamps
#define _product board->timestamps[7]

static CPU_WRITE_HANDLER(jycompany_write_handler);
static void jycompany_reset(struct board *board, int);

static const uint8_t reverse_lookup[256] = {
	0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0,
	0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0,
	0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8,
	0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8,
	0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 0xe4,
	0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4,
	0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec,
	0x1c, 0x9c, 0x5c, 0xdc, 0x3c, 0xbc, 0x7c, 0xfc,
	0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2,
	0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2,
	0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea,
	0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa,
	0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6,
	0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6, 0x76, 0xf6,
	0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee,
	0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe,
	0x01, 0x81, 0x41, 0xc1, 0x21, 0xa1, 0x61, 0xe1,
	0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1,
	0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9,
	0x19, 0x99, 0x59, 0xd9, 0x39, 0xb9, 0x79, 0xf9,
	0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5,
	0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5,
	0x0d, 0x8d, 0x4d, 0xcd, 0x2d, 0xad, 0x6d, 0xed,
	0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd,
	0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3,
	0x13, 0x93, 0x53, 0xd3, 0x33, 0xb3, 0x73, 0xf3,
	0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb,
	0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb,
	0x07, 0x87, 0x47, 0xc7, 0x27, 0xa7, 0x67, 0xe7,
	0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7,
	0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef,
	0x1f, 0x9f, 0x5f, 0xdf, 0x3f, 0xbf, 0x7f, 0xff,
};

static struct bank jycompany_init_prg[] = {
	{0, 0, 0, 0x6000, MAP_PERM_READ, MAP_TYPE_ROM},
	{-1, 0, 0, 0x8000, MAP_PERM_READ, MAP_TYPE_ROM},
	{-1, 0, 0, 0xa000, MAP_PERM_READ, MAP_TYPE_ROM},
	{-1, 0, 0, 0xc000, MAP_PERM_READ, MAP_TYPE_ROM},
	{-1, 0, 0, 0xe000, MAP_PERM_READ, MAP_TYPE_ROM},
	{-1, 0, SIZE_32K, 0x8000, MAP_PERM_READ, MAP_TYPE_ROM},
	{.type = MAP_TYPE_END},
};

static struct bank jycompany_init_chr[] = {
	{-1, 0, SIZE_8K, 0x0000, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{0, 0, 0, 0x0400, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{0, 0, 0, 0x0800, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{0, 0, 0, 0x0c00, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{0, 0, 0, 0x1000, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{0, 0, 0, 0x1400, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{0, 0, 0, 0x1800, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{0, 0, 0, 0x1c00, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	/* "Mirror" char mode support */
	{0, 0, 0, 0x0800, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{0, 0, 0, 0x0c00, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{.type = MAP_TYPE_END},
};

/* FIXME placeholder */
#define reverse_bank(b, k) ((b)->prg_banks[k].bank = reverse_lookup[(b)->prg_banks[k].bank] >> 1)

static void jycompany_ppu_read_hook(struct board *board, int ppu_addr)
{
	int latch;

	if (ppu_addr >= 0x2000)
		return;

	latch = ppu_addr >> 12;

	switch (ppu_addr & 0xff8) {
	case 0xfd8:
	case 0xfe8:
		_chr_latch[latch] = ppu_addr >> 4 &
			((ppu_addr >> 10 & 0x04) | 0x02);
		break;
	default:
		return;
	}

	board->chr_banks0[_chr_latch[latch]].size = SIZE_4K;
	board->chr_banks0[_chr_latch[latch] ^ 2].size = 0;
	/* printf("sizes: %d %d %d %d %d %d\n", */
	/*        latch, _chr_latch[latch], */
	/*        board->chr_banks0[0].size, */
	/*        board->chr_banks0[2].size, */
	/*        board->chr_banks0[4].size, */
	/*        board->chr_banks0[6].size); */
	board_chr_sync(board, 0);
}

static void jycompany_set_prg_mode(struct board *board, int mode)
{
	int last_bank;

	if (mode & 0x04)
		last_bank = 4;
	else
		last_bank = 5;

	if (mode & PRG_MODE_S)
		board->prg_banks[0].size = SIZE_8K;
	else
		board->prg_banks[0].size = 0;

	switch (mode & 0x03)
	{
	case 0x00:
		board->prg_banks[0].bank = board->prg_banks[4].bank * 4 + 3;
		board->prg_banks[1].size = 0;
		board->prg_banks[2].size = 0;
		board->prg_banks[3].size = 0;
		board->prg_banks[last_bank ^ 1].size = 0;
		board->prg_banks[last_bank].size = SIZE_32K;
		board->prg_banks[last_bank].address = 0x8000;
		board->prg_banks[last_bank].shift = 2;
		break;
	case 0x01:
		board->prg_banks[0].bank = board->prg_banks[4].bank * 2 + 3;
		board->prg_banks[1].size = 0;
		board->prg_banks[2].size = SIZE_16K;
		board->prg_banks[3].size = 0;
		board->prg_banks[last_bank ^ 1].size = 0;
		board->prg_banks[last_bank].size = SIZE_16K;
		board->prg_banks[2].address = 0x8000;
		board->prg_banks[last_bank].address = 0xc000;
		board->prg_banks[2].shift = 1;
		board->prg_banks[last_bank].shift = 1;
		break;
	case 0x02:
	case 0x03:
		board->prg_banks[0].bank = board->prg_banks[4].bank;
		board->prg_banks[1].size = SIZE_8K;
		board->prg_banks[2].size = SIZE_8K;
		board->prg_banks[3].size = SIZE_8K;
		board->prg_banks[1].shift = 0;
		board->prg_banks[2].shift = 0;
		board->prg_banks[3].shift = 0;
		board->prg_banks[last_bank].shift = 0;
		board->prg_banks[last_bank ^ 1].size = 0;
		board->prg_banks[last_bank].size = SIZE_8K;
		board->prg_banks[1].address = 0x8000;
		board->prg_banks[2].address = 0xa000;
		board->prg_banks[3].address = 0xc000;
		board->prg_banks[last_bank].address = 0xe000;
		break;
	}

	if ((board->prg_mode ^ mode) & 0x01) {
		reverse_bank(board, 0);
		reverse_bank(board, 1);
		reverse_bank(board, 2);
		reverse_bank(board, 3);
		reverse_bank(board, 4);
	}

	board->prg_mode = mode;
	board_prg_sync(board);
}

static void jycompany_set_chr_mode(struct board *board, int mode)
{
	int i;
	int mirror;

	for (i = 0; i < 10; i++) {
		board->chr_banks0[i].size = 0;
		board->chr_banks0[i].address = i * 0x400;
	}

	mirror = mode & CHR_MODE_MIRROR;
	ppu_set_read_hook(NULL);

	switch (mode & 0x03) {
	case 0x00:
		board->chr_banks0[0].size = SIZE_8K;
		break;
	case 0x01:
		/* FIXME I have no idea if this is right.  Nestopia doesn't
		   support the 'mirror mode' bit at all, and fceux recognizes
		   it but doesn't actually do anything with it.  There is at
		   least one rom that sets the mirror bit with 4k mode, and
		   that expects MMC2/MMC4-like chr bankswitching.

		   Since having a 4K bank mode without MMC2-like switching
		   would be useful, and the ROM that expects the MMC2 switching
		   sets the mirror flag, for now I'm assuming that setting it
		   enables the MMC2-like behavior.
		*/
		board->chr_banks0[0].size = SIZE_4K;
		board->chr_banks0[4].size = SIZE_4K;
		board->chr_banks0[0].address = 0x0000;
		board->chr_banks0[2].address = 0x0000;
		board->chr_banks0[4].address = 0x1000;
		board->chr_banks0[6].address = 0x1000;
		if (mirror)
			ppu_set_read_hook(jycompany_ppu_read_hook);
		break;
	case 0x02:
		board->chr_banks0[0].size = SIZE_2K;
		board->chr_banks0[2].size = mirror ? 0 : SIZE_2K;
		board->chr_banks0[4].size = SIZE_2K;
		board->chr_banks0[6].size = SIZE_2K;
		board->chr_banks0[8].size = mirror ? SIZE_2K : 0;
		break;
	case 0x03:
		board->chr_banks0[0].size = SIZE_1K;
		board->chr_banks0[1].size = SIZE_1K;
		board->chr_banks0[2].size = mirror ? 0 : SIZE_1K;
		board->chr_banks0[3].size = mirror ? 0 : SIZE_1K;
		board->chr_banks0[4].size = SIZE_1K;
		board->chr_banks0[5].size = SIZE_1K;
		board->chr_banks0[6].size = SIZE_1K;
		board->chr_banks0[7].size = SIZE_1K;
		board->chr_banks0[8].size = mirror ? SIZE_1K : 0;
		board->chr_banks0[9].size = mirror ? SIZE_1K : 0;
		break;
	}

	if (!(mode & CHR_MODE_BLOCK)) {
		board->chr_and = 0xff;
		board->chr_or = _chr_block << 8;
	} else {
		board->chr_and = ~0;
		board->chr_or = 0;
	}

	board->chr_mode = mode;
	board_chr_sync(board, 0);
}

static void jycompany_set_mirroring(struct board *board)
{
	int i;
	int type;
	int bank;

	if ((_nmt_mode & NMT_MODE_N)) {
		for (i = 0; i < 4; i++) {
			type = MAP_TYPE_ROM;
			bank = _jy_nmt_banks[i];

			if (!(_nmt_mode & NMT_MODE_R)) {
				if (!((bank ^ _nmt_select) & 0x80)) {
					bank &= 1;
					type = MAP_TYPE_CIRAM;
				}
			}

			board->nmt_banks[i].bank = bank;
			board->nmt_banks[i].type = type;
		}

		board_nmt_sync(board);
		return;
	}


	switch (_mirroring) {
	case 0:
		board_set_ppu_mirroring(board, MIRROR_V);
		break;
	case 1:
		board_set_ppu_mirroring(board, MIRROR_H);
		break;
	case 2:
		board_set_ppu_mirroring(board, MIRROR_1A);
		break;
	case 3:
		board_set_ppu_mirroring(board, MIRROR_1B);
		break;
	}
}

static CPU_READ_HANDLER(jycompany_read_handler)
{
	struct board *board = emu->board;

	if (addr == 0x5000) {
		value = 0x00;
	} else if (addr == 0x5800) {
		value = _product & 0xff;
	} else if (addr == 0x5801) {
		value = (_product >> 8) & 0xff;
	} else if ((addr >= 0x5803) && (addr <= 0x5807)) {
		value = _ram[(addr & 0x07) - 3];
	}

	return value;
}

static CPU_WRITE_HANDLER(jycompany_irq_control)
{
	int old_source, new_source;
	int old_direction, new_direction;
	int prescaler_size;
	struct board *board;

	/* printf("control: %x\n", value); */

	board = emu->board;
	old_source = board->irq_control & IRQ_SOURCE_MASK;
	new_source = value & IRQ_SOURCE_MASK;
	old_direction = board->irq_control & IRQ_DIRECTION_MASK;
	new_direction = value & IRQ_DIRECTION_MASK;
	if (old_direction == IRQ_DIRECTION_HALT2)
		old_direction = IRQ_DIRECTION_HALT;
	if (new_direction == IRQ_DIRECTION_HALT2)
		new_direction = IRQ_DIRECTION_HALT;

	if (new_source != old_source) {
		switch (old_source) {
		case IRQ_SOURCE_CPU_CYCLES:
			break;
		case IRQ_SOURCE_PPU_A12:
			a12_timer_set_counter_enabled(emu->a12_timer, 0, cycles);
			break;
		case IRQ_SOURCE_PPU_READS:
			break;
		case IRQ_SOURCE_CPU_WRITES:
			break;
		}

		/* if (direction != IRQ_DIRECTION_HALT) { */
		/* 	switch (new_source) { */
		/* 	case IRQ_SOURCE_CPU_CYCLES: */
		/* 		break; */
		/* 	case IRQ_SOURCE_PPU_A12: */
		/* 		break; */
		/* 	case IRQ_SOURCE_PPU_READS: */
		/* 		break; */
		/* 	case IRQ_SOURCE_CPU_WRITES: */
		/* 		break; */
		/* 	} */
		/* } */
	}

	if (new_direction != old_direction) {
		int flags;

		switch (new_source) {
		case IRQ_SOURCE_CPU_CYCLES:
			break;
		case IRQ_SOURCE_PPU_A12:
			flags = a12_timer_get_flags(board->emu->a12_timer);
			if (new_direction == IRQ_DIRECTION_UP) {
				flags |= A12_TIMER_FLAG_COUNT_UP;
				a12_timer_set_flags(board->emu->a12_timer, flags, cycles);
				a12_timer_set_counter_enabled(board->emu->a12_timer, 1, cycles);
			} else if (new_direction == IRQ_DIRECTION_DOWN) {
				flags &= ~A12_TIMER_FLAG_COUNT_UP;
				a12_timer_set_flags(board->emu->a12_timer, flags, cycles);
				a12_timer_set_counter_enabled(board->emu->a12_timer, 1, cycles);
			} else {
				/* printf("halt\n"); */
				a12_timer_set_counter_enabled(board->emu->a12_timer, 0, cycles);
			}
			break;
		case IRQ_SOURCE_PPU_READS:
			break;
		case IRQ_SOURCE_CPU_WRITES:
			break;
		}
	}

	if (value & IRQ_PRESCALER_SIZE)
		prescaler_size = 3;
	else
		prescaler_size = 8;

	switch (new_source) {
	case IRQ_SOURCE_CPU_CYCLES:
		break;
	case IRQ_SOURCE_PPU_A12:
		a12_timer_set_prescaler_size(board->emu->a12_timer,
					     prescaler_size, cycles);
		break;
	case IRQ_SOURCE_PPU_READS:
		break;
	case IRQ_SOURCE_CPU_WRITES:
		break;
	}

	board->irq_control = value;
}

static CPU_WRITE_HANDLER(jycompany_irq_enable)
{
	struct board *board = emu->board;

	switch (board->irq_control & IRQ_SOURCE_MASK) {
	case IRQ_SOURCE_CPU_CYCLES:
		break;
	case IRQ_SOURCE_PPU_A12:
		a12_timer_irq_enable(emu, addr, value, cycles);
		break;
	case IRQ_SOURCE_PPU_READS:
		break;
	case IRQ_SOURCE_CPU_WRITES:
		break;
	}
}

static CPU_WRITE_HANDLER(jycompany_irq_disable)
{
	struct board *board = emu->board;

	switch (board->irq_control & IRQ_SOURCE_MASK) {
	case IRQ_SOURCE_CPU_CYCLES:
		break;
	case IRQ_SOURCE_PPU_A12:
		a12_timer_irq_disable(emu, addr, value, cycles);
		break;
	case IRQ_SOURCE_PPU_READS:
		break;
	case IRQ_SOURCE_CPU_WRITES:
		break;
	}
}

static CPU_WRITE_HANDLER(jycompany_irq_set_counter)
{
	struct board *board = emu->board;

	switch (board->irq_control & IRQ_SOURCE_MASK) {
	case IRQ_SOURCE_CPU_CYCLES:
		break;
	case IRQ_SOURCE_PPU_A12:
		a12_timer_set_counter(emu->a12_timer, value, cycles);
		break;
	case IRQ_SOURCE_PPU_READS:
		break;
	case IRQ_SOURCE_CPU_WRITES:
		break;
	}
}

static CPU_WRITE_HANDLER(jycompany_irq_set_prescaler)
{
	struct board *board = emu->board;

	switch (board->irq_control & IRQ_SOURCE_MASK) {
	case IRQ_SOURCE_CPU_CYCLES:
		break;
	case IRQ_SOURCE_PPU_A12:
		a12_timer_set_prescaler(emu->a12_timer, value, cycles);
		break;
	case IRQ_SOURCE_PPU_READS:
		break;
	case IRQ_SOURCE_CPU_WRITES:
		break;
	}
}

static CPU_WRITE_HANDLER(jycompany_write_handler)
{
	struct board *board;
	int bank;
	int tmp;

	board = emu->board;

	/* printf("wrote %x to %x\n", value, addr); */

	/* if (addr >= 0xe000) */
	/* 	return; */

	if (addr == 0x5800) {
		_mult1 = value;
		_product = _mult1 * _mult2;
		return;
	} else if (addr == 0x5801) {
		_mult2 = value;
		_product = _mult1 * _mult2;
		return;
	} else if ((addr >= 0x5803) && (addr <= 0x5807)) {
		_ram[(addr & 0x07) - 3] = value;
		return;
	}

	switch (addr & 0xf007) {
	case 0x8000:
	case 0x8001:
	case 0x8002:
	case 0x8003:
	case 0x8004:
	case 0x8005:
	case 0x8006:
	case 0x8007:
		bank = addr & 0x03;
		if ((board->prg_mode & 3) == 3)
			value = reverse_lookup[value];
		update_prg_bank(board, bank + 1, value);
		break;
	case 0x9000:
	case 0x9001:
	case 0x9002:
	case 0x9003:
	case 0x9004:
	case 0x9005:
	case 0x9006:
	case 0x9007:
		bank = addr & 0x0007;
		board->chr_banks0[bank].bank &= 0xff00;
		board->chr_banks0[bank].bank |= value;
		if (bank < 2) {
			board->chr_banks0[bank + 8].bank &= 0xff00;
			board->chr_banks0[bank + 8].bank |= value;
		}
		board_chr_sync(board, 0);
		break;
	case 0xa000:
	case 0xa001:
	case 0xa002:
	case 0xa003:
	case 0xa004:
	case 0xa005:
	case 0xa006:
	case 0xa007:
		bank = addr & 0x0007;
		board->chr_banks0[bank].bank &= 0x00ff;
		board->chr_banks0[bank].bank |= value << 8;
		if (bank < 2) {
			board->chr_banks0[bank + 8].bank &= 0x00ff;
			board->chr_banks0[bank + 8].bank |= value << 8;
		}
		board_chr_sync(board, 0);
		break;
	case 0xb000:
	case 0xb001:
	case 0xb002:
	case 0xb003:
		bank = addr & 0x03;
		_jy_nmt_banks[bank] &= 0xff00;
		_jy_nmt_banks[bank] |= value;
		jycompany_set_mirroring(board);
		break;
	case 0xb004:
	case 0xb005:
	case 0xb006:
	case 0xb007:
		bank = addr & 0x03;
		_jy_nmt_banks[bank] &= 0x00ff;
		_jy_nmt_banks[bank] |= value << 8;
		jycompany_set_mirroring(board);
		break;

	case 0xc000:
		if (value & 1)
			jycompany_irq_enable(emu, addr, value, cycles);
		else
			jycompany_irq_disable(emu, addr, value, cycles);
		break;
	case 0xc001:
		jycompany_irq_control(emu, addr, value, cycles);
		break;
	case 0xc002:
		jycompany_irq_disable(emu, addr, value, cycles);
		break;
	case 0xc003:
		jycompany_irq_enable(emu, addr, value, cycles);
		break;
	case 0xc004:
		jycompany_irq_set_prescaler(emu, addr, value ^ _irq_xor, cycles);
		break;
	case 0xc005:
		jycompany_irq_set_counter(emu, addr, value ^ _irq_xor, cycles);
		break;
	case 0xc006:
		_irq_xor = value;
		break;
	case 0xc007:
		break;

	case 0xd000:
	case 0xd004:
		tmp = ((value & 0x18) >> 3) |
			(board->chr_mode & (CHR_MODE_BLOCK|CHR_MODE_MIRROR));

		_nmt_mode = value & (NMT_MODE_R|NMT_MODE_N);
		if (board->info->board_type == BOARD_TYPE_JYCOMPANY_A)
			_nmt_mode &= ~NMT_MODE_N;
		else if (board->info->board_type == BOARD_TYPE_JYCOMPANY_C)
			_nmt_mode |= NMT_MODE_N;

		jycompany_set_prg_mode(board, value & 0x87);
		jycompany_set_chr_mode(board, tmp);
		jycompany_set_mirroring(board);
		break;
	case 0xd001:
	case 0xd005:
		_mirroring = value & 0x03;
		jycompany_set_mirroring(board);
		break;
	case 0xd002:
	case 0xd006:
		_nmt_select = value & 0x80;
		jycompany_set_mirroring(board);
		break;
	case 0xd003:
	case 0xd007:
		_chr_block = value & 0x1f;
		tmp = (value >> 3) & (CHR_MODE_BLOCK|CHR_MODE_MIRROR);
		tmp |= board->chr_mode & 0x03;
		jycompany_set_chr_mode(board, tmp);
		break;
	}
}


static void jycompany_reset(struct board *board, int hard)
{
	a12_timer_reset(board->emu->a12_timer, hard);
	
	if (!hard)
		return;

	_chr_latch[0] = 0;
	_chr_latch[1] = 4;

	if (board->info->board_type == BOARD_TYPE_JYCOMPANY_C)
		board_set_ppu_mirroring(board, MIRROR_1A);
	else
		board_set_ppu_mirroring(board, MIRROR_V);

	a12_timer_set_flags(board->emu->a12_timer,
			    A12_TIMER_FLAG_WRAP|A12_TIMER_FLAG_IRQ_ON_WRAP, 0);
	a12_timer_set_delta(board->emu->a12_timer, 1, 0); /* may be 0 vs 1? */
}

static void jycompany_end_frame(struct board *board, uint32_t cycles)
{
	a12_timer_end_frame(board->emu->a12_timer, cycles);
	/* FIXME other IRQ sources */
}

static int jycompany_init(struct board *board)
{
	int rc;

	/* FIXME: shouldn't need to specify the variant */
	rc = a12_timer_init(board->emu, A12_TIMER_VARIANT_MMC3_STD);

	/* FIXME other irq sources */

	return rc;
}

static void jycompany_cleanup(struct board *board)
{
	a12_timer_cleanup(board->emu);
	/* FIXME other irq sources */
}

static struct board_funcs jycompany_funcs = {
	.init = jycompany_init,
	.cleanup = jycompany_cleanup,
	.reset = jycompany_reset,
	.end_frame = jycompany_end_frame,
};

static struct board_write_handler jycompany_write_handlers[] = {
	{jycompany_write_handler, 0x5000, SIZE_32K + 12 * 1024, 0},
	{NULL}
};

static struct board_read_handler jycompany_read_handlers[] = {
	{jycompany_read_handler, 0x5000, 1, 0},
	{jycompany_read_handler, 0x5800, 8, 0},
	{NULL}
};

struct board_info board_jycompany_a = {
	.board_type = BOARD_TYPE_JYCOMPANY_A,
	.name = "JYCOMPANY-A",
	.funcs = &jycompany_funcs,
	.init_prg = jycompany_init_prg,
	.init_chr0 = jycompany_init_chr,
	.write_handlers = jycompany_write_handlers,
	.read_handlers = jycompany_read_handlers,
	.max_wram_size = { SIZE_8K, 0 },
	.max_prg_rom_size = SIZE_1024K,
	.max_chr_rom_size = SIZE_1024K, /* FIXME not sure what this should be */
	.flags = BOARD_INFO_FLAG_MIRROR_M,
};

struct board_info board_jycompany_b = {
	.board_type = BOARD_TYPE_JYCOMPANY_B,
	.name = "JYCOMPANY-B",
	.funcs = &jycompany_funcs,
	.init_prg = jycompany_init_prg,
	.init_chr0 = jycompany_init_chr,
	.write_handlers = jycompany_write_handlers,
	.read_handlers = jycompany_read_handlers,
	.max_wram_size = { SIZE_8K, 0 },
	.max_prg_rom_size = SIZE_1024K,
	.max_chr_rom_size = SIZE_1024K, /* FIXME not sure what this should be */
	.flags = BOARD_INFO_FLAG_MIRROR_M,
};

struct board_info board_jycompany_c = {
	.board_type = BOARD_TYPE_JYCOMPANY_C,
	.name = "JYCOMPANY-C",
	.funcs = &jycompany_funcs,
	.init_prg = jycompany_init_prg,
	.init_chr0 = jycompany_init_chr,
	.write_handlers = jycompany_write_handlers,
	.read_handlers = jycompany_read_handlers,
	.max_wram_size = { SIZE_8K, 0 },
	.max_prg_rom_size = SIZE_1024K,
	.max_chr_rom_size = SIZE_1024K, /* FIXME not sure what this should be */
	.flags = BOARD_INFO_FLAG_MIRROR_M,
};
