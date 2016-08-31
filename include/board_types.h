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

#ifndef _BOARD_TYPES_H_
#define _BOARD_TYPES_H_

/*
  Most board types are defined as follows:

  Bits  0-11: NES 2.0 mapper number
  Bits 12-15: NES 2.0 submapper number
  Bits 16-19: NES 2.0 VS. "protection" field
  Bits 20-30: misc data
  Bit     31: indicates type is based on NES 2.0 header fields if 1

  Bits 20-30 (misc data) are currently used to differentiate between
  board types that have the same mapper, submapper, and protection
  fields (like SNROM, SOROM, SUROM, SXROM, for example).  This value
  is completely arbitrary, but as it is only used internally to the
  emulator this doesn't really matter.

  The mapper/submapper/protection values are used so that the mapping
  to a particular board type isn't arbitrary (at least no more so than
  the mapper numbers themselves) and makes finding the correct board
  type for iNES roms (the common case) possible without any additional
  lookup tables or code.

  If bit 31 is 0, then bits 0-30 are all arbitrarily defined.  This is
  useful for board types which do not (yet) have an NES 2.0 mapper.
  For example, some of the more obscure Asian and pirate ROMs which
  only exist as UNIF dumps (as of 2015).
*/

#define INES_TYPE_FLAG (1 << 31)

#define INES_TO_BOARD_TYPE(m, s, c, x) ((m) | ((s) << 12) |	\
					(((c) << 16)) | \
					(((x) << 20)) | \
		                        INES_TYPE_FLAG)

#define BOARD_TYPE_IS_INES(t) ((t) & INES_TYPE_FLAG)
#define BOARD_TYPE_TO_INES_MAPPER(t) ((t) & 0xfff)
#define BOARD_TYPE_TO_INES_SUBMAPPER(t) (((t) & 0xf000) >> 12)
#define BOARD_TYPE_TO_INES_VS_PROTECTION(t) (((t) & 0xf0000) >> 16)
#define BOARD_TYPE_TO_INES_EXTRA(t) (((t) & 0x7ff00000) >> 4)

#define BOARD_TYPE_UNKNOWN ~0

/* Active Enterprises, Ltd. */
#define BOARD_TYPE_ACTION_52               INES_TO_BOARD_TYPE(228,  0, 0, 0)

/* American Video Entertainment (AVE) boards */
#define BOARD_TYPE_NINA_001                INES_TO_BOARD_TYPE( 34,  1, 0, 0)
#define BOARD_TYPE_NINA_03_06              INES_TO_BOARD_TYPE(113,  0, 0, 0)
#define BOARD_TYPE_AGCI_50282              INES_TO_BOARD_TYPE(144,  0, 0, 0)
#define BOARD_TYPE_MAXI_15                 INES_TO_BOARD_TYPE(234,  0, 0, 0)

/* Bandai */
#define BOARD_TYPE_BANDAI_FCG_COMPAT       INES_TO_BOARD_TYPE( 16,  0, 0, 0)
#define BOARD_TYPE_BANDAI_FCG              INES_TO_BOARD_TYPE( 16,  0, 0, 1)
#define BOARD_TYPE_BANDAI_LZ93D50          INES_TO_BOARD_TYPE( 16,  0, 0, 2)
#define BOARD_TYPE_BANDAI_LZ93D50_24C01    INES_TO_BOARD_TYPE(159,  0, 0, 0)
#define BOARD_TYPE_BANDAI_LZ93D50_24C02    INES_TO_BOARD_TYPE( 16,  0, 0, 3)
#define BOARD_TYPE_BANDAI_JUMP2            INES_TO_BOARD_TYPE(153,  0, 0, 0)
#define BOARD_TYPE_BANDAI_OEKAKIDS         INES_TO_BOARD_TYPE( 96,  0, 0, 0)

/* Camerica */
#define BOARD_TYPE_CAMERICA_BF9093         INES_TO_BOARD_TYPE( 71,  0, 0, 0)
#define BOARD_TYPE_CAMERICA_BF9097         INES_TO_BOARD_TYPE( 71,  1, 0, 0)
#define BOARD_TYPE_CAMERICA_GOLDENFIVE     INES_TO_BOARD_TYPE(104,  0, 0, 0)
#define BOARD_TYPE_CAMERICA_BF9096         INES_TO_BOARD_TYPE(232,  0, 0, 0)
#define BOARD_TYPE_CAMERICA_ALADDIN        INES_TO_BOARD_TYPE(232,  1, 0, 0)

