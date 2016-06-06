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

#include "archive.h"

#include <zlib.h>
#include <unzip.h>
#include <stdint.h>

struct archive_zip {
	unzFile zip_file;
	unz_global_info global_info;
};

static int zip_close(struct archive *archive);
static int zip_read_file_by_index(struct archive *archive, int index,
                                  uint8_t *ptr);
static int zip_read_file_by_name(struct archive *archive, const char *name,
                                 uint8_t *ptr);

static struct archive_functions zip_functions = {
	zip_close,
	zip_read_file_by_index,
	zip_read_file_by_name,
};

static int zip_create_file_list(struct archive *archive)
{
	unz_file_info file_info;
	unzFile zip;
	struct archive_zip *data;
	int status = -1;
	int number_entries;
	int i;

	if (!archive || !archive->private)
		return -1;


	data = (struct archive_zip *)archive->private;
	zip = data->zip_file;

	number_entries = data->global_info.number_entry;

	if (archive_create_empty_file_list(archive, number_entries))
		return -1;

	for (i = 0; i < number_entries; i++) {
		int name_length;
		char *name;

		if (unzGetCurrentFileInfo(zip, &file_info, NULL, 0,
		                          NULL, 0, NULL, 0) != UNZ_OK) {
			break;
		}

		name_length = file_info.size_filename + 1;
		name = malloc(name_length);
		if (!name)
			break;

		if (unzGetCurrentFileInfo(zip, &file_info, name, name_length,
		                          NULL, 0, NULL, 0) != UNZ_OK) {
			free(name);
			break;
		}

		archive->file_list->entries[i].name = name;
		archive->file_list->entries[i].size =
		    file_info.uncompressed_size;
		archive->file_list->entries[i].crc =
		    file_info.crc;

		status = unzGoToNextFile(zip);

		if (status == UNZ_END_OF_LIST_OF_FILE) {
			status = 0;
			break;
		} else if (status != UNZ_OK) {
			break;
		}
	}

	return status;
}

int archive_handler_zip(struct archive *archive, const char *filename)
{
	unzFile zip;
	struct archive_zip *private;
	int err;

	if (archive->format != ARCHIVE_FORMAT_UNDEFINED)
		return -1;

	zip = unzOpen(filename);
	if (zip == NULL)
		return -1;

	private = malloc(sizeof(*private));
	if (!private) {
		unzClose(zip);
		return -1;
	}

	err = unzGetGlobalInfo(zip, &private->global_info);
	if (err != UNZ_OK) {
		unzClose(zip);
		return -1;
	}

	private->zip_file = zip;
	archive->private = private;
	archive->functions = &zip_functions;
	archive->format = ARCHIVE_FORMAT_ZIP;

	err = zip_create_file_list(archive);

	if (err) {
		archive->format = ARCHIVE_FORMAT_UNDEFINED;
		unzClose(zip);
		free(archive->private);
		return -1;
	}

	return 0;
}

static int zip_close(struct archive *archive)
{
	struct archive_zip *data;

	if (!archive || !archive->private)
		return -1;

	if (archive->format != ARCHIVE_FORMAT_ZIP)
		return -1;

	data = archive->private;
	
	unzClose(data->zip_file);

	free(archive->private);

	return 0;
}

static int zip_read_current_file(struct archive *archive, uint8_t *ptr)
{
	struct archive_zip *data;
	unz_file_info file_info;
	unzFile zip;
	size_t size;
	int status;

	data = archive->private;
	zip = data->zip_file;

	status = unzOpenCurrentFile(zip);
	if (status != UNZ_OK)
		return -1;

	status = unzGetCurrentFileInfo(zip, &file_info, NULL, 0,
		                       NULL, 0, NULL, 0);
	if (status != UNZ_OK)
		return -1;

	size = file_info.uncompressed_size;

	status = unzReadCurrentFile(zip, ptr, size);
	if (status != UNZ_OK) {
		return -1;
	}
	
	return 0;
}

static int zip_read_file_by_index(struct archive *archive, int index,
                                  uint8_t *ptr)
{
	struct archive_zip *data;
	unzFile zip;
	int status;
	int i;

	data = archive->private;
	zip = data->zip_file;
 
	status = unzGoToFirstFile(zip);
	if (status != UNZ_OK)
		return -1;

	for (i = 0; i < index; i++) {
		status = unzGoToNextFile(zip);

		if (status != UNZ_OK)
			return -1;
	}

	status = zip_read_current_file(archive, ptr);

	unzCloseCurrentFile(zip);

	return 0;
}

static int zip_read_file_by_name(struct archive *archive, const char *name,
                                  uint8_t *ptr)
{
	struct archive_zip *data;
	unzFile zip;
	int status;

	data = archive->private;
	zip = data->zip_file;
 
	status = unzGoToFirstFile(zip);
	if (status != UNZ_OK)
		return -1;

	status = unzLocateFile(zip, name, 0);
	if (status != UNZ_OK)
		return -1;

	status = zip_read_current_file(archive, ptr);

	unzCloseCurrentFile(zip);

	return 0;
}
