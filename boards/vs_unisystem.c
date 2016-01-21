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

static struct io_device vs_unisystem_bankswitch;

static CPU_READ_HANDLER(vs_pinball_read_handler)
{
	if (cpu_is_opcode_fetch(emu->cpu)) {
		/* Unofficial NOP that uses absolute address mode */
		value = 0x0c;
	}

	return value;
}

static int vs_unisystem_init(struct board *board)
{
	struct io_device *device;
	int addr;

	device = io_register_device(board->emu->io, &vs_unisystem_bankswitch, -1);
	io_connect_device(device);

	/* Vs. Pinball (both known versions) have a bug that prevents
	   the player from entering his or her initials in some cases.
	   Patch out a controller read routine call that occurs before
	   NMI on the name entry screen to work around this.

	   Other emulators don't seem to have this problem because
	   they emulate the PPU frame starting at VBlank (instead of
	   the top of the frame, which is where the PPU starts when
	   reset), so the controller read call during NMI is the first
	   one to see the updated input state.

	   This bug is harder to hit on real hardware since the
	   pre-NMI read that causes the problem happens early in the
	   frame, around scanline 6 or 7.  This leaves at least 254
	   scanlines where the player's button press can happen and
	   still be read correctly.

	   It's an ugly hack, but other solutions either require a lot
	   more code or add input latency.
	*/

	addr = 0;
	if (board->info->board_type == BOARD_TYPE_VS_PINBALL)
		addr = 0x8e1b;
	else if (board->info->board_type == BOARD_TYPE_VS_PINBALLJ)
		addr = 0x8e04;

	if (addr) {
		cpu_set_read_handler(board->emu->cpu, addr, 1, 0,
				     vs_pinball_read_handler);
	}

	return 0;
}

static void vs_unisystem_reset(struct board *board, int hard)
{
	if (hard) {
		int banks = board->prg_rom.size / SIZE_8K;
		int i;

		board->prg_banks[1].perms = MAP_PERM_NONE;
		board->prg_banks[2].perms = MAP_PERM_NONE;
		board->prg_banks[3].perms = MAP_PERM_NONE;
		board->prg_banks[4].perms = MAP_PERM_NONE;

		if (banks > 4)
			banks = 4;

		for (i = 4; i >= 1; i--) {
			if (banks > 0) {
				board->prg_banks[i].bank = banks - 1;
				board->prg_banks[i].perms = MAP_PERM_READ;
				banks--;
			}
		}
	}
}

static void vs_unisystem_bankswitch_write(struct io_device *dev, uint8_t value,
					  int mode, uint32_t cycles)
{
	struct board *board = dev->emu->board;

	board->chr_banks0[0].bank = value >> 2;
	board_chr_sync(board, 0);
	if (board->prg_rom.size > SIZE_32K) {
		/* VS. Gumshoe has an extra 8K bank of PRG ROM it swaps at
		   the same time as the CHR ROM. */
		board->prg_banks[1].bank = value & 0x04;
		board_prg_sync(board);
	}
}

static struct io_device vs_unisystem_bankswitch = {
	.name = "VS. Unisystem Bankswitch",
	.id = IO_DEVICE_VS_UNISYSTEM_BANKSWITCH,
	.config_id = "vsssystem_bankswitch",
	.write = vs_unisystem_bankswitch_write,
	.removable = 0,
	.port = PORT_EXP,
};

static struct board_funcs vs_unisystem_funcs = {
	.init = vs_unisystem_init,
	.reset = vs_unisystem_reset,
};

struct board_info board_vs_unisystem = {
	.board_type = BOARD_TYPE_VS_UNISYSTEM,
	.name = "VS-UNISYSTEM",
	.funcs = &vs_unisystem_funcs,
	.init_prg = std_prg_8k,
	.init_chr0 = std_chr_8k,
	.max_prg_rom_size = SIZE_32K + SIZE_8K,
	.max_chr_rom_size = SIZE_16K,
	.min_wram_size = {SIZE_2K, 0},
	.max_wram_size = {SIZE_2K, 0},
};

struct board_info board_vs_pinball = {
	.board_type = BOARD_TYPE_VS_PINBALL,
	.name = "VS-UNISYSTEM-PINBALL",
	.funcs = &vs_unisystem_funcs,
	.init_prg = std_prg_8k,
	.init_chr0 = std_chr_8k,
	.max_prg_rom_size = SIZE_32K,
	.max_chr_rom_size = SIZE_16K,
	.min_wram_size = {SIZE_2K, 0},
	.max_wram_size = {SIZE_2K, 0},
};

struct board_info board_vs_pinballj = {
	.board_type = BOARD_TYPE_VS_PINBALLJ,
	.name = "VS-UNISYSTEM-PINBALL-J",
	.funcs = &vs_unisystem_funcs,
	.init_prg = std_prg_8k,
	.init_chr0 = std_chr_8k,
	.max_prg_rom_size = SIZE_32K,
	.max_chr_rom_size = SIZE_16K,
	.min_wram_size = {SIZE_2K, 0},
	.max_wram_size = {SIZE_2K, 0},
};
