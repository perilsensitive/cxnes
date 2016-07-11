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
#include "m2_timer.h"

#define _x24c0x_scl          (&board->data[0])
#define _x24c0x_sda          (&board->data[2])
#define _x24c0x_rw           (&board->data[4])
#define _x24c0x_data_latch   (&board->data[6])
#define _x24c0x_addr_latch   (&board->data[8])
#define _x24c0x_bit_count    (&board->data[10])
#define _x24c0x_mode         (&board->data[12])
#define _x24c0x_next_mode    (&board->data[14])
#define _x24c0x_output       (&board->data[16])

enum {
	X24C0x_MODE_DEVICE_SELECT,
	X24C0x_MODE_ADDRESS,
	X24C0x_MODE_DATA,
	X24C0x_MODE_READ,
	X24C0x_MODE_WRITE,
	X24C0x_MODE_ACK,
	X24C0x_MODE_NOT_ACK,
	X24C0x_MODE_ACK_WAIT,
	X24C0x_MODE_IDLE,
};

static void i2c_scl_rise(struct board *board, int chip, int value);
static void i2c_scl_fall(struct board *board, int chip);

static void bandai_reset(struct board *board, int);
static CPU_WRITE_HANDLER(bandai_write_handler);
static CPU_READ_HANDLER(bandai_eeprom_read_handler);

static struct board_funcs bandai_funcs = {
	.reset = bandai_reset,
};

static struct board_read_handler bandai_eeprom_read_handlers[] = {
	{bandai_eeprom_read_handler, 0x6000, SIZE_8K, 0},
	{NULL},
};

static struct board_write_handler bandai_fcg_compat_write_handlers[] = {
	{bandai_write_handler, 0x6000, SIZE_8K, 0},
	{bandai_write_handler, 0x8000, SIZE_32K, 0},
	{NULL},
};


static struct board_write_handler bandai_fcg_write_handlers[] = {
	{bandai_write_handler, 0x6000, SIZE_8K, 0},
	{NULL},
};

static struct board_write_handler bandai_lz93d50_write_handlers[] = {
	{bandai_write_handler, 0x8000, SIZE_32K, 0},
	{NULL},
};

struct board_info board_bandai_fcg = {
	.board_type = BOARD_TYPE_BANDAI_FCG,
	.name = "BANDAI-FCG",
	.funcs = &bandai_funcs,
	.init_prg = std_prg_16k,
	.init_chr0 = std_chr_1k,
	.write_handlers = bandai_fcg_write_handlers,
	.max_prg_rom_size = SIZE_256K,
	.max_chr_rom_size = SIZE_256K,
	.flags = BOARD_INFO_FLAG_MIRROR_M |
	         BOARD_INFO_FLAG_M2_TIMER,
	.mirroring_values = std_mirroring_vh01,
};

struct board_info board_bandai_fcg_compat = {
	.board_type = BOARD_TYPE_BANDAI_FCG_COMPAT,
	.name = "BANDAI-FCG-COMPAT",
	.funcs = &bandai_funcs,
	.init_prg = std_prg_16k,
	.init_chr0 = std_chr_1k,
	.write_handlers = bandai_fcg_compat_write_handlers,
	.read_handlers = bandai_eeprom_read_handlers,
	.max_prg_rom_size = SIZE_256K,
	.max_chr_rom_size = SIZE_256K,
	.max_wram_size = {256, 0},
	.min_wram_size = {256, 0},
	.flags = BOARD_INFO_FLAG_MIRROR_M |
	         BOARD_INFO_FLAG_M2_TIMER,
	.mirroring_values = std_mirroring_vh01,
};

struct board_info board_bandai_lz93d50_24c02 = {
	.board_type = BOARD_TYPE_BANDAI_LZ93D50_24C02,
	.name = "BANDAI-LZ93D50+24C02",
	.funcs = &bandai_funcs,
	.init_prg = std_prg_16k,
	.init_chr0 = std_chr_1k,
	.write_handlers = bandai_lz93d50_write_handlers,
	.read_handlers = bandai_eeprom_read_handlers,
	.max_prg_rom_size = SIZE_512K,
	.max_chr_rom_size = SIZE_256K,
	.max_wram_size = {256, 0},
	.min_wram_size = {256, 0},
	.flags = BOARD_INFO_FLAG_MIRROR_M |
	         BOARD_INFO_FLAG_M2_TIMER,
	.mirroring_values = std_mirroring_vh01,
};

