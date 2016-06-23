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

#include <libgen.h>
#ifdef __unix__
#include <sys/mman.h>
#else
#include <SDL_stdinc.h>
#endif
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "board_private.h"
#include "file_io.h"
#include "patch.h"
#include "m2_timer.h"

struct board_name_mapping {
	const char *name;
	const uint32_t type;
};

struct board_fixup {
	uint32_t type;
	uint32_t real_type;
	int mirroring;
	size_t wram_size[2];
	size_t vram_size[2];
};

extern int running;
extern struct board_info board_subor_b;
extern struct board_info board_subor_a;
extern struct board_info board_76in1;
extern struct board_info board_cufrom;
extern struct board_info board_m150in1;
extern struct board_info board_magicseries;
extern struct board_info board_unl_mortalkombat2;
extern struct board_info board_super700in1;
extern struct board_info board_gamestar_b;
extern struct board_info board_superhik_4in1;
extern struct board_info board_superbig_7in1;
extern struct board_info board_resetbased_4_in_1;
extern struct board_info board_hosenkan_electronics;
extern struct board_info board_jycompany_a;
extern struct board_info board_jycompany_b;
extern struct board_info board_jycompany_c;
extern struct board_info board_35in1;
extern struct board_info board_cne_psb;
extern struct board_info board_txc_tw;
extern struct board_info board_cne_shlz;
extern struct board_info board_sachen_74x374b;
extern struct board_info board_kasing;
extern struct board_info board_rumblestation_15_in_1;
extern struct board_info board_22_in_1;
extern struct board_info board_15_in_1;
extern struct board_info board_familybasic;
extern struct board_info board_namco_cnrom_wram;
extern struct board_info board_ines225;
extern struct board_info board_sachen_tcu01;
extern struct board_info board_sachen_sa0036;
extern struct board_info board_sachen_sa0037;
extern struct board_info board_sachen_72007;
extern struct board_info board_sachen_72008;
extern struct board_info board_sachen_nrom;
extern struct board_info board_sachen_8259a;
extern struct board_info board_sachen_8259b;
extern struct board_info board_sachen_8259c;
extern struct board_info board_sachen_8259d;
extern struct board_info board_action52;
extern struct board_info board_caltron_6in1;
extern struct board_info board_contra_100in1;
extern struct board_info board_cnrom;
extern struct board_info board_cnrom_no_conflict;
extern struct board_info board_cnrom_security;
extern struct board_info board_cnrom_security_bank0;
extern struct board_info board_cnrom_security_bank1;
extern struct board_info board_cnrom_security_bank2;
extern struct board_info board_cnrom_security_bank3;
extern struct board_info board_singlechip;
extern struct board_info board_nina001;
extern struct board_info board_streemerz_bundle;
extern struct board_info board_sunsoft5b;
extern struct board_info board_fds;
extern struct board_info board_nsf;
extern struct board_info board_sunsoft4;
extern struct board_info board_taito_x1_005;
extern struct board_info board_taito_x1_005_alt;
extern struct board_info board_taito_x1_017;
extern struct board_info board_namco163;
extern struct board_info board_namco340;
extern struct board_info board_namco340_compat;
extern struct board_info board_namco175;
extern struct board_info board_fxrom;
extern struct board_info board_pxrom;
extern struct board_info board_exrom;
extern struct board_info board_exrom_compat;
extern struct board_info board_sxrom;
extern struct board_info board_sxrom_wram;
extern struct board_info board_snrom;
extern struct board_info board_sorom;
extern struct board_info board_surom;
extern struct board_info board_sXrom;
extern struct board_info board_serom_shrom;
extern struct board_info board_sxrom_compat;
extern struct board_info board_sxrom_mmc1a;
extern struct board_info board_event;
extern struct board_info board_txrom;
extern struct board_info board_txrom_mmc3a;
extern struct board_info board_txsrom;
extern struct board_info board_tqrom;
extern struct board_info board_hkrom;
extern struct board_info board_nes_qj;
extern struct board_info board_pal_zz;
extern struct board_info board_vs_super_xevious;
extern struct board_info board_vs_rbi_baseball;
extern struct board_info board_vs_tko_boxing;
extern struct board_info board_namco108;
extern struct board_info board_ntdec_112;
extern struct board_info board_vrc1;
extern struct board_info board_vrc6a;
extern struct board_info board_vrc6b;
extern struct board_info board_vrc7_compat;
extern struct board_info board_vrc7a;
extern struct board_info board_vrc7b;
extern struct board_info board_vrc3;
extern struct board_info board_vrc2a;
extern struct board_info board_vrc2b;
extern struct board_info board_vrc2c;
extern struct board_info board_vrc4a;
extern struct board_info board_vrc4b;
extern struct board_info board_vrc4c;
extern struct board_info board_vrc4d;
extern struct board_info board_vrc4e;
extern struct board_info board_vrc4f;
extern struct board_info board_vrc4e_compat;
extern struct board_info board_vrc4ac;
extern struct board_info board_vrc4bd;
extern struct board_info board_nrom;
extern struct board_info board_nrom368;
extern struct board_info board_uxrom;
extern struct board_info board_uxrom_pc_prowrestling;
extern struct board_info board_uxrom_no_conflict;
extern struct board_info board_un1rom;
extern struct board_info board_unrom_74hc08;
extern struct board_info board_bnrom;
extern struct board_info board_cprom;
extern struct board_info board_colordreams;
extern struct board_info board_gxrom;
extern struct board_info board_jaleco_jf13;
extern struct board_info board_jaleco_jf14;
extern struct board_info board_jaleco_ss88006;
extern struct board_info board_axrom;
extern struct board_info board_camerica_bf9093;
extern struct board_info board_camerica_bf9096;
extern struct board_info board_camerica_bf9097;
extern struct board_info board_camerica_aladdin;
extern struct board_info board_nina03_06;
extern struct board_info board_vs_unisystem;
extern struct board_info board_vs_pinball;
extern struct board_info board_vs_pinballj;
extern struct board_info board_vs_gumshoe;
extern struct board_info board_jaleco_jf16;
extern struct board_info board_ntdec_193;
extern struct board_info board_74x139_74;
extern struct board_info board_sunsoft1;
extern struct board_info board_pci556;
extern struct board_info board_sunsoft2;
extern struct board_info board_sunsoft3r;
extern struct board_info board_sunsoft3;
extern struct board_info board_74x161_161_32;
extern struct board_info board_irem_holy_diver;
extern struct board_info board_irem_74x161_161_21_138;
extern struct board_info board_irem_tam_s1;
extern struct board_info board_irem_h3001;
extern struct board_info board_irem_g101;
extern struct board_info board_irem_g101_b;
extern struct board_info board_agci_50282;
extern struct board_info board_ines201;
extern struct board_info board_taito_tc0190fmc_pal16r4;
extern struct board_info board_taito_tc0190fmc;
extern struct board_info board_maxi15;
extern struct board_info board_namco3446;
extern struct board_info board_namco3433;
extern struct board_info board_namco3453;
extern struct board_info board_namco3425;
extern struct board_info board_tengen800032;
extern struct board_info board_tengen800037;
extern struct board_info board_acclaim_mc_acc;
extern struct board_info board_bandai_fcg;
extern struct board_info board_bandai_lz93d50;
extern struct board_info board_bandai_jump2;
extern struct board_info board_ines36;
extern struct board_info board_inlnsf;
extern struct board_info board_unrom512;
extern struct board_info board_unrom512_flash;
extern struct board_info board_1200in1;
extern struct board_info board_20in1;
extern struct board_info board_20in1_ines231;
extern struct board_info board_waixing_type_a;
extern struct board_info board_waixing_type_c;
extern struct board_info board_waixing_type_h;
extern struct board_info board_waixing_sgzlz;

static struct board_info *board_info_list[] = {
	&board_subor_b,
	&board_subor_a,
	&board_76in1,
	&board_cufrom,
	&board_m150in1,
	&board_magicseries,
	&board_unl_mortalkombat2,
	&board_super700in1,
	&board_gamestar_b,
	&board_superhik_4in1,
	&board_superbig_7in1,
	&board_resetbased_4_in_1,
	&board_hosenkan_electronics,
	&board_jycompany_a,
	&board_jycompany_b,
	&board_jycompany_c,
	&board_35in1,
	&board_cne_psb,
	&board_txc_tw,
	&board_cne_shlz,
	&board_sachen_74x374b,
	&board_kasing,
	&board_rumblestation_15_in_1,
	&board_22_in_1,
	&board_15_in_1,
	&board_familybasic,
	&board_ines225,
	&board_sachen_tcu01,
	&board_sachen_sa0036,
	&board_sachen_sa0037,
	&board_sachen_72007,
	&board_sachen_72008,
	&board_sachen_nrom,
	&board_sachen_8259a,
	&board_sachen_8259b,
	&board_sachen_8259c,
	&board_sachen_8259d,
	&board_action52,
	&board_caltron_6in1,
	&board_contra_100in1,
	&board_cnrom,
	&board_cnrom_no_conflict,
	&board_cnrom_security,
	&board_cnrom_security_bank0,
	&board_cnrom_security_bank1,
	&board_cnrom_security_bank2,
	&board_cnrom_security_bank3,
	&board_singlechip,
	&board_nina001,
	&board_streemerz_bundle,
	&board_sunsoft5b,
	&board_fds,
	&board_nsf,
	&board_sunsoft4,
	&board_taito_x1_005,
	&board_taito_x1_005_alt,
	&board_taito_x1_017,
	&board_namco163,
	&board_namco340,
	&board_namco340_compat,
	&board_namco175,
	&board_namco_cnrom_wram,
	&board_fxrom,
	&board_pxrom,
	&board_exrom,
	&board_exrom_compat,
	&board_sxrom,
	&board_sxrom_wram,
	&board_snrom,
	&board_sorom,
	&board_surom,
	&board_sXrom,
	&board_serom_shrom,
	&board_sxrom_compat,
	&board_sxrom_mmc1a,
	&board_event,
	&board_txrom,
	&board_txrom_mmc3a,
	&board_txsrom,
	&board_tqrom,
	&board_hkrom,
	&board_nes_qj,
	&board_pal_zz,
	&board_vs_super_xevious,
	&board_vs_rbi_baseball,
	&board_vs_tko_boxing,
	&board_namco108,
	&board_ntdec_112,
	&board_vrc1,
	&board_vrc6a,
	&board_vrc6b,
	&board_vrc7_compat,
	&board_vrc7a,
	&board_vrc7b,
	&board_vrc3,
	&board_vrc2a,
	&board_vrc2b,
	&board_vrc2c,
	&board_vrc4a,
	&board_vrc4b,
	&board_vrc4c,
	&board_vrc4d,
	&board_vrc4e,
	&board_vrc4f,
	&board_vrc4e_compat,
	&board_vrc4ac,
	&board_vrc4bd,
	&board_maxi15,
	&board_nrom,
	&board_nrom368,
	&board_uxrom,
	&board_uxrom_pc_prowrestling,
	&board_uxrom_no_conflict,
	&board_un1rom,
	&board_unrom_74hc08,
	&board_bnrom,
	&board_cprom,
	&board_colordreams,
	&board_gxrom,
	&board_jaleco_jf13,
	&board_jaleco_jf14,
	&board_jaleco_ss88006,
	&board_axrom,
	&board_camerica_bf9093,
	&board_camerica_bf9096,
	&board_camerica_bf9097,
	&board_camerica_aladdin,
	&board_nina03_06,
	&board_vs_unisystem,
	&board_vs_pinball,
	&board_vs_pinballj,
	&board_vs_gumshoe,
	&board_irem_74x161_161_21_138,
	&board_jaleco_jf16,
	&board_irem_holy_diver,
	&board_ntdec_193,
	&board_74x139_74,
	&board_sunsoft1,
	&board_pci556,
	&board_sunsoft2,
	&board_sunsoft3r,
	&board_sunsoft3,
	&board_74x161_161_32,
	&board_irem_tam_s1,
	&board_irem_h3001,
	&board_irem_g101,
	&board_irem_g101_b,
	&board_agci_50282,
	&board_ines201,
	&board_taito_tc0190fmc_pal16r4,
	&board_taito_tc0190fmc,
	&board_namco3446,
	&board_namco3433,
	&board_namco3453,
	&board_namco3425,
	&board_tengen800032,
	&board_tengen800037,
	&board_acclaim_mc_acc,
	&board_bandai_fcg,
	&board_bandai_lz93d50,
	&board_bandai_jump2,
	&board_ines36,
	&board_inlnsf,
	&board_unrom512,
	&board_unrom512_flash,
	&board_1200in1,
	&board_20in1,
	&board_20in1_ines231,
	&board_waixing_type_a,
	&board_waixing_type_c,
	&board_waixing_type_h,
	&board_waixing_sgzlz,
	NULL,
};

