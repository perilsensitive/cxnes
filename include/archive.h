#ifndef __ARCHIVE_H__
#define __ARCHIVE_H__

#include <stdint.h>

struct archive;

enum archive_format {
	ARCHIVE_FORMAT_UNDEFINED,
	ARCHIVE_FORMAT_ZIP,
	ARCHIVE_FORMAT_NULL,
};

struct archive_functions {
	int (*close)(struct archive *);
	int (*read_file_by_index)(struct archive *, int, uint8_t *);
	int (*read_file_by_name)(struct archive *, const char *, uint8_t *);
};

struct archive_file_list_entry {
	size_t size;
	uint32_t crc;
	uint8_t sha1[20];
	char *name;
};

struct archive_file_list {
	struct archive_file_list_entry *entries;
	int count;
};

struct archive {
	enum archive_format format;
	struct archive_functions *functions;
	struct archive_file_list *file_list;
	void *private;
};

int archive_open(struct archive **archive, const char *filename);
int archive_close(struct archive **archive);
int archive_read_file_by_index(struct archive *archive, int index, uint8_t *ptr);
int archive_read_file_by_name(struct archive *archive, const char *name, uint8_t *ptr);
int archive_create_empty_file_list(struct archive *archive, int count);

#endif /* __ARCHIVE_H__ */