struct board_info board_bandai_lz93d50_24c01 = {
	.board_type = BOARD_TYPE_BANDAI_LZ93D50_24C01,
	.name = "BANDAI-LZ93D50+24C01",
	.funcs = &bandai_funcs,
	.init_prg = std_prg_16k,
	.init_chr0 = std_chr_1k,
	.write_handlers = bandai_lz93d50_write_handlers,
	.read_handlers = bandai_eeprom_read_handlers,
	.max_prg_rom_size = SIZE_512K,
	.max_chr_rom_size = SIZE_256K,
	.max_wram_size = {128, 0},
	.min_wram_size = {128, 0},
	.flags = BOARD_INFO_FLAG_MIRROR_M |
	         BOARD_INFO_FLAG_M2_TIMER,
	.mirroring_values = std_mirroring_vh01,
};

struct board_info board_bandai_lz93d50 = {
	.board_type = BOARD_TYPE_BANDAI_LZ93D50,
	.name = "BANDAI-LZ93D50",
	.funcs = &bandai_funcs,
	.init_prg = std_prg_16k,
	.init_chr0 = std_chr_1k,
	.write_handlers = bandai_lz93d50_write_handlers,
	.read_handlers = bandai_eeprom_read_handlers,
	.max_prg_rom_size = SIZE_512K,
	.max_chr_rom_size = SIZE_256K,
	.flags = BOARD_INFO_FLAG_MIRROR_M |
	         BOARD_INFO_FLAG_M2_TIMER,
	.mirroring_values = std_mirroring_vh01,
};

struct board_info board_bandai_jump2 = {
	.board_type = BOARD_TYPE_BANDAI_JUMP2,
	.name = "BANDAI-JUMP2",
	.funcs = &bandai_funcs,
	.init_prg = std_prg_16k,
	.init_chr0 = std_chr_1k,
	.write_handlers = bandai_lz93d50_write_handlers,
	.max_prg_rom_size = SIZE_512K,
	.max_chr_rom_size = SIZE_256K,
	.max_wram_size = {SIZE_8K, 0},
	.flags = BOARD_INFO_FLAG_MIRROR_M |
	         BOARD_INFO_FLAG_M2_TIMER,
	.mirroring_values = std_mirroring_vh01,
};

static void i2c_scl_start(struct board *board, int chip)
{
	if (chip < 0 || chip > 1)
		return;

	_x24c0x_mode[chip] = X24C0x_MODE_DEVICE_SELECT;
	_x24c0x_bit_count[chip] = 0;
	_x24c0x_output[chip] = 0x10;

	if (board->wram[chip].size == 128)
		_x24c0x_addr_latch[chip] = 0;
}

static void i2c_scl_stop(struct board *board, int chip)
{
	if (chip < 0 || chip > 1)
		return;

	_x24c0x_mode[chip] = X24C0x_MODE_IDLE;
	_x24c0x_output[chip] = 0x10;
}

static void i2c_scl_rise(struct board *board, int chip, int value)
{
	int size;
	uint8_t *mem;
	int shift;

	if (chip < 0 || chip > 1)
		return;

	size = board->wram[chip].size;
	mem = board->wram[chip].data;

	if (size == 256)
		shift = 7 - _x24c0x_bit_count[chip];
	else
		shift = _x24c0x_bit_count[chip];

	value &= 0x01;

	switch(_x24c0x_mode[chip]) {
	case X24C0x_MODE_DEVICE_SELECT:
		if (_x24c0x_bit_count[chip] < 8) {
			_x24c0x_data_latch[chip] &= ~(1 << shift);
			_x24c0x_data_latch[chip] |= value << shift;
			_x24c0x_bit_count[chip]++;
		}
		break;
	case X24C0x_MODE_ACK:
		_x24c0x_output[chip] = 0x00;
		break;
	case X24C0x_MODE_NOT_ACK:
		_x24c0x_output[chip] = 0x10;
		break;
	case X24C0x_MODE_ACK_WAIT:
		if (!value) {
			if (size >= 256) {
				_x24c0x_next_mode[chip] = X24C0x_MODE_READ;
				_x24c0x_data_latch[chip] = mem[_x24c0x_addr_latch[chip]];
			} else {
				_x24c0x_next_mode[chip] = X24C0x_MODE_IDLE;
			}
		}
		break;
	case X24C0x_MODE_READ:
		if (_x24c0x_bit_count[chip] < 8) {
			_x24c0x_output[chip] = _x24c0x_data_latch[chip] &
						  (1 << shift);
			_x24c0x_output[chip] = (_x24c0x_output[chip] ? 0x10 : 0x00);
			_x24c0x_bit_count[chip]++;
		}
		break;
	case X24C0x_MODE_WRITE:
		if (_x24c0x_bit_count[chip] < 8) {
			_x24c0x_data_latch[chip] &= ~(1 << shift);
			_x24c0x_data_latch[chip] |= value << shift;
			_x24c0x_bit_count[chip]++;
		}
		break;
	case X24C0x_MODE_ADDRESS:
		if (_x24c0x_bit_count[chip] < 8) {
			_x24c0x_addr_latch[chip] &= ~(1 << shift);
			_x24c0x_addr_latch[chip] |= value << shift;
			_x24c0x_bit_count[chip]++;
		}
		break;
	}
}

