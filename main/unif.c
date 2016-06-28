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

#include <errno.h>
#include <ctype.h>

#include "emu.h"
#include "file_io.h"
#include "db.h"

#define UNIF_HEADER_SIZE 32
#define UNIF_MAX_CHUNKS  32

enum {
	CTRL_BIT_CONTROLLER = 1,
	CTRL_BIT_ZAPPER = 2,
	CTRL_BIT_ROB = 4,
	CTRL_BIT_ARKANOID = 8,
	CTRL_BIT_POWERPAD = 16,
	CTRL_BIT_FOURSCORE = 32,
};

static const char unif_id[4] = { 'U', 'N', 'I', 'F' };

static int unif_lookup_board_type(struct rom *rom, const char *mapr, int battery)
{
	if (board_set_rom_board_type_by_name(rom, mapr, battery)) {
		err_message("Unknown board type %s\n", mapr);
	    return 1;
	}

	return 0;
}

int unif_load(struct emu *emu, struct rom *rom)
{
	size_t chunk_size[UNIF_MAX_CHUNKS];
	off_t chunk_offset[UNIF_MAX_CHUNKS];
	size_t total_prg_size;
	size_t total_chr_size;
	uint8_t *rom_data, *p;
	char *mapr;
	int mirroring;
	int tv_system;
	int battery;
	int ctrl;
	int rc, i;
	
	mapr = NULL;
	total_prg_size = 0;
	total_chr_size = 0;
	mirroring = MIRROR_V;
	tv_system = 0;
	battery = 0;
	ctrl = 0;
	memset(chunk_offset, 0, sizeof(chunk_offset));
	memset(chunk_size, 0, sizeof(chunk_size));

	/* default to failure */
	rc = -1;

	if (rom->buffer_size < UNIF_HEADER_SIZE) {
		rc = -1;
		goto cleanup;
	}

	/* Check header magic */
	if (memcmp(unif_id, rom->buffer, 4) != 0) {
		rc = -1;
		goto cleanup;
	}

	rc = 1;

	/* FIXME version check? */

	p = rom->buffer + UNIF_HEADER_SIZE;
	while (p < rom->buffer + rom->buffer_size) {
		size_t size;

		size = p[4];
		size |= (p[5] <<  8);
		size |= (p[6] << 16);
		size |= (p[7] << 24);

		if ((p + size + 8) > (rom->buffer + rom->buffer_size)) {
			log_err("invalid chunk size %d for "
				"chunk %c%c%c%c\n", (int)size,
				p[0], p[1], p[2], p[3]);
			goto cleanup;
		}

		if (memcmp(p, "MAPR", 4) == 0) {
			mapr = malloc(size + 1);
			if (mapr) {
				strncpy(mapr, (char *)(p + 8),
					size);
				mapr[size] = '\0';
			}
		} else if (memcmp(p, "MIRR", 4) == 0) {
			if (size < 1) {
				log_err("%c%c%c%c size must be at least 1\n",
					p[0], p[1], p[2], p[3]);
				goto cleanup;
			}
			mirroring = p[8];

			switch(mirroring) {
			case 0: mirroring = MIRROR_H;  break;
			case 1: mirroring = MIRROR_V;  break;
			case 2: mirroring = MIRROR_1A; break;
			case 3: mirroring = MIRROR_1B; break;
			case 4: mirroring = MIRROR_4;  break;
			case 5: mirroring = MIRROR_M;  break;
			default:
				log_err("invalid mirroring type %d\n",
					mirroring);
				mirroring = MIRROR_V;
			}
		} else if (memcmp(p, "TVCI", 4) == 0) {
			if (size < 1) {
				log_err("%c%c%c%c size must be at least 1\n",
					p[0], p[1], p[2], p[3]);
				goto cleanup;
			}

			tv_system = *(p + 8);
		} else if (memcmp(p, "BATR", 4) == 0) {
			battery = 1;
		} else if (memcmp(p, "CTRL", 4) == 0) {
			if (size < 1) {
				log_err("%c%c%c%c size must be at least 1\n",
					p[0], p[1], p[2], p[3]);
				goto cleanup;
			}

			ctrl = *(p + 8);
		} else if ((memcmp(p, "PRG", 3) == 0) ||
			   (memcmp(p, "CHR", 3) == 0)) {
			char c[2];
			int chunk;
			char *end;

			errno = 0;
			c[0] = p[3];
			c[1] = '\0';
			chunk = strtol(c, &end, 16);
			if (errno || *end) {
				log_err("invalid chunk id %c%c%c%c\n",
					p[0], p[1], p[2], p[3]);
			} else {
				if (p[0] == 'P') {
					total_prg_size += size;
				} else {
					chunk += 16;
					total_chr_size += size;
				}

				chunk_offset[chunk] = (p - rom->buffer) + 8;
				chunk_size[chunk] = size;
			}
		}

		/* Unsupported chunks are skipped */
		p += size + 8;
	}

	rom_data = malloc(16 + total_prg_size + total_chr_size);
	if (!rom_data)
		goto cleanup;

	memset(rom_data, 0, 16);

	p = rom_data + 16;
	/* Copy chunks in order */
	for (i = 0; i < UNIF_MAX_CHUNKS; i++) {
		if (!chunk_size[i])
			continue;
		
		memcpy(p, rom->buffer + chunk_offset[i], chunk_size[i]);
		p += chunk_size[i];
	}

	rom->buffer_size = 16 + total_prg_size + total_chr_size;
	free(rom->buffer);
	rom->buffer = rom_data;

	rom->info.total_prg_size = total_prg_size;
	rom->info.total_chr_size = total_chr_size;
	rom->info.prg_size[0] = total_prg_size;
	rom->info.chr_size[0] = total_chr_size;
	rom->info.prg_size_count = 1;
	if (rom->info.total_chr_size)
		rom->info.chr_size_count = 1;

	rom->info.mirroring = mirroring;
	if (unif_lookup_board_type(rom, mapr, battery))
		return 1;

	if (ctrl & CTRL_BIT_ZAPPER)
		rom->info.auto_device_id[1] = IO_DEVICE_ZAPPER_2;

	if (ctrl & CTRL_BIT_ARKANOID)
		rom->info.auto_device_id[1] = IO_DEVICE_ARKANOID_NES_2;

	if (ctrl & CTRL_BIT_POWERPAD)
		rom->info.auto_device_id[1] = IO_DEVICE_POWERPAD_B2;

	if (ctrl & CTRL_BIT_FOURSCORE)
		rom->info.four_player_mode = FOUR_PLAYER_MODE_NES;

	/* FIXME R.O.B. not currently supported */

	switch(tv_system) {
	case 0:
		rom->info.system_type = EMU_SYSTEM_TYPE_NES;
		break;
	case 2:
		rom->info.system_type = EMU_SYSTEM_TYPE_NES;
		rom->info.flags |= ROM_FLAG_PAL_NTSC;
		break;
	case 1:
		rom->info.system_type = EMU_SYSTEM_TYPE_PAL_NES;
		break;
	default:
		log_warn("invalid TV standard type %d, "
			 "falling back to NTSC\n", tv_system);
		rom->info.system_type = EMU_SYSTEM_TYPE_NES;
	}

	if (emu->config->guess_system_type_from_filename)
		rom_guess_system_type_from_filename(rom, 1);

	rom->offset = 16;
	rom_calculate_checksum(rom);
	db_lookup(rom, NULL);

	/* Return success */
	rc = 0;

cleanup:
	if (mapr)
		free(mapr);

	return rc;
}
