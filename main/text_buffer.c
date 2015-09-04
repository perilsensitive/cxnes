/*
  cxNES - NES/Famicom Emulator
  Copyright (C) 2011-2015 Ryan Jackson

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
#include <stdarg.h>
#include <stdint.h>

struct text_buffer {
	char *text;
	size_t size;
	size_t used;
};

struct text_buffer *text_buffer_new(void)
{
	struct text_buffer *buffer;

	buffer = malloc(sizeof(*buffer));
	if (!buffer)
		return NULL;

	buffer->text = NULL;
	buffer->size = 0;
	buffer->used = 0;

	return buffer;
}

void text_buffer_free(struct text_buffer *buffer)
{

	if (!buffer)
		return;

	if (buffer->text)
		free(buffer->text);

	free (buffer);
}

void text_buffer_clear(struct text_buffer *buffer)
{
	if (!buffer)
		return;

	buffer->used = 0;
}

const char *text_buffer_get_text(struct text_buffer *buffer)
{
	if (!buffer || !buffer->text || !buffer->used)
		return NULL;

	return buffer->text;
}

int text_buffer_append(struct text_buffer *buffer, const char *format, ...)
{
        va_list args;
	size_t avail;
	size_t needed;
	int rc;

	if (!buffer)
		return -1;

        va_start(args, format);

	avail = buffer->size - buffer->used;
	needed = vsnprintf(buffer->text, 0, format, args) + 1;

        va_end(args);

	if (needed > avail) {
		size_t new_size;
		char *tmp;

		new_size = buffer->size + (needed - avail);

		tmp = realloc(buffer->text, new_size);
		if (!tmp) {
		        va_end(args);
			return -1;
		}

		buffer->text = tmp;
		buffer->size = new_size;
	}

        va_start(args, format);

	rc = vsnprintf(buffer->text + buffer->used, needed, format, args);
	if (rc >= 0)
		buffer->used += needed - 1;

        va_end(args);

	return rc;
}