/* FIXME WAIXING / BMC CONTRA 100-IN-1 -> 15 */
static struct board_name_mapping mapping_list[] =  {
	{ "AGCI-47516", BOARD_TYPE_COLORDREAMS },
	{ "ACCLAIM-AOROM", BOARD_TYPE_AxROM },
	{ "ACCLAIM-TLROM", BOARD_TYPE_TxROM },
	{ "AVE-74*161", BOARD_TYPE_CNROM },
	{ "AVE-MB-91", BOARD_TYPE_NINA_03_06 },
	{ "AVE-NINA-03", BOARD_TYPE_NINA_03_06 },
	{ "AVE-NINA-06", BOARD_TYPE_NINA_03_06 },
	{ "AVE-NINA-07", BOARD_TYPE_COLORDREAMS },
	{ "BANDAI-74*161/32", BOARD_TYPE_CNROM },
	{ "BANDAI-74*161/161/32", BOARD_TYPE_74x161_161_32 },
	{ "BANDAI-74*161/02/74", BOARD_TYPE_BANDAI_OEKAKIDS },
	{ "BANDAI-CNROM", BOARD_TYPE_CNROM },
	{ "BANDAI-FCG-1", BOARD_TYPE_BANDAI_FCG },
	{ "BANDAI-FCG-2", BOARD_TYPE_BANDAI_FCG },
	{ "BANDAI-GNROM", BOARD_TYPE_GxROM },
	{ "BANDAI-NROM-128", BOARD_TYPE_NROM },
	{ "BANDAI-NROM-256", BOARD_TYPE_NROM },
	{ "BANDAI-PT-554", BOARD_TYPE_CNROM },
	{ "BMC 76-IN-1", BOARD_TYPE_76_IN_1 },
	{ "BMC SUPER 42-IN-1", BOARD_TYPE_76_IN_1 },
	{ "CAMERICA-ALGN", BOARD_TYPE_CAMERICA_BF9093 },
	{ "CAMERICA-GAMEGENIE", BOARD_TYPE_NROM },
	{ "CODEMASTERS-NR8N", BOARD_TYPE_CAMERICA_BF9093 },
	{ "HVC-AMROM", BOARD_TYPE_AxROM },
	{ "HVC-AOROM", BOARD_TYPE_AxROM },
	{ "HVC-CNROM", BOARD_TYPE_CNROM },
	{ "HVC-CNROM+SECURITY", BOARD_TYPE_SECURITY_CNROM },
	{ "HVC-EKROM", BOARD_TYPE_EKROM },
	{ "HVC-ELROM", BOARD_TYPE_ExROM },
	{ "HVC-ETROM", BOARD_TYPE_ETROM },
	{ "HVC-EWROM", BOARD_TYPE_EWROM },
	{ "HVC-FJROM", BOARD_TYPE_FxROM_WRAM },
	{ "HVC-FKROM", BOARD_TYPE_FxROM_WRAM },
	{ "HVC-GNROM", BOARD_TYPE_GxROM },
	{ "HVC-HROM", BOARD_TYPE_HROM },
	{ "HVC-NROM-128", BOARD_TYPE_NROM },
	{ "HVC-NROM-256", BOARD_TYPE_NROM },
	{ "HVC-PEEOROM", BOARD_TYPE_PxROM },
	{ "HVC-RROM", BOARD_TYPE_NROM },
	{ "HVC-RTROM", BOARD_TYPE_NROM },
	{ "HVC-SAROM", BOARD_TYPE_SxROM },
	{ "HVC-SEROM", BOARD_TYPE_SEROM_SHROM },
	{ "HVC-SFROM", BOARD_TYPE_SxROM },
	{ "HVC-SGROM", BOARD_TYPE_SxROM },
	{ "HVC-SIROM", BOARD_TYPE_SxROM },
	{ "HVC-SJROM", BOARD_TYPE_SxROM_WRAM },
	{ "HVC-SKEPROM", BOARD_TYPE_SxROM_WRAM },
	{ "HVC-SKROM", BOARD_TYPE_SxROM_WRAM },
	{ "HVC-SL1ROM", BOARD_TYPE_SxROM },
	{ "HVC-SLROM", BOARD_TYPE_SxROM },
	{ "HVC-SLRROM", BOARD_TYPE_SxROM },
	{ "HVC-SMROM", BOARD_TYPE_SxROM },
	{ "HVC-SNROM", BOARD_TYPE_SNROM },
	{ "HVC-SOROM", BOARD_TYPE_SOROM },
	{ "HVC-SROM", BOARD_TYPE_NROM },
	{ "HVC-STROM", BOARD_TYPE_SxROM },
	{ "HVC-SUROM", BOARD_TYPE_SUROM },
	{ "HVC-SXROM", BOARD_TYPE_SXROM },
	{ "HVC-TBROM", BOARD_TYPE_TxROM },
	{ "HVC-TEROM", BOARD_TYPE_TxROM },
	{ "HVC-TFROM", BOARD_TYPE_TxROM },
	{ "HVC-TGROM", BOARD_TYPE_TxROM },
	{ "HVC-TKROM", BOARD_TYPE_TxROM },
	{ "HVC-TKSROM", BOARD_TYPE_TxSROM },
	{ "HVC-TL1ROM", BOARD_TYPE_TxROM },
	{ "HVC-TLROM", BOARD_TYPE_TxROM },
	{ "HVC-TLSROM", BOARD_TYPE_TxSROM },
	{ "HVC-TNROM", BOARD_TYPE_TxROM },
	{ "HVC-TSROM", BOARD_TYPE_TxROM },
	{ "HVC-UNROM", BOARD_TYPE_UxROM },
	{ "HVC-UOROM", BOARD_TYPE_UxROM },
	{ "IREM-BNROM", BOARD_TYPE_BNROM },
	{ "IREM-FCG-1", BOARD_TYPE_BANDAI_LZ93D50 },
	{ "IREM-NROM-128", BOARD_TYPE_NROM },
	{ "IREM-NROM-256", BOARD_TYPE_NROM },
	{ "IREM-UNROM", BOARD_TYPE_UxROM },
	{ "JALECO-JF-01", BOARD_TYPE_NROM },
	{ "JALECO-JF-02", BOARD_TYPE_NROM },
	{ "JALECO-JF-03", BOARD_TYPE_NROM },
	{ "JALECO-JF-04", BOARD_TYPE_NROM },
	{ "JALECO-JF-05", BOARD_TYPE_74x139_74 },
	{ "JALECO-JF-06", BOARD_TYPE_74x139_74 },
	{ "JALECO-JF-07", BOARD_TYPE_74x139_74 },
	{ "JALECO-JF-08", BOARD_TYPE_74x139_74 },
	{ "JALECO-JF-09", BOARD_TYPE_74x139_74 },
	{ "JALECO-JF-10", BOARD_TYPE_74x139_74 },
	{ "JALECO-JF-11", BOARD_TYPE_JALECO_JF14 },
	{ "JALECO-JF-14", BOARD_TYPE_JALECO_JF14 },
	{ "JALECO-JF-15", BOARD_TYPE_UxROM },
	{ "JALECO-JF-18", BOARD_TYPE_UxROM },
	{ "JALECO-JF-20", BOARD_TYPE_VRC1 },
	{ "JALECO-JF-21", BOARD_TYPE_JALECO_JF19 },
	{ "JALECO-JF-22", BOARD_TYPE_VRC1 },
	{ "JALECO-JF-23", BOARD_TYPE_JALECO_SS88006 },
	{ "JALECO-JF-24", BOARD_TYPE_JALECO_SS88006 },
	{ "JALECO-JF-25", BOARD_TYPE_JALECO_SS88006 },
	{ "JALECO-JF-27", BOARD_TYPE_JALECO_SS88006 },
	{ "JALECO-JF-29", BOARD_TYPE_JALECO_SS88006 },
	{ "JALECO-JF-37", BOARD_TYPE_JALECO_SS88006 },
	{ "JALECO-JF-38", BOARD_TYPE_JALECO_SS88006 },
	{ "JALECO-JF-40", BOARD_TYPE_JALECO_SS88006 },
	{ "KONAMI-74*139/74", BOARD_TYPE_74x139_74 },
	{ "KONAMI-CNROM", BOARD_TYPE_CNROM },
	{ "KONAMI-NROM-128", BOARD_TYPE_NROM },
	{ "KONAMI-SLROM", BOARD_TYPE_SxROM },
	{ "KONAMI-TLROM", BOARD_TYPE_TxROM },
	{ "KONAMI-UNROM", BOARD_TYPE_UxROM },
	{ "MXMDHTWO / TXC", BOARD_TYPE_BNROM },
	{ "NAMCOT-3301", BOARD_TYPE_NROM },
	{ "NAMCOT-3302", BOARD_TYPE_NROM },
	{ "NAMCOT-3303", BOARD_TYPE_NROM },
	{ "NAMCOT-3304", BOARD_TYPE_NROM },
	{ "NAMCOT-3305", BOARD_TYPE_NROM },
	{ "NAMCOT-3311", BOARD_TYPE_NROM },
	{ "NAMCOT-3312", BOARD_TYPE_NROM },
	{ "NAMCOT-3401", BOARD_TYPE_NAMCO_108 },
	{ "NAMCOT-3405", BOARD_TYPE_NAMCO_108 },
	{ "NAMCOT-3406", BOARD_TYPE_NAMCO_108 },
	{ "NAMCOT-3407", BOARD_TYPE_NAMCO_108 },
	{ "NAMCOT-3413", BOARD_TYPE_NAMCO_108 },
	{ "NAMCOT-3414", BOARD_TYPE_NAMCO_108 },
	{ "NAMCOT-3415", BOARD_TYPE_NAMCO_108 },
	{ "NAMCOT-3416", BOARD_TYPE_NAMCO_108 },
	{ "NAMCOT-3417", BOARD_TYPE_NAMCO_108 },
	{ "NAMCOT-3443", BOARD_TYPE_NAMCO_3433 },
	{ "NAMCOT-3451", BOARD_TYPE_NAMCO_108 },
	{ "NAMCOT-CNROM+WRAM", BOARD_TYPE_NAMCO_CNROM_WRAM },
	{ "NES-AMROM", BOARD_TYPE_AxROM },
	{ "NES-AN1ROM", BOARD_TYPE_AxROM },
	{ "NES-ANROM", BOARD_TYPE_AxROM },
	{ "NES-AOROM", BOARD_TYPE_AxROM },
	{ "NES-B4", BOARD_TYPE_TxROM },
	{ "NES-BNROM", BOARD_TYPE_BNROM },
	{ "NES-BTR", BOARD_TYPE_SUNSOFT5B },
	{ "NES-CNROM", BOARD_TYPE_CNROM },
	{ "NES-DE1ROM", BOARD_TYPE_NAMCO_108 },
	{ "NES-DEROM", BOARD_TYPE_NAMCO_108 },
	{ "NES-DRROM", BOARD_TYPE_DRROM },
	{ "NES-EKROM", BOARD_TYPE_EKROM },
	{ "NES-ELROM", BOARD_TYPE_ExROM },
	{ "NES-ETROM", BOARD_TYPE_ETROM },
	{ "NES-EWROM", BOARD_TYPE_EWROM },
	{ "NES-GNROM", BOARD_TYPE_GxROM },
	{ "NES-JSROM", BOARD_TYPE_SUNSOFT5B },
	{ "NES-MHROM", BOARD_TYPE_GxROM },
	{ "NES-NROM-128", BOARD_TYPE_NROM },
	{ "NES-NROM-256", BOARD_TYPE_NROM },
	{ "NES-PEEOROM", BOARD_TYPE_PxROM },
	{ "NES-PNROM", BOARD_TYPE_PxROM },
	{ "NES-RROM-128", BOARD_TYPE_NROM },
	{ "NES-SAROM", BOARD_TYPE_SxROM_WRAM },
	{ "NES-SBROM", BOARD_TYPE_SxROM },
	{ "NES-SC1ROM", BOARD_TYPE_SxROM },
	{ "NES-SCROM", BOARD_TYPE_SxROM },
	{ "NES-SEROM", BOARD_TYPE_SxROM },
	{ "NES-SF1ROM", BOARD_TYPE_SxROM },
	{ "NES-SFROM", BOARD_TYPE_SxROM },
	{ "NES-SGROM", BOARD_TYPE_SxROM },
	{ "NES-SH1ROM", BOARD_TYPE_SEROM_SHROM },
	{ "NES-SHROM", BOARD_TYPE_SEROM_SHROM },
	{ "NES-SIEPROM", BOARD_TYPE_SxROM },
	{ "NES-SJROM", BOARD_TYPE_SxROM_WRAM },
	{ "NES-SKEPROM", BOARD_TYPE_SxROM_WRAM },
	{ "NES-SKROM", BOARD_TYPE_SxROM_WRAM },
	{ "NES-SL1ROM", BOARD_TYPE_SxROM },
	{ "NES-SL2ROM", BOARD_TYPE_SxROM },
	{ "NES-SL3ROM", BOARD_TYPE_SxROM },
	{ "NES-SLROM", BOARD_TYPE_SxROM },
	{ "NES-SLRROM", BOARD_TYPE_SxROM },
	{ "NES-SNROM", BOARD_TYPE_SNROM },
	{ "NES-SNWEPROM", BOARD_TYPE_SNROM },
	{ "NES-SOROM", BOARD_TYPE_SOROM },
	{ "NES-SUROM", BOARD_TYPE_SUROM },
	{ "NES-TBROM", BOARD_TYPE_TxROM },
	{ "NES-TEROM", BOARD_TYPE_TxROM },
	{ "NES-TFROM", BOARD_TYPE_TxROM },
	{ "NES-TGROM", BOARD_TYPE_TxROM },
	{ "NES-TKEPROM", BOARD_TYPE_TxROM },
	{ "NES-TKROM", BOARD_TYPE_TxROM },
	{ "NES-TL1ROM", BOARD_TYPE_TxROM },
	{ "NES-TL2ROM", BOARD_TYPE_TxROM },
	{ "NES-TLROM", BOARD_TYPE_TxROM },
	{ "NES-TLSROM", BOARD_TYPE_TxSROM },
	{ "NES-TR1ROM", BOARD_TYPE_TVROM },
	{ "NES-TSROM", BOARD_TYPE_TxROM },
	{ "NES-TVROM", BOARD_TYPE_TVROM },
	{ "NES-UNEPROM", BOARD_TYPE_UxROM },
	{ "NES-UNROM", BOARD_TYPE_UxROM },
	{ "NES-UOROM", BOARD_TYPE_UxROM },
	{ "NR8NV1-1", BOARD_TYPE_CAMERICA_BF9093 },
	{ "NTDEC-N715061", BOARD_TYPE_CNROM },
	{ "NTDEC-N715062", BOARD_TYPE_CNROM },
	{ "NTDEC-TC-112", BOARD_TYPE_NTDEC_193 },
	{ "SACHEN-CNROM", BOARD_TYPE_CNROM_NO_CONFLICT },
	{ "SACHEN-NROM", BOARD_TYPE_NROM },
	{ "SETA-NROM-128", BOARD_TYPE_NROM },
	{ "STUDY & GAME 32-IN-1", BOARD_TYPE_BNROM },
	{ "SUNSOFT-2", BOARD_TYPE_SUNSOFT2 },
	{ "SUNSOFT-5A", BOARD_TYPE_SUNSOFT5B },
	{ "SUNSOFT-FME-7", BOARD_TYPE_SUNSOFT5B },
	{ "SUNSOFT-NROM-128", BOARD_TYPE_NROM },
	{ "SUNSOFT-NROM-256", BOARD_TYPE_NROM },
	{ "TAITO-74*139/74", BOARD_TYPE_74x139_74 },
	{ "TAITO-74*161/161/32", BOARD_TYPE_74x161_161_32 },
	{ "TAITO-CNROM", BOARD_TYPE_CNROM },
	{ "TAITO-NROM-128", BOARD_TYPE_NROM },
	{ "TAITO-NROM-256", BOARD_TYPE_NROM },
	{ "TAITO-TC0190FMC", BOARD_TYPE_TAITO_TC0190FMC },
	{ "TAITO-TC0350FMR", BOARD_TYPE_TAITO_TC0190FMC },
	{ "TAITO-UNROM", BOARD_TYPE_UxROM },
	{ "TENGEN-800002", BOARD_TYPE_NAMCO_108 },
	{ "TENGEN-800003", BOARD_TYPE_NROM },
	{ "TENGEN-800004", BOARD_TYPE_TENGEN_800004 },
	{ "TENGEN-800008", BOARD_TYPE_CNROM },
	{ "TENGEN-800030", BOARD_TYPE_NAMCO_108 },
	{ "TENGEN-800042", BOARD_TYPE_SUNSOFT4 },
	{ "TQROM", BOARD_TYPE_TQROM },
	{ "TVROM", BOARD_TYPE_TVROM },
	{ "TXC-PT8154", BOARD_TYPE_TXC_TW },
	{ "TXC-74*138/175", BOARD_TYPE_NINA_03_06 },
	{ "UNL-SACHEN-8259A", BOARD_TYPE_SACHEN_8259A },
	{ "UNL-SACHEN-8259B", BOARD_TYPE_SACHEN_8259B },
	{ "UNL-SACHEN-8259C", BOARD_TYPE_SACHEN_8259C },
	{ "UNL-SACHEN-8259D", BOARD_TYPE_SACHEN_8259D },
	{ "UNL-SACHEN-74LS374N", BOARD_TYPE_SACHEN_74_374B },
	{ "UNL-SA-016-1M", BOARD_TYPE_NINA_03_06 },
	{ "UNL-SA-0036", BOARD_TYPE_SACHEN_SA0036 },
	{ "UNROM-512-8", BOARD_TYPE_UNROM_512_8 },
	{ "UNROM-512-16", BOARD_TYPE_UNROM_512_16 },
	{ "UNROM-512-32", BOARD_TYPE_UNROM_512_32 },
	{ "VIRGIN-SNROM", BOARD_TYPE_SNROM },
};

