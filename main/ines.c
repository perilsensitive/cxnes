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

#include "emu.h"
#include "db.h"

#define INES_HEADER_SIZE 16

/* FIXME what are all the possible values
   for vs mode? */
#define VS_MODE_NORMAL         0
#define VS_MODE_RBI_BASEBALL   1
#define VS_MODE_TKO_BOXING     2
#define VS_MODE_SUPER_XEVIOUS  3

struct ines_header {
	int prg_rom_banks;
	int chr_rom_banks;
	int mapper;
	int submapper;
	int wram_size;
	int nv_wram_size;
	int vram_size;
	int nv_vram_size;
	int vs_ppu_version;
	int vs_protection;

	int mirroring;
	int four_screen;
	int trainer;
	int battery;
	int playchoice;
	int vs_system;
	int tv_system;
	int tv_system_both;
	int version;
};

static const char ines_id[4] = { 'N', 'E', 'S', 0x1a };

/* Header garbage strings that have been spotted in the wild. */
static const char *garbage[] = {
	"DiskDude!",
	"DisNi0330",		/* looks like DiskDude! got overwritten by Ni0330 */
	"demiforce",
	"iskDude!",		/* Sometimes a ROM will be "fixed" by overwriting the first 'D'
				   with a 0x00 byte.
				 */
	"vimm.net",		/* Unconfirmed (usually, I've seen this at the end of the file) */
	"rinkaku",
	"GitM-DX",		/* Unconfirmed */
	"Ni0330",
	"NI2.1",
	"aster",
	"sl1me",
	"NeCo/WIN",
	"NI 1.3",		/* FIXME also "NI\x001.3", not sure how to check that */
	"NI1.3",
	"Turk",
	"MJR",
	NULL,
};

struct ines_board_map {
	int mapper;
	int submapper;
	const char *board;
};

int fixup_header(uint32_t board_type, struct ines_header *header)
{
	return board_type;
}

/* Tries to remove garbage from iNES headers. The argument must point to
   a buffer at least 16 bytes in size. Return value is a boolean that indicates
   whether or not the header had to be cleaned up.
*/
static int remove_garbage_from_header(uint8_t * buffer)
{
	int i;
	int max_len;

	max_len = 0;
	for (i = 0; garbage[i]; i++) {
		int len = strlen(garbage[i]);
		if (memcmp(buffer + 16 - len, garbage[i], len) == 0) {
			/* This should handle the case where multiple
			   strings match.  Some strings spotted in the
			   wild look like one known string that
			   overwrote the end of another, longer known
			   string.
			 */
			if (len > max_len)
				max_len = len;
		}
	}

	/* If we didn't find a match, fall back on the heuristic of
	   checking the last two (if NES2.0) or four (if iNES) header
	   bytes are non-zero, and setting the last 8 or 9 bytes
	   respectively to 0 if true. NES2.0 only gets 8 bytes set
	   because we assume that no garbage string will use a value
	   in buffer[7] that can be mistaken for the NES2.0 magic.
	   If true, then buffer[7] is still valid and should be
	   preserved since otherwise the upper mapper nibble gets
	   wiped out.

	   NES2.0 roms should only have garbage if they were patched
	   using patches that put garbage in the header.  There are
	   enough romhacks that do this that it's worth checking for.
	 */
	if (!max_len) {
		if ((buffer[7] & 0x0c) == 0x08) {
			if (buffer[14] || buffer[15])
			    max_len = 8;
		} else {
			if (buffer[12] || buffer[13] ||
			    buffer[14] || buffer[15]) {
				max_len = 9;
			}
		}
	}

	if (max_len) {
		memset(buffer + 16 - max_len, 0, max_len);

		/* Force fallback to iNES */
		buffer[7] &= 0xf3;
	}

	return max_len;
}

