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
#include <stdint.h>
#include <math.h>

#include "file_io.h"

/* RC2C05*, RP2C03B and RC2C0C have the same palette */

/* RC2C03B differs at 0x17, 0x25, 0x09, 0x29, 0x39 */

const uint8_t palette_rp2c03b[64 * 3] = {
	0x6d,0x6d,0x6d, 0x00,0x24,0x92, 0x00,0x00,0xdb, 0x6d,0x49,0xdb,
	0x92,0x00,0x6d, 0xb6,0x00,0x6d, 0xb6,0x24,0x00, 0x92,0x49,0x00,
	0x6d,0x49,0x00, 0x24,0x49,0x00, 0x00,0x6d,0x24, 0x00,0x92,0x00,
	0x00,0x49,0x49, 0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00,
	0xb6,0xb6,0xb6, 0x00,0x6d,0xdb, 0x00,0x49,0xff, 0x92,0x00,0xff,
	0xb6,0x00,0xff, 0xff,0x00,0x92, 0xff,0x00,0x00, 0xdb,0x6d,0x00,
	0x92,0x6d,0x00, 0x24,0x92,0x00, 0x00,0x92,0x00, 0x00,0xb6,0x6d,
	0x00,0x92,0x92, 0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00,
	0xff,0xff,0xff, 0x6d,0xb6,0xff, 0x92,0x92,0xff, 0xdb,0x6d,0xff,
	0xff,0x00,0xff, 0xff,0x6d,0xff, 0xff,0x92,0x00, 0xff,0xb6,0x00,
	0xdb,0xdb,0x00, 0x6d,0xdb,0x00, 0x00,0xff,0x00, 0x49,0xff,0xdb,
	0x00,0xff,0xff, 0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00,
	0xff,0xff,0xff, 0xb6,0xdb,0xff, 0xdb,0xb6,0xff, 0xff,0xb6,0xff,
	0xff,0x92,0xff, 0xff,0xb6,0xb6, 0xff,0xdb,0x92, 0xff,0xff,0x49,
	0xff,0xff,0x6d, 0xb6,0xff,0x49, 0x92,0xff,0x6d, 0x49,0xff,0xdb,
	0x92,0xdb,0xff, 0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00
};

const uint8_t palette_rc2c03b[64 * 3] = {
	0x6d,0x6d,0x6d, 0x00,0x24,0x92, 0x00,0x00,0xdb, 0x6d,0x49,0xdb,
	0x92,0x00,0x6d, 0xb6,0x00,0x6d, 0xb6,0x24,0x00, 0x92,0x49,0x00,
	0x6d,0x49,0x00, 0x24,0x00,0x00, 0x00,0x6d,0x24, 0x00,0x92,0x00,
	0x00,0x49,0x49, 0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00,
	0xb6,0xb6,0xb6, 0x00,0x24,0xdb, 0x00,0x49,0xff, 0x92,0x00,0xff,
	0xb6,0x00,0xff, 0xff,0x00,0x92, 0xff,0x00,0x00, 0xdb,0x6d,0x00,
	0x92,0x6d,0x00, 0x24,0x92,0x00, 0x00,0x92,0x00, 0x00,0xb6,0x6d,
	0x00,0x92,0x92, 0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00,
	0xff,0xff,0xff, 0x6d,0xb6,0xff, 0x92,0x92,0xff, 0xdb,0x6d,0xff,
	0xff,0x00,0xff, 0xff,0x24,0xff, 0xff,0x92,0x00, 0xff,0xb6,0x00,
	0xdb,0xdb,0x00, 0x6d,0x92,0x00, 0x00,0xff,0x00, 0x49,0xff,0xdb,
	0x00,0xff,0xff, 0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00,
	0xff,0xff,0xff, 0xb6,0x92,0xff, 0xdb,0xb6,0xff, 0xff,0xb6,0xff,
	0xff,0x92,0xff, 0xff,0xb6,0xb6, 0xff,0xdb,0x92, 0xff,0xff,0x49,
	0xff,0xff,0x6d, 0xb6,0xb6,0x49, 0x92,0xff,0x6d, 0x49,0xff,0xdb,
	0x92,0xdb,0xff, 0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00
};

