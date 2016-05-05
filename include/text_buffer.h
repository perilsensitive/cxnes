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

struct text_buffer;

extern struct text_buffer *text_buffer_new(void);
extern void text_buffer_free(struct text_buffer *buffer);
extern void text_buffer_clear(struct text_buffer *buffer);
extern const char *text_buffer_get_text(struct text_buffer *buffer);
extern int text_buffer_append(struct text_buffer *buffer, const char *format, ...);
