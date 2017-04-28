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

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "archive.h"

extern int archive_handler_zip(struct archive *archive, const char *filename);
extern int archive_handler_7zip(struct archive *archive, const char *filename);
extern int archive_handler_null(struct archive *archive, const char *filename);

static int (*archive_handlers[])(struct archive *, const char *) = {
	archive_handler_zip,
	archive_handler_7zip,
	archive_handler_null,
	NULL,
};

int archive_create_empty_file_list(struct archive *archive, int count)
{
	struct archive_file_list *list;
	struct archive_file_list_entry *entries;

	if (!archive || archive->file_list)
		return -1;

	list = malloc(sizeof(*list));
	if (!list)
		return -1;

	entries = malloc(sizeof(*entries) * count);
	if (!entries) {
		free(list);
		return -1;
	}
	memset(entries, 0, sizeof(*entries) * count);

	list->entries = entries;
	list->count = count;

	archive->file_list = list;

	return 0;
}

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

int archive_open(struct archive **archive, const char *filename)
{
	struct archive *ptr;
	int i;

	ptr = malloc(sizeof(*ptr));
	ptr->format = ARCHIVE_FORMAT_UNDEFINED;
	ptr->file_list = NULL;
	ptr->private = NULL;
	ptr->functions = NULL;

	*archive = NULL;

	for (i = 0; archive_handlers[i]; i++) {
		if (archive_handlers[i](ptr, filename) == 0) {
			*archive = ptr;
			return 0;
		}
	}

	free(ptr);

	return -1;
}

int archive_close(struct archive **archive)
{
	int status;
	struct archive *a;

	status = -1;

	a = *archive;

	if (a->functions && a->functions->close) {
		status = a->functions->close(a);
	}

	free_archive_file_list(a->file_list);

	if (a)
		free(a);

	*archive = NULL;

	return status;
}

int archive_read_file_by_index(struct archive *archive, int index, uint8_t *ptr)
{

	if (!archive || (archive->format == ARCHIVE_FORMAT_UNDEFINED))
		return -1;

	if (!archive->functions || !archive->functions->read_file_by_index)
		return -1;


	return archive->functions->read_file_by_index(archive, index, ptr);
}

int archive_read_file_by_name(struct archive *archive, const char *name, uint8_t *ptr)
{
	if (!archive || (archive->format == ARCHIVE_FORMAT_UNDEFINED))
		return -1;

	if (!archive->functions || !archive->functions->read_file_by_name)
		return -1;


	return archive->functions->read_file_by_name(archive, name, ptr);
}

struct archive_file_list_entry *archive_get_file_entry(struct archive *archive,
                                                       const char *filename)
{
	struct archive_file_list *list;
	struct archive_file_list_entry *entry;
	int i;

	if (!archive || !filename)
		return NULL;

	entry = NULL;

	list = archive->file_list;

	for (i = 0; i < list->count; i++) {
		if (strcasecmp(list->entries[i].name, filename) == 0) {
			entry = &list->entries[i];
			break;
		}
	}

	return entry;
}
