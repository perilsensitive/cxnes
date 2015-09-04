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

#ifndef __BOARD_PRIVATE_H__
#define __BOARD_PRIVATE_H__

#if defined _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "emu.h"

#define MIN(a, b) ((a) > (b) ? (b) : (a))
#define MIRROR_HV  (MIRROR_H|MIRROR_V)
#define BOARD_DESCRIPTOR(x) { x, sizeof(x) / sizeof(x[0]) }

#define page_to_offset(page, bsize, size) (((page) % ((size) / (bsize))) * (bsize))

#define ARRAY_SIZE(list) (sizeof(list) / sizeof((list)[0]))

#define BOARD_STD(t, n, m, f, ip, ic0, wh, p, c, v) { t, n, m, f, ip, ic0, NULL,	\
		NULL, wh, \
		p, c, { 0, 0 },				\
		{ SIZE_8K, 0 }, { 0, 0 }, { 0, 0 }, v, 0 }
#define BOARD_VS(t, n, m, f, ip, ic0, wh, p, c) { t, n, m, f, ip, ic0, NULL, \
		NULL, wh, \
		p, c, { SIZE_2K, 0 },				\
		{ SIZE_2K, 0 }, { 0, 0 }, { 0, 0 }, MIRROR_4, 0 }
#define BOARD_STD_NORAM(t, n, m, f, ip, ic0, wh, p, c, v) { t, n, m, f, ip, ic0, NULL, \
		NULL, wh, \
		p, c, { 0, 0 },				\
		{ 0, 0 }, { 0, 0 }, { 0, 0 }, v, 0 }

enum {
	CHIP_TYPE_NONE,
	CHIP_TYPE_PRG_ROM,
	CHIP_TYPE_CHR_ROM,
	CHIP_TYPE_WRAM,
	CHIP_TYPE_WRAM_NV,
	CHIP_TYPE_VRAM,
	CHIP_TYPE_VRAM_NV,
	CHIP_TYPE_CIRAM,
	CHIP_TYPE_MAPPER_RAM,
	CHIP_TYPE_MAPPER_RAM_NV,
};

#define update_prg_bank(board, slot, value) { \
	uint8_t _old; \
	\
	_old = (board)->prg_banks[(slot)].bank; \
	\
	if (_old != (value)) { \
		(board)->prg_banks[(slot)].bank = value;	\
		board_prg_sync(board); \
	} \
}

#define update_chr0_bank(board, slot, value) { \
	uint8_t _old; \
	\
	_old = (board)->chr_banks0[(slot)].bank; \
	\
	if (_old != (value)) { \
		(board)->chr_banks0[(slot)].bank = value;	\
		board_chr_sync((board), 0);				\
	} \
}

extern CPU_WRITE_HANDLER(simple_prg_write_handler);
extern CPU_WRITE_HANDLER(simple_prg_no_conflict_write_handler);
extern CPU_WRITE_HANDLER(simple_chr_write_handler);
extern CPU_WRITE_HANDLER(simple_chr_no_conflict_write_handler);
extern CPU_WRITE_HANDLER(standard_mirroring_handler);

struct bank {
	unsigned int bank;
	int shift;
	size_t size;
	uint16_t address;
	int perms;
	int type;
};

struct nmt_bank {
	int bank;
	int type;
	int perms;
};

struct board_write_handler {
	cpu_write_handler_t *handler;
	int addr;
	size_t size;
	int mask;
};

struct board_read_handler {
	cpu_read_handler_t *handler;
	int addr;
	size_t size;
	int mask;
};

struct board_info {
	uint32_t board_type;
	const char *name;
	const char *mapper_name;
	uint8_t *mirroring_values;
	uint8_t mirroring_shift;
	struct board_funcs *funcs;

	struct bank *init_prg;
	struct bank *init_chr0;
	struct bank *init_chr1;

	struct board_read_handler *read_handlers;
	struct board_write_handler *write_handlers;

	size_t max_prg_rom_size;
	size_t max_chr_rom_size;
	size_t min_wram_size[2];
	size_t max_wram_size[2];
	size_t min_vram_size[2];
	size_t max_vram_size[2];
	size_t mapper_ram_size;
	int flags;
};

struct board_descriptor {
	struct board_info *info;
	int info_count;
};

struct chip {
	uint8_t *data;
	size_t size;
	int type;
	int mmap;
};

struct board {
	struct board_info *info;
	int mirroring;
	int auto_wram0;

	int use_mmap;
	uint8_t *nv_ram_data;
	size_t save_file_size;
	size_t nv_ram_size;
#if defined __unix__
	int nv_ram_fd;
#elif defined _WIN32
	HANDLE nv_ram_handle;
	HANDLE nv_ram_map_handle;
#endif
	uint8_t *ram_data;
	size_t ram_size;

	/* Yes, CIRAM is located here, and not in ppu.c.  This is
	   because it's really up to the board to determine how CIRAM
	   is mapped into the PPU's address space, or if the internal
	   CIRAM chip is even used at all.  Since the mapper manages
	   CIRAM, it is effectively part of the board.

	   The upshot of this is that CIRAM can be easily mapped into
	   the CHR address space.  This is (so far) only used by one
	   mapper (nocash's single-chip).
	 */

	struct chip prg_rom;
	struct chip chr_rom;
	struct chip wram[2];
	struct chip vram[2];
	struct chip ciram;
	struct chip mapper_ram;

	uint8_t *fill_mode_nmt;
	struct range_list *modified_ranges;

	uint8_t prg_mode;
	uint8_t chr_mode;
	uint8_t irq_control;

	struct bank prg_banks[12];
	struct bank chr_banks0[10];
	struct bank chr_banks1[10];
	struct nmt_bank nmt_banks[4];

	unsigned int prg_and;
	unsigned int prg_or;
	unsigned int chr_and;
	unsigned int chr_or;
	unsigned int wram_and;
	unsigned int wram_or;

	int irq_counter;
	uint32_t irq_counter_reload;
	uint32_t irq_counter_timestamp;

	uint8_t data[16];

	/* Misc. timestamps for IRQs, that sort of thing. */
	uint32_t timestamps[8];

	/* dip switches (if any) */
	uint8_t dip_switches;
	void *private;

	int ppu_type;
	int num_dip_switches;

	struct emu *emu;
};

struct board_funcs {
	int  (*init) (struct board *);
	void (*reset) (struct board *, int hard);
	void (*cleanup) (struct board *);
	void (*end_frame) (struct board *, uint32_t);
	void (*nvram_pre_save)(struct board *);
	void (*nvram_post_load)(struct board *);
	void (*run) (struct board *, uint32_t);
	int  (*apply_config) (struct board *);
	int  (*load_state) (struct board *, struct save_state *);
	int  (*save_state) (struct board *, struct save_state *);
};

extern struct bank std_prg_46k[];
extern struct bank std_prg_32k[];
extern struct bank std_prg_16k[];
extern struct bank std_prg_8k[];
extern struct bank std_prg_4k[];
extern struct bank std_chr_8k[];
extern struct bank std_chr_4k[];
extern struct bank std_chr_2k[];
extern struct bank std_chr_2k_1k[];
extern struct bank std_chr_1k[];
extern uint8_t std_mirroring_vh[];
extern uint8_t std_mirroring_hv[];
extern uint8_t std_mirroring_01[];
extern uint8_t std_mirroring_vh01[];
extern uint8_t std_mirroring_hv01[];
#endif				/* __BOARD_PRIVATE_H_ */
