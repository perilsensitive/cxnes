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

#ifndef __STATE_H__
#define __STATE_H__

#include <stdint.h>
#include <stddef.h>

struct save_state;

struct state_item {
	size_t offset;
	size_t size;
	size_t member_size;
	int nmemb;
};

#define STATE_ITEM(type, mbr, sz) \
{ \
	.offset = offsetof(struct type, mbr), \
 	.member_size = sizeof(((struct type *)0)->mbr), \
	.size = sz, \
	.nmemb = 1, \
}

#define STATE_ITEM_ARRAY(type, mbr, sz)	\
{				  \
	.offset = offsetof(struct type, mbr), \
	.size = sz, \
	.member_size = sizeof(((struct type *)0)->mbr[0]),  \
	.nmemb = sizeof(((struct type *)0)->mbr) /	    \
		        sizeof(((struct type *)0)->mbr[0]), \
}

#define STATE_8BIT(type, member) STATE_ITEM(type, member, sizeof(uint8_t))
#define STATE_16BIT(type, member) STATE_ITEM(type, member, sizeof(uint16_t))
#define STATE_32BIT(type, member) STATE_ITEM(type, member, sizeof(uint32_t))

#define STATE_8BIT_ARRAY(type, member) STATE_ITEM_ARRAY(type, member, \
							sizeof(uint8_t))
#define STATE_16BIT_ARRAY(type, member) STATE_ITEM_ARRAY(type, member, \
							sizeof(uint16_t))
#define STATE_32BIT_ARRAY(type, member) STATE_ITEM_ARRAY(type, member, \
							 sizeof(uint32_t))

#define STATE_ITEM_END() { 0, 0, 0, 0 }

size_t pack_state(void *data, struct state_item *items, uint8_t *buf);
size_t unpack_state(void *data, struct state_item *items, uint8_t *buffer);
int save_state_add_chunk(struct save_state *state, const char *id,
			 uint8_t *buf, size_t size);
int save_state_replace_chunk(struct save_state *state, const char *id,
			     uint8_t *buf, size_t size);
int save_state_find_chunk(struct save_state *state, const char *id,
			  uint8_t **bufp, size_t *sizep);

struct save_state *create_save_state(void);
void destroy_save_state(struct save_state *state);

int save_state_write(struct save_state *state, const char *filename);
int save_state_read(struct save_state *state, const char *filename);

#endif