/* Homebrew boards */
#define BOARD_TYPE_SINGLECHIP              INES_TO_BOARD_TYPE(218,  0, 0, 0)
#define BOARD_TYPE_STREEMERZ_BUNDLE        INES_TO_BOARD_TYPE( 28,  0, 0, 0)
#define BOARD_TYPE_UNROM_512	           INES_TO_BOARD_TYPE( 30,  0, 0, 0)
#define BOARD_TYPE_UNROM_512_8	           INES_TO_BOARD_TYPE( 30,  0, 0, 2)
#define BOARD_TYPE_UNROM_512_16	           INES_TO_BOARD_TYPE( 30,  0, 0, 3)
#define BOARD_TYPE_UNROM_512_32	           INES_TO_BOARD_TYPE( 30,  0, 0, 4)
#define BOARD_TYPE_UNROM_512_FLASH         INES_TO_BOARD_TYPE( 30,  0, 0, 1)
#define BOARD_TYPE_INLNSF                  INES_TO_BOARD_TYPE( 31,  0, 0, 0)

/* Irem */
#define BOARD_TYPE_IREM_TAM_S1             INES_TO_BOARD_TYPE( 97,  0, 0, 0)
#define BOARD_TYPE_IREM_H3001              INES_TO_BOARD_TYPE( 65,  0, 0, 0)
#define BOARD_TYPE_IREM_G101               INES_TO_BOARD_TYPE( 32,  0, 0, 0)
#define BOARD_TYPE_IREM_G101_B             INES_TO_BOARD_TYPE( 32,  1, 0, 0)
#define BOARD_TYPE_IREM_74x161_161_21_138  INES_TO_BOARD_TYPE( 77,  0, 0, 0)
#define BOARD_TYPE_IREM_HOLY_DIVER         INES_TO_BOARD_TYPE( 78,  3, 0, 0)

/* Jaleco */
#define BOARD_TYPE_JALECO_JF13             INES_TO_BOARD_TYPE( 86,  0, 0, 0)
#define BOARD_TYPE_JALECO_JF14             INES_TO_BOARD_TYPE(140,  0, 0, 0)
#define BOARD_TYPE_JALECO_SS88006          INES_TO_BOARD_TYPE( 18,  0, 0, 0)
#define BOARD_TYPE_JALECO_JF16             INES_TO_BOARD_TYPE( 78,  1, 0, 0)
#define BOARD_TYPE_JALECO_JF17             INES_TO_BOARD_TYPE( 72,  1, 0, 0)
#define BOARD_TYPE_JALECO_JF19             INES_TO_BOARD_TYPE( 92,  1, 0, 0)

/* Konami */
#define BOARD_TYPE_VRC1                    INES_TO_BOARD_TYPE( 75,  0, 0, 0)
#define BOARD_TYPE_VRC2A                   INES_TO_BOARD_TYPE( 22,  0, 0, 0)
#define BOARD_TYPE_VRC2B                   INES_TO_BOARD_TYPE( 23,  3, 0, 0)
#define BOARD_TYPE_VRC2C                   INES_TO_BOARD_TYPE( 25,  3, 0, 0)
#define BOARD_TYPE_VRC3                    INES_TO_BOARD_TYPE( 73,  0, 0, 0)
#define BOARD_TYPE_VRC4A                   INES_TO_BOARD_TYPE( 21,  1, 0, 0)
#define BOARD_TYPE_VRC4B                   INES_TO_BOARD_TYPE( 25,  1, 0, 0)
#define BOARD_TYPE_VRC4C                   INES_TO_BOARD_TYPE( 21,  2, 0, 0)
#define BOARD_TYPE_VRC4D                   INES_TO_BOARD_TYPE( 25,  2, 0, 0)
#define BOARD_TYPE_VRC4E                   INES_TO_BOARD_TYPE( 23,  2, 0, 0)
#define BOARD_TYPE_VRC4F                   INES_TO_BOARD_TYPE( 23,  1, 0, 0)
#define BOARD_TYPE_VRC4E_COMPAT            INES_TO_BOARD_TYPE( 23,  0, 0, 0)
#define BOARD_TYPE_VRC4AC                  INES_TO_BOARD_TYPE( 21,  0, 0, 0)
#define BOARD_TYPE_VRC4BD                  INES_TO_BOARD_TYPE( 25,  0, 0, 0)
#define BOARD_TYPE_VRC6A                   INES_TO_BOARD_TYPE( 24,  0, 0, 0)
#define BOARD_TYPE_VRC6B                   INES_TO_BOARD_TYPE( 26,  0, 0, 0)
#define BOARD_TYPE_VRC7_COMPAT             INES_TO_BOARD_TYPE( 85,  0, 0, 0)
#define BOARD_TYPE_VRC7A                   INES_TO_BOARD_TYPE( 85,  0, 0, 1)
#define BOARD_TYPE_VRC7B                   INES_TO_BOARD_TYPE( 85,  0, 0, 2)

