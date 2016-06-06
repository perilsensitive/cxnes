#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "archive.h"

extern int archive_handler_zip(struct archive *archive, const char *filename);

static int (*archive_handlers[])(struct archive *, const char *) = {
	archive_handler_zip,
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

