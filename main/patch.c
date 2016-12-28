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

#include "log.h"
#include "crc32.h"
#include "state.h"
#include "patch.h"

static struct state_item range_list_state_items[] = {
	STATE_32BIT(range_list, offset),
	STATE_32BIT(range_list, length),
	STATE_ITEM_END(),
};

struct range_list *add_range(struct range_list **ranges, int off, int len)
{
	struct range_list *prev, *current, *new_range, *tmp;
	int new_length;
	int merge;

	prev = NULL;
	current = *ranges;
	merge = 0;
	while (current) {
		if (off + len < current->offset) {
			break;
		} else if (off <= current->offset + current->length) {
			merge = 1;
			break;
		}

		prev = current;
		current = current->next;
	}

	/* Merge new range with existing range */
	if (merge) {
		new_length = current->offset + current->length;
		if (off + len > new_length)
			new_length = off + len;

		if (off < current->offset)
			current->offset = off;

		current->length = new_length - current->offset;

		tmp = current->next;

		/* Combine this node with the next one if they
		   overlap now.
		*/
		while (tmp &&
		       (current->offset + current->length > tmp->offset)) {
			new_length = current->offset + current->length;
			if (tmp->offset + tmp->length > new_length)
				new_length = tmp->offset + tmp->length;

			current->length = new_length - current->offset;
			current->next = tmp->next;
			free(tmp);
			tmp = current->next;
		}

		return current;
	}

	/* Insert range into the list */
	new_range = malloc(sizeof(*new_range));
	if (!new_range) {
		log_err("add_range: malloc() failed\n");
		return NULL;
	}

	new_range->offset = off;
	new_range->length = len;
	new_range->output_offset = -1;
	new_range->next = current;
	if (prev)
		prev->next = new_range;
	else
		*ranges = new_range;

	return new_range;
}

void free_range_list(struct range_list **list)
{
	struct range_list *range, *tmp;

	range = *list;
	while (range) {
		tmp = range;
		range = range->next;
		free(tmp);
	}
	*list = NULL;
}

static uint64_t decode(uint8_t *patch, unsigned *offset)
{
	uint64_t value;
	uint64_t shift;

	value = 0;
	shift = 1;
	while (1) {
		uint8_t tmp;

		tmp = patch[*offset];
		*offset = *offset + 1;
		value += (tmp & 0x7f) * shift;

		if (tmp & 0x80)
			break;

		shift <<= 7;
		value += shift;
	}

	return value;
}

int patch_get_input_size(uint8_t *patch, size_t patch_size, size_t *input_size)
{
	unsigned patch_offset;
	size_t expected_data_size;

	patch_offset = 0;

	if (patch_size < 4)
		return -1;

	if ((memcmp(patch, "UPS1", 4) == 0) ||
	    (memcmp(patch, "BPS1", 4) == 0)) {
		patch_offset += 4; /* Skip ID string */
		expected_data_size = decode(patch, &patch_offset);

		if (input_size)
			*input_size = expected_data_size;

		return 0;
	}

	return -1;
}

int patch_get_output_size(uint8_t *patch, size_t patch_size, size_t *output_size)
{
	unsigned patch_offset;
	size_t target_size;

	patch_offset = 0;

	if (patch_size < 4)
		return -1;

	if ((memcmp(patch, "UPS1", 4) == 0) ||
	    (memcmp(patch, "BPS1", 4) == 0)) {
		patch_offset += 4; /* Skip ID string */

		/* First size is expected data size, skip */
		target_size = decode(patch, &patch_offset);
		target_size = decode(patch, &patch_offset);

		if (output_size)
			*output_size = target_size;

		return 0;
	}

	return -1;
}

int patch_get_input_crc(uint8_t *patch, size_t patch_size, uint32_t *crc)
{
	if (patch_size < 16)
		return -1;

	if ((memcmp(patch, "UPS1", 4) == 0) ||
	    (memcmp(patch, "BPS1", 4) == 0)) {
		int offset = patch_size - 12;

		if (crc) {
			*crc = patch[offset];
			*crc |= patch[offset + 1] << 8;
			*crc |= patch[offset + 2] << 16;
			*crc |= patch[offset + 3] << 24;
		}

		return 0;
	}

	return -1;
}