/* Namco */
#define BOARD_TYPE_NAMCO_163               INES_TO_BOARD_TYPE( 19,  0, 0, 0)
#define BOARD_TYPE_NAMCO_340_COMPAT        INES_TO_BOARD_TYPE(210,  0, 0, 0)
#define BOARD_TYPE_NAMCO_340               INES_TO_BOARD_TYPE(210,  2, 0, 0)
#define BOARD_TYPE_NAMCO_175               INES_TO_BOARD_TYPE(210,  1, 0, 0)
#define BOARD_TYPE_NAMCO_108               INES_TO_BOARD_TYPE(206,  0, 0, 0)
#define BOARD_TYPE_DRROM                   INES_TO_BOARD_TYPE(206,  0, 0, 1)
#define BOARD_TYPE_NAMCO_3446              INES_TO_BOARD_TYPE( 76,  0, 0, 0)
#define BOARD_TYPE_NAMCO_3433              INES_TO_BOARD_TYPE( 88,  0, 0, 0)
#define BOARD_TYPE_NAMCO_3453              INES_TO_BOARD_TYPE(154,  0, 0, 0)
#define BOARD_TYPE_NAMCO_3425              INES_TO_BOARD_TYPE( 95,  0, 0, 0)
#define BOARD_TYPE_NAMCO_CNROM_WRAM        INES_TO_BOARD_TYPE(  3,  0, 0, 1)

