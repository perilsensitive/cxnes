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

#define _bank_select board->data[0]
#define _split_chr   board->data[1]

/* Security counter for VS. Super Xevious, VS. TKO Boxing and
   VS. RBI Baseball.  */
#define security_counter board->data[2]

static uint8_t vs_tko_boxing_security_data[32] = {
	0xFF, 0xBF, 0xB7, 0x97, 0x97, 0x17, 0x57, 0x4F,
	0x6F, 0x6B, 0xEB, 0xA9, 0xB1, 0x90, 0x94, 0x14,
	0x56, 0x4E, 0x6F, 0x6B, 0xEB, 0xA9, 0xB1, 0x90,
	0xD4, 0x5C, 0x3E, 0x26, 0x87, 0x83, 0x13, 0x00
};

static CPU_WRITE_HANDLER(namco108_write_handler);
static CPU_READ_HANDLER(vs_super_xevious_security);
static CPU_READ_HANDLER(vs_rbi_baseball_security);
static CPU_READ_HANDLER(vs_tko_boxing_security);

static void namco108_reset(struct board *board, int);

static struct board_funcs namco108_funcs = {
	.reset = namco108_reset,
};

static struct bank namco_3446_init_chr[] = {
	{0, 0, 0, 0x0000, MAP_PERM_READWRITE, MAP_TYPE_NONE},
	{0, 0, 0, 0x0000, MAP_PERM_READWRITE, MAP_TYPE_NONE},
	{0, 0, SIZE_2K, 0x0000, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{0, 0, SIZE_2K, 0x0800, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{0, 0, SIZE_2K, 0x1000, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{0, 0, SIZE_2K, 0x1800, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{.type = MAP_TYPE_END},
};

static struct bank namco_3433_init_chr[] = {
	{0, 1, SIZE_2K, 0x0000, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{0, 1, SIZE_2K, 0x0800, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{0x40, 0, SIZE_1K, 0x1000, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{0x40, 0, SIZE_1K, 0x1400, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{0x40, 0, SIZE_1K, 0x1800, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{0x40, 0, SIZE_1K, 0x1c00, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{.type = MAP_TYPE_END},
};

static struct board_write_handler namco108_write_handlers[] = {
	{namco108_write_handler, 0x8000, SIZE_32K, 0},
	{NULL}
};

static struct board_read_handler vs_xevious_read_handlers[] = {
	{vs_super_xevious_security, 0x54ff, 1, 0},
	{vs_super_xevious_security, 0x5567, 1, 0},
	{vs_super_xevious_security, 0x5678, 1, 0},
	{vs_super_xevious_security, 0x578f, 1, 0},
	{NULL},
};

static struct board_read_handler vs_rbi_baseball_handlers[] = {
	{vs_rbi_baseball_security, 0x5e00, 2, 0},
	{NULL},
};

static struct board_read_handler vs_tko_boxing_handlers[] = {
	{vs_tko_boxing_security, 0x5e00, 2, 0},
	{NULL},
};

struct board_info board_namco108 = {
	.board_type = BOARD_TYPE_NAMCO_108,
	.name = "NAMCOT-108",
	.mapper_name = "Namco 108",
	.funcs = &namco108_funcs,
	.init_prg = std_prg_8k,
	.init_chr0 = std_chr_2k_1k,
	.write_handlers = namco108_write_handlers,
	.max_prg_rom_size = SIZE_128K,
	.max_chr_rom_size = SIZE_64K,
	.max_wram_size = {SIZE_8K, 0},
};

struct board_info board_namco3446 = {
	.board_type = BOARD_TYPE_NAMCO_3446,
	.name = "NAMCOT-3446",
	.mapper_name = "Namco 108",
	.funcs = &namco108_funcs,
	.init_prg = std_prg_8k,
	.init_chr0 = namco_3446_init_chr,
	.write_handlers = namco108_write_handlers,
	.max_prg_rom_size = SIZE_128K,
	.max_chr_rom_size = SIZE_128K,
	.max_wram_size = {SIZE_8K, 0},
};

struct board_info board_namco3433 = {
	.board_type = BOARD_TYPE_NAMCO_3433,
	.name = "NAMCOT-3433",
	.mapper_name = "Namco 108",
	.funcs = &namco108_funcs,
	.init_prg = std_prg_8k,
	.init_chr0 = namco_3433_init_chr,
	.write_handlers = namco108_write_handlers,
	.max_prg_rom_size = SIZE_128K,
	.max_chr_rom_size = SIZE_128K,
	.max_wram_size = {SIZE_8K, 0},
};

struct board_info board_namco3453 = {
	.board_type = BOARD_TYPE_NAMCO_3453,
	.name = "NAMCOT-3453",
	.mapper_name = "Namco 108",
	.funcs = &namco108_funcs,
	.init_prg = std_prg_8k,
	.init_chr0 = namco_3433_init_chr,
	.write_handlers = namco108_write_handlers,
	.max_prg_rom_size = SIZE_128K,
	.max_chr_rom_size = SIZE_128K,
	.max_wram_size = {SIZE_8K, 0},
	.flags = BOARD_INFO_FLAG_MIRROR_M,
	.mirroring_values = std_mirroring_01,
	.mirroring_shift = 6,
};

struct board_info board_namco3425 = {
	.board_type = BOARD_TYPE_NAMCO_3425,
	.name = "NAMCOT-3425",
	.mapper_name = "Namco 108",
	.funcs = &namco108_funcs,
	.init_prg = std_prg_8k,
	.init_chr0 = namco_3433_init_chr,
	.write_handlers = namco108_write_handlers,
	.max_prg_rom_size = SIZE_128K,
	.max_chr_rom_size = SIZE_64K,
	.max_wram_size = {SIZE_8K, 0},
	.flags = BOARD_INFO_FLAG_MIRROR_M,
};

struct board_info board_vs_super_xevious = {
	.board_type = BOARD_TYPE_VS_SUPER_XEVIOUS,
	.name = "NAMCOT-108-VS-SUPER-XEVIOUS",
	.mapper_name = "Namco 108",
	.funcs = &namco108_funcs,
	.init_prg = std_prg_8k,
	.init_chr0 = std_chr_2k_1k,
	.read_handlers = vs_xevious_read_handlers,
	.write_handlers = namco108_write_handlers,
	.max_prg_rom_size = SIZE_128K,
	.max_chr_rom_size = SIZE_64K,
};

struct board_info board_vs_rbi_baseball = {
	.board_type = BOARD_TYPE_VS_RBI_BASEBALL,
	.name = "NAMCOT-108-VS-RBI-BASEBALL",
	.mapper_name = "Namco 108",
	.funcs = &namco108_funcs,
	.init_prg = std_prg_8k,
	.init_chr0 = std_chr_2k_1k,
	.read_handlers = vs_rbi_baseball_handlers,
	.write_handlers = namco108_write_handlers,
	.max_prg_rom_size = SIZE_128K,
	.max_chr_rom_size = SIZE_64K,
};

struct board_info board_vs_tko_boxing = {
	.board_type = BOARD_TYPE_VS_TKO_BOXING,
	.name = "NAMCOT-108-VS-TKO-BOXING",
	.mapper_name = "Namco 108",
	.funcs = &namco108_funcs,
	.init_prg = std_prg_8k,
	.init_chr0 = std_chr_2k_1k,
	.read_handlers = vs_tko_boxing_handlers,
	.write_handlers = namco108_write_handlers,
	.max_prg_rom_size = SIZE_128K,
	.max_chr_rom_size = SIZE_64K,
};

static void namco108_reset(struct board *board, int hard)
{
	unsigned long type;

	type = board->info->board_type;

	if (hard)
		_bank_select = 0;

	if ((type == BOARD_TYPE_NAMCO_3453) || (type == BOARD_TYPE_NAMCO_3433)) {
		_split_chr = 1;
	} else {
		_split_chr = 0;
	}

	security_counter = 0;
}

static CPU_WRITE_HANDLER(namco108_write_handler)
{
	struct board *board;
	unsigned long type;

	board = emu->board;
	type = board->info->board_type;

	if (type == BOARD_TYPE_NAMCO_3453) {
		standard_mirroring_handler(emu, addr, value, cycles);
	}

	if (addr & 0x01) {
		if (_bank_select < 6) {
			if (_split_chr) {
				value &= 0x3f;
				if (_bank_select > 1)
					value |= 0x40;
			}
			update_chr0_bank(emu->board, _bank_select, value);
		} else {
			update_prg_bank(emu->board, _bank_select - 6 + 1, value);
		}

		if ((type == BOARD_TYPE_NAMCO_3425) && (_bank_select < 2)) {
			int nmt = _bank_select << 1;
			board->nmt_banks[nmt].bank =
			    (value & 0x20) >> 5;
			board->nmt_banks[nmt + 1].bank =
			    (value & 0x20) >> 5;
			board_nmt_sync(board);
		}

	} else {
		_bank_select = value & 0x07;
	}
}

static CPU_READ_HANDLER(vs_super_xevious_security)
{
	struct board *board = emu->board;

	switch (addr) {
	case 0x54ff:
		value = 0x05;
		break;
	case 0x5567:
		value = ((security_counter ^= 1) ? 0x37 : 0x3e);
		break;
	case 0x5678:
		value = security_counter ? 0x00 : 0x01;
		break;
	case 0x578f:
		value = security_counter ? 0xd1 : 0x89;
		break;
	}

	return value;
}

static CPU_READ_HANDLER(vs_tko_boxing_security)
{
	struct board *board = emu->board;

	switch (addr) {
	case 0x5e00:
		value = 0;
		security_counter = 0;
		break;
	case 0x5e01:
		value = vs_tko_boxing_security_data[security_counter];
		security_counter = (security_counter + 1) & 0x1f;
		break;
	}

	return value;
}

static CPU_READ_HANDLER(vs_rbi_baseball_security)
{
	struct board *board = emu->board;

	switch (addr) {
	case 0x5e00:
		value = 0;
		security_counter = 0;
		break;
	case 0x5e01:
		value = (security_counter++ == 0x09) ? 0x6f : 0xb4;
		break;
	}

	return value;
}