static int parse_ines_header(struct config *config,
			     uint8_t * data, struct ines_header *header)
{
	if (memcmp(data, ines_id, sizeof(ines_id)) != 0)
		return -1;

	if (!header)
		return -1;

	memset(header, 0, sizeof(*header));

	remove_garbage_from_header(data);

	if ((data[7] & 0x0c) == 0x08) {
		header->version = 2;
	} else {
		header->version = 1;
	}

	/* Basic iNES 1.0 parsing */
	header->prg_rom_banks = data[4];
	header->chr_rom_banks = data[5];
	header->mirroring = data[6] & 0x01;
	header->battery = data[6] & 0x02;
	header->trainer = data[6] & 0x04;
	header->four_screen = data[6] & 0x08;
	header->mapper = (data[6] & 0xf0) >> 4;
	header->mapper |= data[7] & 0xf0;
	header->playchoice = data[7] & 0x02;
	header->vs_system = data[7] & 0x01;
	header->wram_size = 0;
	header->nv_wram_size = 0;
	header->vram_size = 0;
	header->nv_vram_size = 0;
	header->vs_protection = 0;
	header->vs_ppu_version = 0;
	header->tv_system = 0;

	/* If this appears to be an NES 2.0 ROM and the battery flag
	   is set but no non-volatile WRAM or VRAM is specified,
	   revert to standard iNES.
	*/
	if ((header->version == 2) && (header->battery) &&
	    !(data[10] & 0xf0) && !(data[11] & 0xf0))
	{
		header->version = 1;
	}

	/* NES 2.0 format */
	if (header->version == 2) {
		header->prg_rom_banks |= (data[9] & 0x0f) << 8;
		header->chr_rom_banks |= (data[9] & 0xf0) << 4;
		if (data[10] & 0x0f)
			header->wram_size = 1 << ((data[10] & 0x0f) + 6);
		if (data[10] & 0xf0)
			header->nv_wram_size =
			    1 << (((data[10] & 0xf0) >> 4) + 6);
		if (data[11] & 0x0f)
			header->vram_size = 1 << ((data[11] & 0x0f) + 6);
		if (data[11] & 0xf0)
			header->nv_vram_size =
			    1 << (((data[11] & 0xf0) >> 4) + 6);
		header->submapper = (data[8] & 0xf0) >> 4;
		header->mapper |= (data[8] & 0x0f) << 8;

		header->tv_system = (data[12] & 0x01);
		header->tv_system_both = (data[12] & 0x02);

		if (header->vs_system) {
			header->vs_ppu_version = data[13] & 0x0f;
			header->vs_protection = (data[13] & 0xf0) >> 4;
		}
	} else {
		if (header->battery)
			header->nv_wram_size = data[8] * SIZE_8K;
		else
			header->wram_size = data[8] * SIZE_8K;

		header->tv_system = data[9] & 0x01;
	}

	return 0;
}

static uint8_t nes2_fix_size(size_t size)
{
	int i;

	if (!size)
		return 0;

	for (i = 1; i < 15; i++) {
		if (size <= (1 << (i + 6)))
			break;
	}

	return i;
}