int patch_get_patch_crc(uint8_t *patch, size_t patch_size, uint32_t *crc)
{
	if (patch_size < 16)
		return -1;

	if ((memcmp(patch, "UPS1", 4) == 0) ||
	    (memcmp(patch, "BPS1", 4) == 0)) {
		int offset = patch_size - 4;

		if (crc) {
			*crc = patch[offset];
			*crc |= patch[offset + 1] << 8;
			*crc |= patch[offset + 2] << 16;
			*crc |= patch[offset + 3] << 24;
		}

		return 0;
	}

	return -1;
}

uint32_t patch_calc_patch_crc(uint8_t *patch, size_t patch_size)
{
	uint32_t patch_crc;
	size_t size;

	if (patch_size < 16)
		return -1;

	if ((memcmp(patch, "UPS1", 4) == 0) ||
	    (memcmp(patch, "BPS1", 4) == 0)) {
		size = patch_size - 4;
	} else {
		size = patch_size;
	}

	patch_crc = crc32buf(patch, size, NULL);

	return patch_crc;
}

int patch_get_output_crc(uint8_t *patch, size_t patch_size, uint32_t *crc)
{
	if (patch_size < 16)
		return -1;

	if ((memcmp(patch, "UPS1", 4) == 0) ||
	    (memcmp(patch, "BPS1", 4) == 0)) {
		int offset = patch_size - 8;

		if (crc) {
			*crc = patch[offset];
			*crc |= patch[offset + 1] << 8;
			*crc |= patch[offset + 2] << 16;
			*crc |= patch[offset + 3] << 24;
		}

		return 0;
	}

	return -1;
}

int patch_apply(uint8_t **data, size_t *data_size, off_t data_offset,
		off_t target_offset, uint32_t data_crc, uint8_t *patch,
		size_t patch_size)
{
	int rc;
	uint32_t input_crc, output_crc;
	uint32_t expected_patch_crc, patch_crc;

	rc = -1;

	patch_crc = patch_calc_patch_crc(patch, patch_size);
	if ((patch_get_patch_crc(patch, patch_size, &expected_patch_crc) == 0) &&
	    (patch_crc != expected_patch_crc)) {
		log_warn("Patch CRC doesn't match expected CRC "
			 "(%08x != %08x)\n", patch_crc,
			 expected_patch_crc);
	}

	if ((patch_get_input_crc(patch, patch_size, &input_crc) == 0) &&
	    (data_crc != input_crc)) {
		log_warn("Input CRC doesn't match expected CRC "
			 "(%08x != %08x)\n", data_crc,
			 input_crc);
	}

	if ((patch_size >= 4) && (memcmp(patch, "BPS1", 4) == 0)) {
		rc = patch_apply_bps(data, data_size, data_offset,
				     target_offset, patch, patch_size);
	} else if ((patch_size >= 4) && (memcmp(patch, "UPS1", 4) == 0)) {
		rc = patch_apply_ups(data, data_size, data_offset,
				     target_offset, patch, patch_size);
	} else if ((patch_size >= 5) &&(memcmp(patch, "PATCH", 5) == 0)) {
		rc = patch_apply_ips(data, data_size, data_offset,
				     target_offset, patch, patch_size, NULL);
	}

	data_crc = crc32buf(*data, *data_size, NULL);

	if ((patch_get_output_crc(patch, patch_size, &output_crc) == 0) &&
	    (data_crc != output_crc) && (rc == 0)) {
		log_warn("Output CRC doesn't match expected CRC "
			 "(%08x != %08x)\n", data_crc,
			 output_crc);
	}

	return rc;
}

