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
#include "patch.h"

#define _upper_address_bits board->data[0]
#define _command_index board->data[1]
#define _command_id board->data[2]
#define _software_id_mode board->data[3]
#define _dirty_flag board->data[4]

static CPU_WRITE_HANDLER(unrom512_write_handler);
static CPU_WRITE_HANDLER(unrom512_flash_write_handler);

static void unrom512_flash_end_frame(struct board *board, uint32_t cycles);

static struct board_funcs unrom512_flash_funcs = {
	.end_frame = unrom512_flash_end_frame,
};

static struct bank unrom512_init_prg[] = {
	{0, 0, SIZE_16K, 0x8000, MAP_PERM_READ, MAP_TYPE_ROM},
	{-1, 0, SIZE_16K, 0xc000, MAP_PERM_READ, MAP_TYPE_ROM},
	{.type = MAP_TYPE_END},
};

static struct board_write_handler unrom512_write_handlers[] = {
	{unrom512_write_handler, 0x8000, SIZE_32K, 0},
	{NULL},
};

static struct board_write_handler unrom512_flash_write_handlers[] = {
	{unrom512_flash_write_handler, 0x8000, SIZE_16K, 0},
	{unrom512_write_handler, 0xc000, SIZE_16K, 0},
	{NULL},
};

struct board_info board_unrom512 = {
	.board_type = BOARD_TYPE_UNROM_512,
	.name = "UNROM-512",
	.init_prg = unrom512_init_prg,
	.init_chr0 = std_chr_8k,
	.write_handlers = unrom512_write_handlers,
	.max_prg_rom_size = SIZE_512K,
	.max_chr_rom_size = SIZE_32K,
	.flags = BOARD_INFO_FLAG_MIRROR_M,
	.mirroring_values = std_mirroring_01,
	.mirroring_shift = 7,
};

struct board_info board_unrom512_flash = {
	.board_type = BOARD_TYPE_UNROM_512_FLASH,
	.name = "UNROM-512-FLASH",
	.funcs = &unrom512_flash_funcs,
	.init_prg = unrom512_init_prg,
	.init_chr0 = std_chr_8k,
	.write_handlers = unrom512_flash_write_handlers,
	.max_prg_rom_size = SIZE_512K,
	.max_chr_rom_size = SIZE_32K,
	.flags = BOARD_INFO_FLAG_MIRROR_M|BOARD_INFO_FLAG_PRG_IPS,
	.mirroring_values = std_mirroring_01,
	.mirroring_shift = 7,
};

static CPU_WRITE_HANDLER(unrom512_write_handler)
{
	struct board *board = emu->board;

	update_prg_bank(emu->board, 0, (value & 0x1f));
	update_chr0_bank(emu->board, 0, (value & 0x60) >> 5);
	standard_mirroring_handler(emu, addr, value, cycles);
	_upper_address_bits = value;
}

static CPU_READ_HANDLER(unrom512_flash_read_handler)
{
	/* Only implements reads while in software ID
	   mode; normal chip reads are handled using
	   standard memory map code.
	*/

	if (!(addr & 1)) {
		/* Return SST manufacturer ID byte */
		return 0xbf;
	} else {
		size_t size = emu->board->prg_rom.size;
		uint8_t id;

		/* Return device ID byte (based on emulated
		   ROM size) */
		if (size == SIZE_512K) {
			id = 0xb7;
		} else if (size == SIZE_256K) {
			id = 0xb6;
		} else if (size == SIZE_128K) {
			id = 0xb5;
		} else {
			/* Unknown */
			id = 0xff;
		}

		return id;
	}
}

static CPU_WRITE_HANDLER(unrom512_flash_write_handler)
{
	struct board *board = emu->board;
	int flash_address, rom_address;

	/*
	  This emulates the Sector Erase, Byte Program and Software ID
	  commands of the SST39SF040 flash chip.  Chip Erase is not
	  implemented.
	*/

	flash_address = (addr & 0x7fff) | ((_upper_address_bits & 0x1f) << 14);
	rom_address = flash_address % board->prg_rom.size;

	switch (_command_index) {
	case 0:
		if ((flash_address == 0x5555) && (value == 0xaa))
			_command_index = 1;
		else if ((value == 0xf0) && _software_id_mode) {
			_software_id_mode = 0;
			cpu_set_read_handler(emu->cpu, 0x8000, SIZE_32K, 0,
					     NULL);
		}
		break;
	case 1:
	case 4:
		if ((flash_address == 0x2aaa) && (value == 0x55))
			_command_index++;
		else
			_command_index = 0;
		break;
	case 2:
		if ((value == 0x80) || (value == 0xa0) || (value == 0x90) ||
		    (value == 0xf0)) {
			_command_id = value;
			_command_index = 3;
			if (_command_id == 0x90) {
				_software_id_mode = 1;
				cpu_set_read_handler(emu->cpu, 0x8000, SIZE_32K, 0,
						     unrom512_flash_read_handler);
			} else if (_command_id == 0xf0) {
				_software_id_mode = 0;
				cpu_set_read_handler(emu->cpu, 0x8000, SIZE_32K, 0,
						     NULL);
			}
		} else {
			_command_index = 0;
		}
		break;
	case 3:
		if (_command_id == 0xa0) {
			_dirty_flag = 1;
			board->prg_rom.data[rom_address] = value;
			/* Mark the whole page as modified */
			add_range(&board->modified_ranges, rom_address & 0x7f000,
				  SIZE_4K);
			_command_index = 0;
		} else if ((_command_id == 0x80) && (value == 0xaa)) {
			_command_index = 4;
		} else {
			_command_index = 0;
		}
		break;
	case 5:
		_dirty_flag = 1;
		memset(&board->prg_rom.data[rom_address & 0x7f000], 0xff,
		       SIZE_4K);
		add_range(&board->modified_ranges, rom_address & 0x7f000,
			  SIZE_4K);
		_command_index = 0;
		break;
	default:
		_command_index = 0;
	}
}

static void unrom512_flash_end_frame(struct board *board, uint32_t cycles)
{
	if (_dirty_flag) {
		board_write_ips_save(board, board->modified_ranges);
		_dirty_flag = 0;
	}
}