static struct board_fixup fixup_list[] = {
	{ BOARD_TYPE_HROM, BOARD_TYPE_NROM, MIRROR_V,
	  { 0, 0 }, { 0, 0 } },
	{ BOARD_TYPE_TVROM, BOARD_TYPE_TxROM, MIRROR_4,
	  { 0, 0 }, { 0, 0 } },
	{ BOARD_TYPE_DRROM, BOARD_TYPE_NAMCO_108, MIRROR_4,
	  { 0, 0 }, { 0, 0 } },
	{ BOARD_TYPE_TENGEN_800004, BOARD_TYPE_NAMCO_108, MIRROR_4,
	  { 0, 0 }, { 0, 0 } },
	{ BOARD_TYPE_SxROM_WRAM, BOARD_TYPE_SxROM, MIRROR_UNDEF,
	  { SIZE_8K , 0 }, { 0, 0 } },
	{ BOARD_TYPE_TxROM_WRAM, BOARD_TYPE_TxROM, MIRROR_UNDEF,
	  { SIZE_8K , 0 }, { 0, 0 } },
	{ BOARD_TYPE_FxROM_WRAM, BOARD_TYPE_FxROM, MIRROR_UNDEF,
	  { SIZE_8K , 0 }, { 0, 0 } },
	{ BOARD_TYPE_EKROM, BOARD_TYPE_ExROM, MIRROR_UNDEF,
	  { SIZE_8K, 0 }, { 0, 0 } },
	{ BOARD_TYPE_ETROM, BOARD_TYPE_ExROM, MIRROR_UNDEF,
	  { SIZE_8K, SIZE_8K }, { 0, 0 } },
	{ BOARD_TYPE_EWROM, BOARD_TYPE_ExROM, MIRROR_UNDEF,
	  { SIZE_32K, 0 }, { 0, 0 } },
	{ BOARD_TYPE_NAMCO_CNROM_WRAM, BOARD_TYPE_CNROM, MIRROR_UNDEF,
	  { SIZE_2K, 0 }, { 0, 0 } },
	{ BOARD_TYPE_UNROM_512_8, BOARD_TYPE_UNROM_512, MIRROR_UNDEF,
	  { 0, 0 }, { SIZE_8K, 0 } },
	{ BOARD_TYPE_UNROM_512_16, BOARD_TYPE_UNROM_512, MIRROR_UNDEF,
	  { 0, 0 }, { SIZE_16K, 0 } },
	{ BOARD_TYPE_UNROM_512_32, BOARD_TYPE_UNROM_512, MIRROR_UNDEF,
	  { 0, 0 }, { SIZE_32K, 0 } },
};

static struct state_item board_state_items[] = {
	STATE_8BIT(board, prg_mode),
	STATE_8BIT(board, chr_mode),
	STATE_8BIT(board, irq_control),

	/* FIXME check types */
	STATE_32BIT(board, prg_and),
	STATE_32BIT(board, prg_or),
	STATE_32BIT(board, chr_and),
	STATE_32BIT(board, chr_or),
	STATE_32BIT(board, wram_and),
	STATE_32BIT(board, wram_or),

	STATE_32BIT(board, irq_counter),
	STATE_32BIT(board, irq_counter_reload),
	STATE_32BIT(board, irq_counter_timestamp),

	STATE_8BIT_ARRAY(board, data),
	STATE_32BIT_ARRAY(board, timestamps),

	STATE_8BIT(board, dip_switches),
	STATE_ITEM_END(),
};

static struct state_item bank_state_items[] = {
	STATE_32BIT(bank, bank),
	STATE_32BIT(bank, shift), /* FIXME check type */
	STATE_16BIT(bank, size),
	STATE_16BIT(bank, address),
	STATE_8BIT(bank, perms),
	STATE_8BIT(bank, type),
	STATE_ITEM_END(),
};

static struct state_item nmt_bank_state_items[] = {
	STATE_32BIT(nmt_bank, bank),
	STATE_8BIT(nmt_bank, perms),
	STATE_8BIT(nmt_bank, type),
	STATE_ITEM_END(),
};

static uint8_t zero_nmt[1024];

static int get_bank_list_count(struct bank *list);
static CPU_WRITE_HANDLER(blargg_wram_hook);
static cpu_write_handler_t *orig_wram_write_handler = NULL;
extern void blargg_reset_request(struct emu *emu);
static void board_internal_nmt_sync(struct board *board);
void board_nmt_sync(struct board *board);