int patch_apply_ups(uint8_t **data, size_t *data_size, off_t data_offset,
		    off_t target_offset, uint8_t *patch, size_t patch_size)
{
	size_t expected_data_size;
	size_t target_size;
	uint8_t *target, *real_target, *_data;
	unsigned patch_offset;
	unsigned length;
	off_t orig_target_offset, orig_data_offset;

	patch_offset = 0;
	orig_target_offset = target_offset;
	orig_data_offset = data_offset;

	/* Make sure the header is valid */
	if (memcmp(patch, "UPS1", 4) != 0) {
		log_err("patch_apply_ups: invalid UPS header\n");
		return -1;
	}

	patch_offset += 4; /* Skip ID string */
	expected_data_size = decode(patch, &patch_offset);

	if (expected_data_size != *data_size - data_offset) {
		log_err("patch_apply_ups: expected data size (%zu) "
			"does not equal actual data size (%zu)\n",
			expected_data_size, *data_size);
		return -1;
	}
	target_size = decode(patch, &patch_offset);

	real_target = malloc(target_size + target_offset);
	if (!real_target) {
		log_err("patch_apply_ups: malloc() failed\n");
		return -1;
	}

	if ((data_offset == target_offset) && (data_offset > 0)) {
		memcpy(real_target, *data, data_offset);
	}

	target = real_target + target_offset;

	_data = *data + data_offset;

	target_offset = 0;
	data_offset = 0;

	while (patch_offset < patch_size - 12) {
		/* Stop before CRC32 checksums at the end of the file */
		length = decode(patch, &patch_offset);
		memcpy(target + target_offset, _data + data_offset, length);
		target_offset += length;
		data_offset += length;
		while (1) {
			uint8_t tmp;
			tmp = patch[patch_offset];
			target[target_offset] = _data[data_offset] ^ tmp;
			patch_offset++;
			target_offset++;
			data_offset++;
			
			if (!tmp)
				break;
		}
	}


	length = *data_size - data_offset - orig_data_offset;
	memcpy(target + target_offset, _data + data_offset, length);
	target_offset += length;

	length = target_size - target_offset;
	memset(target + target_offset, 0, length);
	target_offset += length;
	
	free(*data);
	*data = real_target;
	*data_size = target_size + orig_target_offset;

	return 0;
}

int patch_apply_bps(uint8_t **data, size_t *data_size, off_t data_offset,
		    off_t target_offset, uint8_t *patch, size_t patch_size)
{
	size_t expected_data_size;
	size_t target_size;
	size_t markup_size;
	uint8_t *target, *real_target, *_data;
	unsigned patch_offset;
	unsigned data_relative_offset;
	unsigned target_relative_offset;
	off_t orig_target_offset;

	orig_target_offset = target_offset;

	patch_offset = 0;

	/* Make sure the header is valid */
	if (memcmp(patch, "BPS1", 4) != 0) {
		log_err("patch_apply_bps: invalid BPS header\n");
		return -1;
	}

	patch_offset += 4; /* Skip ID string */
	expected_data_size = decode(patch, &patch_offset);

	if (expected_data_size != *data_size - data_offset) {
		log_err("patch_apply_bps: expected data size (%zu) "
			"does not equal actual data size (%zu)\n",
			expected_data_size, *data_size);
		return -1;
	}
	target_size = decode(patch, &patch_offset);
	markup_size = decode(patch, &patch_offset);
	patch_offset += markup_size; /* Skip markup */

	_data = *data + data_offset;

	real_target = malloc(target_size + target_offset);
	if (!real_target) {
		log_err("patch_apply_bps: malloc() failed\n");
		return -1;
	}
	target = real_target + target_offset;

	if ((data_offset == target_offset) && (data_offset > 0)) {
		memcpy(real_target, *data, data_offset);
	}

	data_offset = 0;
	target_offset = 0;
	data_relative_offset = 0;
	target_relative_offset = 0;

	while (patch_offset < patch_size - 12) {
		/* Stop before CRC32 checksums at the end of the file */
		unsigned length;
		int mode;
		int offset;
		int negative;
		uint8_t *src;
		unsigned *offset_ptr;

		length = decode(patch, &patch_offset);
		mode = length & 0x03;
		length = (length >> 2) + 1;

		switch (mode) {
		case 0:
			src = _data + target_offset;
			offset_ptr = NULL;
			break;
		case 1:
			src = patch + patch_offset;
			offset_ptr = &patch_offset;
			break;
		case 2:
		case 3:
			offset = decode(patch, &patch_offset);
			negative = offset & 1;
			offset >>= 1;

			if (negative)
				offset = -offset;

			if (mode == 2) {
				data_relative_offset += offset;
				src = _data + data_relative_offset;
				offset_ptr = &data_relative_offset;
			} else {
				target_relative_offset += offset;
				src = target + target_relative_offset;
				offset_ptr = &target_relative_offset;
			}
			break;
		}

		while (length--) {
			target[target_offset] = *src;
			target_offset++;
			src++;
			if (offset_ptr)
				(*offset_ptr)++;
		}
	}

	free(*data);
	*data = real_target;
	*data_size = target_size + orig_target_offset;

	return 0;
}

