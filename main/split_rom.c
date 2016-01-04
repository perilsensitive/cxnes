#include "emu.h"
#include "db.h"
#include "crc32.h"
#include "sha1.h"
#include <zlib.h>
#include <unzip.h>

#define MAX_PRG_ROMS 4
#define MAX_CHR_ROMS 2
#define MAX_ROMS (MAX_PRG_ROMS + MAX_CHR_ROMS)

static void free_archive_file_list(struct archive_file_list *list)
{
	int i;

	if (!list)
		return;
	
	if (list->entries) {
		for (i = 0; i < list->count; i++) {
			if (list->entries[i].name)
				free(list->entries[i].name);
		}
			free(list->entries);
	}
	free(list);
}

static int build_archive_file_list(const char *filename, struct archive_file_list **ptr)
{
	unzFile zip;
	unz_global_info global_info;
	unz_file_info file_info;
	struct archive_file_list *list;
	struct archive_file_list_entry *entries;
	uint8_t *tmp_buffer;
	size_t tmp_buffer_size;
	int list_size;
	int err;
	int i;
	int rc;

	zip = unzOpen(filename);
	if (zip == NULL)
		return -1;

	err = unzGetGlobalInfo(zip, &global_info);
	if (err != UNZ_OK) {
		unzClose(zip);
		return -1;
	}

	list = malloc(sizeof(*list));
	if (!list) {
		unzClose(zip);
		return -1;
	}

	entries = malloc(sizeof(*entries) * global_info.number_entry);
	if (!entries) {
		unzClose(zip);
		free(list);
		return -1;
	}

	list->entries = entries;

	list_size = 0;

	tmp_buffer = NULL;
	tmp_buffer_size = 0;
	rc = -1;

	for (i = 0; i < global_info.number_entry; i++) {
		sha1nfo sha1nfo;
		uint8_t *result;
		int bytes_read;
		int name_length;
		char *name;

		err = unzGetCurrentFileInfo(zip, &file_info,
					    NULL, 0, NULL, 0, NULL, 0);
		if (err != UNZ_OK)
			break;

		entries[list_size].size = file_info.uncompressed_size;
		name_length = file_info.size_filename + 1;
		name = malloc(name_length);
		entries[list_size].name = name;
		if (name) {
			err = unzGetCurrentFileInfo(zip, &file_info, name, name_length,
						    NULL, 0, NULL, 0);
			if (err != UNZ_OK)
				break;
		}

		if (file_info.uncompressed_size > tmp_buffer_size) {
			uint8_t *tmp = realloc(tmp_buffer,
					       entries[list_size].size);

			if (!tmp)
				break;

			tmp_buffer = tmp;
			tmp_buffer_size = entries[list_size].size;
		}

		err = unzOpenCurrentFile(zip);
		if (err != UNZ_OK)
			break;

		bytes_read = unzReadCurrentFile(zip, tmp_buffer,
					 file_info.uncompressed_size);
		err = unzCloseCurrentFile(zip);

		if ((err != UNZ_OK) || (bytes_read != entries[list_size].size))
			break;


		sha1_init(&sha1nfo);
		entries[list_size].crc = crc32buf(tmp_buffer,
						  entries[list_size].size, NULL);
		sha1_write(&sha1nfo, (char *)tmp_buffer,
			   entries[list_size].size);
		result = sha1_result(&sha1nfo);
		memcpy(entries[list_size].sha1, result, 20);
			   
		list_size++;

		err = unzGoToNextFile(zip);
		if (err != UNZ_OK) {
			if (err == UNZ_END_OF_LIST_OF_FILE)
				rc = 0;

			break;
		}
	}

	unzClose(zip);
	if (tmp_buffer)
		free(tmp_buffer);

	if (rc == 0) {
		*ptr = list;
		list->count = list_size;
	} else {
		free_archive_file_list(list);
	}

	return rc;
}

