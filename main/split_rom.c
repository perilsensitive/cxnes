#include "archive.h"
#include "emu.h"
#include "db.h"
#include "crc32.h"
#include "sha1.h"

#define MAX_PRG_ROMS 4
#define MAX_CHR_ROMS 2
#define MAX_ROMS (MAX_PRG_ROMS + MAX_CHR_ROMS)

static int load_split_rom_parts(struct archive *archive, int *chip_list, struct rom_info *rom_info,
                                 uint8_t **bufferp, size_t *sizep)
{
	uint8_t *buffer;
	uint8_t *ptr;
	size_t size;
	off_t offsets[MAX_ROMS];
	int file_list[MAX_ROMS];
	int num_chips;
	int status;
	int i;

	size = 0;

	num_chips = rom_info->prg_size_count + rom_info->chr_size_count;

	for (i = 0; i < MAX_ROMS; i++)
		file_list[i] = -1;

	status = 0;
	for (i = 0; i < num_chips; i++) {
		int j;

		for (j = 0; j < archive->file_list->count; j++) {
			if (chip_list[j] == i) {
				offsets[i] = size;
				size += archive->file_list->entries[j].size;
				break;
			}
		}
	}

	buffer = malloc(INES_HEADER_SIZE + size);
	if (!buffer) {
		return -1;
	}

	ptr = buffer + INES_HEADER_SIZE;

	for (i = 0; i < num_chips; i++) {
		int j;

		for (j = 0; j < archive->file_list->count; j++) {
			if (chip_list[j] == i) {
				sha1nfo sha1;
				uint8_t *result;

				file_list[i] = j;
				status = archive_read_file_by_index(archive, file_list[i], ptr);
				if (status) {
					free(buffer);
					return -1;
				}

				/* Validate each chip's SHA1, if present. If it doesn't match,
				   then move on to the next match for that chip in the archive.
				 */

				if ((i < rom_info->prg_size_count) && rom_info->prg_sha1_count) {
					sha1_init(&sha1);
					sha1_write(&sha1, (char *)ptr,
					           archive->file_list->entries[file_list[i]].size);
					result = sha1_result(&sha1);
					if (memcmp(result, rom_info->prg_sha1[i], 20) != 0) {
						continue;
					}
				} else if (rom_info->chr_sha1_count) {
					sha1_init(&sha1);
					sha1_write(&sha1, (char *)ptr,
					           archive->file_list->entries[file_list[i]].size);
					result = sha1_result(&sha1);
					if (memcmp(result, rom_info->chr_sha1[i -
					                   rom_info->prg_size_count], 20) != 0) {
						continue;
					}
				}

				ptr += archive->file_list->entries[file_list[i]].size;
				break;
			}
		}

		if (j == archive->file_list->count) {
			status = -1;
			break;
		}
	}

	if (status < 0) {
		if (bufferp)
			*bufferp = NULL;
		return 0;
	}

	status = -1;

	for (i = 0; (i < MAX_ROMS) && (file_list[i] >= 0); i++) {
		uint8_t *ptr;

		ptr = buffer + INES_HEADER_SIZE + offsets[i];
		status = archive_read_file_by_index(archive, file_list[i], ptr);

		if (status)
			break;
	}

	if (status) {
		free(buffer);
		if (bufferp)
			*bufferp = NULL;
		return 0;
	}

	if (sizep) {
		*sizep = size + INES_HEADER_SIZE;
	}

	*bufferp = buffer;
	return 0;
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
	int *chip_list;
	int i;

	if (!romptr)
		return -1;

	chip_list = NULL;

	*romptr = NULL;

	if (archive_open(&archive, filename))
		return -1;

	chip_list = malloc(archive->file_list->count * sizeof(*chip_list));
	if (!chip_list)
		return -1;

	rom_info = NULL;
	rom = NULL;
	buffer = NULL;
	do {
		rom_info = db_lookup_split_rom(archive, chip_list, rom_info);
		if (!rom_info)
			break;

		if (load_split_rom_parts(archive, chip_list, rom_info, &buffer,
		                         &size)) {
			break;
		}

		if (!buffer)
			continue;

		rom = rom_alloc(filename, buffer, size);
		if (!rom)
			break;

		/* Store the filename of the first PRG chip as the empty string;
		   this allows patches to be included with split roms as well,
		   but they need to be located in the top-level directory of the
		   archive.
		*/
		rom->compressed_filename = strdup("");

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

	if (chip_list)
		free(chip_list);

	if (buffer)
		free(buffer);

	if (!rom)
		return -1;

	*romptr = rom;

	return 0;

}
