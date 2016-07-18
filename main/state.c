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

#include "emu.h"
#include "file_io.h"

struct save_state {
	uint8_t *data;
	size_t size;
};

static uint8_t *unpack_item(uint8_t *buffer, void *data, struct state_item *item)
{
	int count;
	char *tmp;

	tmp = ((char *)data + item->offset);

	for (count = item->nmemb; count > 0; count--) {
		uint32_t t = 0;

		switch (item->size) {
		case 4:
			t |= *(buffer++);
			t |= *(buffer++) << 8;
			t |= *(buffer++) << 16;
			t |= *(buffer++) << 24;
			break;
		case 2:
			t |= *(buffer++);
			t |= *(buffer++) << 8;
			break;
		case 1:
			t |= *(buffer++);
			break;
		}

		switch (item->member_size) {
		case 8:
			*((uint64_t *)tmp) = t;
			break;
		case 4:
			*((uint32_t *)tmp) = t;
			break;
		case 2:
			*((uint16_t *)tmp) = t & 0xffff;
			break;
		case 1:
			*((uint8_t *)tmp) = t & 0xff;
			break;
		}

		tmp += item->member_size;
	}

	return buffer;
}

static uint8_t *pack_item(uint8_t *buffer, void *data, struct state_item *item)
{
	int count;
	char *tmp;

	tmp = ((char *)data + item->offset);

	for (count = item->nmemb; count > 0; count--) {
		uint32_t t;
		switch (item->member_size) {
		case 8:
			/* 64-bit state items not supported, but
			   64-bit members can be truncated to 32 bits
			*/
			t = *((uint64_t *)tmp);
			break;
		case 4:
			t = *((uint32_t *)tmp);
			break;
		case 2:
			t = *((uint16_t *)tmp);
			break;
		case 1:
			t = *((uint8_t *)tmp);
			break;
		default:
			/* Invalid size */
			printf("invalid member size: %zu\n", item->member_size);
			continue;
		}

		switch (item->size) {
		case 4:
			*(buffer++) = (t >>  0) & 0xff;
			*(buffer++) = (t >>  8) & 0xff;
			*(buffer++) = (t >> 16) & 0xff;
			*(buffer++) = (t >> 24) & 0xff;
			break;
		case 2:
			*(buffer++) = t & 0xff;
			*(buffer++) = (t >> 8) & 0xff;
			break;
		case 1:
			*(buffer++) = t & 0xff;
			break;
		}

		tmp += item->member_size;
	}

	return buffer;
}

size_t pack_state(void *data, struct state_item *items, uint8_t *buf)
{
	struct state_item *item;
	uint8_t *p;
	size_t data_size;

	data_size = 0;
	for (item = items; item->size > 0; item++)
		data_size += item->size * item->nmemb;

	if (buf) {
		p = buf;
		for (item = items; item->size > 0; item++)
			p = pack_item(p, data, item);
	}

	return data_size;
}

size_t unpack_state(void *data, struct state_item *items, uint8_t *buffer)
{
	struct state_item *item;
	size_t size;
	uint8_t *p;

	p = buffer;
	size = 0;
	for (item = items; item->size > 0; item++) {
		p = unpack_item(p, data, item);
		size += item->size * item->nmemb;
	}

	return size;
}

struct save_state *create_save_state(void)
{
	struct save_state *state;

	state = malloc(sizeof(*state));
	if (state)
		memset(state, 0, sizeof(*state));

	/* Add header chunk; size is set before writing out
	   state */
	if (save_state_add_chunk(state, "CXNS", NULL, 0) < 0) {
		free(state);
		return NULL;
	}

	return state;
}

void destroy_save_state(struct save_state *state)
{
	if (state->data)
		free(state->data);
	free(state);
}

int save_state_find_chunk(struct save_state *state, const char *id,
			  uint8_t **bufp, size_t *sizep)
{
	uint8_t *ptr;
	size_t size;

	/* FIXME skip header */

	/* ptr points to first chunk (not counting the header */
	ptr = state->data + 8;

	while (ptr < state->data + state->size) {
		size = ptr[4];
		size |= ptr[5] << 8;
		size |= ptr[6] << 16;
		size |= ptr[7] << 24;

		printf("%c%c%c%c\n", ptr[0], ptr[1], ptr[2], ptr[3]);

		if (memcmp(ptr, id, 4) == 0) {
			*bufp = ptr + 8;
			*sizep = size;

			return 0;
		}

		ptr += size + 8;
	}

	return -1;
}

int save_state_add_chunk(struct save_state *state, const char *id,
			 uint8_t *buf, size_t size)
{
	size_t new_size;
	uint8_t *tmp, *p;

	new_size = state->size + 4 + 4 + size;

	tmp = realloc(state->data, new_size);
	if (!tmp)
		return -1;

	p = tmp + state->size;
	state->size = new_size;
	state->data = tmp;

	*(p++) = id[0];
	*(p++) = id[1];
	*(p++) = id[2];
	*(p++) = id[3];

	*(p++) = size & 0xff;
	*(p++) = (size >>  8) & 0xff;
	*(p++) = (size >> 16) & 0xff;
	*(p++) = (size >> 24) & 0xff;

	if (buf)
		memcpy(p, buf, size);

	return 0;
}

int save_state_read(struct save_state *state, const char *filename)
{
	uint8_t *buf;
	ssize_t size;
	size_t state_size;

	size = get_file_size(filename);
	if (size < 0)
		return -1;

	buf = malloc(size);
	if (!buf)
		return -1;

	if (readfile(filename, buf, size) < 0) {
		free(buf);
		return -1;
	}

	/* Check for header chunk */
	if ((size < 8) || (memcmp(buf, "CXNS", 4) != 0)) {
		free(buf);
		return -1;
	}

	state_size  = buf[4];
	state_size |= buf[5] << 8;
	state_size |= buf[6] << 16;
	state_size |= buf[7] << 24;

	/* make sure chunk size is valid (allows for garbage at
	   end of file just in case) */
	if (state_size > size - 8) {
		free(buf);
		return -1;
	}

	state->data = buf;
	state->size = size;

	printf("read %d bytes\n", size);

	return 0;
}

int save_state_write(struct save_state *state, const char *filename)
{
	size_t size = state->size - 8;

	state->data[4] = size & 0xff;
	state->data[5] = (size >> 8)  & 0xff;
	state->data[6] = (size >> 16) & 0xff;
	state->data[7] = (size >> 24) & 0xff;
	return writefile(filename, state->data, state->size);
}
