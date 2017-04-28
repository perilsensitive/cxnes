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

#ifndef __ROM_H__
#define __ROM_H__

#include "emu.h"
#include "text_buffer.h"

struct emu;
struct config;
struct board_info;

#define MAX_PRG_ROMS         4
#define MAX_CHR_ROMS         2
#define MAX_ROMS (MAX_PRG_ROMS + MAX_CHR_ROMS)

#define ROM_FLAG_WRAM0_NV    1
#define ROM_FLAG_WRAM1_NV    2
#define ROM_FLAG_VRAM0_NV    4
#define ROM_FLAG_VRAM1_NV    8
#define ROM_FLAG_MAPPER_NV  16
#define ROM_FLAG_IN_DB      32
#define ROM_FLAG_PLAYCHOICE 64
#define ROM_FLAGS_NV (ROM_FLAG_WRAM0_NV|ROM_FLAG_WRAM1_NV|ROM_FLAG_VRAM0_NV|\
		      ROM_FLAG_VRAM1_NV|ROM_FLAG_MAPPER_NV)
#define ROM_FLAG_HAS_CRC   128
#define ROM_FLAG_HAS_SHA1  256
#define ROM_FLAG_TIMING_NTSC  512
#define ROM_FLAG_TIMING_PAL   1024
#define ROM_FLAG_TIMING_DENDY 2048
#define ROM_FLAG_TIMING_MASK (512|1024|2048)
#define ROM_FLAG_TIMING_ALL (512|1024|2048)
#define ROM_FLAG_TIMING_PAL_NTSC (512|1024)

struct rom_info {
	/* char *title; */
	char *name;
	size_t total_prg_size;
	size_t total_chr_size;
	uint32_t wram_size[2];
	uint32_t vram_size[2];
	uint32_t prg_crc[MAX_PRG_ROMS];
	uint32_t chr_crc[MAX_CHR_ROMS];
	uint8_t prg_sha1[MAX_PRG_ROMS][20];
	uint8_t chr_sha1[MAX_CHR_ROMS][20];
	uint32_t prg_size[MAX_PRG_ROMS];
	uint32_t chr_size[MAX_CHR_ROMS];
	uint32_t board_type;
	uint32_t combined_crc;
	uint8_t system_type;
	uint8_t four_player_mode;
	uint8_t vs_controller_mode;
	uint8_t auto_device_id[5];
	uint8_t combined_sha1[20];
	uint8_t prg_crc_count;
	uint8_t chr_crc_count;
	uint8_t prg_size_count;
	uint8_t chr_size_count;
	uint8_t prg_sha1_count;
	uint8_t chr_sha1_count;
	int mirroring;
	int flags;

	struct rom_info *next;
};

struct rom {
	struct rom_info info;
	char *filename;
	char *compressed_filename;
	uint8_t *buffer;
	size_t buffer_size;
	struct board_info *board_info;
	int offset;
	int disk_side_size;
};

extern void print_rom_info(struct rom *rom);
extern int rom_calculate_checksum(struct rom *rom);
extern struct rom *rom_load_file(struct emu *emu, const char *filename);
extern struct rom *rom_reload_file(struct emu *emu, struct rom *rom);
extern int rom_apply_patches(struct rom *rom, int count, char **patchfiles, int from_archive);
extern char **rom_find_autopatches(struct config *, struct rom *rom);
extern char **rom_find_archive_autopatches(struct config *, struct rom *rom);
extern void rom_get_info(struct rom *rom, struct text_buffer *tbuffer);

extern struct rom *rom_alloc(const char *filename, uint8_t *buffer, size_t size);
extern void rom_free(struct rom *);
extern void rom_guess_system_type_from_filename(struct rom *rom, int trust_timing);
extern int rom_load_file_data(struct emu *emu, const char *filename,
                              struct rom **romptr, uint8_t *buffer,
                              size_t size);
#endif				/* __ROM_H__ */