int patch_apply_ips(uint8_t **data, size_t *data_size, off_t data_offset,
		    off_t target_offset, uint8_t *patch, size_t patch_size,
		    struct range_list **ranges)
{
	int i;
	size_t target_size;
	uint8_t *real_target, *target, *_data;

	/* Make sure the header and footer are valid, and allow for
	   the "truncate" extension after the footer. */
	if (memcmp(patch, "PATCH", 5) != 0) {
		log_err("patch_apply_ips: invalid IPS header\n");
		return -1;
	} else if ((memcmp(patch + patch_size - 3, "EOF", 3) != 0) &&
		   (memcmp(patch + patch_size - 6, "EOF", 3) != 0)) {
		log_err("patch_apply_ips: invalid IPS footer\n");
		return -1;
	}

	_data = *data + data_offset;

	/* Calculate target size only */
	target_size = *data_size - data_offset;
	i = 5;			/* Skip 'PATCH' header */
	while (i < patch_size - 3) {
		int offset;
		int size;
		int rle_size;
		int tmp;

		offset = (patch[i] << 16) | (patch[i + 1] << 8) | patch[i + 2];
		size = (patch[i + 3] << 8) | patch[i + 4];
		rle_size = 0;

		if (size == 0) {
			/* Patch is RLE */
			rle_size = (patch[i + 5] << 8) | patch[i + 6];
			i += 3;
		}

		if (rle_size)
			tmp = offset + rle_size;
		else
			tmp = offset + size;

		if (tmp > target_size)
			target_size = tmp;

		i += 5 + size;
	}
	/* FIXME truncate? */

	real_target = malloc(target_size + target_offset);
	if (!real_target) {
		log_err("patch_apply_bps: malloc() failed\n");
		return -1;
	}

	if ((data_offset == target_offset) && (data_offset > 0)) {
		memcpy(real_target, *data, data_offset);
	}

	target = real_target + target_offset;

	if (*data_size - data_offset < target_size)
		memcpy(target, _data, *data_size - data_offset);
	else
		memcpy(target, _data, target_size);

	i = 5;			/* Skip 'PATCH' header */
	while (i < patch_size - 3) {
		/* Stop before 'EOF' footer */
		int offset;
		int size;
		int rle_size;
		int chunk_size;
		uint8_t rle_value = 0;

		offset = (patch[i] << 16) | (patch[i + 1] << 8) | patch[i + 2];
		size = (patch[i + 3] << 8) | patch[i + 4];
		chunk_size = size;
		rle_size = 0;

		if (size == 0) {
			/* Patch is RLE */
			rle_size = (patch[i + 5] << 8) | patch[i + 6];
			rle_value = patch[i + 7];

			if (rle_size + offset > target_size) {
				rle_size = target_size - offset;
				if (rle_size < 0)
					rle_size = 0;
			}

			size = rle_size;

			i += 3;
		} else if (size + offset > target_size) {
			size = target_size - offset;
			if (size < 0)
				size = 0;
		}


		/* point to start of data */
		i += 5;

		if (ranges && size) {
			add_range(ranges, offset + data_offset, size);
		}

		if (rle_size > 0) {
			memset(target + offset, rle_value, size);
		} else if (size > 0) {
			//printf("offset: %d, size: %d\n", offset, size);
			memcpy(target + offset, patch + i, size);
		}

		/* Point to start of next patch (if rle, size = 0
		   and we are already pointing to it */
		i += chunk_size;
	}

	free(*data);
	*data = real_target;
	*data_size = target_size + target_offset;

	return 0;
}