/* Nintendo-made boards (and variants) */
#define BOARD_TYPE_AxROM                   INES_TO_BOARD_TYPE(  7,  0, 0, 0)
#define BOARD_TYPE_BNROM                   INES_TO_BOARD_TYPE( 34,  2, 0, 0)
#define BOARD_TYPE_CNROM                   INES_TO_BOARD_TYPE(  3,  0, 0, 0)
#define BOARD_TYPE_CNROM_NO_CONFLICT       INES_TO_BOARD_TYPE(  3,  1, 0, 0)
#define BOARD_TYPE_CPROM                   INES_TO_BOARD_TYPE( 13,  0, 0, 0)
#define BOARD_TYPE_ExROM                   INES_TO_BOARD_TYPE(  5,  0, 0, 0)
#define BOARD_TYPE_EKROM                   INES_TO_BOARD_TYPE(  5,  0, 0, 2)
#define BOARD_TYPE_ETROM                   INES_TO_BOARD_TYPE(  5,  0, 0, 3)
#define BOARD_TYPE_EWROM                   INES_TO_BOARD_TYPE(  5,  0, 0, 4)
#define BOARD_TYPE_ExROM_COMPAT            INES_TO_BOARD_TYPE(  5,  0, 0, 1)
#define BOARD_TYPE_FxROM                   INES_TO_BOARD_TYPE( 10,  0, 0, 0)
#define BOARD_TYPE_FxROM_WRAM              INES_TO_BOARD_TYPE( 10,  0, 0, 1)
#define BOARD_TYPE_FAMILYBASIC             INES_TO_BOARD_TYPE(  0,  0, 0, 1)
#define BOARD_TYPE_GxROM                   INES_TO_BOARD_TYPE( 66,  0, 0, 0)
#define BOARD_TYPE_HROM                    INES_TO_BOARD_TYPE(  0,  0, 0, 3)
#define BOARD_TYPE_HKROM                   INES_TO_BOARD_TYPE(  4,  1, 0, 0)
#define BOARD_TYPE_NROM                    INES_TO_BOARD_TYPE(  0,  0, 0, 0)
#define BOARD_TYPE_NROM368                 INES_TO_BOARD_TYPE(  0,  0, 0, 2)
#define BOARD_TYPE_PxROM                   INES_TO_BOARD_TYPE(  9,  0, 0, 0)
#define BOARD_TYPE_SxROM                   INES_TO_BOARD_TYPE(  1,  0, 0, 0)
#define BOARD_TYPE_SxROM_MMC1A             INES_TO_BOARD_TYPE(155,  0, 0, 0)
#define BOARD_TYPE_SNROM                   INES_TO_BOARD_TYPE(  1,  0, 0, 1)
#define BOARD_TYPE_SOROM                   INES_TO_BOARD_TYPE(  1,  2, 0, 0)
#define BOARD_TYPE_SUROM                   INES_TO_BOARD_TYPE(  1,  1, 0, 0)
#define BOARD_TYPE_SXROM                   INES_TO_BOARD_TYPE(  1,  4, 0, 0)
#define BOARD_TYPE_SEROM_SHROM             INES_TO_BOARD_TYPE(  1,  5, 0, 0)
#define BOARD_TYPE_SxROM_COMPAT            INES_TO_BOARD_TYPE(  1,  0, 0, 7)
#define BOARD_TYPE_SxROM_WRAM              INES_TO_BOARD_TYPE(  1,  0, 0, 6)
#define BOARD_TYPE_TxROM                   INES_TO_BOARD_TYPE(  4,  0, 0, 0)
#define BOARD_TYPE_TxROM_COMPAT            INES_TO_BOARD_TYPE(  4,  0, 0, 3)
#define BOARD_TYPE_TxROM_WRAM              INES_TO_BOARD_TYPE(  4,  0, 0, 1)
#define BOARD_TYPE_TVROM                   INES_TO_BOARD_TYPE(  4,  0, 0, 2)
#define BOARD_TYPE_ACCLAIM_MC_ACC          INES_TO_BOARD_TYPE(  4,  3, 0, 0)
#define BOARD_TYPE_TxROM_MMC3A             INES_TO_BOARD_TYPE(  4,  4, 0, 0)
#define BOARD_TYPE_TxSROM                  INES_TO_BOARD_TYPE(118,  0, 0, 0)
#define BOARD_TYPE_TQROM                   INES_TO_BOARD_TYPE(119,  0, 0, 0)
#define BOARD_TYPE_UxROM                   INES_TO_BOARD_TYPE(  2,  0, 0, 0)
#define BOARD_TYPE_UxROM_PC_PROWRESTLING   INES_TO_BOARD_TYPE(  2,  0, 0, 1)
#define BOARD_TYPE_UxROM_NO_CONFLICT       INES_TO_BOARD_TYPE(  2,  1, 0, 0)
#define BOARD_TYPE_UN1ROM                  INES_TO_BOARD_TYPE( 94,  0, 0, 0)
#define BOARD_TYPE_UNROM_74HC08            INES_TO_BOARD_TYPE(180,  0, 0, 0)
#define BOARD_TYPE_QJ                      INES_TO_BOARD_TYPE( 47,  0, 0, 0)
#define BOARD_TYPE_ZZ                      INES_TO_BOARD_TYPE( 37,  0, 0, 0)
#define BOARD_TYPE_SECURITY_CNROM          INES_TO_BOARD_TYPE(185,  0, 0, 0)
#define BOARD_TYPE_SECURITY_CNROM_BANK0    INES_TO_BOARD_TYPE(185,  4, 0, 0)
#define BOARD_TYPE_SECURITY_CNROM_BANK1    INES_TO_BOARD_TYPE(185,  5, 0, 0)
#define BOARD_TYPE_SECURITY_CNROM_BANK2    INES_TO_BOARD_TYPE(185,  6, 0, 0)
#define BOARD_TYPE_SECURITY_CNROM_BANK3    INES_TO_BOARD_TYPE(185,  7, 0, 0)
#define BOARD_TYPE_EVENT	           INES_TO_BOARD_TYPE(105,  0, 0, 0)