static void board_info_apply_sanity_checks(struct rom *rom, int battery)
{
	struct board_info *info;
	size_t max, min;
	int i;
	int mask, flags;

	info = rom->board_info;
	flags = board_info_get_flags(info);

	if (battery) {
		mask  = BOARD_INFO_FLAG_WRAM0_NV;
		mask |= BOARD_INFO_FLAG_WRAM1_NV;
		mask |= BOARD_INFO_FLAG_VRAM0_NV;
		mask |= BOARD_INFO_FLAG_VRAM1_NV;
		mask |= BOARD_INFO_FLAG_MAPPER_NV;

		if (flags & BOARD_INFO_FLAG_WRAM0_NV)
			rom->info.flags |= ROM_FLAG_WRAM0_NV;

		if (flags & BOARD_INFO_FLAG_WRAM1_NV)
			rom->info.flags |= ROM_FLAG_WRAM1_NV;

		if (flags & BOARD_INFO_FLAG_VRAM0_NV)
			rom->info.flags |= ROM_FLAG_VRAM0_NV;

		if (flags & BOARD_INFO_FLAG_VRAM1_NV)
			rom->info.flags |= ROM_FLAG_VRAM1_NV;

		if (flags & BOARD_INFO_FLAG_MAPPER_NV)
			rom->info.flags |= ROM_FLAG_MAPPER_NV;

		/* If no flags have been specified, but the board has a
		   max size for WRAM0 and doesn't allow WRAM1, then we
		   assume WRAM0 is 8K, non-volatile.
		*/
		if (!system_type_is_vs(rom->info.system_type) && !(flags & mask)) {
			size_t wram1_max;
			size_t wram0_max;

			wram0_max = info->max_wram_size[0];
			wram1_max = info->max_wram_size[1];

			if (wram0_max && !wram1_max) {
				size_t wram_size;
				
				wram_size = rom->info.wram_size[0];
				if (!wram_size)
					wram_size = SIZE_8K;

				if (wram_size > wram0_max)
					wram_size = wram0_max;

				rom->info.wram_size[0] = wram_size;
				rom->info.flags |= ROM_FLAG_WRAM0_NV;
			}
		}

		if (system_type_is_vs(rom->info.system_type)) {
			if (rom->info.wram_size[0] < SIZE_2K)
				rom->info.wram_size[0] = SIZE_2K;
			rom->info.flags |= ROM_FLAG_WRAM0_NV;
			rom->info.mirroring = MIRROR_4;
		}
	}

	for (i = 0; i < 2; i++) {
		if (rom->info.wram_size[i] < info->min_wram_size[i]) {
			rom->info.wram_size[i] = info->min_wram_size[i];
		}
		if (rom->info.wram_size[i] > info->max_wram_size[i]) {
			rom->info.wram_size[i] = info->max_wram_size[i];
		}
	}

	min = info->min_vram_size[0];
	max = info->max_vram_size[0];

	if (!rom->info.total_chr_size) {
		if (!max)
			max = info->max_chr_rom_size;

		if (!min)
			min = SIZE_8K;
	}

	if (rom->info.vram_size[0] < min) {
		rom->info.vram_size[0] = min;
	}

	if (rom->info.vram_size[0] > max) {
		rom->info.vram_size[0] = max;
	}

	if (rom->info.vram_size[1] < info->min_vram_size[1]) {
		rom->info.vram_size[1] = info->min_vram_size[1];
	}
	if (rom->info.vram_size[1] > info->max_vram_size[1]) {
		rom->info.vram_size[1] = info->max_vram_size[1];
	}
}

static void board_apply_fixups(struct rom *rom, int battery)
{
	int i, count;

	count = sizeof(fixup_list) / sizeof(fixup_list[0]);
	for (i = 0; i < count; i++) {
		if (fixup_list[i].type == rom->info.board_type)
			break;
	}

	if (i < count) {
		rom->info.board_type = fixup_list[i].real_type;
		if ((fixup_list[i].mirroring != MIRROR_UNDEF) &&
		    (rom->info.mirroring == MIRROR_UNDEF)) {
			rom->info.mirroring = fixup_list[i].mirroring;
		}

		if (fixup_list[i].wram_size[0])
			rom->info.wram_size[0] = fixup_list[i].wram_size[0];

		if (fixup_list[i].wram_size[1])
			rom->info.wram_size[1] = fixup_list[i].wram_size[1];

		if (fixup_list[i].vram_size[0])
			rom->info.vram_size[0] = fixup_list[i].vram_size[0];

		if (fixup_list[i].vram_size[1])
			rom->info.vram_size[1] = fixup_list[i].vram_size[1];
	}

	switch (rom->info.board_type) {
	case BOARD_TYPE_UNROM_512:
		if (battery) {
			rom->info.board_type = BOARD_TYPE_UNROM_512_FLASH;
			rom->info.flags |= ROM_FLAG_MAPPER_NV;
		}
		break;
	case BOARD_TYPE_SUNSOFT1:
		if (rom->info.total_prg_size > SIZE_32K) {
			rom->info.board_type = BOARD_TYPE_SUNSOFT3R;
		}
		break;
	case BOARD_TYPE_SUNSOFT2:
		if ((rom->info.mirroring != MIRROR_M) &&
		    (rom->info.mirroring != MIRROR_UNDEF)) {
			rom->info.board_type = BOARD_TYPE_SUNSOFT3R;
		}
		break;
	}

	/* Miscellaneous fixups here */
	if ((rom->info.board_type == BOARD_TYPE_UNROM_512) && battery) {
		rom->info.board_type = BOARD_TYPE_UNROM_512_FLASH;
		rom->info.flags |= ROM_FLAG_MAPPER_NV;
	}
}

struct board_info *board_info_find_by_type(uint32_t type)
{
	int i;
	for (i = 0; board_info_list[i]; i++) {
		if (board_info_list[i]->board_type == type) {
			return board_info_list[i];
		}
	}

	return NULL;
}

int board_set_rom_board_type(struct rom *rom, uint32_t type, int battery)
{
	struct board_info *info;

	rom->info.board_type = type;

	info = NULL;

	board_apply_fixups(rom, battery);
	info = board_info_find_by_type(rom->info.board_type);
	if (!info)
		return -1;
	
	rom->board_info = info;
	board_info_apply_sanity_checks(rom, battery);

	return 0;
}

int board_set_rom_board_type_by_name(struct rom *rom, const char *name, int battery)
{
	int i;
	int count;
	struct board_info *info;

	count = sizeof(mapping_list) / sizeof(mapping_list[0]);

	for (i = 0; i < count; i++) {
		if (strcasecmp(name, mapping_list[i].name) == 0) {
			rom->info.board_type = mapping_list[i].type;
			break;
		}
	}

	if (i < count) {
		board_apply_fixups(rom, battery);
		info = board_info_find_by_type(rom->info.board_type);
	} else {
		info = NULL;
		for (i = 0; board_info_list[i]; i++) {
			if (strcmp(name, board_info_list[i]->name) == 0) {
				info = board_info_list[i];
				break;
			}
		}

		if (info) {
			rom->info.board_type = info->board_type;
			rom->board_info = info;
		}
	}

	if (!info)
		return -1;

	rom->board_info = info;
	board_info_apply_sanity_checks(rom, battery);

	return 0;
}

uint32_t board_find_type_by_name(const char *name)
{
	int i;
	int count;
	uint32_t type;

	count = sizeof(mapping_list) / sizeof(mapping_list[0]);

	for (i = 0; i < count; i++) {
		if (strcasecmp(name, mapping_list[i].name) == 0) {
			type = mapping_list[i].type;
			return type;
		}
	}

	for (i = 0; board_info_list[i]; i++) {
		if (strcmp(name, board_info_list[i]->name) == 0) {
			return board_info_list[i]->board_type;
		}
	}

	return BOARD_TYPE_UNKNOWN;
}

uint32_t board_info_get_type(struct board_info *info)
{
	if (info)
		return info->board_type;

	return BOARD_TYPE_UNKNOWN;
}

const char *board_info_get_name(struct board_info *info)
{
	if (info)
		return info->name;

	return NULL;
}

int board_info_get_flags(struct board_info *info)
{
	if (info)
		return info->flags;
	else
		return -1;
}

void board_init_mapper_ram(struct board *board, int nv)
{
	if (board->info->mapper_ram_size)
		board->mapper_ram.size = board->info->mapper_ram_size;

	if (nv)
		board->mapper_ram.type = CHIP_TYPE_MAPPER_RAM_NV;
	else
		board->mapper_ram.type = CHIP_TYPE_MAPPER_RAM;

}

int board_set_type(struct board *board, uint32_t board_type)
{
	struct board_info *info;

	info = board_info_find_by_type(board_type);

	if (!info)
		return 1;

	if (info->mapper_name)
		log_dbg("Board: %s (%s)\n", info->name, info->mapper_name);
	else
		log_dbg("Board: %s\n", info->name);

	//memset(&board, 0, sizeof(board));
	board->info = info;
	board->mirroring = MIRROR_UNDEF;

	return 0;
}

void board_set_rom_data(struct board *board, uint8_t * data,
			int prg_rom_offset, int prg_rom_size,
			int chr_rom_offset, int chr_rom_size)
{
	board->prg_rom.data = data + prg_rom_offset;
	board->prg_rom.size = prg_rom_size;

	if (chr_rom_size > 0) {
		board->chr_rom.data = data + chr_rom_offset;
		board->chr_rom.size = chr_rom_size;
	}
}

#if defined __unix__

static int board_memmap_nvram(struct board *board, size_t size,
			      const char *file)
{
	off_t file_size;
	int count;
	long page_size;
	char c;

	board->nv_ram_fd = -1;

	if (size <= 0)
		return -1;

	if (!board->use_mmap)
		return -1;

	if (create_directory(file, 1, 1))
		return -1;

	board->nv_ram_fd = open(file, O_RDWR | O_CREAT, 0600);
	if (board->nv_ram_fd < 0)
		goto err;

	file_size = lseek(board->nv_ram_fd, 0, SEEK_END);
	if (file_size < 0)
		goto err;

	board->save_file_size = file_size;

	file_size = lseek(board->nv_ram_fd, size - 1, SEEK_SET);
	if (file_size < 0)
		goto err;

	c = 0;
	errno = 0;
	while ((count = write(board->nv_ram_fd, &c, 1)) < 1) {
		if ((count < 0) && (errno != EAGAIN))
			goto err;
	}

	board->nv_ram_data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED,
				 board->nv_ram_fd, 0);
	if (!board->nv_ram_data)
		goto err;

	board->nv_ram_size = size;

	/* Populate the cache now by forcing a page fault on
	   each page.  Could pass MAP_POPULATE on Linux, but
	   this is more portable.
	*/
	page_size = sysconf(_SC_PAGE_SIZE);
	while ((page_size > 0) && (size > 0)) {
		c = board->nv_ram_data[size - 1];
		if (size < page_size)
			break;
		size -= page_size;
	}

	return 0;

err:
	if (board->nv_ram_fd >= 0) {
		close(board->nv_ram_fd);
		board->nv_ram_fd = -1;
	}

	return -1;
}

#elif defined _WIN32

static int board_memmap_nvram(struct board *board, size_t size,
			      const char *file)
{
	HANDLE hFile;
	HANDLE hMapFile;
	LPVOID lpMapAddress;
	LARGE_INTEGER file_size, desired_size;
	LPTSTR tstr;

	hFile = NULL;
	hMapFile = NULL;
	lpMapAddress = NULL;

	if (!board->use_mmap)
		return -1;

	if (size <= 0)
		return -1;

	if (create_directory(file, 1, 1))
		return -1;

#if UNICODE
	/* FIXME find some way to *NOT* directly use SDL here */
	tstr = (TCHAR *)SDL_iconv_string("UTF-16LE", "UTF-8", (char *)(file),
	                                 strlen(file)+1);
#else
	tstr = strdup(file);
#endif
	hFile = CreateFile(tstr, GENERIC_READ | GENERIC_WRITE, 0, NULL,
			   OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	
	if (hFile == INVALID_HANDLE_VALUE)
		goto err;

	if (!GetFileSizeEx(hFile, &file_size))
		goto err;

	board->save_file_size = file_size.QuadPart;
	desired_size.QuadPart = size;

	if (SetFilePointerEx(hFile, desired_size, NULL, FILE_BEGIN) ==
	    INVALID_SET_FILE_POINTER) {
	    	goto err;
	}

	if (!SetEndOfFile(hFile)) 
		goto err;

	hMapFile = CreateFileMapping(hFile, NULL, PAGE_READWRITE, 0, size,
	                             NULL);

	if (!hMapFile)
		goto err;

	lpMapAddress = MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, size);

	if (!lpMapAddress)
		goto err;

#if UNICODE
	SDL_free(tstr);
#else
	free(tstr);
#endif

	board->nv_ram_size = size;
	board->nv_ram_handle = hFile;
	board->nv_ram_map_handle = hMapFile;
	board->nv_ram_data = lpMapAddress;

	return 0;

err:
	if (tstr) {
#if UNICODE
		SDL_free(tstr);
#else
		free(tstr);
#endif
	}

	if (hMapFile)
		CloseHandle(hMapFile);

	if (hFile)
		CloseHandle(hFile);

	return -1;
}