/* The RP2C04 palettes, reordered to be as close as possible to the
   RP2C03 palette.
*/
const uint8_t palette_rp2c04_master[64 * 3] = {
	0x6d,0x6d,0x6d, 0x00,0x24,0x92, 0x00,0x00,0xdb, 0x6d,0x49,0xdb,
	0x92,0x00,0x6d, 0xb6,0x00,0x6d, 0xb6,0x24,0x00, 0x92,0x49,0x00,
	0x6d,0x49,0x00, 0x24,0x49,0x00, 0x00,0x6d,0x24, 0x00,0x00,0x00,
	0x00,0x49,0x49, 0x24,0x24,0x24, 0x00,0x00,0x6d, 0x00,0x00,0x00,
	0xb6,0xb6,0xb6, 0x00,0x6d,0xdb, 0x00,0x49,0xff, 0x92,0x00,0xff,
	0xb6,0x00,0xff, 0xff,0x00,0x92, 0xff,0x00,0x00, 0xdb,0x6d,0x00,
	0x92,0x6d,0x00, 0x24,0x92,0x00, 0x00,0x92,0x00, 0x00,0xb6,0x6d,
	0x00,0x92,0x92, 0x49,0x49,0x49, 0x49,0x00,0x00, 0x6d,0x24,0x00,
	0xff,0xff,0xff, 0x6d,0xb6,0xff, 0x92,0x92,0xff, 0xdb,0x6d,0xff,
	0xff,0x00,0xff, 0xff,0x6d,0xff, 0xff,0x92,0x00, 0xff,0xb6,0x00,
	0xdb,0xdb,0x00, 0x6d,0xdb,0x00, 0x00,0xff,0x00, 0x00,0x00,0x00,
	0x00,0xff,0xff, 0x92,0x92,0x92, 0x00,0x00,0x00, 0x00,0x49,0x00,
	0xff,0xff,0xff, 0xb6,0xdb,0xff, 0xdb,0xb6,0xff, 0xff,0xb6,0xff,
	0xff,0x92,0xff, 0xff,0xb6,0xb6, 0xff,0xdb,0x92, 0xff,0xff,0x00,
	0xff,0xff,0x6d, 0xb6,0xff,0x49, 0x92,0xff,0x6d, 0x49,0xff,0xdb,
	0x92,0xdb,0xff, 0xdb,0xdb,0xdb, 0xdb,0xb6,0x6d, 0xff,0xdb,0x00,
};

/* Applies the emphasis used by RGB PPUs to a 64-color palette */
/* palette is a pointer to an array of 512 * 3 uint8_t */
/* The first 64 colors of the palette must already be set */
void palette_rgb_emphasis(uint8_t * palette)
{
	int i, j;
	uint8_t emph[8 * 3] = {
		0x00,0x00,0x00, 0x00,0x00,0xff, 0x00,0xff,0x00, 0x00,0xff,0xff,
		0xff,0x00,0x00, 0xff,0x00,0xff, 0xff,0xff,0x00, 0xff,0xff,0xff
	};

	for (i = 1; i < 8; i++) {
		for (j = 0; j < 64; j++) {
			palette[(i * 64 * 3) + (j * 3) + 0] = palette[j * 3 + 0] | emph[i + 0];
			palette[(i * 64 * 3) + (j * 3) + 1] = palette[j * 3 + 1] | emph[i + 1];
			palette[(i * 64 * 3) + (j * 3) + 2] = palette[j * 3 + 2] | emph[i + 2];
		}
	}
}

/* filename is the filename containing the palette, *palette points
   to a 512-color palette of uint8_ts
*/
int load_external_palette(uint8_t *palette, const char *filename)
{
	int size;

	if (!palette)
		return -1;

	size = get_file_size(filename);
	if (size < 192)
		return -1;

	if (size < 1536)
		size = 192;
	else
		size = 1536;

	if (readfile(filename, palette, size) != 0) {
		return -1;
	}

	return size / 3;
}