static void i2c_scl_fall(struct board *board, int chip)
{
	int size;
	uint8_t *mem;

	if (chip < 0 || chip > 1)
		return;

	size = board->wram[chip].size;
	mem = board->wram[chip].data;

	switch(_x24c0x_mode[chip]) {
	case X24C0x_MODE_DEVICE_SELECT:
		if (_x24c0x_bit_count[chip] == 8) {
			if ((size >= 256) && ((_x24c0x_data_latch[chip] & 0xa0)
			                      != 0xa0)) {
				_x24c0x_mode[chip] = X24C0x_MODE_NOT_ACK;
				_x24c0x_next_mode[chip] = X24C0x_MODE_IDLE;
				_x24c0x_output[chip] = 0x10;
				return;
			}

			if (size == 128) {
				_x24c0x_rw[chip] = _x24c0x_data_latch[chip] & 0x80;
				_x24c0x_data_latch[chip] &= 0x7f;
				_x24c0x_addr_latch[chip] = _x24c0x_data_latch[chip];
			} else {
				_x24c0x_rw[chip] = _x24c0x_data_latch[chip] & 0x01;
			}

			if (_x24c0x_rw[chip]) {
				_x24c0x_next_mode[chip] = X24C0x_MODE_READ;
				_x24c0x_data_latch[chip] = mem[_x24c0x_addr_latch[chip]];
			} else if (size == 256) {
				_x24c0x_next_mode[chip] = X24C0x_MODE_ADDRESS;
			} else {
				_x24c0x_next_mode[chip] = X24C0x_MODE_WRITE;
			}

			_x24c0x_mode[chip] = X24C0x_MODE_ACK;
			_x24c0x_output[chip] = 0x10;
		}
		break;
	case X24C0x_MODE_ACK:
		_x24c0x_mode[chip] = _x24c0x_next_mode[chip];
		_x24c0x_bit_count[chip] = 0;
		_x24c0x_output[chip] = 0x10;
		break;
	case X24C0x_MODE_NOT_ACK:
		_x24c0x_mode[chip] = X24C0x_MODE_IDLE;
		_x24c0x_bit_count[chip] = 0;
		_x24c0x_output[chip] = 0x10;
		break;
	case X24C0x_MODE_ACK_WAIT:
		if (size == 256) {
			_x24c0x_mode[chip] = _x24c0x_next_mode[chip];
			_x24c0x_bit_count[chip] = 0;
			_x24c0x_output[chip] = 0x10;
		}
		break;
	case X24C0x_MODE_READ:
		if (_x24c0x_bit_count[chip] == 8) {
			_x24c0x_mode[chip] = X24C0x_MODE_ACK_WAIT;
			_x24c0x_addr_latch[chip]++;
			_x24c0x_addr_latch[chip] %= size;
		}
		break;
	case X24C0x_MODE_WRITE:
		if (_x24c0x_bit_count[chip] == 8) {
			_x24c0x_mode[chip] = X24C0x_MODE_ACK;
			if (size == 256) {
				_x24c0x_bit_count[chip] = 0;
				_x24c0x_next_mode[chip] = X24C0x_MODE_WRITE;
			} else {
				_x24c0x_next_mode[chip] = X24C0x_MODE_IDLE;
			}
			mem[_x24c0x_addr_latch[chip]] = _x24c0x_data_latch[chip];
			_x24c0x_addr_latch[chip]++;
			_x24c0x_addr_latch[chip] %= size;
		}
		break;
	case X24C0x_MODE_ADDRESS:
		if (_x24c0x_bit_count[chip] == 8) {
			_x24c0x_bit_count[chip] = 0;
			_x24c0x_mode[chip] = X24C0x_MODE_ACK;
			_x24c0x_next_mode[chip] = (_x24c0x_rw[chip]) ?
			     X24C0x_MODE_IDLE : X24C0x_MODE_WRITE;
			_x24c0x_output[chip] = 0x10;
		}
		break;
	}
}