#else

static int board_memmap_nvram(struct board *board, size_t size,
			      const char *file)
{
	return -1;
}

#endif

static int board_init_nvram(struct board *board, size_t size)
{
	char *save_file, *romdir_save_file, *file;
	int len;
	int rc;

	rc = -1;

	len = strlen(board->emu->rom_path) + 1 +
		strlen(board->emu->save_file) + 1;
	romdir_save_file = malloc(len);

	if (romdir_save_file) {
		snprintf(romdir_save_file, len, "%s%c%s",
			 board->emu->rom_path,
			 PATHSEP[0], board->emu->save_file);
	}

	save_file = config_get_path(board->emu->config,
					CONFIG_DATA_DIR_SAVE,
					board->emu->save_file, 1);

	if (!romdir_save_file || !save_file)
		return -1;

	file = romdir_save_file;
	if (!check_file_exists(romdir_save_file)) {
		if (!board->emu->config->save_uses_romdir)
			file = save_file;
	}

	if (board_memmap_nvram(board, size, file) >= 0) {
		rc = 0;
		goto cleanup;
	}

	if (!size)
		goto cleanup;

	board->nv_ram_data = malloc(size);

	if (!board->nv_ram_data)
		goto cleanup;

	board->nv_ram_size = size;
	memset(board->nv_ram_data, 0x00, board->nv_ram_size);

	if (check_file_exists(file)) {
		size = get_file_size(file);
		board->save_file_size = size;
	} else {
		size = 0;
	}
	
	if ((size > 0) &&
	    readfile(file, board->nv_ram_data,
		     board->nv_ram_size)) {
		goto cleanup;
	}
	board->save_file_size = 0;

	rc = 0;

cleanup:
	free(romdir_save_file);
	free(save_file);

	return rc;
}

static int board_init_ram(struct board *board, size_t size)
{
	board->ram_data = NULL;
	board->ram_size = 0;

	if (!size)
		return 0;

	board->ram_data = malloc(size);
	if (!board->ram_data)
		return -1;

	board->ram_size = size;

	return 0;
}

static int board_init_ram_chips(struct board *board)
{
	int i;
	size_t total_nonvolatile_size;
	size_t total_volatile_size;
	uint8_t *ram, *nv_ram;

	total_nonvolatile_size = 0;
	total_volatile_size = 0;

	if (board->mapper_ram.type == CHIP_TYPE_MAPPER_RAM_NV)
		total_nonvolatile_size += board->mapper_ram.size;
	else
		total_volatile_size += board->mapper_ram.size;

	for (i = 0; i < 2; i++) {
		if (board->wram[i].type == CHIP_TYPE_WRAM_NV)
			total_nonvolatile_size += board->wram[i].size;
		else
			total_volatile_size += board->wram[i].size;
	}

	for (i = 0; i < 2; i++) {
		if (board->vram[i].type == CHIP_TYPE_VRAM_NV)
			total_nonvolatile_size += board->vram[i].size;
		else
			total_volatile_size += board->vram[i].size;
	}

	board_init_ram(board, total_volatile_size);
	board_init_nvram(board, total_nonvolatile_size);

	/* Any board-specific post-load functions may only change
	   the contents pointed to by nv_ram_data.  Freeing or
	   resizing the buffer is not allowed.
	*/

	if (board->info->funcs && board->info->funcs->nvram_post_load)
		board->info->funcs->nvram_post_load(board);

	/* Set up the pointers for all wram and vram
	   chips to point to our newly-allocated/mmapped
	   data.
	*/
	ram = board->ram_data;
	nv_ram = board->nv_ram_data;

	for (i = 0; i < 2; i++) {
		if (!board->wram[i].size)
			continue;

		if (board->wram[i].type == CHIP_TYPE_WRAM_NV) {
			board->wram[i].data = nv_ram;
			nv_ram += board->wram[i].size;
		} else {
			board->wram[i].data = ram;
			ram += board->wram[i].size;

		}
	}

	for (i = 0; i < 2; i++) {
		if (!board->vram[i].size)
			continue;

		if (board->vram[i].type == CHIP_TYPE_VRAM_NV) {
			board->vram[i].data = nv_ram;
			nv_ram += board->vram[i].size;
		} else {
			board->vram[i].data = ram;
			ram += board->vram[i].size;
		}
	}

	if (board->mapper_ram.type == CHIP_TYPE_MAPPER_RAM_NV) {
		board->mapper_ram.data = nv_ram;
		nv_ram += board->mapper_ram.size;
	} else {
		board->mapper_ram.data = ram;
		ram += board->mapper_ram.size;
	}

	return 0;
}

static int board_init_wram(struct board *board, int chip, int size, int nv)
{
	if ((chip < 0 || chip > 1) || size < 0) {
		return -1;
	}

	if (!size)
		return 0;

	board->wram[chip].size = size;
	if (nv)
		board->wram[chip].type = CHIP_TYPE_WRAM_NV;
	else
		board->wram[chip].type = CHIP_TYPE_WRAM;

	return 0;
}

static int board_init_vram(struct board *board, int chip, int size, int nv)
{
	if ((chip < 0 || chip > 1) || size < 0) {
		return -1;
	}

	if (!size)
		return 0;

	board->vram[chip].size = size;
	if (nv)
		board->vram[chip].type = CHIP_TYPE_VRAM_NV;
	else
		board->vram[chip].type = CHIP_TYPE_VRAM;

	return 0;
}

int board_set_mirroring(struct board *board, int type)
{
	board->mirroring = type;

	return 1;
}

#if defined __unix__

static int board_memunmap_nvram(struct board *board)
{
	int rc;

	if (board->nv_ram_fd < 0)
		return -1;

	munmap(board->nv_ram_data,
	       board->nv_ram_size);
	rc = ftruncate(board->nv_ram_fd, board->save_file_size);
	close(board->nv_ram_fd);
	board->nv_ram_fd = -1;
	board->nv_ram_data = 0;
	board->nv_ram_size = 0;

	return rc;
}

#elif defined _WIN32

static int board_memunmap_nvram(struct board *board)
{
	LARGE_INTEGER new_size;

	UnmapViewOfFile(board->nv_ram_data);
	CloseHandle(board->nv_ram_map_handle);
	board->nv_ram_map_handle = NULL;

	new_size.QuadPart = board->save_file_size;
	SetFilePointerEx(board->nv_ram_handle, new_size, NULL, FILE_BEGIN);
	SetEndOfFile(board->nv_ram_handle);

	CloseHandle(board->nv_ram_handle);
	board->nv_ram_handle = NULL;

	board->nv_ram_data = NULL;
	board->nv_ram_size = 0;

	return 0;
}

#else

static int board_memunmap_nvram(struct board *board)
{
	return -1;
}

#endif

static void board_cleanup_nvram(struct board *board)
{
	char *save_file, *romdir_save_file, *file;
	int len;

	if (!board->nv_ram_data)
		return;

	/* If the board defines a pre-save hook, that hook needs to
	   make sure that nv_ram_data points to valid data as it
	   should appear on disk, with save_file_size set
	   appropriately.  Note that the board should NOT attempt to
	   free nv_ram_data or resize it, if it is already set.
	   Changing the data is fair game, however.
	   
	   Otherwise, the board is free to allocate memory and set
	   nv_ram_data to point to it, provided it also sets
	   save_file_size.  The memory nv_ram_data points to will
	   be free()d though, so it must be dynamically allocated.
	   
	   If the board defines no pre-save hook, nv_ram_data is used
	   as-is, and the board's nv_ram_size is used as the file
	   size.
	*/
	if (board->info->funcs && board->info->funcs->nvram_pre_save)
		board->info->funcs->nvram_pre_save(board);
	else
		board->save_file_size = board->nv_ram_size;

	if (board_memunmap_nvram(board) >= 0)
		return;

	len = strlen(board->emu->rom_path) + 1 +
		strlen(board->emu->save_file) + 1;
	romdir_save_file = malloc(len);

	if (romdir_save_file) {
		snprintf(romdir_save_file, len, "%s%c%s",
			 board->emu->rom_path,
			 PATHSEP[0], board->emu->save_file);
	} else {
		return;
	}

	save_file = config_get_path(board->emu->config,
					CONFIG_DATA_DIR_SAVE,
					board->emu->save_file, 1);

	if (!save_file) {
		free(romdir_save_file);
		return;
	}

	file = romdir_save_file;
	if (!check_file_exists(file)) {
		if (!board->emu->config->save_uses_romdir)
			file = save_file;
	}

	/* printf("file: %s size: %zu data: %p\n", */
	/*        file, board->save_file_size, board->nv_ram_data); */

	if (writefile(file, board->nv_ram_data,
		      board->save_file_size)) {
		log_err("failed to write file \"%s\"\n",
			file);
	}

	free(save_file);
	free(romdir_save_file);
	free(board->nv_ram_data);

	board->nv_ram_data = NULL;
	board->nv_ram_size = 0;
}

static void board_cleanup_ram(struct board *board)
{
	if (!board->ram_data)
		return;
	
	free(board->ram_data);
	board->ram_data = NULL;
	board->ram_size = 0;
}

static void board_cleanup_ram_chips(struct board *board)
{

	board_cleanup_ram(board);
	board_cleanup_nvram(board);
}

void board_init_dipswitches(struct board *board)
{
	int i;
	struct config *config;

	config = board->emu->config;
	board->dip_switches = 0;

	for (i = 0; i < 8; i++)
		board->dip_switches |= config->dip_switch[i] << i;
}

static int board_apply_ips_save(struct board *board)
{
	uint8_t *p;
	uint8_t *data_to_patch;
	ssize_t size;
	size_t data_size;
	off_t offset;
	char *save_file;
	int rc;

	data_to_patch = NULL;

	if (!board->info)
		return -1;

	if (board->info->flags & BOARD_INFO_FLAG_PRG_IPS) {
		data_to_patch = board->emu->rom->buffer;
		data_size = board->emu->rom->buffer_size;
		offset = board->emu->rom->offset;
	}

	if (!data_to_patch)
		return 0;

	p = NULL;
	rc = -1;

	save_file = config_get_path(board->emu->config,
					CONFIG_DATA_DIR_SAVE,
					board->emu->save_file, 1);

	if (!save_file)
		goto done;

	if (!check_file_exists(save_file))
		goto done;

	size = get_file_size(save_file);
	if (size < 0) {
		goto done;
	} else if (size == 0) {
		rc = 0;
		goto done;
	}

	p = malloc(size);
	if (!p)
		goto done;

	printf("Save file: %s\n", save_file);
	if (readfile(save_file, p, size)) {
		goto done;
	}

	if (patch_apply_ips(&data_to_patch, &data_size, offset, offset,
				 p, size,
				 &board->modified_ranges) != 0) {

		err_message("failed to apply IPS save file %s\n",
		            save_file);
		goto done;
	}

	board->emu->rom->buffer = data_to_patch;
	board->emu->rom->buffer_size = data_size;
	board->prg_rom.data = data_to_patch + offset;

	rc = 0;

done:

	if (save_file)
		free(save_file);

	if (p)
		free(p);

	return rc;
}

