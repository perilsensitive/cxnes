#include "archive.h"
#include "emu.h"
#include "db.h"
#include "crc32.h"
#include "sha1.h"
#include <zlib.h>
#include <unzip.h>

#define MAX_PRG_ROMS 4
#define MAX_CHR_ROMS 2
#define MAX_ROMS (MAX_PRG_ROMS + MAX_CHR_ROMS)

uint8_t *load_split_rom_parts(struct archive *archive, int *chip_list, size_t *sizep)
{
	uint8_t *buffer;
	size_t size;
	off_t offsets[MAX_ROMS];
	int status;
	int i;

	size = 0;

	for (i = 0; i < MAX_ROMS; i++)	{
		int file_index = chip_list[i];

		if (file_index < 0)
			break;

		offsets[i] = size;
		size += archive->file_list->entries[file_index].size;
	}

	buffer = malloc(INES_HEADER_SIZE + size);
	if (!buffer)
		return NULL;

	status = -1;

	for (i = 0; i < MAX_ROMS; i++) {
		uint8_t *ptr;

		if (chip_list[i] < 0) {
			status = 0;
			break;
		}

		ptr = buffer + INES_HEADER_SIZE + offsets[i];
		status = archive_read_file_by_index(archive, i, ptr);
		if (status)
			break;
	}

	if (status) {
		free(buffer);
		return NULL;
	}

	if (sizep) {
		*sizep = size;
	}

	return buffer;
}

/* This is a generic split-ROM format loader.  It requires the
   database entry for the game in question to have the sizes and CRC32
   and/or SHA1 checksums for each PRG chunk and CHR chunk listed in
   the correct order.  Once a database entry is found for which all
   rom files are included in the archive, each split file is loaded
   into memory in order (based on the checksum lists in the database
   entry), then the final buffer is checksummed again and the size,
   CRC32 and SHA1 of the combined entry is checked against the same
   database entry (assuming the db entry provides a full-rom crc32 or
   sha1) .  If the checksums of the rom match what's in the database
   entry, the load is considered successful.
*/
 
int split_rom_load(struct emu *emu, const char *filename, struct rom **romptr)
{
	struct archive *archive;
	uint8_t *buffer;
	size_t size;
	struct rom_info *rom_info;
	struct rom *rom;
	int chip_list[MAX_ROMS];
	int i;

	if (!romptr)
		return -1;

	*romptr = NULL;

	if (archive_open(&archive, filename))
		return -1;

	rom_info = NULL;
	rom = NULL;
	buffer = NULL;
	do {
		rom_info = db_lookup_split_rom(archive, chip_list, rom_info);
		if (!rom_info)
			break;

		buffer = load_split_rom_parts(archive, chip_list, &size);

		if (!buffer)
			break;

		rom = rom_alloc(filename, buffer, size);
		if (!rom)
			break;

		/* Store the filename of the first PRG chip as the compressed
		   filename; this allows patches to be included (provided they
		   exist in the same dir in the zip as this prg chip.
		*/
		if (archive->file_list->entries[chip_list[0]].name) {
			rom->compressed_filename =
				strdup(archive->file_list->entries[chip_list[0]].name);
		}

		buffer = NULL;
		memcpy(&rom->info, rom_info, sizeof(*rom_info));
		rom->offset = INES_HEADER_SIZE;
		rom_calculate_checksum(rom);

		for (i = 0; i < archive->file_list->count; i++) {
			char *basename;

			if (!archive->file_list->entries[i].name)
				continue;

			basename = strrchr(archive->file_list->entries[i].name, '/');
			if (!basename)
				basename = archive->file_list->entries[i].name;

			/* Hack for PlayChoice ROMs; most of these are the same as NES
			   carts, so check for a file called 'security.prm' to distinguish
			   them.
			*/
			if (!strcasecmp(basename, "security.prm")) {
				if (rom->info.flags & ROM_FLAG_PLAYCHOICE)
					rom->info.system_type = EMU_SYSTEM_TYPE_PLAYCHOICE;
			}
		}

		/* Validate individual chip CRCs or SHA1s if present, then
		   the combined CRC and/or SHA1, if present.
		*/
		if (!validate_checksums(rom, rom_info))
			goto invalid;

		if ((rom->info.flags & ROM_FLAG_HAS_CRC) &&
		    (rom->info.combined_crc != rom_info->combined_crc)) {
			goto invalid;
		}

		if ((rom->info.flags & ROM_FLAG_HAS_SHA1) &&
		    memcmp(rom->info.combined_sha1, rom_info->combined_sha1, 20)) {
			goto invalid;
		}
		
		break;

	invalid:
		rom_free(rom);
		rom = NULL;
	} while (rom_info);

	archive_close(&archive);

	if (buffer)
		free(buffer);

	if (!rom)
		return -1;

	*romptr = rom;

	return 0;

}
