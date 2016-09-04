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
#include "a12_timer.h"
#include "m2_timer.h"
#include "mmc3.h"

#define _first_bank_select board->data[2]
#define _mmc6_compat_hack board->data[3]

static CPU_WRITE_HANDLER(txrom_compat_bank_select);
static CPU_WRITE_HANDLER(txrom_compat_wram_protect);
static CPU_WRITE_HANDLER(waixing_bank_data);
static CPU_WRITE_HANDLER(hosenkan_write_handler);
static CPU_WRITE_HANDLER(bmc_superbig7in1_write_handler);
static CPU_WRITE_HANDLER(bmc_superhik4in1_write_handler);
static CPU_WRITE_HANDLER(bmc_superhik8in1_write_handler);
static CPU_WRITE_HANDLER(multicart_bank_switch);
static CPU_WRITE_HANDLER(zz_bank_switch);
static CPU_WRITE_HANDLER(m15_in_1_write_handler);
static CPU_WRITE_HANDLER(txc_tw_write_handler);
static CPU_WRITE_HANDLER(bmc_marioparty7in1_write_handler);

struct board_funcs mmc3_funcs = {
	.init = mmc3_init,
	.reset = mmc3_reset,
	.cleanup = mmc3_cleanup,
	.end_frame = mmc3_end_frame,
	.load_state = mmc3_load_state,
	.save_state = mmc3_save_state,
};