int board_init(struct emu *emu, struct rom *rom)
{
	struct board *board;
	struct board_info *info;
	size_t prg_size, chr_size;
	int i;

	if (!rom)
		return 1;

	board = malloc(sizeof(*board));
	if (!board)
		return 1;

	memset(board, 0, sizeof(*board));
	emu->board = board;
	board->emu = emu;

	board->ram_data = NULL;
	board->ram_size = 0;
	board->nv_ram_data = NULL;
	board->nv_ram_size = 0;
#if defined __unix__
	board->nv_ram_fd = -1;
#elif defined _WIN32
	board->nv_ram_handle = NULL;
	board->nv_ram_map_handle = NULL;
#endif
	board->use_mmap = board->emu->config->nvram_uses_memmap;

	prg_size  = rom->info.total_prg_size;
	chr_size  = rom->info.total_chr_size;

	board_set_rom_data(board, rom->buffer, rom->offset, prg_size,
			   rom->offset + prg_size, chr_size);
	board_set_type(board, rom->info.board_type);

	/* If mirroring wasn't defined by the ROM header (or database),
	   define it here as mapper-controlled if possible, vertical
	   otherwise.
	*/
   
	if (rom->info.mirroring == MIRROR_UNDEF) {
		if (board->info->flags & BOARD_INFO_FLAG_MIRROR_M) {
			rom->info.mirroring = MIRROR_M;
		} else {
			rom->info.mirroring = MIRROR_V;
		}
	}

	board_set_mirroring(board, rom->info.mirroring);
	board_init_mapper_ram(board, rom->info.flags & ROM_FLAG_MAPPER_NV);

	board_init_wram(board, 0, rom->info.wram_size[0],
			rom->info.flags & ROM_FLAG_WRAM0_NV);
	board_init_wram(board, 1, rom->info.wram_size[1],
			rom->info.flags & ROM_FLAG_WRAM1_NV);
	board_init_vram(board, 0, rom->info.vram_size[0],
			rom->info.flags & ROM_FLAG_VRAM0_NV);
	board_init_vram(board, 1, rom->info.vram_size[1],
			rom->info.flags & ROM_FLAG_VRAM1_NV);

	info = board->info;

	/* Shouldn't be necessary, but just in case... */
	memset(zero_nmt, 0, sizeof(zero_nmt));

	if (board->prg_rom.data && board->prg_rom.size > info->max_prg_rom_size) {
		log_err("board_init: WARNING "
			"PRG-ROM size %zu greater than allowed for %s (%zu)\n",
			board->prg_rom.size, info->name,
			info->max_prg_rom_size);
	}

	if (board->chr_rom.data && board->chr_rom.size > info->max_chr_rom_size) {
		log_err("board_init: WARNING "
			"CHR-ROM size %zu greater than allowed for %s (%zu)\n",
			board->chr_rom.size, info->name,
			info->max_chr_rom_size);
	}

	if (board->mirroring == MIRROR_4)
		board->ciram.size = SIZE_4K;
	else
		board->ciram.size = SIZE_2K;

	board->ciram.data = malloc(board->ciram.size);
	if (!board->ciram.data)
		return 1;

	board->ciram.type = CHIP_TYPE_CIRAM;

	board_init_ram_chips(board);
	emu_load_cheat(board->emu);
	board_init_dipswitches(board);
	board_apply_ips_save(board);

	board->prg_and = ~0;
	board->chr_and = ~0;

	/* Set up the read/write handlers for the board */

       for (i = 0; info->read_handlers && info->read_handlers[i].handler; i++) {
		int addr = info->read_handlers[i].addr;
		size_t size = info->read_handlers[i].size;
		int mask = info->read_handlers[i].mask;

		cpu_set_read_handler(board->emu->cpu, addr, size, mask,
				     info->read_handlers[i].handler);
	}

	for (i = 0; info->write_handlers &&
	     info->write_handlers[i].handler; i++) {
		int addr = info->write_handlers[i].addr;
		size_t size = info->write_handlers[i].size;
		int mask = info->write_handlers[i].mask;

		cpu_set_write_handler(board->emu->cpu, addr, size, mask,
				      info->write_handlers[i].handler);
	}

	/* Init the timer before the board in case the board init code
	   needs to tweak timer settings.
	*/
	   
	if (info->flags & BOARD_INFO_FLAG_M2_TIMER)
		m2_timer_init(board->emu);

	if (info->funcs && info->funcs->init) {
		return info->funcs->init(board);
	}

	return 0;
}

void board_run(struct board *board, uint32_t cycles)
{
	if (board->emu->overclocking)
		return;

	if (board->info->funcs && board->info->funcs->run)
		board->info->funcs->run(board, cycles);

	if (board->emu->m2_timer)
		m2_timer_run(board->emu->m2_timer, cycles);
}

int board_apply_config(struct board *board)
{
	board_init_dipswitches(board);

	if (board->info->funcs &&
	    board->info->funcs->apply_config) {
		return board->info->funcs->apply_config(board);
	}

	return 0;
}

void board_reset(struct board *board, int hard)
{
	struct board_info *info;
	int count;

	if (!board->info)
		return;

	info = board->info;

	if (hard && info->init_prg) {
		count = get_bank_list_count(info->init_prg);
		memset(board->prg_banks, 0,
		       sizeof(board->prg_banks));
		memcpy(board->prg_banks,
		       info->init_prg, count * sizeof(struct bank));
	}

	if (hard && info->init_chr0) {
		count = get_bank_list_count(info->init_chr0);
		memset(board->chr_banks0, 0,
		       sizeof(board->chr_banks0));
		memcpy(board->chr_banks0,
		       info->init_chr0, count * sizeof(struct bank));
	}

	if (hard && info->init_chr1) {
		count = get_bank_list_count(info->init_chr1);
		memset(board->chr_banks1, 0,
		       sizeof(board->chr_banks1));
		memcpy(board->chr_banks1,
		       info->init_chr1, count * sizeof(struct bank));
	}

	if (hard) {
		if (board->ram_size)
			memset(board->ram_data, 0x00, board->ram_size);

		memset(board->ciram.data, 0xff, board->ciram.size);
	}

	if (hard) {
		board->prg_and = ~0;
		board->prg_or = 0;
		board->wram_and = ~0;
		board->wram_or = 0;
		board->chr_and = ~0;
		board->chr_or = 0;
	}

	if (hard) {
		int mirroring;

		if (board->mirroring == MIRROR_M) {
			if (board->info->mirroring_values)
				mirroring = board->info->mirroring_values[0];
			else
				mirroring = MIRROR_1A;
		} else {
			mirroring = board->mirroring;
		}

		board_set_ppu_mirroring(board, mirroring);
		board_internal_nmt_sync(board);
	}

	if (board->emu->m2_timer)
		m2_timer_reset(board->emu->m2_timer, hard);

	if (board->info->funcs && board->info->funcs->reset)
		board->info->funcs->reset(board, hard);

	if (hard) {
		board_prg_sync(board);
		board_chr_sync(board, 0);
		board_chr_sync(board, 1);
		ppu_select_bg_pagemap(board->emu->ppu, 0, 0);
		ppu_select_spr_pagemap(board->emu->ppu, 0, 0);
	}

	if (board->emu->config->blargg_test_rom_hack_enabled) {
		int i;
		orig_wram_write_handler =
		    cpu_get_write_handler(board->emu->cpu, 0x6000);
		if (orig_wram_write_handler != blargg_wram_hook) {
			for (i = 0x6000; i < 0x8000; i++) {
				cpu_set_write_handler(board->emu->cpu,
						      i, 1, 0,
						      blargg_wram_hook);
			}
		} else {
			orig_wram_write_handler = NULL;
		}
	}
}

void board_write_ips_save(struct board *board, struct range_list *range_list)
{
	uint8_t *p;
	uint8_t *data_to_patch;
	char *save_file;
	uint32_t size;

	if (range_list == NULL)
		range_list = board->modified_ranges;

	data_to_patch = NULL;

	if (!board->info)
		return;

	if (board->info->flags & BOARD_INFO_FLAG_PRG_IPS)
		data_to_patch = board->prg_rom.data;

	if (!data_to_patch)
		return;

	p = NULL;

	save_file = config_get_path(board->emu->config,
					CONFIG_DATA_DIR_SAVE,
					board->emu->save_file, 1);

	if (!save_file)
		return;

	size = patch_create_ips(data_to_patch, range_list,
				&p);

	if (size && writefile(save_file, p, size)) {
		log_err("failed to write IPS save file \"%s\"\n",
			save_file);
	}

	if (p)
		free(p);

	if (save_file)
		free(save_file);
}

void board_cleanup(struct board *board)
{
	struct emu *emu;

	emu = board->emu;

	if (board && board->info && board->info->funcs &&
	    board->info->funcs->cleanup) {
		board->info->funcs->cleanup(board);
	}

	if (board && board->info &&
	    (board->info->flags & BOARD_INFO_FLAG_M2_TIMER)) {
		m2_timer_cleanup(board->emu);
	}

	if (emu->cheat_file && board->emu->config->autosave_cheats) {
		char *buffer = config_get_path(board->emu->config,
						   CONFIG_DATA_DIR_CHEAT,
						   emu->cheat_file, 1);
		if (buffer)
			cheat_save_file(board->emu, buffer);

		free(buffer);
	}

	board_write_ips_save(board, board->modified_ranges);
	free_range_list(&board->modified_ranges);
	board_cleanup_ram_chips(board);

	if (board->ciram.data)
		free(board->ciram.data);

	if (board->fill_mode_nmt) {
		free(board->fill_mode_nmt);
		board->fill_mode_nmt = NULL;
	}

	board->emu->board = NULL;
	free(board);
}

void board_end_frame(struct board *board, uint32_t cycles)
{
	if (board->info->funcs && board->info->funcs->end_frame) {
		board->info->funcs->end_frame(board, cycles);
	}

	if (board->emu->m2_timer)
		m2_timer_end_frame(board->emu->m2_timer, cycles);
}

void board_set_ppu_mirroring(struct board *board, int type)
{
	int i;

	for (i = 0; i < 4; i++) {
		board->nmt_banks[i].type = MAP_TYPE_CIRAM;
		board->nmt_banks[i].perms = MAP_PERM_READWRITE;
		board->nmt_banks[i].bank = type & 0x03;
		type >>= 2;
	}

	board_nmt_sync(board);
}

void board_toggle_dip_switch(struct board *board, int sw)
{
	if (board->num_dip_switches == 0)
		return;

	if (sw <= 0 || sw > board->num_dip_switches)
		return;

	board->dip_switches ^= (1 << (sw - 1));
	board->emu->config->dip_switch[sw - 1] ^= 1;

	emu_save_rom_config(board->emu);

	osdprintf("DIP switch %d %s", sw,
		  board->dip_switches & (1 << (sw - 1)) ?
		  "on" : "off");
}

void board_set_dip_switch(struct board *board, int sw, int on)
{
	if (board->num_dip_switches == 0)
		return;

	if (sw <= 0 || sw > board->num_dip_switches)
		return;

	if (on)
		board->dip_switches |= (1 << (sw - 1));
	else
		board->dip_switches &= ~(1 << (sw - 1));

	board->emu->config->dip_switch[sw - 1] = on;

	emu_save_rom_config(board->emu);
}

int board_get_num_dip_switches(struct board *board)
{
	if (!board)
		return 0;
	
	return board->num_dip_switches;
}

int board_set_num_dip_switches(struct board *board, int num)
{
	if (num < 0 || num > 8)
		return -1;

	board->num_dip_switches = num;

	return num;
}

uint8_t board_get_dip_switches(struct board * board)
{
	return board->dip_switches;
}

