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

#include "emu.h"
#include "file_io.h"

#define NSF_REGION_PAL      0x01
#define NSF_REGION_BOTH     0x02
#define NSF_REGION_RESERVED 0xfc

#define NSF_PLAYER_SIZE SIZE_4K

struct nsf_header {
	uint8_t id[5];
	uint8_t version;
	uint8_t song_count;
	uint8_t starting_song;
	uint8_t load_address[2];
	uint8_t init_address[2];
	uint8_t play_address[2];
	char title[32];
	char artist[32];
	char copyright[32];
	uint8_t ntsc_speed[2];
	uint8_t bankswitch_init[8];
	uint8_t pal_speed[2];
	uint8_t region;
	uint8_t expansion_sound;
	uint8_t reserved[4];
} __attribute__ ((packed));

const char *nsf_expansion_audio_chips[] = {
	"VRC6", "VRC7", "FDS", "MMC5", "Namco 163", "Sunsoft 5B"
};

static const char nsf_id[5] = { 'N', 'E', 'S', 'M', 0x1a };

int nsf_load(struct emu *emu, struct rom *rom)
{
	struct nsf_header header;
	size_t new_buffer_size;
	char *nsf_player_rom;
	uint8_t *tmp;
	int padding;
	int bankswitch;
	int i;
	uint8_t *nsf_data;
	int new_starting_song;

	if (rom->buffer_size < sizeof(header))
		return -1;

	if (memcmp(rom->buffer, nsf_id, sizeof(nsf_id)) != 0)
		return -1;

	memcpy(&header, rom->buffer, sizeof(header));

	padding = 0;
	bankswitch = 0;

	for (i = 0; i < 8; i++) {
		if (header.bankswitch_init[i]) {
			bankswitch = 1;
			break;
		}
	}

	/* Non-bankswitched files are converted to bankswitched ones,
	   so padding is always checked for.
	*/
	padding = ((header.load_address[1] << 8) |
		   (header.load_address[0])) & 0xfff;


	/* If this file isn't bankswitched, then set up the initial
	   bank mappings as if it were. */
	if (!bankswitch) {
		int bank = 8 - ((header.load_address[1] >> 4) & 0x07);
		int j;

		for (j = 0; j < 8; j++) {
			header.bankswitch_init[j] = bank++ & 0x07;
		}
	}

	new_buffer_size = rom->buffer_size - sizeof(header) + padding;

	/* NSF data should be padded to 32K if < 32K in size, or
	   the nearest multiple of 4K if > 32K in size. The padding
	   before the start of the data is included in this.
	 */
	if (new_buffer_size < SIZE_32K)
		new_buffer_size = SIZE_32K;
	else if (new_buffer_size % SIZE_4K)
		new_buffer_size = ((new_buffer_size / SIZE_4K) + 1) * SIZE_4K;

	/* Add size of player ROM */
	new_buffer_size += sizeof(header) + NSF_PLAYER_SIZE + 16;

	tmp = malloc(new_buffer_size);
	if (!tmp) {
		log_err("nsf_load: %s: failed to allocate memory\n",
			rom->filename);
		return 1;
	}

	memset(tmp, 0, new_buffer_size);
	memcpy(tmp + NSF_PLAYER_SIZE + 16, rom->buffer, rom->buffer_size);
	free(rom->buffer);
	rom->buffer = tmp;

	nsf_player_rom = config_get_nsf_rom(emu->config);
	if (!nsf_player_rom) {
		err_message("Unable to locate NSF player ROM\n");
		return 1;
	}
	
	if (readfile(nsf_player_rom, rom->buffer + 16, NSF_PLAYER_SIZE)) {
		err_message("Failed to load NSF player ROM\n");
		free(nsf_player_rom);
		return 1;
	}
	free(nsf_player_rom);

	nsf_data = rom->buffer + NSF_PLAYER_SIZE + 16;

	if (padding) {
		memmove(nsf_data + sizeof(header) + padding,
			nsf_data + sizeof(header),
			rom->buffer_size - sizeof(header));
		memset(nsf_data + sizeof(header), 0, padding);
	}

	new_starting_song = emu->config->nsf_first_track;
	if (new_starting_song && new_starting_song <= header.song_count)
		header.starting_song = new_starting_song;
	/* Overwrite the header in case we patched it up */
	memcpy(nsf_data, &header, sizeof(header));

	rom->buffer_size = new_buffer_size;

	/* Load the NSF header into the prg rom */
	memcpy(rom->buffer + 0x180 + 16, nsf_data, 0x80);

	rom->info.board_type = BOARD_TYPE_NSF;

	/* if BOTH, prefer NTSC unless forced to PAL */
	if (header.region & NSF_REGION_PAL)
		rom->info.system_type = EMU_SYSTEM_TYPE_PAL_NES;
	else
		rom->info.system_type = EMU_SYSTEM_TYPE_NES;

	rom->info.mirroring = MIRROR_V;
	/* Now all of the NSF data can just be copied straight into PRG RAM */
	rom->info.vram_size[0] = SIZE_8K;
	rom->info.wram_size[0] = SIZE_8K;
	rom->info.wram_size[1] = new_buffer_size -
		sizeof(header) - 16 -
		(NSF_PLAYER_SIZE);
	rom->info.total_prg_size = new_buffer_size - 16;
	rom->offset = 16;
	rom_calculate_checksum(rom);

	return 0;
}

const char *nsf_get_title(struct rom *rom)
{
	struct nsf_header *header;
	if (rom->info.board_type != BOARD_TYPE_NSF)
		return NULL;

	header = (struct nsf_header *)(rom->buffer + NSF_PLAYER_SIZE + 16);

	return (const char *)(header->title);
}

const char *nsf_get_artist(struct rom *rom)
{
	struct nsf_header *header;
	if (rom->info.board_type != BOARD_TYPE_NSF)
		return NULL;

	header = (struct nsf_header *)(rom->buffer + NSF_PLAYER_SIZE + 16);

	return (const char *)(header->artist);
}

const char *nsf_get_copyright(struct rom *rom)
{
	struct nsf_header *header;
	if (rom->info.board_type != BOARD_TYPE_NSF)
		return NULL;

	header = (struct nsf_header *)(rom->buffer + NSF_PLAYER_SIZE + 16);

	return (const char *)(header->copyright);
}

int nsf_get_song_count(struct rom *rom)
{
	struct nsf_header *header;
	if (rom->info.board_type != BOARD_TYPE_NSF)
		return -1;

	header = (struct nsf_header *)(rom->buffer + NSF_PLAYER_SIZE + 16);

	return header->song_count;
}

int nsf_get_first_song(struct rom *rom)
{
	struct nsf_header *header;
	if (rom->info.board_type != BOARD_TYPE_NSF)
		return -1;

	header = (struct nsf_header *)(rom->buffer + NSF_PLAYER_SIZE + 16);

	return header->starting_song;
}

const char *nsf_get_region(struct rom *rom)
{
	struct nsf_header *header;
	if (rom->info.board_type != BOARD_TYPE_NSF)
		return NULL;

	header = (struct nsf_header *)(rom->buffer + NSF_PLAYER_SIZE + 16);

	return (header->region & 1 ? "PAL" : "NTSC");
}

int nsf_get_expansion_sound(struct rom *rom)
{
	struct nsf_header *header;
	if (rom->info.board_type != BOARD_TYPE_NSF)
		return -1;

	header = (struct nsf_header *)(rom->buffer + NSF_PLAYER_SIZE + 16);

	return header->expansion_sound & 0x3f;
}
