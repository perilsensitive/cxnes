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

#ifndef __PALETTE_H__
#define __PALETTE_H__

#include <stdint.h>

extern const uint8_t palette_firebrandx_nes_classic[62 * 3];
extern const uint8_t palette_firebrandx_nostalgia[63 * 3];
extern const uint8_t palette_fceux[62 * 3];
const uint8_t rp2c04_yuv_lookup[64];
extern const uint8_t palette_rc2c03b[64 * 3];
extern const uint8_t palette_rp2c03b[64 * 3];
extern const uint8_t palette_rp2c04_master[64 * 3];

extern void palette_rgb_emphasis(uint8_t *palette);
extern int load_external_palette(uint8_t *palette, const char *filename);

#endif				/* __PALETTE_H__ */