/* Sachen */
#define BOARD_TYPE_SACHEN_TCU01            INES_TO_BOARD_TYPE(147,  0, 0, 0)
#define BOARD_TYPE_SACHEN_SA0036           INES_TO_BOARD_TYPE(149,  0, 0, 0)
#define BOARD_TYPE_SACHEN_SA0037           INES_TO_BOARD_TYPE(148,  0, 0, 0)
#define BOARD_TYPE_SACHEN_8259A            INES_TO_BOARD_TYPE(141,  0, 0, 0)
#define BOARD_TYPE_SACHEN_8259B            INES_TO_BOARD_TYPE(138,  0, 0, 0)
#define BOARD_TYPE_SACHEN_8259C            INES_TO_BOARD_TYPE(139,  0, 0, 0)
#define BOARD_TYPE_SACHEN_8259D            INES_TO_BOARD_TYPE(137,  0, 0, 0)
#define BOARD_TYPE_SACHEN_NROM             INES_TO_BOARD_TYPE(143,  0, 0, 0)
#define BOARD_TYPE_SACHEN_72008            INES_TO_BOARD_TYPE(133,  0, 0, 0)
#define BOARD_TYPE_SACHEN_72007            INES_TO_BOARD_TYPE(145,  0, 0, 0)
#define BOARD_TYPE_SACHEN_74LS374N         INES_TO_BOARD_TYPE(150,  0, 0, 0)

/* Sunsoft */
#define BOARD_TYPE_SUNSOFT1                INES_TO_BOARD_TYPE(184,  0, 0, 0)
#define BOARD_TYPE_SUNSOFT2                INES_TO_BOARD_TYPE( 89,  0, 0, 0)
#define BOARD_TYPE_SUNSOFT3R               INES_TO_BOARD_TYPE( 93,  0, 0, 0)
#define BOARD_TYPE_SUNSOFT2_2_3R           INES_TO_BOARD_TYPE( 93,  0, 0, 1)
#define BOARD_TYPE_SUNSOFT3                INES_TO_BOARD_TYPE( 67,  0, 0, 0)
#define BOARD_TYPE_SUNSOFT4                INES_TO_BOARD_TYPE( 68,  0, 0, 0)
#define BOARD_TYPE_SUNSOFT5B               INES_TO_BOARD_TYPE( 69,  0, 0, 0)

/* Taito */
#define BOARD_TYPE_TAITO_TC0190FMC_PAL16R4 INES_TO_BOARD_TYPE( 48,  0, 0, 0)
#define BOARD_TYPE_TAITO_TC0190FMC         INES_TO_BOARD_TYPE( 33,  0, 0, 0)
#define BOARD_TYPE_TAITO_X1_017		   INES_TO_BOARD_TYPE( 82,  0, 0, 0)
#define BOARD_TYPE_TAITO_X1_005		   INES_TO_BOARD_TYPE( 80,  0, 0, 0)
#define BOARD_TYPE_TAITO_X1_005_ALT	   INES_TO_BOARD_TYPE(207,  0, 0, 0)

/* Tengen */
#define BOARD_TYPE_TENGEN_800032           INES_TO_BOARD_TYPE( 64,  0, 0, 0)
#define BOARD_TYPE_TENGEN_800037           INES_TO_BOARD_TYPE(158,  0, 0, 0)
#define BOARD_TYPE_TENGEN_800004           INES_TO_BOARD_TYPE(206,  0, 0, 0)

/* Wisdom Tree */		
#define BOARD_TYPE_RUMBLESTATION           INES_TO_BOARD_TYPE( 46,  0, 0, 0)

/* Waixing */
#define BOARD_TYPE_WAIXING_TYPE_A          INES_TO_BOARD_TYPE( 74,  0, 0, 0)
#define BOARD_TYPE_WAIXING_TYPE_C          INES_TO_BOARD_TYPE(192,  0, 0, 0)
#define BOARD_TYPE_WAIXING_TYPE_H          INES_TO_BOARD_TYPE(245,  0, 0, 0)
#define BOARD_TYPE_WAIXING_SGZLZ           INES_TO_BOARD_TYPE(178,  0, 0, 0)
#define BOARD_TYPE_WAIXING_PS2             INES_TO_BOARD_TYPE( 15,  0, 0, 0)

/* JY Company */
#define BOARD_TYPE_JYCOMPANY_A             INES_TO_BOARD_TYPE( 90,  0, 0, 0)
#define BOARD_TYPE_JYCOMPANY_B             INES_TO_BOARD_TYPE(209,  0, 0, 0)
#define BOARD_TYPE_JYCOMPANY_C             INES_TO_BOARD_TYPE(211,  0, 0, 0)