uint8_t *load_split_rom_parts(const char *filename, struct archive_file_list *file_list,
			      int *chip_list, size_t *sizep)
{
	uint8_t *buffer;
	unzFile zip;
	size_t size;
	off_t offsets[MAX_ROMS];
	int err;
	int i;

	size = 0;

	for (i = 0; i < MAX_ROMS; i++)	{
		int file_index = chip_list[i];

		if (file_index < 0)
			break;

		offsets[i] = size;
		size += file_list->entries[file_index].size;
	}

	buffer = malloc(INES_HEADER_SIZE + size);
	if (!buffer)
		return NULL;

	zip = unzOpen(filename);
	if (zip == NULL)
		return NULL;

	err = UNZ_OK;

	for (i = 0; i < file_list->count; i++) {
		int j;

		for (j = 0; j < MAX_ROMS; j++) {
			if (chip_list[j] == i)
				break;
		}

		if (j == MAX_ROMS) {
			err = unzGoToNextFile(zip);
			if (err != UNZ_OK)
				break;

			continue;
		}


		err = UNZ_OK;

		err = unzOpenCurrentFile(zip);
		if (err != UNZ_OK)
			break;

		err = unzReadCurrentFile(zip, buffer + INES_HEADER_SIZE +
					 offsets[j], file_list->entries[i].size);
		if (err != file_list->entries[i].size)
			break;

		err = unzCloseCurrentFile(zip);
		if (err != UNZ_OK)
			break;

		err = unzGoToNextFile(zip);
		if (err != UNZ_OK)
			break;
	}

	if (err != UNZ_END_OF_LIST_OF_FILE) {
		free(buffer);
		buffer = NULL;
	}

	unzClose(zip);

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
	struct archive_file_list *list;
	uint8_t *buffer;
	size_t size;
	struct rom_info *rom_info;
	struct rom *rom;
	int chip_list[MAX_ROMS];
	int i;

	if (!romptr)
		return -1;

	*romptr = NULL;

	if (build_archive_file_list(filename, &list) < 0)
		return -1;

	rom_info = NULL;
	rom = NULL;
	buffer = NULL;
	do {
		rom_info = db_lookup_split_rom(list, chip_list, rom_info);
		if (!rom_info)
			break;

		buffer = load_split_rom_parts(filename, list, chip_list, &size);

		if (!buffer)
			break;

		rom = rom_alloc(filename, buffer, size);
		if (!rom)
			break;

		/* Store the filename of the first PRG chip as the compressed
		   filename; this allows patches to be included (provided they
		   exist in the same dir in the zip as this prg chip.
		*/
		if (list->entries[chip_list[0]].name) {
			rom->compressed_filename =
				strdup(list->entries[chip_list[0]].name);
		}

		buffer = NULL;
		memcpy(&rom->info, rom_info, sizeof(*rom_info));
		rom->offset = INES_HEADER_SIZE;
		rom_calculate_checksum(rom);

		for (i = 0; i < list->count; i++) {
			char *basename;

			if (!list->entries[i].name)
				continue;

			basename = strrchr(list->entries[i].name, '/');
			if (!basename)
				basename = list->entries[i].name;

			/* Hack for PlayChoice ROMs; most of these are the same as NES
			   carts, so check for a file called 'security.prm' to distinguish
			   them.
			*/
			if (!strcasecmp(basename, "security.prm")) {
				if (rom->info.flags & ROM_FLAG_PLAYCHOICE)
					rom->info.system_type = EMU_SYSTEM_TYPE_PLAYCHOICE;
			}
		}

		/* Quick fixup for Vs. System (mapper 99) ROMs with PRG size > 32K */
		if ((rom->info.board_type == BOARD_TYPE_VS_UNISYSTEM) &&
		    (rom->info.total_prg_size >= 40 * 1024)) {
			uint8_t *buf = malloc(8192);
			if (!buf)
				return 0;

			memcpy(buf, rom->buffer + rom->offset + SIZE_8K, SIZE_8K);
			memmove(rom->buffer + rom->offset + SIZE_8K,
				rom->buffer + rom->offset + SIZE_16K,
				rom->info.total_prg_size - SIZE_16K);
			memcpy(rom->buffer + rom->offset + SIZE_32K, buf, SIZE_8K);
			free(buf);
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

	free_archive_file_list(list);

	if (buffer)
		free(buffer);

	if (!rom)
		return -1;

	*romptr = rom;

	return 0;

}