int ines_generate_header(struct rom *rom, int version)
{
	struct ines_header header;
	size_t prg_size, chr_size;
	size_t wram_size, nv_wram_size;
	size_t vram_size, nv_vram_size;
	uint8_t *data;
	int i;

	data = rom->buffer;

	/* If we can't convert this type an iNES mapper/submapper,
	   abort.
	*/
	if (!BOARD_TYPE_IS_INES(rom->info.board_type))
		return -1;

	memset(&header, 0, sizeof(header));

	prg_size = 0;
	chr_size = 0;

	for (i = 0; i < rom->info.prg_size_count; i++) {
		prg_size += rom->info.prg_size[i];
	}

	for (i = 0; i < rom->info.chr_size_count; i++) {
		chr_size += rom->info.chr_size[i];
	}

	prg_size += rom->offset - INES_HEADER_SIZE;
	/* PRG data must be a multiple of 16K */
	if (prg_size % SIZE_16K)
		return -1;

	/* CHR data must be a multiple of 8K */
	if (chr_size % SIZE_8K)
		return -1;

	header.prg_rom_banks = prg_size / SIZE_16K;
	header.chr_rom_banks = chr_size / SIZE_8K;

	switch (rom->info.mirroring) {
	case MIRROR_V:
		header.mirroring = 0x01;
		break;
	case MIRROR_4:
		header.four_screen = 0x08;
		break;
	case MIRROR_1A:
	case MIRROR_1B:
	case MIRROR_M:
	case MIRROR_H:
	case MIRROR_UNDEF:
		break;
	}

	wram_size = 0;
	vram_size = 0;
	nv_vram_size = 0;
	nv_wram_size = 0;

	if (rom->info.flags & ROM_FLAG_WRAM0_NV)
		nv_wram_size += rom->info.wram_size[0];
	else
		wram_size += rom->info.wram_size[0];

	if (rom->info.flags & ROM_FLAG_WRAM1_NV)
		nv_wram_size += rom->info.wram_size[1];
	else
		wram_size += rom->info.wram_size[1];

	if (rom->info.flags & ROM_FLAG_VRAM0_NV)
		nv_vram_size += rom->info.vram_size[0];
	else
		vram_size += rom->info.vram_size[0];

	if (rom->info.flags & ROM_FLAG_VRAM1_NV)
		nv_vram_size += rom->info.vram_size[1];
	else
		vram_size += rom->info.vram_size[1];

	if (rom->info.flags & ROM_FLAG_MAPPER_NV) {
		nv_wram_size =
			board_info_get_mapper_ram_size(rom->board_info);
	}

	header.nv_wram_size = nes2_fix_size(nv_wram_size);
	header.wram_size = nes2_fix_size(wram_size);
	header.nv_vram_size = nes2_fix_size(nv_vram_size);
	header.vram_size = nes2_fix_size(vram_size);

	if ((header.nv_wram_size == 0x0f) ||
	    (header.wram_size == 0x0f) ||
	    (header.nv_vram_size == 0x0f) ||
	    (header.vram_size == 0x0f)) {
		return -1;
	}

	if (rom->info.flags & ROM_FLAGS_NV)
		header.battery = 0x02;

	if ((rom->info.system_type == EMU_SYSTEM_TYPE_UNDEFINED) ||
	    (rom->info.system_type == EMU_SYSTEM_TYPE_AUTO)) {
		return -1;
	} else if (rom->info.system_type < EMU_SYSTEM_TYPE_FAMICOM) {
		header.vs_system = 0x01;
		header.vs_ppu_version = rom->info.system_type;
	} else if (rom->info.system_type == EMU_SYSTEM_TYPE_PLAYCHOICE) {
		header.playchoice = 0x02;
	} else if (rom->info.system_type == EMU_SYSTEM_TYPE_PAL_NES) {
		header.tv_system = 0x01;
	}

	header.mapper = BOARD_TYPE_TO_INES_MAPPER(rom->info.board_type);
	header.submapper = BOARD_TYPE_TO_INES_SUBMAPPER(rom->info.board_type);
	header.vs_protection =
		BOARD_TYPE_TO_INES_VS_PROTECTION(rom->info.board_type);

	if ((header.prg_rom_banks > 0xff) ||
	    (header.chr_rom_banks > 0xff) ||
	    (header.mapper > 0xff) ||
	    (header.vs_protection > 0x00) ||
	    (header.tv_system) ||
	    (header.vs_system) ||
	    (header.submapper > 0x00)) {
		version = 2;
	} else {
		version = 1;
	}
	

	if (version == 2)
		header.version = 0x08;

	data[0]  = 'N';
	data[1]  = 'E';
	data[2]  = 'S';
	data[3]  = 0x1a;
	data[4]  = header.prg_rom_banks & 0xff;
	data[5]  = header.chr_rom_banks & 0xff;
	data[6]  = (header.mapper & 0x00f) << 4;
	data[6] |= header.mirroring | 1;
	data[6] |= header.four_screen;
	data[6] |= header.trainer;
	data[6] |= header.battery;
	data[7]  = header.mapper & 0x0f0;
	data[7] |= header.version;
	data[7] |= header.playchoice;
	data[7] |= header.vs_system;
	if (version == 2) {
		data[8]  = (header.mapper & 0xf00) >> 8;
		data[8] |= header.submapper << 4;
		data[9]  = (header.prg_rom_banks & 0xf00) >> 8;
		data[9] |= (header.chr_rom_banks & 0xf00) >> 4;
		data[10]  = header.wram_size;
		data[10] |= header.nv_wram_size << 4;
		data[11]  = header.vram_size;
		data[11] |= header.nv_vram_size << 4;
		data[12]  = header.tv_system;
		data[13]  = header.vs_ppu_version;
		data[13] |= header.vs_protection;
		data[14] = 0x00;
		data[15] = 0x00;
	} else {
		memset(data + 8, 0, 8);
	}

	return 0;
}