struct bank mmc3_init_prg[] = {
	{0, 0, SIZE_8K, 0x6000, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{0, 0, SIZE_8K, 0x8000, MAP_PERM_READ, MAP_TYPE_ROM},
	{1, 0, SIZE_8K, 0xa000, MAP_PERM_READ, MAP_TYPE_ROM},
	{-2, 0, SIZE_8K, 0xc000, MAP_PERM_READ, MAP_TYPE_ROM},
	{-1, 0, SIZE_8K, 0xe000, MAP_PERM_READ, MAP_TYPE_ROM},
	/* For Kasing and TXC / TW boards */
	{ 0, 0, 0, 0x8000, MAP_PERM_READ, MAP_TYPE_ROM},
	{.type = MAP_TYPE_END},
};

struct bank mmc3_init_chr0[] = {
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

struct board_write_handler txrom_compat_write_handlers[] = {
	{txrom_compat_bank_select, 0x8000, SIZE_8K, 0x8001},
	{mmc3_bank_data, 0x8001, SIZE_8K, 0x8001},
	{standard_mirroring_handler, 0xa000, SIZE_8K, 0xa001},
	{txrom_compat_wram_protect, 0xa001, SIZE_8K, 0xa001},
	{a12_timer_irq_latch, 0xc000, SIZE_8K, 0xc001},
	{a12_timer_irq_reload, 0xc001, SIZE_8K, 0xc001},
	{a12_timer_irq_disable, 0xe000, SIZE_8K, 0xe001},
	{a12_timer_irq_enable, 0xe001, SIZE_8K, 0xe001},
	{NULL}
};

struct board_write_handler mmc3_write_handlers[] = {
	{mmc3_bank_select, 0x8000, SIZE_8K, 0x8001},
	{mmc3_bank_data, 0x8001, SIZE_8K, 0x8001},
	{standard_mirroring_handler, 0xa000, SIZE_8K, 0xa001},
	{mmc3_wram_protect, 0xa001, SIZE_8K, 0xa001},
	{a12_timer_irq_latch, 0xc000, SIZE_8K, 0xc001},
	{a12_timer_irq_reload, 0xc001, SIZE_8K, 0xc001},
	{a12_timer_irq_disable, 0xe000, SIZE_8K, 0xe001},
	{a12_timer_irq_enable, 0xe001, SIZE_8K, 0xe001},
	{NULL}
};

static struct board_write_handler waixing_write_handlers[] = {
	{mmc3_bank_select, 0x8000, SIZE_8K, 0x8001},
	{waixing_bank_data, 0x8001, SIZE_8K, 0x8001},
	{standard_mirroring_handler, 0xa000, SIZE_8K, 0xa001},
	{mmc3_wram_protect, 0xa001, SIZE_8K, 0xa001},
	{a12_timer_irq_latch, 0xc000, SIZE_8K, 0xc001},
	{a12_timer_irq_reload, 0xc001, SIZE_8K, 0xc001},
	{a12_timer_irq_disable, 0xe000, SIZE_8K, 0xe001},
	{a12_timer_irq_enable, 0xe001, SIZE_8K, 0xe001},
	{NULL}
};

static struct board_write_handler bmc_superbig7in1_write_handlers[] = {
	{mmc3_bank_select, 0x8000, SIZE_8K, 0x8001},
	{mmc3_bank_data, 0x8001, SIZE_8K, 0x8001},
	{standard_mirroring_handler, 0xa000, SIZE_8K, 0xa001},
	{bmc_superbig7in1_write_handler, 0xa001, SIZE_8K, 0xa001},
	{a12_timer_irq_latch, 0xc000, SIZE_8K, 0xc001},
	{a12_timer_irq_reload, 0xc001, SIZE_8K, 0xc001},
	{a12_timer_irq_disable, 0xe000, SIZE_8K, 0xe001},
	{a12_timer_irq_enable, 0xe001, SIZE_8K, 0xe001},
	{NULL}
};

static struct board_write_handler bmc_superhik4in1_write_handlers[] = {
	{bmc_superhik4in1_write_handler, 0x6000, SIZE_8K, 0},
	{mmc3_bank_select, 0x8000, SIZE_8K, 0x8001},
	{mmc3_bank_data, 0x8001, SIZE_8K, 0x8001},
	{standard_mirroring_handler, 0xa000, SIZE_8K, 0xa001},
	{mmc3_wram_protect, 0xa001, SIZE_8K, 0xa001},
	{a12_timer_irq_latch, 0xc000, SIZE_8K, 0xc001},
	{a12_timer_irq_reload, 0xc001, SIZE_8K, 0xc001},
	{a12_timer_irq_disable, 0xe000, SIZE_8K, 0xe001},
	{a12_timer_irq_enable, 0xe001, SIZE_8K, 0xe001},
	{NULL}
};

static struct board_write_handler bmc_superhik8in1_write_handlers[] = {
	{bmc_superhik8in1_write_handler, 0x6000, SIZE_8K, 0},
	{mmc3_bank_select, 0x8000, SIZE_8K, 0x8001},
	{mmc3_bank_data, 0x8001, SIZE_8K, 0x8001},
	{standard_mirroring_handler, 0xa000, SIZE_8K, 0xa001},
	{mmc3_wram_protect, 0xa001, SIZE_8K, 0xa001},
	{a12_timer_irq_latch, 0xc000, SIZE_8K, 0xc001},
	{a12_timer_irq_reload, 0xc001, SIZE_8K, 0xc001},
	{a12_timer_irq_disable, 0xe000, SIZE_8K, 0xe001},
	{a12_timer_irq_enable, 0xe001, SIZE_8K, 0xe001},
	{NULL}
};

static struct board_write_handler hosenkan_write_handlers[] = {
	{standard_mirroring_handler, 0x8001, SIZE_8K, 0x8001},
	{hosenkan_write_handler, 0xa000, SIZE_8K, 0xa001},
	{hosenkan_write_handler, 0xc000, SIZE_8K, 0},
	{a12_timer_irq_disable, 0xe000, SIZE_8K, 0xe001},
	{a12_timer_irq_enable, 0xe001, SIZE_8K, 0xe001},
	{NULL}
};

static struct board_write_handler qj_write_handlers[] = {
	{mmc3_bank_select, 0x8000, SIZE_8K, 0x8001},
	{mmc3_bank_data, 0x8001, SIZE_8K, 0x8001},
	{standard_mirroring_handler, 0xa000, SIZE_8K, 0xa001},
	{mmc3_wram_protect, 0xa001, SIZE_8K, 0xa001},
	{multicart_bank_switch, 0x6000, SIZE_8K, 0},
	{a12_timer_irq_latch, 0xc000, SIZE_8K, 0xc001},
	{a12_timer_irq_reload, 0xc001, SIZE_8K, 0xc001},
	{a12_timer_irq_disable, 0xe000, SIZE_8K, 0xe001},
	{a12_timer_irq_enable, 0xe001, SIZE_8K, 0xe001},
	{NULL},
};

static struct board_write_handler zz_write_handlers[] = {
	{mmc3_bank_select, 0x8000, SIZE_8K, 0x8001},
	{mmc3_bank_data, 0x8001, SIZE_8K, 0x8001},
	{standard_mirroring_handler, 0xa000, SIZE_8K, 0xa001},
	{mmc3_wram_protect, 0xa001, SIZE_8K, 0xa001},
	{zz_bank_switch, 0x6000, SIZE_8K, 0},
	{a12_timer_irq_latch, 0xc000, SIZE_8K, 0xc001},
	{a12_timer_irq_reload, 0xc001, SIZE_8K, 0xc001},
	{a12_timer_irq_disable, 0xe000, SIZE_8K, 0xe001},
	{a12_timer_irq_enable, 0xe001, SIZE_8K, 0xe001},
	{NULL},
};

static struct board_write_handler m15_in_1_write_handlers[] = {
	{mmc3_bank_select, 0x8000, SIZE_8K, 0x8001},
	{mmc3_bank_data, 0x8001, SIZE_8K, 0x8001},
	{standard_mirroring_handler, 0xa000, SIZE_8K, 0xa001},
	{mmc3_wram_protect, 0xa001, SIZE_8K, 0xa001},
	{a12_timer_irq_latch, 0xc000, SIZE_8K, 0xc001},
	{a12_timer_irq_reload, 0xc001, SIZE_8K, 0xc001},
	{a12_timer_irq_disable, 0xe000, SIZE_8K, 0xe001},
	{a12_timer_irq_enable, 0xe001, SIZE_8K, 0xe001},
	{m15_in_1_write_handler, 0x6000, SIZE_8K, 0},
	{NULL}
};

static struct board_write_handler txc_tw_write_handlers[] = {
	{mmc3_bank_select, 0x8000, SIZE_8K, 0x8001},
	{mmc3_bank_data, 0x8001, SIZE_8K, 0x8001},
	{standard_mirroring_handler, 0xa000, SIZE_8K, 0xa001},
	{mmc3_wram_protect, 0xa001, SIZE_8K, 0xa001},
	{a12_timer_irq_latch, 0xc000, SIZE_8K, 0xc001},
	{a12_timer_irq_reload, 0xc001, SIZE_8K, 0xc001},
	{a12_timer_irq_disable, 0xe000, SIZE_8K, 0xe001},
	{a12_timer_irq_enable, 0xe001, SIZE_8K, 0xe001},
	{txc_tw_write_handler, 0x4120, SIZE_16K - 0x0120, 0},
	{NULL}
};

static struct board_write_handler bmc_marioparty7in1_write_handlers[] = {
	{mmc3_bank_select, 0x8000, SIZE_8K, 0x8001},
	{mmc3_bank_data, 0x8001, SIZE_8K, 0x8001},
	{standard_mirroring_handler, 0xa000, SIZE_8K, 0xa001},
	{mmc3_wram_protect, 0xa001, SIZE_8K, 0xa001},
	{a12_timer_irq_latch, 0xc000, SIZE_8K, 0xc001},
	{a12_timer_irq_reload, 0xc001, SIZE_8K, 0xc001},
	{a12_timer_irq_disable, 0xe000, SIZE_8K, 0xe001},
	{a12_timer_irq_enable, 0xe001, SIZE_8K, 0xe001},
	{bmc_marioparty7in1_write_handler, 0x6000, SIZE_8K, 0},
	{NULL}
};

struct board_info board_txrom_compat = {
	.board_type = BOARD_TYPE_TxROM_COMPAT,
	.name = "TxROM/HKROM",
	.mapper_name = "MMC3/MMC6",
	.funcs = &mmc3_funcs,
	.init_prg = mmc3_init_prg,
	.init_chr0 = mmc3_init_chr0,
	.write_handlers = txrom_compat_write_handlers,
	.max_prg_rom_size = SIZE_2048K,
	.max_chr_rom_size = SIZE_256K,
	.max_wram_size = {SIZE_8K, 0},
	.flags = BOARD_INFO_FLAG_MIRROR_M,
	.mirroring_values = std_mirroring_vh,
};

struct board_info board_txrom = {
	.board_type = BOARD_TYPE_TxROM,
	.name = "TxROM",
	.mapper_name = "MMC3",
	.funcs = &mmc3_funcs,
	.init_prg = mmc3_init_prg,
	.init_chr0 = mmc3_init_chr0,
	.write_handlers = mmc3_write_handlers,
	.max_prg_rom_size = SIZE_2048K,
	.max_chr_rom_size = SIZE_256K,
	.max_wram_size = {SIZE_8K, 0},
	.flags = BOARD_INFO_FLAG_MIRROR_M,
	.mirroring_values = std_mirroring_vh,
};

struct board_info board_acclaim_mc_acc = {
	.board_type = BOARD_TYPE_ACCLAIM_MC_ACC,
	.name = "ACCLAIM-MC-ACC",
	.mapper_name = "MC-ACC",
	.funcs = &mmc3_funcs,
	.init_prg = mmc3_init_prg,
	.init_chr0 = mmc3_init_chr0,
	.write_handlers = mmc3_write_handlers,
	.max_prg_rom_size = SIZE_512K,
	.max_chr_rom_size = SIZE_256K,
	.max_wram_size = {SIZE_8K, 0},
	.flags = BOARD_INFO_FLAG_MIRROR_M,
	.mirroring_values = std_mirroring_vh,
};

struct board_info board_txrom_mmc3a = {
	.board_type = BOARD_TYPE_TxROM_MMC3A,
	.name = "TxROM-MMC3A",
	.mapper_name = "MMC3A",
	.funcs = &mmc3_funcs,
	.init_prg = mmc3_init_prg,
	.init_chr0 = mmc3_init_chr0,
	.write_handlers = mmc3_write_handlers,
	.max_prg_rom_size = SIZE_2048K,
	.max_chr_rom_size = SIZE_256K,
	.max_wram_size = {SIZE_8K, 0},
	.flags = BOARD_INFO_FLAG_MIRROR_M,
	.mirroring_values = std_mirroring_vh,
};

struct board_info board_nes_qj = {
	.board_type = BOARD_TYPE_QJ,
	.name = "NES-QJ",
	.mapper_name = "MMC3",
	.funcs = &mmc3_funcs,
	.init_prg = mmc3_init_prg,
	.init_chr0 = mmc3_init_chr0,
	.write_handlers = qj_write_handlers,
	.max_prg_rom_size = SIZE_512K,
	.max_chr_rom_size = SIZE_256K,
	.flags = BOARD_INFO_FLAG_MIRROR_M,
	.mirroring_values = std_mirroring_vh,
};

struct board_info board_pal_zz = {
	.board_type = BOARD_TYPE_ZZ,
	.name = "PAL-ZZ",
	.mapper_name = "MMC3",
	.funcs = &mmc3_funcs,
	.init_prg = mmc3_init_prg,
	.init_chr0 = mmc3_init_chr0,
	.write_handlers = zz_write_handlers,
	.max_prg_rom_size = SIZE_256K,
	.max_chr_rom_size = SIZE_256K,
	.flags = BOARD_INFO_FLAG_MIRROR_M,
	.mirroring_values = std_mirroring_vh,
};

struct board_info board_bmc_15_in_1 = {
	.board_type = BOARD_TYPE_BMC_15_IN_1,
	.name = "BMC-15IN1",
	.mapper_name = "MMC3",
	.funcs = &mmc3_funcs,
	.init_prg = mmc3_init_prg,
	.init_chr0 = mmc3_init_chr0,
	.write_handlers = m15_in_1_write_handlers,
	.max_prg_rom_size = SIZE_2048K,
	.max_chr_rom_size = SIZE_512K,
	.flags = BOARD_INFO_FLAG_MIRROR_M,
	.mirroring_values = std_mirroring_vh,
};

struct board_info board_waixing_type_a = {
	.board_type = BOARD_TYPE_WAIXING_TYPE_A,
	.name = "WAIXING-A",
	.funcs = &mmc3_funcs,
	.init_prg = mmc3_init_prg,
	.init_chr0 = mmc3_init_chr0,
	.write_handlers = waixing_write_handlers,
	.max_prg_rom_size = SIZE_512K,
	.max_chr_rom_size = SIZE_256K,
	.flags = BOARD_INFO_FLAG_MIRROR_M,
	.min_vram_size = { SIZE_2K, 0 },
	.max_vram_size = { SIZE_2K, 0 },
	.max_wram_size = {SIZE_8K, 0},
	.mirroring_values = std_mirroring_vh,
};

struct board_info board_waixing_type_c = {
	.board_type = BOARD_TYPE_WAIXING_TYPE_C,
	.name = "WAIXING-C",
	.funcs = &mmc3_funcs,
	.init_prg = mmc3_init_prg,
	.init_chr0 = mmc3_init_chr0,
	.write_handlers = waixing_write_handlers,
	.max_prg_rom_size = SIZE_512K,
	.max_chr_rom_size = SIZE_256K,
	.flags = BOARD_INFO_FLAG_MIRROR_M,
	.min_vram_size = { SIZE_4K, 0 },
	.max_vram_size = { SIZE_4K, 0 },
	.max_wram_size = {SIZE_8K, 0},
	.mirroring_values = std_mirroring_vh,
};

struct board_info board_kasing = {
	.board_type = BOARD_TYPE_KASING,
	.name = "KASING",
	.funcs = &mmc3_funcs,
	.init_prg = mmc3_init_prg,
	.init_chr0 = mmc3_init_chr0,
	.write_handlers = qj_write_handlers,
	.max_prg_rom_size = SIZE_512K,
	.max_chr_rom_size = SIZE_256K,
	.flags = BOARD_INFO_FLAG_MIRROR_M,
	.mirroring_values = std_mirroring_vh,
};

struct board_info board_waixing_type_h = {
	.board_type = BOARD_TYPE_WAIXING_TYPE_H,
	.name = "WAIXING-H",
	.funcs = &mmc3_funcs,
	.init_prg = mmc3_init_prg,
	.init_chr0 = mmc3_init_chr0,
	.write_handlers = waixing_write_handlers,
	.max_prg_rom_size = SIZE_2048K,
	.max_chr_rom_size = SIZE_256K,
	.max_wram_size = {SIZE_8K, 0},
	.flags = BOARD_INFO_FLAG_MIRROR_M,
	.mirroring_values = std_mirroring_vh,
};

struct board_info board_txc_tw = {
	.board_type = BOARD_TYPE_TXC_TW,
	.name = "TXC-TW",
	.funcs = &mmc3_funcs,
	.init_prg = mmc3_init_prg,
	.init_chr0 = mmc3_init_chr0,
	.write_handlers = txc_tw_write_handlers,
	.max_prg_rom_size = SIZE_2048K,
	.max_chr_rom_size = SIZE_256K,
	.flags = BOARD_INFO_FLAG_MIRROR_M,
	.mirroring_values = std_mirroring_vh,
};

struct board_info board_bmc_marioparty7in1 = {
	.board_type = BOARD_TYPE_BMC_MARIOPARTY_7_IN_1,
	.name = "BMC-MARIOPARTY-7IN1",
	.funcs = &mmc3_funcs,
	.init_prg = mmc3_init_prg,
	.init_chr0 = mmc3_init_chr0,
	.write_handlers = bmc_marioparty7in1_write_handlers,
	.max_prg_rom_size = SIZE_2048K,
	.max_chr_rom_size = SIZE_2048K,
	.flags = BOARD_INFO_FLAG_MIRROR_M,
	.mirroring_values = std_mirroring_vh,
};

struct board_info board_hosenkan_electronics = {
	.board_type = BOARD_TYPE_UNL_HOSENKAN,
	.name = "UNL-HOSENKAN",
	.funcs = &mmc3_funcs,
	.init_prg = mmc3_init_prg,
	.init_chr0 = mmc3_init_chr0,
	.write_handlers = hosenkan_write_handlers,
	.max_prg_rom_size = SIZE_2048K,
	.max_chr_rom_size = SIZE_256K,
	.max_wram_size = {SIZE_8K, 0},
	.flags = BOARD_INFO_FLAG_MIRROR_M,
	.mirroring_values = std_mirroring_vh,
};

struct board_info board_bmc_superbig_7in1 = {
	.board_type = BOARD_TYPE_BMC_SUPERBIG_7_IN_1,
	.name = "BMC-SUPERBIG-7IN1",
	.funcs = &mmc3_funcs,
	.init_prg = mmc3_init_prg,
	.init_chr0 = mmc3_init_chr0,
	.write_handlers = bmc_superbig7in1_write_handlers,
	.max_prg_rom_size = SIZE_1024K,
	.max_chr_rom_size = SIZE_1024K,
	.max_wram_size = {SIZE_8K, 0},
	.flags = BOARD_INFO_FLAG_MIRROR_M,
	.mirroring_values = std_mirroring_vh,
};

struct board_info board_bmc_superhik_4in1 = {
	.board_type = BOARD_TYPE_BMC_SUPERHIK_4_IN_1,
	.name = "BMC-SUPERHIK-4IN1",
	.funcs = &mmc3_funcs,
	.init_prg = mmc3_init_prg,
	.init_chr0 = mmc3_init_chr0,
	.write_handlers = bmc_superhik4in1_write_handlers,
	.max_prg_rom_size = SIZE_512K,
	.max_chr_rom_size = SIZE_512K,
	.flags = BOARD_INFO_FLAG_MIRROR_M,
	.mirroring_values = std_mirroring_vh,
};

struct board_info board_bmc_superhik_8in1 = {
	.board_type = BOARD_TYPE_BMC_SUPERHIK_8_IN_1,
	.name = "BMC-SUPERHIK8IN1",
	.funcs = &mmc3_funcs,
	.init_prg = mmc3_init_prg,
	.init_chr0 = mmc3_init_chr0,
	.write_handlers = bmc_superhik8in1_write_handlers,
	.max_prg_rom_size = SIZE_512K,
	.max_chr_rom_size = SIZE_512K,
	.flags = BOARD_INFO_FLAG_MIRROR_M,
	.mirroring_values = std_mirroring_vh,
};

void mmc3_end_frame(struct board *board, uint32_t cycles)
{
	a12_timer_end_frame(board->emu->a12_timer, cycles);
}

int mmc3_init(struct board *board)
{
	struct emu *emu;
	int variant;

	emu = board->emu;
	/*
	 * Startropics and Startropics II are the only two MMC6 games,
	 * and they both work fine on an MMC3 as long as the PRG RAM
	 * protection bits are ignored (so RAM is always enabled).
	 * Since leaving RAM enabled breaks Low G Man another way to
	 * support them must be used.
	 * 
	 * To detect them, we examine the first write to the bank
	 * select register to see if bit 5 is set (0x20).  Both MMC6
	 * games do this to enable PRG RAM before touching the PRG RAM
	 * protect register.  This bit only does anything on MMC6, so
	 * MMC3 games are unlikely to set it.
	 *
	 * On the first write to the bank select register, if bit 5 is
	 * set then the permissions for the PRG RAM bank are set to
	 * read-write and all subsequent writes to the RAM protection
	 * register are ignored.  Only the first write to the bank
	 * select register matters; subsequent writes are treated
	 * normally.
	 *
	 * This hack is very unlikely to cause any problems so it is
	 * enabled by default.  Reasons:
	 *
	 * - MMC3 games that never set bit 5 of _bank_select get normal
	 *   MMC3 behavior (i.e. working protection register).
	 *   Likewise for those that do set it but not on the first
	 *   write.
	 * - MMC3 games that do set bit 5 of _bank_select on the first
	 *   write will work fine as long as they work using the usual
	 *   "PRG RAM always enabled hack".
	 * - MMC3 games that don't work using the "RAM always enabled"
	 *   hack (such as Low G Man) will work as long as they don't
	 *   set bit 5 of _bank_select on the first write.  Low G Man
	 *   falls into this category.
	 * - Both MMC6 games work using this hack (with one difference:
	 *   their save files will be 8K in size instead of 1K).
	 * - This hack only applies to generic TxROM games, not those
	 *   that use TxSROM, TQROM, HKROM, or any other board
	 *   variant.  These function normally regardless of the first
	 *   write to _bank_select.
	 *
	 * In short, all games that work using the standard "RAM always
	 * enabled" hack should still work, plus Low G Man and any other
	 * games that expect open bus from $6000-$7FFF.
	 */

	_mmc6_compat_hack = 0;
	if (board->info->board_type == BOARD_TYPE_TxROM_COMPAT)
		_mmc6_compat_hack = 1;

	variant = A12_TIMER_VARIANT_MMC3_STD;
	switch (board->info->board_type) {
	case BOARD_TYPE_ACCLAIM_MC_ACC:
		variant = A12_TIMER_VARIANT_ACCLAIM_MC_ACC;
		break;
	case BOARD_TYPE_TxROM_MMC3A:
		variant = A12_TIMER_VARIANT_MMC3_ALT;
		break;
	}

	if (a12_timer_init(emu, variant))
		return 1;

	return 0;
}

void mmc3_cleanup(struct board *board)
{
	a12_timer_cleanup(board->emu);
}

int mmc3_load_state(struct board *board, struct save_state *state)
{
	return a12_timer_load_state(board->emu, state);
}

int mmc3_save_state(struct board *board, struct save_state *state)
{
	return a12_timer_save_state(board->emu, state);
}

void mmc3_reset(struct board *board, int hard)
{
	if (hard) {
		a12_timer_reset(board->emu->a12_timer, hard);
		_first_bank_select = 1;

		board->prg_mode = 0;
		board->chr_mode = 0;

		board->prg_or = 0;
		board->chr_or = 0;

		switch (board->info->board_type) {
		case BOARD_TYPE_BMC_MARIOPARTY_7_IN_1:
			board->prg_and = 0x1f;
			board->chr_and = 0xff;
			board->prg_or = 0x00;
			board->chr_or = 0x000;
			break;
		case BOARD_TYPE_BMC_SUPERHIK_8_IN_1:
			_extra_regs[4] = 0;
			_extra_regs[3] &= 0x3f;
			board->prg_or = 0x00;
			board->chr_or = 0x00;
			/* FIXME these may not be correct */
			board->prg_and = 0xff;
			board->chr_and = 0xff;
			break;
		case BOARD_TYPE_BMC_SUPERHIK_4_IN_1:
			board->prg_and = 0x0f;
			board->chr_and = 0x7f;
			board->prg_or = 0x00;
			board->chr_or = 0x000;
			board->prg_banks[5].size = SIZE_32K;
			board->prg_banks[5].bank = 0;
			break;
		case BOARD_TYPE_BMC_SUPERBIG_7_IN_1:
			board->prg_and = 0x0f;
			board->prg_or = 0x00;
			board->chr_and = 0x7f;
			board->chr_or = 0x000;
			break;
		case BOARD_TYPE_BMC_15_IN_1:
			board->prg_and = 0x1f;
			board->prg_or = 0x00;
			board->chr_and = 0xff;
			board->chr_or = 0x000;
			break;
		case BOARD_TYPE_QJ:
			board->prg_and = 0xf;
			board->chr_and = 0x7f;
			break;
		case BOARD_TYPE_ZZ:
			board->prg_and = 0x07;
			board->chr_and = 0x7f;
			break;
		case BOARD_TYPE_WAIXING_TYPE_H:
			board->prg_and = 0x3f;
			break;
		case BOARD_TYPE_TXC_TW:
			board->prg_banks[5].size = SIZE_32K;
			break;
		default:
			board->prg_and = ~0;
			board->chr_and = ~0;
			break;
		}

		if (emu_system_is_vs(board->emu))
			board->prg_banks[0].perms = MAP_PERM_READWRITE;

		a12_timer_set_counter_enabled(board->emu->a12_timer, 1, 0);

		_bank_select = 0;
	}
}

static CPU_WRITE_HANDLER(zz_bank_switch)
{
	struct board *board = emu->board;
	uint32_t old_prg_or, old_chr_or;
	uint8_t old_prg_and, old_chr_and;

	if ((_wram_protect & 0xc0) != 0x80)
		return;

	old_prg_or = board->prg_or;
	old_chr_or = board->chr_or;
	old_prg_and = board->prg_and;
	old_chr_and = board->chr_and;

	value &= 0x07;

	board->prg_or = (value << 2) & 0x10;
	if ((value & 0x03) == 0x03)
		board->prg_or |= 0x08;
	board->prg_and = ((value & 0x04) << 1 | 0x07);
	board->chr_or = (value << 5) & 0x80;

	if ((old_prg_and != board->prg_and) ||
	    (old_prg_or != board->prg_or))
		board_prg_sync(board);

	if ((old_chr_and != board->chr_and) ||
	    (old_chr_or != board->chr_or))
		board_chr_sync(board, 0);
}

static CPU_WRITE_HANDLER(multicart_bank_switch)
{
	struct board *board = emu->board;
	uint32_t old_prg_or, old_chr_or;
	uint8_t old_prg_and, old_chr_and;

	if (board->info->board_type == BOARD_TYPE_QJ &&
	    ((_wram_protect & 0xc0) != 0x80))
		return;

	old_prg_or = board->prg_or;
	old_chr_or = board->chr_or;
	old_prg_and = board->prg_and;
	old_chr_and = board->chr_and;

	switch (board->info->board_type) {
	case BOARD_TYPE_QJ:
		board->prg_or = (value & 1) << 4;
		board->chr_or = (value & 1) << 7;
		break;
	case BOARD_TYPE_KASING:
		if (addr & 0x01) {
			board->chr_or = (value & 1) << 8;
		} else {
			if (value & 0x80) {
				board->prg_banks[5].size = SIZE_32K;
				board->prg_banks[5].shift = 1;
			} else {
				board->prg_banks[5].size = 0;
			}

			board->prg_banks[5].bank = (value & 0x0f);
			board_prg_sync(board);
		}
		break;
	}

	if ((old_prg_and != board->prg_and) ||
	    (old_prg_or != board->prg_or))
		board_prg_sync(board);

	if ((old_chr_and != board->chr_and) ||
	    (old_chr_or != board->chr_or))
		board_chr_sync(board, 0);
}

void mmc3_txsrom_mirroring(struct board *board)
{
	int i;

	for (i = 0; i < 4; i++) {
		int reg = i;
		if (board->chr_mode)
			reg += 4;
		else
			reg = reg & 0xfe;

		board->nmt_banks[i].type = MAP_TYPE_CIRAM;
		board->nmt_banks[i].bank =
		    board->chr_banks0[reg].
		    bank & 0x80 ? 1 : 0;
		board->nmt_banks[i].perms =
		    MAP_PERM_READWRITE;
	}

	board_nmt_sync(board);
}

static CPU_WRITE_HANDLER(hosenkan_write_handler)
{
	static uint8_t reg_map[8] = { 0, 3, 1, 5, 6, 7, 2, 4 };

	addr &= 0xe001;

	if (addr == 0xa000) {
		mmc3_bank_select(emu, 0x8000, reg_map[value & 0x07], cycles);
	} else if (addr == 0xc000) {
		mmc3_bank_data(emu, 0x8001, value, cycles);
	} else if (addr == 0xc001) {
		a12_timer_irq_latch(emu, addr, value, cycles);
		a12_timer_irq_reload(emu, addr, value, cycles);
	}
}

static CPU_WRITE_HANDLER(bmc_superbig7in1_write_handler)
{
	struct board *board;
	int bank;

	board = emu->board;

	bank = value & 7;

	mmc3_wram_protect(emu, 0xa001, value & 0xc0, cycles);
	if (bank < 6) {
		board->prg_and = 0x0f;
		board->chr_and = 0x7f;
		board->prg_or = (bank << 4);
		board->chr_or = (bank << 7);
	} else {
		board->prg_and = 0x1f;
		board->chr_and = 0xff;
		board->prg_or = 0x60;
		board->chr_or = 0x300;
	}

	board_prg_sync(board);
	board_chr_sync(board, 0);
}

CPU_WRITE_HANDLER(mmc3_bank_select)
{
	struct board *board;
	int old;

	board = emu->board;
	old = _bank_select & 0xc0;
	_bank_select = value;

	board->prg_mode = value & 0x40;
	board->chr_mode = value & 0x80;

	if ((value & 0x80) != (old & 0x80)) {
		if (!(board->chr_mode & 0x80)) {
			board->chr_banks0[0].address &= 0x0fff;
			board->chr_banks0[1].address &= 0x0fff;
			board->chr_banks0[2].address &= 0x0fff;
			board->chr_banks0[3].address &= 0x0fff;
			board->chr_banks0[4].address |= 0x1000;
			board->chr_banks0[5].address |= 0x1000;
			board->chr_banks0[6].address |= 0x1000;
			board->chr_banks0[7].address |= 0x1000;
		} else {
			board->chr_banks0[0].address |= 0x1000;
			board->chr_banks0[1].address |= 0x1000;
			board->chr_banks0[2].address |= 0x1000;
			board->chr_banks0[3].address |= 0x1000;
			board->chr_banks0[4].address &= 0x0fff;
			board->chr_banks0[5].address &= 0x0fff;
			board->chr_banks0[6].address &= 0x0fff;
			board->chr_banks0[7].address &= 0x0fff;
		}

		board_chr_sync(board, 0);
	}

	if ((value & 0x40) != (old & 0x40)) {
		if (!board->prg_mode) {
			board->prg_banks[1].address = 0x8000;
			board->prg_banks[2].address = 0xa000;
			board->prg_banks[3].address = 0xc000;
		} else {
			board->prg_banks[1].address = 0xc000;
			board->prg_banks[2].address = 0xa000;
			board->prg_banks[3].address = 0x8000;
		}

		board_prg_sync(board);
	}
}

static CPU_WRITE_HANDLER(txrom_compat_bank_select)
{
	struct board *board;
	board = emu->board;

	mmc3_bank_select(emu, addr, value, cycles);

	if (_first_bank_select) {
		if (_mmc6_compat_hack) {
			if (value & 0x20) {
				/* FIXME should this be a debugging statement? */
				printf
				    ("MMC6 compatibility hack enabled\n");
				board->prg_banks[0].perms =
				    MAP_PERM_READWRITE;
				board->prg_banks[0].type =
				    MAP_TYPE_AUTO;
				board_prg_sync(board);
			} else {
				_mmc6_compat_hack = 0;
			}
		}
		_first_bank_select = 0;
	}
}

CPU_WRITE_HANDLER(mmc3_bank_data)
{
	struct board *board;
	int bank;

	board = emu->board;

	bank = _bank_select & 0x07;

	switch (bank) {
	case 0:
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
		if (bank < 2) {
			board->chr_banks0[bank * 2].bank = value & 0xfe;
			board->chr_banks0[bank * 2 + 1].bank = value | 0x01;
		} else {
			board->chr_banks0[bank + 2].bank = value;
		}

		board_chr_sync(board, 0);
		break;
	case 6:
	case 7:
		board->prg_banks[bank - 0x05].bank = value;
		board_prg_sync(board);
		break;
	}
}

static CPU_WRITE_HANDLER(waixing_bank_data)
{
	struct board *board;
	int bank;
	int type;

	board = emu->board;
	bank = _bank_select & 0x07;

	if ((board->info->board_type == BOARD_TYPE_WAIXING_TYPE_A) ||
	    (board->info->board_type == BOARD_TYPE_WAIXING_TYPE_C)) {
		int max = 0x0b;

		type = MAP_TYPE_AUTO;

		if (board->vram[0].size == SIZE_2K)
			max = 0x09;

		if ((value >= 0x08) && (value <= max)) {
			value -= 0x08;
			type = MAP_TYPE_RAM0;
		}

		if (bank < 2) {
			board->chr_banks0[bank * 2].type = type;
			board->chr_banks0[bank * 2 + 1].type = type;
		} else {
			board->chr_banks0[bank + 2].type = type;
		}
	} else if ((board->info->board_type == BOARD_TYPE_WAIXING_TYPE_H) &&
		   (bank == 0)) {
		board->prg_or = (value & 0x02) << 5;
	}

	mmc3_bank_data(emu, addr, value, cycles);
}

CPU_WRITE_HANDLER(mmc3_wram_protect)
{
	struct board *board = emu->board;
	int perms;

	if (emu_system_is_vs(board->emu))
		return;

	_wram_protect = value;

	if (!(value & 0x80))
		perms = 0;
	else if (value & 0x40)
		perms = MAP_PERM_READ;
	else
		perms = MAP_PERM_READWRITE;

	board->prg_banks[0].perms = perms;
	board_prg_sync(board);
}

static CPU_WRITE_HANDLER(txrom_compat_wram_protect)
{
	struct board *board = emu->board;

	if (!_first_bank_select && _mmc6_compat_hack)
		return;

	mmc3_wram_protect(emu, addr, value, cycles);
}

static CPU_WRITE_HANDLER(bmc_superhik4in1_write_handler)
{
	struct board *board;
	size_t size;

	board = emu->board;


	if (board->prg_banks[0].perms != MAP_PERM_READWRITE)
		return;

	board->prg_banks[5].bank = (value >> 4) & 0x03;

	if (!(value & 0x01)) {
		size = SIZE_32K;
		board->prg_or = 0;
		board->chr_or = 0;
	} else {
		size = 0;
		board->prg_or = (value & 0xc0) >> 2;
		board->chr_or = (value & 0xc0) << 1;
	}

	board->prg_banks[5].size = size;

	board_prg_sync(board);
	board_chr_sync(board, 0);
}

static CPU_WRITE_HANDLER(bmc_superhik8in1_write_handler)
{
	struct board *board;
	int next_reg;

	board = emu->board;

	if (_extra_regs[3] & 0x40) {
		if (board->prg_banks[0].perms == MAP_PERM_WRITE) {
			if (board->wram[0].size) {
				addr -= 0x6000;
				addr %= board->wram[0].size;
				board->wram[0].data[addr] = value;
			}
		}

		return;
	}

	next_reg = _extra_regs[4];
	_extra_regs[next_reg] = value;

	switch (next_reg) {
	case 1:
		board->prg_or = value;
		board_prg_sync(board);
		break;
	case 3:
		board->prg_and = value ^ 0x3f;
		board_prg_sync(board);
		break;
	}

	next_reg = (next_reg + 1) & 0x03;
	_extra_regs[4] = next_reg;

	board->chr_or = ((_extra_regs[2] << 4) & 0xf00) | _extra_regs[0];

	if (_extra_regs[2] & 0x08)
		board->chr_and = (1 << ((_extra_regs[2] & 0x07) + 1)) - 1;
	else if (_extra_regs[2] & 0x07)
		board->chr_and = 0x00;
	else {
		board->chr_and = 0xff;
	}

	board_chr_sync(board, 0);
}

static CPU_WRITE_HANDLER(bmc_marioparty7in1_write_handler)
{
	struct board *board;

	board = emu->board;


	if (board->prg_banks[0].perms != MAP_PERM_READWRITE)
		return;

	board->prg_or = (value & 0x07) << 4;
	board->prg_and = 0x0f;
	board->chr_or = ((value & 0x30) << 3) | ((value & 0x04) << 7);
	board->chr_and = 0x7f;

	if (!(value & 0x08)) {
		board->prg_and |= 0x10;
		board->prg_or &= 0x60;
	}

	if (!(value & 0x40)) {
		board->chr_and = 0xff;
		board->chr_or &= 0x300;;
	}

	board_prg_sync(board);
	board_chr_sync(board, 0);
}

static CPU_WRITE_HANDLER(txc_tw_write_handler)
{
	struct board *board;

	board = emu->board;

	update_prg_bank(board, 5, (value | (value >> 4)) & 0x0f);
}

static CPU_WRITE_HANDLER(m15_in_1_write_handler)
{
	struct board *board;

	board = emu->board;
	
	switch (value & 0x03) {
	case 0x00:
		board->prg_and = 0x1f;
		board->prg_or = 0x00;
		board->chr_and = 0xff;
		board->chr_or = 0x000;
		break;
	case 0x01:
		board->prg_and = 0x1f;
		board->prg_or = 0x10;
		board->chr_and = 0xff;
		board->chr_or = 0x080;
		break;
	case 0x02:
		board->prg_and = 0x0f;
		board->prg_or = 0x20;
		board->chr_and = 0x7f;
		board->chr_or = 0x100;
		break;
	case 0x03:
		board->prg_and = 0x0f;
		board->prg_or = 0x30;
		board->chr_and = 0x7f;
		board->chr_or = 0x180;
		break;
	}

	board_prg_sync(board);
	board_chr_sync(board, 0);
}
