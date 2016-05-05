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

#ifndef __FDS_H__
#define __FDS_H__

#include "emu.h"

struct fds_block_list_entry {
	uint8_t type;
	off_t offset;
	off_t new_offset;
	size_t size;
	int crc;
	int calculated_crc;
	int data_crc;
	int calculated_data_crc;
        uint32_t crc32;
	uint8_t sha1[20];
};

struct fds_block_list {
	int total_entries;
	int available_entries;
	struct fds_block_list_entry *entries;
};

const uint16_t fds_crc_table[256];

int fds_load_bios(struct emu *emu, struct rom *rom);
int fds_load(struct emu *, struct rom *rom);
int fds_convert_to_raw(struct rom *rom);
int fds_convert_to_fds(struct rom *rom);
struct fds_block_list *fds_block_list_new(void);
void fds_block_list_free(struct fds_block_list *list);
void fds_block_list_print(struct fds_block_list *list);
int fds_validate_image(struct rom *rom,
		       struct fds_block_list **block_listp, int do_crc);
int fds_get_disk_info(struct rom *rom, struct text_buffer *buffer);
int fds_disk_sanity_checks(struct rom *rom);

#endif				/* __FDS_H__ */