void board_prg_sync(struct board *board)
{
	int i;
	struct bank *b;
	int prg_entries;

	prg_entries = sizeof(board->prg_banks) /
	    sizeof(board->prg_banks[0]);

	for (i = 0; i < prg_entries; i++) {
		unsigned and;
		unsigned or;
		int bank;
		uint8_t *data;
		size_t data_size;
		int type, perms;
		int addr;
		size_t size;

		b = &board->prg_banks[i];
		if (b->size == 0)
			continue;

		data = NULL;
		data_size = 0;
		perms = 0;
		bank = 0;

		and = board->prg_and;
		or = board->prg_or;
		if (b->type != MAP_TYPE_ROM) {
			and = board->wram_and;
			or = board->wram_or;
		}

		or &= (~and);

		type = b->type;
		perms = b->perms;

		if (type == MAP_TYPE_AUTO) {
			if (board->wram[0].data)
				type = MAP_TYPE_RAM0;
			else
				type = MAP_TYPE_NONE;
		}

		switch (type) {
		case MAP_TYPE_ROM:
			data = board->prg_rom.data;
			data_size = board->prg_rom.size;
			perms &= MAP_PERM_READ;
			break;
		case MAP_TYPE_RAM0:
			data = board->wram[0].data;
			data_size = board->wram[0].size;
			break;
		case MAP_TYPE_RAM1:
			data = board->wram[1].data;
			data_size = board->wram[1].size;
			break;
		case MAP_TYPE_NONE:
			data = NULL;
			data_size = b->size;
			break;
		case MAP_TYPE_MAPPER_RAM:
			data = board->mapper_ram.data;
			data_size = board->mapper_ram.size;
			break;
		default:
			log_err("board_prg_sync: invalid type %d\n",
				type);
			type = MAP_TYPE_NONE;
			break;
		}

		bank = b->bank;
		while ((bank < 0) && b->size) {
			bank = (data_size / b->size) + bank;
			/* printf("bank = %d (data_size %d, b->size %d)\n", bank, data_size, b->size); */
		}
		bank = ((bank & and) | or) >> b->shift;

		if (data) {
			int offset;

			offset = (int)bank * b->size;
			if (offset < 0) {
				offset = -(-offset % data_size);
				data += (data_size + offset);
			} else {
				offset = offset % data_size;
				data += offset;
			}
		} else {
			data_size = b->size;
		}


             /* printf("%x: mapping %x to %x, bank=%x bankfu=%x, size=%x, perms=%x\n", type, */
             /*        data, b->address, b->bank, bank, b->size, perms); */
 
		size = (b->size <= data_size) ? b->size : data_size;

		for (addr = b->address;
		     size && addr < (uint32_t) b->address + b->size;
		     addr += size) {
			uint8_t *ptr;

			ptr = data;
			if (size < CPU_PAGE_SIZE)
				ptr = NULL;

			cpu_set_pagetable_entry(board->emu->cpu, addr, size,
						ptr, perms);
		}

//              printf("mapping done\n");

	}
}

static void board_internal_nmt_sync(struct board *board)
{
	int i;
	uint32_t cycles;

	cycles = cpu_get_cycles(board->emu->cpu);

	for (i = 0; i < 4; i++) {
		uint8_t *data;
		int perms;
		int data_size;
		int bank;
		int count;

		data = NULL;
		data_size = 0;
		perms = board->nmt_banks[i].perms;
		bank = board->nmt_banks[i].bank;

		switch (board->nmt_banks[i].type) {
		case MAP_TYPE_CIRAM:
			data = board->ciram.data;
			data_size = board->ciram.size;
			perms = MAP_PERM_READ | MAP_PERM_WRITE;
			break;
		case MAP_TYPE_ROM:
			data = board->chr_rom.data;
			data_size = board->chr_rom.size;
			perms &= MAP_PERM_READ;
			break;
		case MAP_TYPE_RAM0:
			data = board->vram[0].data;
			data_size = board->vram[0].size;
			break;
		case MAP_TYPE_RAM1:
			data = board->vram[1].data;
			data_size = board->vram[1].size;
			break;
		case MAP_TYPE_MAPPER_RAM:
			data = board->mapper_ram.data;
			data_size = board->mapper_ram.size;
			/* FIXME should this be readonly? */
			break;
		case MAP_TYPE_FILL:
			data = board->fill_mode_nmt;
			data_size = SIZE_1K;
			perms = MAP_PERM_READ;
			break;
		case MAP_TYPE_ZERO:
			data = zero_nmt;
			data_size = SIZE_1K;
			perms = MAP_PERM_READ;
		case MAP_TYPE_NONE:
			data = NULL;
			data_size = SIZE_1K;
			break;
		}

		count = data_size / SIZE_1K;
		if (count == 0)
			data = NULL;

		if (data) {
			bank %= count ? count : 1;
			data += bank * SIZE_1K;
		}

		ppu_map_nametable(board->emu->ppu, i, data, perms, cycles);
	}
}

void board_nmt_sync(struct board *board)
{
	/* Only allow mirroring changes at init (for
	   default mirroring) or if mirroring is
	   mapper-controlled. */
	
	if (board->mirroring != MIRROR_M)
		return;

	board_internal_nmt_sync(board);
}

void board_chr_sync(struct board *board, int set)
{
	int i;
	struct bank *b;
	uint32_t cycles;

	cycles = cpu_get_cycles(board->emu->cpu);

	for (i = 0; i < 10; i++) {
		uint32_t and;
		uint32_t or;
		uint32_t bank;
		uint8_t *data;
		uint32_t data_size;
		int type, perms;
		int addr;
		size_t size;

		if (set)
			b = &board->chr_banks1[i];
		else
			b = &board->chr_banks0[i];
		if (b->size == 0)
			continue;

		and = board->chr_and;
		or = board->chr_or;

		data = NULL;
		data_size = 0;

		type = b->type;
		bank = ((b->bank & and) | or) >> b->shift;
		perms = b->perms;

		if (type == MAP_TYPE_AUTO) {
			if (board->chr_rom.data)
				type = MAP_TYPE_ROM;
			else if (board->vram[0].data)
				type = MAP_TYPE_RAM0;
		}

		switch (type) {
		case MAP_TYPE_ROM:
			data = board->chr_rom.data;
			data_size = board->chr_rom.size;
			perms &= MAP_PERM_READ;
			break;
		case MAP_TYPE_RAM0:
			data = board->vram[0].data;
			data_size = board->vram[0].size;
			break;
		case MAP_TYPE_RAM1:
			data = board->vram[1].data;
			data_size = board->vram[1].size;
			break;
		case MAP_TYPE_CIRAM:
			data = board->ciram.data;
			data_size = board->ciram.size;
			break;
		case MAP_TYPE_NONE:
			data = NULL;
			data_size = b->size;
			break;
		default:
			log_err("board_chr_sync: invalid type %d\n",
				type);
			type = MAP_TYPE_NONE;
			break;
		}

		if (data) {
			int offset;

			offset = (int)bank * b->size;
			if (offset < 0) {
				offset = -(-offset % data_size);
				data += (data_size + offset);
			} else {
				offset = offset % data_size;
				data += offset;
			}
		}

		size = (b->size <= data_size) ? b->size : data_size;

		/* printf("mapping %x to %x, size=%x, perms=%x size=%x type=%d\n", */
		/*        data, b->address, b->size, perms, size, type); */

		for (addr = b->address;
		     size && addr < (uint32_t) b->address + b->size;
		     addr += size) {
			uint8_t *ptr;

			ptr = data;
			if (size < SIZE_1K)
				ptr = NULL;

			ppu_set_pagemap_entry(board->emu->ppu, set, addr, size,
					      ptr, perms, cycles);

		}
	}
}

uint32_t board_get_type(struct board *board)
{
	if (board->info)
		return board->info->board_type;
	else
		return -1;
}

static CPU_WRITE_HANDLER(blargg_wram_hook)
{
	static int is_blargg = 0;
	struct board *board;
	size_t size;

	board = emu->board;

	size = board->wram[0].size;

	if (orig_wram_write_handler)
		orig_wram_write_handler(emu, addr, value, cycles);

	if (!board->wram[0].data)
		return;

	board->wram[0].data[(addr - 0x6000) % size] = value;
	if (!is_blargg &&
	    (memcmp(&board->wram[0].data[1], "\xde\xb0\x61", 3) == 0)) {
		printf("blargg test rom detected\n");
		is_blargg = 1;
	}

	if (!is_blargg || addr != 0x6000)
		return;

	/* Addr must be 0x6000 */

	if (value < 0x80) {
		int i;
		int prefix;
		printf("BLARGG: TEST FINISHED (%02x)\n", value);

		prefix = 1;
		for (i = 4; i < board->wram[0].size &&
		     board->wram[0].data[i]; i++) {
			if (prefix) {
				printf("BLARGG: ");
				prefix = 0;
			}
			fputc(board->wram[0].data[i], stdout);
			if (board->wram[0].data[i] == '\n')
				prefix = 1;
		}

		running = 0;
	} else if (value == 0x81) {
		/* Reset */
		blargg_reset_request(emu);
	}
}

void board_get_chr_rom(struct board *board, uint8_t **romptr,
		       size_t *sizeptr)
{
	*romptr = board->chr_rom.data;
	*sizeptr = board->chr_rom.size;
}

void board_get_mapper_ram(struct board *board, uint8_t **ramptr,
			  size_t *sizeptr)
{
	*ramptr = board->mapper_ram.data;
	*sizeptr = board->mapper_ram.size;
}

CPU_WRITE_HANDLER(simple_prg_write_handler)
{
	struct board *board = emu->board;
	value &= cpu_peek(emu->cpu, addr);

	if (board->prg_banks[1].bank != value) {
		board->prg_banks[1].bank = value;
		board_prg_sync(board);
	}
}

CPU_WRITE_HANDLER(simple_prg_no_conflict_write_handler)
{
	struct board *board = emu->board;
	if (board->prg_banks[1].bank != value) {
		board->prg_banks[1].bank = value;
		board_prg_sync(board);
	}
}

CPU_WRITE_HANDLER(simple_chr_write_handler)
{
	struct board *board = emu->board;
	value &= cpu_peek(emu->cpu, addr);

	if (board->chr_banks0[0].bank != value) {
		board->chr_banks0[0].bank = value;
		board_chr_sync(board, 0);
	}
}

CPU_WRITE_HANDLER(simple_chr_no_conflict_write_handler)
{
	struct board *board = emu->board;
	if (board->chr_banks0[0].bank != value) {
		board->chr_banks0[0].bank = value;
		board_chr_sync(board, 0);
	}
}

uint8_t std_mirroring_vh[] = { MIRROR_V, MIRROR_H, MIRROR_V, MIRROR_H };
uint8_t std_mirroring_hv[] = { MIRROR_H, MIRROR_V, MIRROR_H, MIRROR_V };
uint8_t std_mirroring_01[] = { MIRROR_1A, MIRROR_1B, MIRROR_1A, MIRROR_1B };
uint8_t std_mirroring_vh01[] = { MIRROR_V, MIRROR_H, MIRROR_1A, MIRROR_1B };
uint8_t std_mirroring_hv01[] = { MIRROR_H, MIRROR_V, MIRROR_1A, MIRROR_1B };

CPU_WRITE_HANDLER(standard_mirroring_handler)
{
	struct board_info *info = emu->board->info;

	if (!info->mirroring_values)
		return;

	value = (value >> info->mirroring_shift) & 0x03;
	board_set_ppu_mirroring(emu->board, info->mirroring_values[value]);
}

/* FIXME I don't know if these should go in their own .c file */
struct bank std_prg_46k[] = {
	{-23, 0, SIZE_2K, 0x4800, MAP_PERM_READ, MAP_TYPE_ROM},
	{-11, 0, SIZE_4K, 0x5000, MAP_PERM_READ, MAP_TYPE_ROM},
	{ -5, 0, SIZE_8K, 0x6000, MAP_PERM_READ, MAP_TYPE_ROM},
	{ -1, 0, SIZE_32K, 0x8000, MAP_PERM_READ, MAP_TYPE_ROM},
	{.type = MAP_TYPE_END},
};