/* Kasing */
#define BOARD_TYPE_KASING                  INES_TO_BOARD_TYPE(115,  0, 0, 0)

/* VS. Unisystem games */
#define BOARD_TYPE_VS_UNISYSTEM            INES_TO_BOARD_TYPE( 99,  0, 0, 0)
#define BOARD_TYPE_VS_PINBALL              INES_TO_BOARD_TYPE( 99,  0, 0, 1)
#define BOARD_TYPE_VS_PINBALLJ             INES_TO_BOARD_TYPE( 99,  0, 0, 2)
#define BOARD_TYPE_VS_GUMSHOE              INES_TO_BOARD_TYPE( 99,  0, 0, 3)
#define BOARD_TYPE_VS_RBI_BASEBALL         INES_TO_BOARD_TYPE(206,  0, 1, 0)
#define BOARD_TYPE_VS_TKO_BOXING           INES_TO_BOARD_TYPE(206,  0, 2, 0)
#define BOARD_TYPE_VS_SUPER_XEVIOUS        INES_TO_BOARD_TYPE(206,  0, 3, 0)

#define BOARD_TYPE_FDS  0x1fffe
#define BOARD_TYPE_NSF  0x1ffff

#define BOARD_TYPE_CALTRON_6_IN_1          INES_TO_BOARD_TYPE( 41,  0, 0, 0)
#define BOARD_TYPE_NTDEC_193               INES_TO_BOARD_TYPE(193,  0, 0, 0)
#define BOARD_TYPE_COLORDREAMS             INES_TO_BOARD_TYPE( 11,  0, 0, 0)
#define BOARD_TYPE_74x161_161_32           INES_TO_BOARD_TYPE( 70,  0, 0, 0)
#define BOARD_TYPE_74x139_74               INES_TO_BOARD_TYPE( 87,  0, 0, 0)
#define BOARD_TYPE_PCI556                  INES_TO_BOARD_TYPE( 38,  0, 0, 0)

/* Some boards can't be easily identified by their board names or
   components since we don't necessarily have that info.  These we
   just identify by the iNES mapper number.
*/

