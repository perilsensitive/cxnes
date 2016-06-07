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
#include "file_io.h"
#include "crc32.h"

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static int null_close(struct archive *archive);
static int null_read_file_by_index(struct archive *archive, int index,
                                  uint8_t *ptr);
static int null_read_file_by_name(struct archive *archive, const char *name,
                                  uint8_t *ptr);

static struct archive_functions null_functions = {
	null_close,
	null_read_file_by_index,
	null_read_file_by_name,
};

static int null_create_file_list(struct archive *archive)
{
	const char *filename;
	size_t size;
	uint8_t *buffer;

	if (!archive || !archive->private)
		return -1;

	if (archive_create_empty_file_list(archive, 1))
		return -1;

	filename = archive->private;

	size = get_file_size(filename);
	
	buffer = malloc(size);
	if (!buffer)
		return -1;

	if (readfile(filename, buffer, size)) {
		free(buffer);
		return -1;
	}

	archive->file_list->entries[0].flags = 0;
	archive->file_list->entries[0].size = size;
	archive->file_list->entries[0].name = strdup(filename);
	archive->file_list->entries[0].crc = crc32buf(buffer, size, NULL);

	free(buffer);

	return 0;
}

int archive_handler_null(struct archive *archive, const char *filename)
{
	char *private;

	if (archive->format != ARCHIVE_FORMAT_UNDEFINED)
		return -1;

	if (!check_file_exists(filename))
		return -1;

	private = strdup(filename);
	if (!private) 
		return -1;

	archive->private = private;
	archive->functions = &null_functions;
	archive->format = ARCHIVE_FORMAT_NULL;

	if (null_create_file_list(archive)) {
		archive->format = ARCHIVE_FORMAT_UNDEFINED;
		free(archive->private);
		return -1;
	}

	return 0;
}

static int null_close(struct archive *archive)
{
	if (!archive || !archive->private)
		return -1;

	if (archive->format != ARCHIVE_FORMAT_NULL)
		return -1;

	free(archive->private);

	return 0;
}

static int null_read_file_by_index(struct archive *archive, int index,
                                  uint8_t *ptr)
{
	const char *filename;
	size_t size;

	if (index != 0)
		return -1;

	filename = archive->private;
	size = get_file_size(filename);
	if ((int)size < 0)
		return -1;

	if (readfile(filename, ptr, size))
		return -1;

	return 0;
}

static int null_read_file_by_name(struct archive *archive, const char *name,
                                  uint8_t *ptr)
{
	if (!archive || !archive->private)
		return -1;

	if (strcasecmp(name, (const char *)(archive->private)) != 0)
		return -1;

	return 0;
}