static void i2c_update_scl_sda(struct board *board, int chip, int scl, int sda)
{
	scl &= 0x01;
	sda &= 0x01;

	if ((board->wram[chip].size != 128) && (board->wram[chip].size != 256))
		return;


	if (_x24c0x_scl[chip] && (sda < _x24c0x_sda[chip]))
		i2c_scl_start(board, chip);
	else if (_x24c0x_scl[chip] && (sda > _x24c0x_sda[chip]))
		i2c_scl_stop(board, chip);
	else if (scl > _x24c0x_scl[chip])
		i2c_scl_rise(board, chip, sda);
	else if (scl < _x24c0x_scl[chip])
		i2c_scl_fall(board, chip);

	_x24c0x_scl[chip] = scl;
	_x24c0x_sda[chip] = sda;
}

static void i2c_update_scl(struct board *board, int chip, int scl)
{
	i2c_update_scl_sda(board, chip, scl, _x24c0x_sda[chip]);
}

static void i2c_update_sda(struct board *board, int chip, int sda)
{
	i2c_update_scl_sda(board, chip, _x24c0x_scl[chip], sda);
}

static CPU_READ_HANDLER(bandai_eeprom_read_handler)
{
	struct board *board;
	uint8_t eeprom_output;
	int size0;
	int size1;
	int chips_present;

	board = emu->board;

	size0 = board->wram[0].size;
	size1 = board->wram[1].size;
	chips_present = 0;

	eeprom_output = 0;

	if ((size0 == 128) || (size0 == 256)) {
		chips_present = 1;
		eeprom_output |= _x24c0x_output[0];
	}

	if ((size1 == 128) || (size1 == 256)) {
		chips_present = 1;
		eeprom_output |= _x24c0x_output[1];
	}

	if (chips_present) {
		value &= ~0x10;
		value |= eeprom_output;
	}

	return value;
}

static CPU_WRITE_HANDLER(bandai_write_handler)
{
	struct board *board = emu->board;

	addr &= 0x0f;

	switch (addr) {
	case 0x00:
	case 0x01:
	case 0x02:
	case 0x03:
		if (board->info->board_type == BOARD_TYPE_BANDAI_JUMP2) {
			value = value ? 0x10 : 0;
			if (value != board->prg_or) {
				board->prg_or = value;
				board_prg_sync(board);
			}
			break;
		}
		i2c_update_scl(board, 1, (value & 0x08) >> 3);
	case 0x04:
	case 0x05:
	case 0x06:
	case 0x07:
		if (board->info->board_type != BOARD_TYPE_BANDAI_JUMP2) {
			update_chr0_bank(board, addr, value);
		}
		break;
	case 0x08:
		update_prg_bank(board, 1, value);
		break;
	case 0x09:
		standard_mirroring_handler(emu, addr, value, cycles);
		break;
	case 0x0a:
		m2_timer_force_reload(emu->m2_timer, cycles);
		m2_timer_set_irq_enabled(emu->m2_timer, value & 1, cycles);
		break;
	case 0x0b:
		m2_timer_set_reload_lo(emu->m2_timer, value, cycles);
		break;
	case 0x0c:
		m2_timer_set_reload_hi(emu->m2_timer, value, cycles);
		break;
	case 0x0d:
		/* FIXME 0x80 is read-protect? */
		i2c_update_scl_sda(board, 0, (value & 0x20) >> 5,
		                   (value & 0x40) >> 6);
		i2c_update_sda(board, 1, (value & 0x40) >> 6);
		break;
	}
}

static void bandai_reset(struct board *board, int hard)
{
	if (hard) {
		int i;
		m2_timer_set_irq_enabled(board->emu->m2_timer, 0, 0);
		if (board->info->board_type == BOARD_TYPE_BANDAI_JUMP2) {
			board->prg_and = 0x0f;
			board->prg_or = 0x00;
			for (i = 0; i < 8; i++)
				board->chr_banks0[i].bank = i;
		}
	}

	m2_timer_set_flags(board->emu->m2_timer, M2_TIMER_FLAG_RELOAD, 0);

	_x24c0x_scl[0] = 0;
	_x24c0x_scl[1] = 0;
	_x24c0x_sda[0] = 0;
	_x24c0x_sda[1] = 0;
	_x24c0x_mode[0] = X24C0x_MODE_IDLE;
	_x24c0x_mode[1] = X24C0x_MODE_IDLE;
	_x24c0x_bit_count[0] = 0;
	_x24c0x_bit_count[1] = 0;
	_x24c0x_addr_latch[0] = 0;
	_x24c0x_addr_latch[1] = 0;
	_x24c0x_data_latch[0] = 0;
	_x24c0x_data_latch[1] = 0;
	_x24c0x_rw[0] = 0;
	_x24c0x_rw[1] = 0;
	_x24c0x_output[0] = 0x10;
	_x24c0x_output[1] = 0x10;
}