int patch_create_ips(uint8_t *src, struct range_list *ranges, uint8_t **patch)
{
	struct range_list *range;
	uint8_t *data = NULL;
	int size = 0;

	if (ranges == NULL)
		return 0;

	/* Calculate the size of the save data */
	size = 5; /* header */
	for (range = ranges; range; range = range->next) {
		/* 3 for offset, 2 for length */
		size += range->length + 3 + 2;
	}
	size += 3; /* footer */

	data = malloc(size);
	if (!data)
		return -1;

	memcpy(data, "PATCH", 5);

	size = 5;
	/* Now generate the IPS patch */
	for (range = ranges; range; range = range->next) {
		if (range->output_offset == -1)
			range->output_offset = range->offset;

		data[size] = (range->output_offset >> 16) & 0xff;
		data[size + 1] = (range->output_offset >> 8) & 0xff;
		data[size + 2] = (range->output_offset) & 0xff;
		data[size + 3] = (range->length >> 8) & 0xff;
		data[size + 4] = range->length & 0xff;
		memcpy(data + size + 5, src + range->offset, range->length);
		size += range->length + 5;
	}

	memcpy(data + size, "EOF", 3);
	size += 3;

	*patch = data;

	return size;
}

int range_list_load_state(struct range_list **ranges,
			  uint8_t *data, size_t data_size,
			  struct save_state *state,
			  const char *id)
{
	uint8_t *buf, *ptr;
	size_t size, remaining, chunk_header_size;
	struct range_list tmp;

	if (save_state_find_chunk(state, id, &buf, &size) < 0)
		return 1;

	chunk_header_size = pack_state(*ranges, range_list_state_items, NULL);

	free_range_list(ranges);

	remaining = size;
	ptr = buf;
	while (remaining > 0) {
		ptr += unpack_state(&tmp, range_list_state_items, ptr);
		remaining -= chunk_header_size;
		add_range(ranges, tmp.offset, tmp.length);

		if (tmp.offset + tmp.length <= data_size)
			memcpy(data + tmp.offset, ptr, tmp.length);

		ptr += tmp.length;
		remaining -= tmp.length;
	}

	return 0;
}

int range_list_save_state(struct range_list **ranges,
			  uint8_t *data, size_t data_size,
			  struct save_state *state,
			  const char *id)
{
	uint8_t *buf, *ptr;
	size_t size;
	struct range_list *head;
	int count;
	int rc;

	head = *ranges;

	if (!head)
		return 0;
	

	count = 0;
	while (head) {
		count++;
		head = head->next;
	}

	size = count * pack_state(*ranges, range_list_state_items, NULL);
	head = *ranges;
	while (head) {
		size += head->length;
		head = head->next;
	}

	buf = malloc(size);
	if (!buf)
		return -1;

	ptr = buf;
	head = *ranges;
	while (head) {
		ptr += pack_state(head, range_list_state_items, ptr);
		memcpy(ptr, data + head->offset, head->length);
		ptr += head->length;
		head = head->next;
	}

	rc = save_state_add_chunk(state, id, buf, size);

	free(buf);

	return rc;
}

int patch_is_ips(uint8_t *patch, size_t patch_size)
{
	if (patch_size < 8)
		return 0;

	if (memcmp(patch, "PATCH", 5) != 0)
		return 0;

	if ((memcmp(patch + patch_size - 3, "EOF", 3) != 0) &&
	    (memcmp(patch + patch_size - 6, "EOF", 3) != 0)) {
		return 0;
	}

	return 1;
}

struct range_list *patch_generate_range_list(uint8_t *dst, size_t dst_size,
                                             uint8_t *src, size_t src_size)
{
	int i;
	size_t min_size;
	struct range_list *ranges;

	ranges = NULL;

	if (dst_size > src_size)
		min_size = src_size;
	else
		min_size = dst_size;

	for (i = 0; i < min_size; i++) {
		if (dst[i] == src[i])
			continue;

		add_range(&ranges, i, 1);
	}

	if (dst_size > src_size)
		add_range(&ranges, src_size, dst_size - src_size);
	else if (src_size > dst_size)
		add_range(&ranges, dst_size, src_size - dst_size);

	return ranges;
}