struct bank std_prg_32k[] = {
	{0, 0, SIZE_8K, 0x6000, MAP_PERM_READWRITE, MAP_TYPE_RAM0},
	{0, 0, SIZE_32K, 0x8000, MAP_PERM_READ, MAP_TYPE_ROM},
	{.type = MAP_TYPE_END},
};

struct bank std_prg_16k[] = {
	{0, 0, SIZE_8K, 0x6000, MAP_PERM_READWRITE, MAP_TYPE_RAM0},
	{0, 0, SIZE_16K, 0x8000, MAP_PERM_READ, MAP_TYPE_ROM},
	{-1, 0, SIZE_16K, 0xc000, MAP_PERM_READ, MAP_TYPE_ROM},
	{.type = MAP_TYPE_END},
};

struct bank std_prg_8k[] = {
	{0, 0, SIZE_8K, 0x6000, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{0, 0, SIZE_8K, 0x8000, MAP_PERM_READ, MAP_TYPE_ROM},
	{0, 0, SIZE_8K, 0xa000, MAP_PERM_READ, MAP_TYPE_ROM},
	{-2, 0, SIZE_8K, 0xc000, MAP_PERM_READ, MAP_TYPE_ROM},
	{-1, 0, SIZE_8K, 0xe000, MAP_PERM_READ, MAP_TYPE_ROM},
	{.type = MAP_TYPE_END},
};

struct bank std_prg_4k[] = {
	{0, 0, SIZE_8K, 0x6000, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{0, 0, SIZE_4K, 0x8000, MAP_PERM_READ, MAP_TYPE_ROM},
	{0, 0, SIZE_4K, 0x9000, MAP_PERM_READ, MAP_TYPE_ROM},
	{0, 0, SIZE_4K, 0xa000, MAP_PERM_READ, MAP_TYPE_ROM},
	{0, 0, SIZE_4K, 0xb000, MAP_PERM_READ, MAP_TYPE_ROM},
	{0, 0, SIZE_4K, 0xc000, MAP_PERM_READ, MAP_TYPE_ROM},
	{0, 0, SIZE_4K, 0xd000, MAP_PERM_READ, MAP_TYPE_ROM},
	{0, 0, SIZE_4K, 0xe000, MAP_PERM_READ, MAP_TYPE_ROM},
	{-1, 0, SIZE_4K, 0xf000, MAP_PERM_READ, MAP_TYPE_ROM},
	{.type = MAP_TYPE_END},
};

struct bank std_chr_8k[] = {
	{0, 0, SIZE_8K, 0x0000, MAP_PERM_READWRITE, MAP_TYPE_AUTO,},
	{.type = MAP_TYPE_END},
};

struct bank std_chr_4k[] = {
	{0, 0, SIZE_4K, 0x0000, MAP_PERM_READWRITE, MAP_TYPE_AUTO,},
	{0, 0, SIZE_4K, 0x1000, MAP_PERM_READWRITE, MAP_TYPE_AUTO,},
	{.type = MAP_TYPE_END},
};

struct bank std_chr_2k[] = {
	{0, 0, SIZE_2K, 0x0000, MAP_PERM_READWRITE, MAP_TYPE_AUTO,},
	{0, 0, SIZE_2K, 0x0800, MAP_PERM_READWRITE, MAP_TYPE_AUTO,},
	{0, 0, SIZE_2K, 0x1000, MAP_PERM_READWRITE, MAP_TYPE_AUTO,},
	{0, 0, SIZE_2K, 0x1800, MAP_PERM_READWRITE, MAP_TYPE_AUTO,},
	{.type = MAP_TYPE_END},
};

struct bank std_chr_1k[] = {
	{0, 0, SIZE_1K, 0x0000, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{0, 0, SIZE_1K, 0x0400, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{0, 0, SIZE_1K, 0x0800, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{0, 0, SIZE_1K, 0x0c00, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{0, 0, SIZE_1K, 0x1000, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{0, 0, SIZE_1K, 0x1400, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{0, 0, SIZE_1K, 0x1800, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{0, 0, SIZE_1K, 0x1c00, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{.type = MAP_TYPE_END},
};

/* MMC3-ish CHR bank mappings.  The default bank values map the first
   8K in, and appears to be required by at least one demo.  MMC3-like
   chips tend to use this mapping (although the default values may not
   be the same).
*/
struct bank std_chr_2k_1k[] = {
	{0, 1, SIZE_2K, 0x0000, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{2, 1, SIZE_2K, 0x0800, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{4, 0, SIZE_1K, 0x1000, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{5, 0, SIZE_1K, 0x1400, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{6, 0, SIZE_1K, 0x1800, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{7, 0, SIZE_1K, 0x1c00, MAP_PERM_READWRITE, MAP_TYPE_AUTO},
	{.type = MAP_TYPE_END},
};

static int get_bank_list_count(struct bank *list)
{
	int count;

	count = 0;
	while (list[count].type != MAP_TYPE_END)
		count++;

	return count;
}

static int chip_load_state(struct chip *chip, struct save_state *state,
			   const char *id)
{
	uint8_t *buf;
	size_t size;

	if (!chip || !chip->data || !chip->size)
		return 0;

	if (save_state_find_chunk(state, id, &buf, &size) < 0)
		return -1;

	if (chip->size != size)
		return -1;

	memcpy(chip->data, buf, size);

	return 0;
}

static int banks_load_state(struct bank *banks, struct save_state *state,
			    const char *id)
{
	uint8_t *buf, *ptr;
	size_t size;
	int i;

	if (save_state_find_chunk(state, id, &buf, &size) < 0)
		return -1;

	ptr = buf;
	i = 0;
	while (ptr < buf + size) {
		ptr += unpack_state(&banks[i], bank_state_items, ptr);
		i++;
	}

	return 0;
}

int board_load_state(struct board *board, struct save_state *state)
{
	uint8_t *buf;
	size_t size;
	int i;
	int rc;

	if (save_state_find_chunk(state, "BRD ", &buf, &size) < 0)
		return -1;

	buf += unpack_state(board, board_state_items, buf);

	for (i = 0; i < 4; i++) {
		buf += unpack_state(&board->nmt_banks[i], nmt_bank_state_items,
				    buf);
	}

	if (board->info && board->info->flags &
	    BOARD_INFO_FLAG_PRG_IPS) {
		uint8_t *data;
		size_t size;

		data = board->prg_rom.data;
		size = board->prg_rom.size;

		if (range_list_load_state(&board->modified_ranges, data,
					  size, state, "PTCH") < 0) {
			return -1;
		}
	}

	if (banks_load_state(board->prg_banks, state, "PRGB") < 0)
		return -1;

	if (banks_load_state(board->chr_banks0, state, "CHB0") < 0)
		return -1;

	if (board->info->init_chr1) {
		if (banks_load_state(board->chr_banks1, state, "CHB1") < 0)
			return -1;
	}

	rc = 0;
	rc |= chip_load_state(&board->ciram, state, "CIRM");
	rc |= chip_load_state(&board->mapper_ram, state, "MPRM");
	rc |= chip_load_state(&board->wram[0], state, "WRM0");
	rc |= chip_load_state(&board->wram[1], state, "WRM1");
	rc |= chip_load_state(&board->vram[0], state, "VRM0");
	rc |= chip_load_state(&board->vram[1], state, "VRM1");

	if (rc)
		return -1;

	/* printf("syncing\n"); */

	board_prg_sync(board);
	board_chr_sync(board, 0);
	board_chr_sync(board, 1);
	board_nmt_sync(board);

	if (board->info->flags & BOARD_INFO_FLAG_M2_TIMER)
		m2_timer_load_state(board->emu->m2_timer, state);

	if (board->info->funcs) {
		if (board->info->funcs->load_state)
			return board->info->funcs->load_state(board, state);
	} else {
		return 0;
	}

	return 0;
}

static int banks_save_state(struct bank *banks, struct bank *init_banks,
			    struct save_state *state, const char *id)
{
	size_t size;
	uint8_t *buf, *ptr;
	int count;
	int i, rc;

	if (!banks || !init_banks)
		return 0;

	count = 0;
	for (i = 0; init_banks[i].type != MAP_TYPE_END; i++)
		count++;

	if (!count)
		return 0;

	size = count * pack_state(banks, bank_state_items, NULL);

	buf = malloc(size);
	if (!buf)
		return -1;

	ptr = buf;
	for (i = 0; i < count; i++)
		ptr += pack_state(&banks[i], bank_state_items, ptr);

	rc = save_state_add_chunk(state, id, buf, size);
	free(buf);
	if (rc < 0)
		return -1;

	return 0;
}

int board_save_state(struct board *board, struct save_state *state)
{
	uint8_t *buf, *ptr;
	size_t size;
	int i;
	int rc;

	size = pack_state(board, board_state_items, NULL);
	size += 4 * pack_state(&board->nmt_banks[0], nmt_bank_state_items,
			       NULL);

	buf = malloc(size);
	if (!buf)
		return -1;

	ptr = buf;
	ptr += pack_state(board, board_state_items, ptr);
	for (i = 0; i < 4; i++) {
		ptr += pack_state(&board->nmt_banks[i],
				  nmt_bank_state_items, ptr);
	}

	rc = save_state_add_chunk(state, "BRD ", buf, size);
	free(buf);
	if (rc < 0)
		return -1;

	if (banks_save_state(board->prg_banks, board->info->init_prg,
			     state, "PRGB") < 0) {
		return -1;
	}
	
	if (banks_save_state(board->chr_banks0, board->info->init_chr0,
			     state, "CHB0") < 0) {
		return -1;
	}

	if (banks_save_state(board->chr_banks1, board->info->init_chr1,
			     state, "CHB1") < 0) {
		return -1;
	}

	if (board->info && board->info->flags &
	    BOARD_INFO_FLAG_PRG_IPS) {
		uint8_t *data;
		size_t size;

		data = board->prg_rom.data;
		size = board->prg_rom.size;

		if (board->modified_ranges &&
		    range_list_save_state(&board->modified_ranges, data,
					  size, state, "PTCH") < 0) {
			return -1;
		}
	}

	if (board->ciram.data) {
		rc = save_state_add_chunk(state, "CIRM", board->ciram.data,
					  board->ciram.size);
		if (rc < 0)
			return -1;
	}

	if (board->mapper_ram.data) {
		rc = save_state_add_chunk(state, "MPRM",
					  board->mapper_ram.data,
					  board->mapper_ram.size);
		if (rc < 0)
			return -1;
	}

	if (board->wram[0].data) {
		rc = save_state_add_chunk(state, "WRM0",
					  board->wram[0].data,
					  board->wram[0].size);
		if (rc < 0)
			return -1;
	}

	if (board->wram[1].data) {
		rc = save_state_add_chunk(state, "WRM1",
					  board->wram[1].data,
					  board->wram[1].size);
		if (rc < 0)
			return -1;
	}

	if (board->vram[0].data) {
		rc = save_state_add_chunk(state, "VRM0",
					  board->vram[0].data,
					  board->vram[0].size);
		if (rc < 0)
			return -1;
	}

	if (board->vram[1].data) {
		rc = save_state_add_chunk(state, "VRM1",
					  board->vram[1].data,
					  board->vram[1].size);
		if (rc < 0)
			return -1;
	}

	if (board->info->flags & BOARD_INFO_FLAG_M2_TIMER)
		m2_timer_save_state(board->emu->m2_timer, state);

	if (board->info->funcs) {
		if (board->info->funcs->save_state)
			return board->info->funcs->save_state(board, state);
	}

	return 0;
}

size_t board_info_get_mapper_ram_size(struct board_info *info)
{
	if (!info)
		return 0;

	return info->mapper_ram_size;
}