int ines_load(struct emu *emu, struct rom *rom)
{
	uint32_t board_type;
	struct ines_header header;
	size_t prg_size, chr_size;
	size_t wram_size[2];
	size_t vram_size[2];
	int wram_nv[2];
	int vram_nv[2];
	int mapper_ram_nv;
	int system_type;
	int mirroring;
	int total_wram_size;
	int total_chr_size;
	struct board_info *info;

	if (rom->buffer_size < INES_HEADER_SIZE) {
		return -1;
	}

	if (parse_ines_header(emu->config, rom->buffer, &header) != 0) {
		return -1;
	}

	mirroring = MIRROR_UNDEF;

	wram_size[0] = wram_size[1] = 0;
	wram_nv[0] = wram_nv[1] = 0;
	vram_size[0] = vram_size[1] = 0;
	vram_nv[0] = vram_nv[1] = 0;
	mapper_ram_nv = 0;

	prg_size = header.prg_rom_banks * SIZE_16K;
	chr_size = header.chr_rom_banks * SIZE_8K;

	if (rom->buffer_size < (prg_size + chr_size))
		return 1;

	switch (header.mapper) {
	case 185:
		if (header.submapper) {
			header.submapper &= 0x03;
			header.submapper |= 0x04;
		}
		break;
	case 39:
		/* Subor Study & Game 32-in-1 is oversized BNROM */
		header.mapper = 34;
		header.submapper = 2;
		break;
	case 241:
		header.mapper = 34;
		header.submapper = 0;
		break;
	case 152:
		header.mapper = 70;
		mirroring = MIRROR_M;
		break;
	case 99:
		header.vs_system = 1;
		break;
	case 151:
		/* VRC1 with enforce 4-screen mirroring */
		header.mapper = 75;
		header.vs_system = 1;
		break;
	case 34:
		/* BNROM vs. NINA-001 */
		if (!header.submapper && header.chr_rom_banks)
			header.submapper = 1;
		else if (!header.submapper)
			header.submapper = 2;
		break;
	case 79:
	case 146:
		header.mapper = 113;
		break;
	case 78:
		if (!header.submapper)
			header.submapper = 1;
		break;
	}

	if (header.vs_system)
		header.four_screen = 1;

	board_type = INES_TO_BOARD_TYPE(header.mapper,
					header.submapper,
					header.vs_protection, 0);

	total_wram_size = header.wram_size + header.nv_wram_size;
	total_chr_size = header.vram_size + header.nv_vram_size +
		header.chr_rom_banks * SIZE_8K;

	switch (board_type) {
	case BOARD_TYPE_VS_UNISYSTEM:
		if (header.prg_rom_banks > 2)
			board_type = BOARD_TYPE_VS_GUMSHOE;
		break;
	case BOARD_TYPE_NROM:
		if (header.prg_rom_banks == 3)
			board_type = BOARD_TYPE_NROM368;
		break;
	case BOARD_TYPE_ExROM:
		if (header.version == 1)
			board_type = BOARD_TYPE_ExROM_COMPAT;
		break;
	case BOARD_TYPE_UNROM_512:
		if (header.battery)
			board_type = BOARD_TYPE_UNROM_512_FLASH;
		break;
	case BOARD_TYPE_SxROM:
		if (total_chr_size > SIZE_8K)
			break;

		if (header.prg_rom_banks == 32) {
			if ((total_wram_size == 0) &&
			    (header.version == 1)) {
				board_type = BOARD_TYPE_SxROM_COMPAT;
			} else if (total_wram_size > SIZE_8K) {
				board_type = BOARD_TYPE_SXROM;
			} else {
				board_type = BOARD_TYPE_SUROM;
			}
		} else if (total_wram_size > SIZE_16K) {
			board_type = BOARD_TYPE_SXROM;
		} else if (total_wram_size == SIZE_16K) {
			board_type = BOARD_TYPE_SOROM;
		} else if (header.version == 1) {
			board_type = BOARD_TYPE_SxROM_COMPAT;
		}
		break;
	}

	info = board_info_find_by_type(board_type);

	/* PRG RAM hack for iNES: give the board 8K of NVRAM if
	   battery present but no NVRAM specified yet.  Otherwise, if
	   no battery present and auto_wram is enabled, give the
	   board 8K of RAM if no RAM present. */
	if ((header.version == 1) && (emu->config->auto_wram)) {
		if (!header.wram_size)
			header.wram_size = SIZE_8K;
	}

	if (header.mapper == 218) {
		if (header.four_screen) {
			if (header.mirroring)
				mirroring = MIRROR_1B;
			else
				mirroring = MIRROR_1A;
		} else {
			if (header.mirroring)
				mirroring = MIRROR_V;
			else
				mirroring = MIRROR_H;
		}
	} else if (header.mapper == 30) {
		if (header.four_screen)
			mirroring = MIRROR_M;
		else if (header.mirroring)
			mirroring = MIRROR_V;
		else
			mirroring = MIRROR_H;
	} else if (board_info_get_flags(info) &
		   BOARD_INFO_FLAG_MIRROR_M) {
		if (header.four_screen)
			mirroring = MIRROR_4;
		else
			mirroring = MIRROR_M;
	} else if (mirroring == MIRROR_UNDEF) {
		if (header.four_screen)
			mirroring = MIRROR_4;
		else if (header.mirroring)
			mirroring = MIRROR_V;
		else
			mirroring = MIRROR_H;
	}

	if (header.vs_system)
		system_type =
			vs_ppu_type_to_system_type(header.vs_ppu_version);
	else if (header.playchoice)
		system_type = EMU_SYSTEM_TYPE_PLAYCHOICE;
	else if (header.tv_system == 1)
		system_type = EMU_SYSTEM_TYPE_PAL_NES;
	else
		system_type = EMU_SYSTEM_TYPE_NES;

	if (board_type == BOARD_TYPE_ExROM) {
		if ((header.wram_size > SIZE_32K) ||
		    (header.nv_wram_size > SIZE_32K)) {
			wram_size[0] = wram_size[1] = SIZE_32K;
			if (header.nv_wram_size)
				wram_nv[0] = wram_nv[1] = 1;
		} else if (header.nv_wram_size) {
			wram_size[0] = header.nv_wram_size;
			wram_size[1] = header.wram_size;
			wram_nv[0] = 1;
		} else {
			/* No NV RAM specified, size of RAM <= 32K */
			wram_size[0] = header.wram_size;
		}
	} else if (board_type == BOARD_TYPE_NAMCO_163) {
		/* Namco 163 has 128 bytes of internal RAM for the
		   audio chip that can also be battery-backed.  If the
		   first prg-ram size is 128 bytes, it is really
		   referring to the internal ram.  This RAM is always
		   present.

		   Otherwise, it behaves like all of the other mappers
		   with 8K MAX of RAM.
		*/
		   if (header.nv_wram_size == 128) {
			   header.nv_wram_size = 0;
			   header.wram_size = 0;
			   mapper_ram_nv = 1;
			   wram_nv[0] = 0;
			   wram_nv[1] = 0;
			   wram_size[0] = 0;
			   wram_size[1] = 0;
		   } else if (header.wram_size == 128) {
			   header.nv_wram_size = 0;
			   header.wram_size = 0;
			   mapper_ram_nv = 0;
			   wram_nv[0] = 0;
			   wram_nv[1] = 0;
			   wram_size[0] = 0;
			   wram_size[1] = 0;
		   }

		   if (header.nv_wram_size) {
			   wram_size[0] = header.nv_wram_size;
			   wram_nv[0] = 1;
		   } else {
			   wram_size[0] = header.wram_size;
			   wram_nv[0] = 0;
		   }

		   wram_size[1] = 0;
		   wram_nv[1] = 0;
	} else {
		/* Most boards have only one PRG RAM chip,
		   and it may be battery-backed */
		if (header.nv_wram_size)
			wram_size[0] = header.nv_wram_size;
		else
			wram_size[0] = header.wram_size;

		wram_size[1] = 0;
	}

	if (header.nv_vram_size) {
		vram_size[0] = header.nv_vram_size;
		vram_size[1] = header.vram_size;
	} else {
		vram_size[0] = header.vram_size;
		vram_nv[0] = 0;
		vram_size[1] = 0;
	}

	/* Set everything but ROM sizes here,
	   as db_rom_load will mess with those.
	*/
	rom->info.system_type = system_type;
	rom->info.board_type = board_type;
	rom->info.mirroring = mirroring;
	rom->info.wram_size[0] = wram_size[0];
	rom->info.wram_size[1] = wram_size[1];
	rom->info.vram_size[0] = vram_size[0];
	rom->info.vram_size[1] = vram_size[1];

	if (!header.vs_system && !header.playchoice &&
	    emu->config->guess_region_from_filename) {
		int trust_timing_info;

		if ((header.version == 1) && (!header.tv_system))
			trust_timing_info = 0;
		else
			trust_timing_info = 1;

		rom_guess_system_type_from_filename(rom, trust_timing_info);
	}


	/* Try loading based on a database match first.  This way
	   even if the header data is invalid (unknown mapper, incorrect
	   size, etc.) the rom may still load.
	*/
	if (!db_rom_load(emu, rom)) {
		return 0;
	}

	rom->info.total_prg_size = prg_size;
	rom->info.total_chr_size = chr_size;
	rom->info.prg_size[0] = prg_size;
	rom->info.chr_size[0] = chr_size;

	rom->info.prg_size_count = 1;
	if (chr_size)
		rom->info.chr_size_count = 1;

	if (wram_nv[0])
		rom->info.flags |= ROM_FLAG_WRAM0_NV;

	if (wram_nv[1])
		rom->info.flags |= ROM_FLAG_WRAM1_NV;

	if (vram_nv[0])
		rom->info.flags |= ROM_FLAG_VRAM0_NV;

	if (vram_nv[1])
		rom->info.flags |= ROM_FLAG_VRAM1_NV;

	if (mapper_ram_nv)
		rom->info.flags |= ROM_FLAG_MAPPER_NV;

	if (board_set_rom_board_type(rom, board_type, header.battery ||
				     header.nv_wram_size ||
				     header.nv_vram_size)) {
		int old_submapper = header.submapper;
		int invalid = 1;

		if (header.submapper) {
			/* Try the same mapper with submapper 0 as a fallback */
			board_type = INES_TO_BOARD_TYPE(header.mapper, 0,
							header.vs_protection, 0);

			if (!board_set_rom_board_type(rom, board_type,
						      header.battery)) {
				log_err("Submapper %d unsupported, falling "
					"back to submapper 0.\n", header.submapper);
				invalid = 0;
			}
		}

		if (invalid) {
			err_message("Unsupported iNES mapper %d.%d\n",
				    header.mapper, old_submapper);
			return 1;
		}
	}

	rom->offset = 16;
	rom_calculate_checksum(rom);

	return 0;
}
