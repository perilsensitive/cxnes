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

#ifndef __PATCH_H__
#define __PATCH_H__

#include <stdint.h>

struct range_list {
	int offset;
	int length;
	int output_offset;
	struct range_list *next;
};

struct save_state;

int patch_apply(uint8_t **data, size_t *data_size, off_t data_offset,
		off_t target_offset, uint32_t data_crc, uint8_t *patch, size_t patch_size);
int patch_apply_ups(uint8_t ** data, size_t * data_size, off_t data_offset,
		    off_t target_offset, uint8_t * patch, size_t patch_size);
int patch_apply_bps(uint8_t ** data, size_t * data_size, off_t data_offset,
		    off_t target_offset, uint8_t * patch, size_t patch_size);
int patch_apply_ips(uint8_t ** data, size_t * data_size, off_t data_offset,
		    off_t target_offset, uint8_t * patch, size_t patch_size, struct range_list **);
int patch_create_ips(uint8_t *, struct range_list *, uint8_t **);
struct range_list *add_range(struct range_list **ranges, int off, int len);
void free_range_list(struct range_list **list);
int range_list_load_state(struct range_list **ranges, uint8_t *data,
			  size_t data_size, struct save_state *state,
			  const char *id);
int range_list_save_state(struct range_list **ranges, uint8_t *data,
			  size_t data_size, struct save_state *state,
			  const char *id);

int patch_get_input_size(uint8_t* patch, size_t patch_size,
			  size_t *input_size);
int patch_get_output_size(uint8_t* patch, size_t patch_size,
			  size_t *input_size);
int patch_get_input_crc(uint8_t *patch, size_t patch_size, uint32_t *crc);
int patch_get_output_crc(uint8_t *patch, size_t patch_size, uint32_t *crc);
int patch_get_patch_crc(uint8_t *patch, size_t patch_size, uint32_t *crc);
uint32_t patch_calc_patch_crc(uint8_t *patch, size_t patch_size);
int patch_is_ips(uint8_t *patch, size_t patch_size);
#endif
