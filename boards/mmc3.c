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
#include "m2_timer.h"

#define _bank_select board->data[0]
#define _wram_protect board->data[1]
#define _mmc6_compat_hack board->data[3]
#define _first_bank_select board->data[4]
#define _bank_select_mask board->data[5]
#define _chr_mode_mask board->data[6]
#define _has_alt_mirroring board->data[7]
#define _cpu_cycle_irq_enable board->data[8]

static CPU_WRITE_HANDLER(mmc3_write_handler);
static CPU_WRITE_HANDLER(hosenkan_write_handler);
static CPU_WRITE_HANDLER(superbig7in1_write_handler);
static CPU_WRITE_HANDLER(superhik4in1_write_handler);
static CPU_READ_HANDLER(mmc6_wram_read_handler);
static CPU_WRITE_HANDLER(mmc6_wram_write_handler);
static CPU_WRITE_HANDLER(multicart_bank_switch);
static CPU_WRITE_HANDLER(m15_in_1_write_handler);
static CPU_WRITE_HANDLER(txc_tw_write_handler);
static CPU_WRITE_HANDLER(rambo1_irq_latch);
static CPU_WRITE_HANDLER(rambo1_irq_reload);
static CPU_WRITE_HANDLER(rambo1_irq_disable);
static CPU_WRITE_HANDLER(rambo1_irq_enable);
static int mmc3_init(struct board *board);
static void mmc3_cleanup(struct board *board);
static void mmc3_reset(struct board *board, int);
static void mmc3_end_frame(struct board *board, uint32_t cycles);
static void mmc3_update_prg(struct board *board);
static void mmc3_update_chr(struct board *board);
static int mmc3_save_state(struct board *board, struct save_state *state);
static int mmc3_load_state(struct board *board, struct save_state *state);

static struct board_funcs mmc3_funcs = {
	.init = mmc3_init,
	.reset = mmc3_reset,
	.cleanup = mmc3_cleanup,
	.end_frame = mmc3_end_frame,
	.load_state = mmc3_load_state,
	.save_state = mmc3_save_state,
};

static struct board_funcs rambo1_funcs = {
	.init = mmc3_init,
	.reset = mmc3_reset,
	.cleanup = mmc3_cleanup,
	.end_frame = mmc3_end_frame,
	.load_state = mmc3_load_state,
	.save_state = mmc3_save_state,
};

