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
#include "7z/Types.h"
#include "7z/7z.h"
#include "7z/7zAlloc.h"
#include "7z/7zCrc.h"
#include "7z/7zFile.h"
#include "7z/7zVersion.h"

#include <string.h>
#include <stdint.h>

struct archive_7zip {
	UInt32 block_index;
	Byte *out_buffer;
	size_t out_buffer_size;
	ISzAlloc alloc_imp;
	ISzAlloc alloc_temp_imp;
	CFileInStream archive_string;
	CLookToRead look_stream;
	CSzArEx db;
};

static int p7zip_close(struct archive *archive);
static int p7zip_read_file_by_index(struct archive *archive, int index,
                                  uint8_t *ptr);
static int p7zip_read_file_by_name(struct archive *archive, const char *name,
                                 uint8_t *ptr);

static struct archive_functions p7zip_functions = {
	p7zip_close,
	p7zip_read_file_by_index,
	p7zip_read_file_by_name,
};

static Byte kUtf8Limits[5] = { 0xC0, 0xE0, 0xF0, 0xF8, 0xFC };

static int Utf16_To_Utf8(char *dest, size_t *destLen, const uint16_t *src, size_t srcLen)
{
	size_t destPos = 0;
	size_t srcPos = 0;

	for (;;) {
		unsigned numAdds;
		uint32_t value;

		if (srcPos == srcLen) {
			*destLen = destPos;
			return 1;
		}

		value = src[srcPos];
		srcPos++;

		if (value < 0x80) {
			if (dest)
				dest[destPos] = (char)value;

			destPos++;
			continue;
		}

		if (value >= 0xD800 && value < 0xE000) {
			uint32_t c2;

			if (value >= 0xDC00 || srcPos == srcLen)
				break;

			c2 = src[srcPos];
			srcPos++;

			if (c2 < 0xDC00 || c2 >= 0xE000)
				break;

			value = (((value - 0xD800) << 10) | (c2 - 0xDC00)) + 0x10000;
		}

		for (numAdds = 1; numAdds < 5; numAdds++) {
			if (value < (((uint32_t)1) << (numAdds * 5 + 6)))
				break;
		}

		if (dest) {
			dest[destPos] = (char)(kUtf8Limits[numAdds - 1] +
			                (value >> (6 * numAdds)));
		}

		destPos++;

		do {
			numAdds--;
			if (dest) {
				dest[destPos] = (char)(0x80 + ((value >>
							(6 * numAdds)) & 0x3F));
			}

			destPos++;
		} while (numAdds != 0);
	}

	*destLen = destPos;
	return 0;
}

static int Utf16_To_Utf8Buf(char **dest, const uint16_t *src, size_t srcLen)
{
	size_t destLen = 0;
	int res;

	Utf16_To_Utf8(NULL, &destLen, src, srcLen);
	destLen += 1;

	*dest = malloc(destLen);
	if (!*dest)
		return -1;

	res = Utf16_To_Utf8(*dest, &destLen, src, srcLen);
	(*dest)[destLen] = 0;

	return res ? destLen : -1;
}

static int Utf16_To_Char(char **buf, const uint16_t *s)
{
	int len = 0;

	while (s[len] != '\0')
		len++;

	return Utf16_To_Utf8Buf(buf, s, len);
}

static int p7zip_create_file_list(struct archive *archive)
{
	struct archive_7zip *data;
	int count;
	uint16_t *name;
	int name_size;
	int i;

	if (!archive || !archive->private)
		return -1;

	data = archive->private;
	count = data->db.db.NumFiles;

	name = NULL;
	name_size = 0;

	if (archive_create_empty_file_list(archive, count))
		return -1;

	for (i = 0; i < count; i++) {
		char *utf8_name;
		int len;
		CSzFileItem *f;

		utf8_name = NULL;

 	      	len = SzArEx_GetFileNameUtf16(&data->db, i, NULL);

		if (len > name_size) {
			if (name)
				free(name);
			name_size = len;
			name = malloc(name_size * sizeof(*name));
			if (!name)
				return -1;
		}

 	      	SzArEx_GetFileNameUtf16(&data->db, i, name);
		
		Utf16_To_Char(&utf8_name, name);

		f = data->db.db.Files + i;

		archive->file_list->entries[i].size = f->Size;
		archive->file_list->entries[i].crc = f->Crc;
		archive->file_list->entries[i].name = utf8_name;

		printf("%d: %s\n", i, utf8_name);
	}

	if (name)
		free(name);

	return 0;
}

int archive_handler_7zip(struct archive *archive, const char *filename)
{
	struct archive_7zip *data;
	SRes res;

	data = malloc(sizeof(*data));
	if (!data)
		return -1;

	data->block_index = ~0;
	data->out_buffer = NULL;
	data->out_buffer_size = 0;
	data->alloc_imp.Alloc = SzAlloc;
	data->alloc_imp.Free = SzFree;
	data->alloc_temp_imp.Alloc = SzAllocTemp;
	data->alloc_temp_imp.Free = SzFreeTemp;

	if (InFile_Open(&data->archive_string.file, filename))
		return -1;

	/* FIXME Error checking? */
	FileInStream_CreateVTable(&data->archive_string);
	LookToRead_CreateVTable(&data->look_stream, False);
	  
	data->look_stream.realStream = &data->archive_string.s;
	LookToRead_Init(&data->look_stream);

	CrcGenerateTable();

	SzArEx_Init(&data->db);
	res = SzArEx_Open(&data->db, &data->look_stream.s, &data->alloc_imp,
	                  &data->alloc_temp_imp);

	if (res != SZ_OK) {
		/* FIXME how to clean up? */
		SzArEx_Free(&data->db, &data->alloc_imp);
		IAlloc_Free(&data->alloc_imp, data->out_buffer);
		File_Close(&data->archive_string.file);
		free(data);
	}

	archive->format = ARCHIVE_FORMAT_7ZIP;
	archive->private = data;
	archive->functions = &p7zip_functions;

	res = p7zip_create_file_list(archive);
	if (res)
		return -1;

	return 0;
}

static int p7zip_close(struct archive *archive)
{
	struct archive_7zip *data;

	if (!archive || !archive->private)
		return -1;

	data = archive->private;

	IAlloc_Free(&data->alloc_imp, data->out_buffer);
	SzArEx_Free(&data->db, &data->alloc_imp);
	File_Close(&data->archive_string.file);

	return -1;
}

static int p7zip_read_file_by_index(struct archive *archive, int index,
                                    uint8_t *ptr)
{
	struct archive_7zip *data;
        size_t offset = 0;
        size_t outSizeProcessed = 0;
	int rc;

	if (!archive || !archive->private)
		return -1;

	printf("calling read_file_by_index: %d\n", index);

	data = archive->private;

	rc = SzArEx_Extract(&data->db, &data->look_stream.s, index,
		            &data->block_index, &data->out_buffer,
	                    &data->out_buffer_size,
		            &offset, &outSizeProcessed,
		            &data->alloc_imp, &data->alloc_temp_imp);

	if (rc)
		return -1;

	memcpy(ptr, data->out_buffer + offset,
	       archive->file_list->entries[index].size);

	return 0;
}

static int p7zip_read_file_by_name(struct archive *archive, const char *name,
                                   uint8_t *ptr)
{
	int i;

	if (!archive || !archive->private)
		return -1;

	for (i = 0; i < archive->file_list->count; i++) {
		if (strcasecmp(name, archive->file_list->entries[i].name) == 0)
			break;
	}

	if (i == archive->file_list->count)
		return -1;

	return p7zip_read_file_by_index(archive, i, ptr);

	return 0;
}