#define BOARD_TYPE_NTDEC_112	    	   INES_TO_BOARD_TYPE(112,  0, 0, 0)
#define BOARD_TYPE_INES201                 INES_TO_BOARD_TYPE(201,  0, 0, 0)
#define BOARD_TYPE_BMC_110_IN_1            INES_TO_BOARD_TYPE(255,  0, 0, 0)
#define BOARD_TYPE_BMC_MARIOPARTY_7_IN_1   INES_TO_BOARD_TYPE( 52,  0, 0, 0)
#define BOARD_TYPE_INES36                  INES_TO_BOARD_TYPE( 36,  0, 0, 0)
#define BOARD_TYPE_BMC_1200_IN_1           INES_TO_BOARD_TYPE(200,  0, 0, 0)
#define BOARD_TYPE_BMC_20_IN_1             INES_TO_BOARD_TYPE(231,  0, 0, 0)
#define BOARD_TYPE_BMC_15_IN_1             INES_TO_BOARD_TYPE(205,  0, 0, 0)
#define BOARD_TYPE_22_IN_1                 INES_TO_BOARD_TYPE(230,  0, 0, 0)
#define BOARD_TYPE_BMC_35_IN_1             INES_TO_BOARD_TYPE(203,  0, 0, 0)
#define BOARD_TYPE_BMC_SUPERBIG_7_IN_1     INES_TO_BOARD_TYPE( 44,  0, 0, 0)
#define BOARD_TYPE_BMC_SUPERHIK_4_IN_1     INES_TO_BOARD_TYPE( 49,  0, 0, 0)
#define BOARD_TYPE_BMC_SUPERHIK_8_IN_1     INES_TO_BOARD_TYPE( 45,  0, 0, 0)
#define BOARD_TYPE_GAMESTAR_B              INES_TO_BOARD_TYPE( 58,  0, 0, 0)
#define BOARD_TYPE_BMC_SUPER700IN1         INES_TO_BOARD_TYPE( 62,  0, 0, 0)
#define BOARD_TYPE_UNL_MORTALKOMBAT2       INES_TO_BOARD_TYPE( 91,  0, 0, 0)
#define BOARD_TYPE_BTL_SMB2A               INES_TO_BOARD_TYPE( 40,  0, 0, 0)
#define BOARD_TYPE_BTL_SMB2B               INES_TO_BOARD_TYPE( 50,  0, 0, 0)
#define BOARD_TYPE_BTL_SMB2C               INES_TO_BOARD_TYPE( 43,  0, 0, 0)
#define BOARD_TYPE_BTL_SUPERBROS11         INES_TO_BOARD_TYPE(196,  0, 0, 0)
#define BOARD_TYPE_WHIRLWIND_2706          INES_TO_BOARD_TYPE(108,  0, 0, 0)
#define BOARD_TYPE_UNL_TXC_22211B          INES_TO_BOARD_TYPE(172,  0, 0, 0)
#define BOARD_TYPE_UNL_TXC_22211A          INES_TO_BOARD_TYPE(132,  0, 0, 0)
#define BOARD_TYPE_CNE_DECATHLON           INES_TO_BOARD_TYPE(244,  0, 0, 0)
#define BOARD_TYPE_UNL_LH32                0x1e000
#define BOARD_TYPE_UNL_BB                  0x1e001
#define BOARD_TYPE_UNL_SMB2J               0x1e002
#define BOARD_TYPE_UNL_AC08                0x1e003
#define BOARD_TYPE_UNL_CC_21               0x1e004
#define BOARD_TYPE_UNL_KS7037              0x1e005
#define BOARD_TYPE_UNL_KS7057              0x1e006
#define BOARD_TYPE_MAGICSERIES             INES_TO_BOARD_TYPE(107,  0, 0, 0)
#define BOARD_TYPE_BMC_150_IN_1            INES_TO_BOARD_TYPE(202,  0, 0, 0)
#define BOARD_TYPE_76_IN_1                 INES_TO_BOARD_TYPE(226,  0, 0, 0)
#define BOARD_TYPE_SUBOR_BOARD_1           INES_TO_BOARD_TYPE(166,  0, 0, 0)
#define BOARD_TYPE_SUBOR_BOARD_0           INES_TO_BOARD_TYPE(167,  0, 0, 0)
#define BOARD_TYPE_RCM_GS2015              INES_TO_BOARD_TYPE(216,  0, 0, 0)
#define BOARD_TYPE_RCM_TETRISFAMILY        INES_TO_BOARD_TYPE( 61,  0, 0, 0)

#define BOARD_TYPE_CNE_SHLZ                INES_TO_BOARD_TYPE(240,  0, 0, 0)
#define BOARD_TYPE_TXC_TW                  INES_TO_BOARD_TYPE(189,  0, 0, 0)
#define BOARD_TYPE_CNE_PSB                 INES_TO_BOARD_TYPE(246,  0, 0, 0)
#define BOARD_TYPE_UNL_HOSENKAN            INES_TO_BOARD_TYPE(182,  0, 0, 0)
#define BOARD_TYPE_RESETBASED_4_IN_1       INES_TO_BOARD_TYPE( 60,  0, 0, 0)

#define BOARD_TYPE_RETROUSB_CUFROM         INES_TO_BOARD_TYPE( 29,  0, 0, 0)

/* These are handled internally to the iNES ROM loader because they're
   essentially duplicates or subsets of other mappers.  They're listed
   here so that I can automate extracting the list of supported mappers
   from this file and not need to account for these by hand.

#define BOARD_TYPE_INES_39                 INES_TO_BOARD_TYPE( 39,  0, 0, 0)
#define BOARD_TYPE_INES_79                 INES_TO_BOARD_TYPE( 79,  0, 0, 0)
#define BOARD_TYPE_INES_146                INES_TO_BOARD_TYPE(146,  0, 0, 0)
#define BOARD_TYPE_INES_151                INES_TO_BOARD_TYPE(151,  0, 0, 0)
#define BOARD_TYPE_INES_152                INES_TO_BOARD_TYPE(152,  0, 0, 0)
#define BOARD_TYPE_INES_225                INES_TO_BOARD_TYPE(225,  0, 0, 0)
#define BOARD_TYPE_INES_241                INES_TO_BOARD_TYPE(241,  0, 0, 0)
*/

#endif				/* _BOARD_TYPES_H_ */