static struct bank mmc3_init_prg[] = {
	{0, 0, SIZE_8K, 0x6000, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{0, 0, SIZE_8K, 0x8000, MAP_PERM_READ, MAP_TYPE_ROM},
	{1, 0, SIZE_8K, 0xa000, MAP_PERM_READ, MAP_TYPE_ROM},
	{-2, 0, SIZE_8K, 0xc000, MAP_PERM_READ, MAP_TYPE_ROM},
	{-1, 0, SIZE_8K, 0xe000, MAP_PERM_READ, MAP_TYPE_ROM},
	/* For Kasing and TXC / TW boards */
	{ 0, 0, 0, 0x8000, MAP_PERM_READ, MAP_TYPE_ROM},
	{.type = MAP_TYPE_END},
};

static struct bank rambo1_init_chr0[] = {
	{0, 1, SIZE_2K, 0x0000, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{2, 1, SIZE_2K, 0x0800, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{4, 0, SIZE_1K, 0x1000, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{5, 0, SIZE_1K, 0x1400, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{6, 0, SIZE_1K, 0x1800, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{7, 0, SIZE_1K, 0x1c00, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{0, 0, 0, 0x0400, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{0, 0, 0, 0x0c00, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{.type = MAP_TYPE_END},
};

static struct board_write_handler mmc3_write_handlers[] = {
	{mmc3_write_handler, 0x8000, SIZE_8K, 0},
	{standard_mirroring_handler, 0xa000, SIZE_8K, 0xa001},
	{mmc3_write_handler, 0xa001, SIZE_8K, 0xa001},
	{a12_timer_irq_latch, 0xc000, SIZE_8K, 0xc001},
	{a12_timer_irq_reload, 0xc001, SIZE_8K, 0xc001},
	{a12_timer_irq_disable, 0xe000, SIZE_8K, 0xe001},
	{a12_timer_irq_enable, 0xe001, SIZE_8K, 0xe001},
	{NULL}
};

static struct board_write_handler superbig7in1_write_handlers[] = {
	{mmc3_write_handler, 0x8000, SIZE_8K, 0},
	{standard_mirroring_handler, 0xa000, SIZE_8K, 0xa001},
	{superbig7in1_write_handler, 0xa001, SIZE_8K, 0xa001},
	{a12_timer_irq_latch, 0xc000, SIZE_8K, 0xc001},
	{a12_timer_irq_reload, 0xc001, SIZE_8K, 0xc001},
	{a12_timer_irq_disable, 0xe000, SIZE_8K, 0xe001},
	{a12_timer_irq_enable, 0xe001, SIZE_8K, 0xe001},
	{NULL}
};

static struct board_write_handler superhik4in1_write_handlers[] = {
	{superhik4in1_write_handler, 0x6000, SIZE_8K, 0},
	{mmc3_write_handler, 0x8000, SIZE_8K, 0},
	{standard_mirroring_handler, 0xa000, SIZE_8K, 0xa001},
	{mmc3_write_handler, 0xa001, SIZE_8K, 0xa001},
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

static struct board_write_handler rambo1_write_handlers[] = {
	{mmc3_write_handler, 0x8000, SIZE_8K, 0},
	{standard_mirroring_handler, 0xa000, SIZE_8K, 0xa001},
	{mmc3_write_handler, 0xa001, SIZE_8K, 0xa001},
	{rambo1_irq_latch, 0xc000, SIZE_8K, 0xc001},
	{rambo1_irq_reload, 0xc001, SIZE_8K, 0xc001},
	{rambo1_irq_disable, 0xe000, SIZE_8K, 0xe001},
	{rambo1_irq_enable, 0xe001, SIZE_8K, 0xe001},
	{NULL}
};

static struct board_write_handler txsrom_write_handlers[] = {
	{mmc3_write_handler, 0x8000, SIZE_8K, 0},
	{mmc3_write_handler, 0xa000, SIZE_8K, 0xa001},
	{a12_timer_irq_latch, 0xc000, SIZE_8K, 0xc001},
	{a12_timer_irq_reload, 0xc001, SIZE_8K, 0xc001},
	{a12_timer_irq_disable, 0xe000, SIZE_8K, 0xe001},
	{a12_timer_irq_enable, 0xe001, SIZE_8K, 0xe001},
	{NULL},
};

static struct board_write_handler tengen800037_write_handlers[] = {
	{mmc3_write_handler, 0x8000, SIZE_8K, 0},
	{standard_mirroring_handler, 0xa000, SIZE_8K, 0xa001},
	{mmc3_write_handler, 0xa001, SIZE_8K, 0xa001},
	{rambo1_irq_latch, 0xc000, SIZE_8K, 0xc001},
	{rambo1_irq_reload, 0xc001, SIZE_8K, 0xc001},
	{rambo1_irq_disable, 0xe000, SIZE_8K, 0xe001},
	{rambo1_irq_enable, 0xe001, SIZE_8K, 0xe001},
	{NULL},
};

static struct board_write_handler qj_zz_write_handlers[] = {
	{mmc3_write_handler, 0x8000, SIZE_8K, 0},
	{standard_mirroring_handler, 0xa000, SIZE_8K, 0xa001},
	{mmc3_write_handler, 0xa001, SIZE_8K, 0xa001},
	{multicart_bank_switch, 0x6000, SIZE_8K, 0},
	{a12_timer_irq_latch, 0xc000, SIZE_8K, 0xc001},
	{a12_timer_irq_reload, 0xc001, SIZE_8K, 0xc001},
	{a12_timer_irq_disable, 0xe000, SIZE_8K, 0xe001},
	{a12_timer_irq_enable, 0xe001, SIZE_8K, 0xe001},
	{NULL},
};

static struct board_write_handler hkrom_write_handlers[] = {
	{mmc3_write_handler, 0x8000, SIZE_8K, 0},
	{mmc6_wram_write_handler, 0x7000, SIZE_4K, 0},
	{standard_mirroring_handler, 0xa000, SIZE_8K, 0xa001},
	{mmc3_write_handler, 0xa001, SIZE_8K, 0xa001},
	{a12_timer_irq_latch, 0xc000, SIZE_8K, 0xc001},
	{a12_timer_irq_reload, 0xc001, SIZE_8K, 0xc001},
	{a12_timer_irq_disable, 0xe000, SIZE_8K, 0xe001},
	{a12_timer_irq_enable, 0xe001, SIZE_8K, 0xe001},
	{NULL},
};

static struct board_read_handler hkrom_read_handlers[] = {
	{mmc6_wram_read_handler, 0x7000, SIZE_4K, 0},
	{NULL},
};

static struct board_write_handler m15_in_1_write_handlers[] = {
	{mmc3_write_handler, 0x8000, SIZE_8K, 0},
	{standard_mirroring_handler, 0xa000, SIZE_8K, 0xa001},
	{mmc3_write_handler, 0xa001, SIZE_8K, 0xa001},
	{a12_timer_irq_latch, 0xc000, SIZE_8K, 0xc001},
	{a12_timer_irq_reload, 0xc001, SIZE_8K, 0xc001},
	{a12_timer_irq_disable, 0xe000, SIZE_8K, 0xe001},
	{a12_timer_irq_enable, 0xe001, SIZE_8K, 0xe001},
	{m15_in_1_write_handler, 0x6000, SIZE_8K, 0},
	{NULL}
};

static struct board_write_handler txc_tw_write_handlers[] = {
	{mmc3_write_handler, 0x8000, SIZE_8K, 0},
	{standard_mirroring_handler, 0xa000, SIZE_8K, 0xa001},
	{mmc3_write_handler, 0xa001, SIZE_8K, 0xa001},
	{a12_timer_irq_latch, 0xc000, SIZE_8K, 0xc001},
	{a12_timer_irq_reload, 0xc001, SIZE_8K, 0xc001},
	{a12_timer_irq_disable, 0xe000, SIZE_8K, 0xe001},
	{a12_timer_irq_enable, 0xe001, SIZE_8K, 0xe001},
	{txc_tw_write_handler, 0x4120, SIZE_16K - 0x0120, 0},
	{NULL}
};

struct board_info board_txrom = {
	.board_type = BOARD_TYPE_TxROM,
	.name = "TxROM",
	.mapper_name = "MMC3",
	.funcs = &mmc3_funcs,
	.init_prg = mmc3_init_prg,
	.init_chr0 = std_chr_2k_1k,
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
	.init_chr0 = std_chr_2k_1k,
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
	.init_chr0 = std_chr_2k_1k,
	.write_handlers = mmc3_write_handlers,
	.max_prg_rom_size = SIZE_2048K,
	.max_chr_rom_size = SIZE_256K,
	.max_wram_size = {SIZE_8K, 0},
	.flags = BOARD_INFO_FLAG_MIRROR_M,
	.mirroring_values = std_mirroring_vh,
};

struct board_info board_txsrom = {
	.board_type = BOARD_TYPE_TxSROM,
	.name = "TxSROM",
	.mapper_name = "MMC3",
	.funcs = &mmc3_funcs,
	.init_prg = mmc3_init_prg,
	.init_chr0 = std_chr_2k_1k,
	.write_handlers = txsrom_write_handlers,
	.max_prg_rom_size = SIZE_2048K,
	.max_chr_rom_size = SIZE_256K,
	.max_wram_size = {SIZE_8K, 0},
	.flags = BOARD_INFO_FLAG_MIRROR_M,
	.mirroring_values = std_mirroring_vh,
};

struct board_info board_tqrom = {
	.board_type = BOARD_TYPE_TQROM,
	.name = "NES-TQROM",
	.mapper_name = "MMC3",
	.funcs = &mmc3_funcs,
	.init_prg = mmc3_init_prg,
	.init_chr0 = std_chr_2k_1k,
	.write_handlers = mmc3_write_handlers,
	.max_prg_rom_size = SIZE_2048K,
	.max_chr_rom_size = SIZE_64K,
	.max_wram_size = {SIZE_8K, 0},
	.min_vram_size = {SIZE_8K, 0},
	.max_vram_size = {SIZE_64K, 0},
	.flags = BOARD_INFO_FLAG_MIRROR_M,
	.mirroring_values = std_mirroring_vh,
};

struct board_info board_hkrom = {
	.board_type = BOARD_TYPE_HKROM,
	.name = "NES-HKROM",
	.mapper_name = "MMC6",
	.funcs = &mmc3_funcs,
	.init_prg = mmc3_init_prg,
	.init_chr0 = std_chr_2k_1k,
	.read_handlers = hkrom_read_handlers,
	.write_handlers = hkrom_write_handlers,
	.max_prg_rom_size = SIZE_512K,
	.max_chr_rom_size = SIZE_256K,
	.min_wram_size = {0, 0},
	.max_wram_size = {0, 0},
	.flags = BOARD_INFO_FLAG_MIRROR_M | BOARD_INFO_FLAG_MAPPER_NV,
	.mapper_ram_size = SIZE_1K,
	.mirroring_values = std_mirroring_vh,
};

struct board_info board_nes_qj = {
	.board_type = BOARD_TYPE_QJ,
	.name = "NES-QJ",
	.mapper_name = "MMC3",
	.funcs = &mmc3_funcs,
	.init_prg = mmc3_init_prg,
	.init_chr0 = std_chr_2k_1k,
	.write_handlers = qj_zz_write_handlers,
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
	.init_chr0 = std_chr_2k_1k,
	.write_handlers = qj_zz_write_handlers,
	.max_prg_rom_size = SIZE_256K,
	.max_chr_rom_size = SIZE_256K,
	.flags = BOARD_INFO_FLAG_MIRROR_M,
	.mirroring_values = std_mirroring_vh,
};

struct board_info board_tengen800032 = {
	.board_type = BOARD_TYPE_TENGEN_800032,
	.name = "TENGEN-800032",
	.mapper_name = "RAMBO-1",
	.funcs = &rambo1_funcs,
	.init_prg = mmc3_init_prg,
	.init_chr0 = rambo1_init_chr0,
	.write_handlers = rambo1_write_handlers,
	.max_prg_rom_size = SIZE_512K,
	.max_chr_rom_size = SIZE_256K,
	.max_wram_size = {SIZE_8K, 0},
	.flags = BOARD_INFO_FLAG_MIRROR_M |
	         BOARD_INFO_FLAG_M2_TIMER,
	.mirroring_values = std_mirroring_vh,
};

struct board_info board_tengen800037 = {
	.board_type = BOARD_TYPE_TENGEN_800037,
	.name = "TENGEN-800037",
	.mapper_name = "RAMBO-1",
	.funcs = &rambo1_funcs,
	.init_prg = mmc3_init_prg,
	.init_chr0 = rambo1_init_chr0,
	.write_handlers = tengen800037_write_handlers,
	.max_prg_rom_size = SIZE_512K,
	.max_chr_rom_size = SIZE_256K,
	.max_wram_size = {SIZE_8K, 0},
	.flags = BOARD_INFO_FLAG_MIRROR_M |
	         BOARD_INFO_FLAG_M2_TIMER,
	.mirroring_values = std_mirroring_vh,
};

struct board_info board_15_in_1 = {
	.board_type = BOARD_TYPE_15_IN_1,
	.name = "BMC 15/3-IN-1",
	.mapper_name = "MMC3",
	.funcs = &mmc3_funcs,
	.init_prg = mmc3_init_prg,
	.init_chr0 = std_chr_2k_1k,
	.write_handlers = m15_in_1_write_handlers,
	.max_prg_rom_size = SIZE_2048K,
	.max_chr_rom_size = SIZE_512K,
	.flags = BOARD_INFO_FLAG_MIRROR_M,
	.mirroring_values = std_mirroring_vh,
};

struct board_info board_waixing_type_a = {
	.board_type = BOARD_TYPE_WAIXING_TYPE_A,
	.name = "WAIXING (a)",
	.funcs = &mmc3_funcs,
	.init_prg = mmc3_init_prg,
	.init_chr0 = std_chr_2k_1k,
	.write_handlers = mmc3_write_handlers,
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
	.name = "WAIXING (c)",
	.funcs = &mmc3_funcs,
	.init_prg = mmc3_init_prg,
	.init_chr0 = std_chr_2k_1k,
	.write_handlers = mmc3_write_handlers,
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
	.init_chr0 = std_chr_2k_1k,
	.write_handlers = qj_zz_write_handlers,
	.max_prg_rom_size = SIZE_512K,
	.max_chr_rom_size = SIZE_256K,
	.flags = BOARD_INFO_FLAG_MIRROR_M,
	.mirroring_values = std_mirroring_vh,
};

struct board_info board_waixing_type_h = {
	.board_type = BOARD_TYPE_WAIXING_TYPE_H,
	.name = "WAIXING (h)",
	.funcs = &mmc3_funcs,
	.init_prg = mmc3_init_prg,
	.init_chr0 = std_chr_2k_1k,
	.write_handlers = mmc3_write_handlers,
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
	.init_chr0 = std_chr_2k_1k,
	.write_handlers = txc_tw_write_handlers,
	.max_prg_rom_size = SIZE_2048K,
	.max_chr_rom_size = SIZE_256K,
	.flags = BOARD_INFO_FLAG_MIRROR_M,
	.mirroring_values = std_mirroring_vh,
};

struct board_info board_hosenkan_electronics = {
	.board_type = BOARD_TYPE_HOSENKAN,
	.name = "HOSENKAN ELECTRONICS",
	.funcs = &mmc3_funcs,
	.init_prg = mmc3_init_prg,
	.init_chr0 = std_chr_2k_1k,
	.write_handlers = hosenkan_write_handlers,
	.max_prg_rom_size = SIZE_2048K,
	.max_chr_rom_size = SIZE_256K,
	.max_wram_size = {SIZE_8K, 0},
	.flags = BOARD_INFO_FLAG_MIRROR_M,
	.mirroring_values = std_mirroring_vh,
};

struct board_info board_superbig_7in1 = {
	.board_type = BOARD_TYPE_SUPERBIG_7_IN_1,
	.name = "BMC SUPERBIG 7-IN-1",
	.funcs = &mmc3_funcs,
	.init_prg = mmc3_init_prg,
	.init_chr0 = std_chr_2k_1k,
	.write_handlers = superbig7in1_write_handlers,
	.max_prg_rom_size = SIZE_1024K,
	.max_chr_rom_size = SIZE_1024K,
	.max_wram_size = {SIZE_8K, 0},
	.flags = BOARD_INFO_FLAG_MIRROR_M,
	.mirroring_values = std_mirroring_vh,
};

struct board_info board_superhik_4in1 = {
	.board_type = BOARD_TYPE_SUPERHIK_4_IN_1,
	.name = "BMC SUPERHIK 4-IN-1",
	.funcs = &mmc3_funcs,
	.init_prg = mmc3_init_prg,
	.init_chr0 = std_chr_2k_1k,
	.write_handlers = superhik4in1_write_handlers,
	.max_prg_rom_size = SIZE_512K,
	.max_chr_rom_size = SIZE_512K,
	.flags = BOARD_INFO_FLAG_MIRROR_M,
	.mirroring_values = std_mirroring_vh,
};

static void mmc3_end_frame(struct board *board, uint32_t cycles)
{
	a12_timer_end_frame(board->emu->a12_timer, cycles);
}

static int mmc3_init(struct board *board)
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
	if (board->info->board_type == BOARD_TYPE_TxROM)
		_mmc6_compat_hack =
			emu->config->mmc6_compat_hack_enabled;


	variant = A12_TIMER_VARIANT_MMC3_STD;
	_bank_select_mask = 0x07;
	_chr_mode_mask = 0x80;
	_has_alt_mirroring = 0;
	switch (board->info->board_type) {
	case BOARD_TYPE_ACCLAIM_MC_ACC:
		variant = A12_TIMER_VARIANT_ACCLAIM_MC_ACC;
		break;
	case BOARD_TYPE_TxROM_MMC3A:
		variant = A12_TIMER_VARIANT_MMC3_ALT;
		break;
	case BOARD_TYPE_TENGEN_800037:
		_has_alt_mirroring = 1;
	case BOARD_TYPE_TENGEN_800032:
		variant = A12_TIMER_VARIANT_RAMBO1;
		_bank_select_mask = 0x0f;
		_chr_mode_mask = 0xa0;
		break;
	case BOARD_TYPE_TxSROM:
		_has_alt_mirroring = 1;
		break;
	}

	if (a12_timer_init(emu, variant))
		return 1;

	return 0;
}

static void mmc3_cleanup(struct board *board)
{
	a12_timer_cleanup(board->emu);
}

static int mmc3_load_state(struct board *board, struct save_state *state)
{
	return a12_timer_load_state(board->emu, state);
}

static int mmc3_save_state(struct board *board, struct save_state *state)
{
	return a12_timer_save_state(board->emu, state);
}

static void mmc3_reset(struct board *board, int hard)
{
	if (hard) {
		a12_timer_reset(board->emu->a12_timer, hard);
		_first_bank_select = 1;

		board->prg_mode = 0;
		board->chr_mode = 0;

		board->prg_or = 0;
		board->chr_or = 0;

		switch (board->info->board_type) {
		case BOARD_TYPE_SUPERHIK_4_IN_1:
			board->prg_and = 0x0f;
			board->chr_and = 0x7f;
			board->prg_or = 0x00;
			board->chr_or = 0x000;
			board->prg_banks[5].size = SIZE_32K;
			board->prg_banks[5].bank = 0;
			break;
		case BOARD_TYPE_SUPERBIG_7_IN_1:
			board->prg_and = 0x0f;
			board->prg_or = 0x00;
			board->chr_and = 0x7f;
			board->chr_or = 0x000;
			break;
		case BOARD_TYPE_15_IN_1:
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
		case BOARD_TYPE_TENGEN_800032:
		case BOARD_TYPE_TENGEN_800037:
			board->prg_and = ~0;
			board->chr_and = ~0;
			m2_timer_set_enabled(board->emu->m2_timer, 0, 0);
			m2_timer_set_prescaler(board->emu->m2_timer, 3, 0);
			m2_timer_set_prescaler_reload(board->emu->m2_timer, 3, 0);
			m2_timer_set_irq_delay(board->emu->m2_timer, 2, 0);
			m2_timer_set_size(board->emu->m2_timer, 8, 0);
			m2_timer_set_flags(board->emu->m2_timer,
					   M2_TIMER_FLAG_RELOAD |
					   M2_TIMER_FLAG_DELAYED_RELOAD |
					   M2_TIMER_FLAG_PRESCALER |
					   M2_TIMER_FLAG_PRESCALER_RELOAD,
					   0);
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

static CPU_READ_HANDLER(mmc6_wram_read_handler)
{
	struct board *board = emu->board;
	uint8_t rc;

	/* Reads are open-bus if neither 512-byte page is readable */

	addr &= 0x3ff;

	/* Note that there will always be battery-backed PRG-RAM; the RAM
	   itself is built into the MMC6, and all HKROM boards in existence
	   have a battery */

	if (!(_wram_protect & 0xa0))
		rc = value;
	if ((addr < 0x200 && ((_wram_protect & 0xa0) == 0x80)) ||
	    (addr >= 0x200 && ((_wram_protect & 0xa0) == 0x20)))
		rc = 0;
	else
		rc = board->mapper_ram.data[addr];

	return rc;
}

static CPU_WRITE_HANDLER(mmc6_wram_write_handler)
{
	struct board *board = emu->board;

	addr &= 0x3ff;

	if ((addr < 0x200 && ((_wram_protect & 0x30) == 0x30)) ||
	    (addr >= 0x200 && ((_wram_protect & 0xc0) == 0xc0)))
		board->mapper_ram.data[addr] = value;
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
	case BOARD_TYPE_ZZ:
		board->prg_or = (value << 2) & 0x10;
		if ((value & 0x03) == 0x03)
			board->prg_or |= 0x08;
		board->prg_and = (value << 1 | 0x07);
		board->chr_or = (value << 5) & 0x80;
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
			mmc3_update_prg(board);
		}
		break;
	}

	if ((old_prg_and != board->prg_and) ||
	    (old_prg_or != board->prg_or))
		mmc3_update_prg(board);

	if ((old_chr_and != board->chr_and) ||
	    (old_chr_or != board->chr_or))
		mmc3_update_chr(board);
}

static void mmc3_update_prg(struct board *board)
{
	if (!board->prg_mode) {
		board->prg_banks[1].address = 0x8000;
		board->prg_banks[2].address = 0xa000;
		board->prg_banks[3].address = 0xc000;
	} else if (_bank_select_mask == 0x0f) {	/* FIXME probably not the best check */
		board->prg_banks[1].address = 0xa000;
		board->prg_banks[2].address = 0xc000;
		board->prg_banks[3].address = 0x8000;
	} else {
		board->prg_banks[1].address = 0xc000;
		board->prg_banks[2].address = 0xa000;
		board->prg_banks[3].address = 0x8000;
	}

	board_prg_sync(board);
}

static void mmc3_update_chr(struct board *board)
{
	if (!(board->chr_mode & 0x80)) {
		board->chr_banks0[0].address &= 0x0fff;
		board->chr_banks0[1].address &= 0x0fff;
		board->chr_banks0[2].address |= 0x1000;
		board->chr_banks0[3].address |= 0x1000;
		board->chr_banks0[4].address |= 0x1000;
		board->chr_banks0[5].address |= 0x1000;
		board->chr_banks0[6].address &= 0x0fff;
		board->chr_banks0[7].address &= 0x0fff;
	} else {
		board->chr_banks0[0].address |= 0x1000;
		board->chr_banks0[1].address |= 0x1000;
		board->chr_banks0[2].address &= 0x0fff;
		board->chr_banks0[3].address &= 0x0fff;
		board->chr_banks0[4].address &= 0x0fff;
		board->chr_banks0[5].address &= 0x0fff;
		board->chr_banks0[6].address |= 0x1000;
		board->chr_banks0[7].address |= 0x1000;
	}

	if (!(board->chr_mode & 0x20)) {
		board->chr_banks0[0].size = SIZE_2K;
		board->chr_banks0[1].size = SIZE_2K;
		board->chr_banks0[6].size = 0;
		board->chr_banks0[7].size = 0;
		board->chr_banks0[0].shift = 1;
		board->chr_banks0[1].shift = 1;
	} else {
		board->chr_banks0[0].size = SIZE_1K;
		board->chr_banks0[1].size = SIZE_1K;
		board->chr_banks0[6].size = SIZE_1K;
		board->chr_banks0[7].size = SIZE_1K;
		board->chr_banks0[0].shift = 0;
		board->chr_banks0[1].shift = 0;
	}

//      if (board->info->board_type == BOARD_TYPE_TxSROM) {
	if (_has_alt_mirroring) {
		int i;

		for (i = 0; i < 4; i++) {
			int reg = i;
			if (board->chr_mode)
				reg += 2;
			else
				reg /= 2;

			board->nmt_banks[i].type = MAP_TYPE_CIRAM;
			board->nmt_banks[i].bank =
			    board->chr_banks0[reg].
			    bank & 0x80 ? 1 : 0;
			board->nmt_banks[i].perms =
			    MAP_PERM_READWRITE;
		}

		board_nmt_sync(board);
	}

	board_chr_sync(board, 0);
}

static CPU_WRITE_HANDLER(hosenkan_write_handler)
{
	static uint8_t reg_map[8] = { 0, 3, 1, 5, 6, 7, 2, 4 };

	addr &= 0xe001;

	if (addr == 0xa000) {
		mmc3_write_handler(emu, 0x8000, reg_map[value & 0x07], cycles);
	} else if (addr == 0xc000) {
		mmc3_write_handler(emu, 0x8001, value, cycles);
	} else if (addr == 0xc001) {
		a12_timer_irq_latch(emu, addr, value, cycles);
		a12_timer_irq_reload(emu, addr, value, cycles);
	}
}

static CPU_WRITE_HANDLER(superbig7in1_write_handler)
{
	struct board *board;
	int bank;

	board = emu->board;

	bank = value & 7;

	if ((addr & 0xe001) == 0xa001) {
		mmc3_write_handler(emu, 0xa001, value & 0xc0, cycles);
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
}

static CPU_WRITE_HANDLER(mmc3_write_handler)
{
	struct board *board = emu->board;
	int perms;
	int bank;
	int old;

	switch (addr & 0xa001) {
	case 0x8000:
		old = _bank_select & 0xc0;

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

		_bank_select = value;

		/* MMC6 only */
		if (board->info->board_type == BOARD_TYPE_HKROM
		    && !(_bank_select & 0x20))
			_wram_protect = 0;

		board->prg_mode = value & 0x40;
		board->chr_mode = value & _chr_mode_mask;

		if ((value & 0x80) != (old & 0x80)) {
			mmc3_update_chr(board);
		}

		if ((value & 0x40) != (old & 0x40)) {
			mmc3_update_prg(board);
		}
		break;
	case 0x8001:
		bank = _bank_select & _bank_select_mask;

		switch (bank) {
		case 0:
		case 1:
		case 2:
		case 3:
		case 4:
		case 5:
			if (board->info->board_type == BOARD_TYPE_TQROM) {
				int type = MAP_TYPE_AUTO;
				if (value & 0x40) {
					value &= 0x3f;
					type = MAP_TYPE_RAM0;
				}
				board->chr_banks0[bank].type = type;
			} else if ((board->info->board_type == BOARD_TYPE_WAIXING_TYPE_A) ||
				   (board->info->board_type == BOARD_TYPE_WAIXING_TYPE_C)) {
				int type = MAP_TYPE_AUTO;
				int max = 0x0b;

				if (board->vram[0].size == SIZE_2K)
					max = 0x09;

				if ((value >= 0x08) && (value <= max)) {
					value -= 0x08;
					type = MAP_TYPE_RAM0;
				}
				board->chr_banks0[bank].type = type;
			} else if ((board->info->board_type == BOARD_TYPE_WAIXING_TYPE_H) &&
				   (bank == 0)) {
				board->prg_or = (value & 0x02) << 5;
			}

			board->chr_banks0[bank].bank = value;
			break;
		case 6:
		case 7:
			board->prg_banks[bank - 0x05].bank = value;
			break;
		case 8:
		case 9:
			board->chr_banks0[bank - 2].bank = value;
			break;
		case 15:
			board->prg_banks[3].bank = value;
			break;
		}

		mmc3_update_prg(board);
		mmc3_update_chr(board);
		break;
	case 0xa001:
		perms = MAP_PERM_READWRITE;

		if (!_first_bank_select && _mmc6_compat_hack)
			break;

		if (emu_system_is_vs(board->emu))
			break;

		if ((board->info->board_type != BOARD_TYPE_HKROM)
		    || (_bank_select & 0x20))
			_wram_protect = value;

		if (board->info->board_type != BOARD_TYPE_HKROM) {
			if (!(value & 0x80))
				perms = 0;
			else if (value & 0x40)
				perms ^= MAP_PERM_WRITE;

			board->prg_banks[0].perms = perms;
			board_prg_sync(board);
		}
		break;
	}
}

static CPU_WRITE_HANDLER(rambo1_irq_latch)
{
	emu->board->irq_counter_reload = value;
	m2_timer_set_reload(emu->m2_timer, value, cycles);
	a12_timer_set_reload(emu->a12_timer, value, cycles);
}

static CPU_WRITE_HANDLER(rambo1_irq_reload)
{
	struct board *board;
	int delay;

	board = emu->board;
	
	if (board->irq_control != (value & 1)) {
		board->irq_control = value & 1;
		m2_timer_set_counter_enabled(emu->m2_timer, value & 0x01, cycles);
		a12_timer_set_counter_enabled(emu->a12_timer, !(value & 0x01), cycles);
	}

	if (board->irq_counter_reload == 0)
		delay = 0;
	else
		delay = 1;

	if (board->irq_control) {
		m2_timer_force_reload(emu->m2_timer, cycles);
		m2_timer_set_prescaler(emu->m2_timer, 3, cycles);
		m2_timer_set_force_reload_delay(emu->m2_timer, delay ? 2 : 0, cycles);
	} else {
		//a12_timer_set_force_reload_delay(emu->a12_timer, delay, cycles);
		a12_timer_force_reload(emu->a12_timer, cycles);
	}
}

static CPU_WRITE_HANDLER(rambo1_irq_disable)
{
	struct board *board;

	board = emu->board;
	
	if (board->irq_control)
		m2_timer_set_irq_enabled(emu->m2_timer, 0, cycles);
	else
		a12_timer_set_irq_enabled(emu->a12_timer, 0, cycles);
}

static CPU_WRITE_HANDLER(rambo1_irq_enable)
{
	struct board *board;

	board = emu->board;

	if (board->irq_control)
		m2_timer_set_irq_enabled(emu->m2_timer, 1, cycles);
	else
		a12_timer_set_irq_enabled(emu->a12_timer, 1, cycles);
}

static CPU_WRITE_HANDLER(superhik4in1_write_handler)
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
