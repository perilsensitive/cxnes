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

/* Configure library by modifying this file */

#ifndef NES_NTSC_CONFIG_H
#define NES_NTSC_CONFIG_H

/* Uncomment to enable emphasis support and use a 512 color palette instead
of the base 64 color palette. */
#define NES_NTSC_EMPHASIS 1 

/* The following affect the built-in blitter only; a custom blitter can
handle things however it wants. */

/* Bits per pixel of output. Can be 15, 16, 32, or 24 (same as 32). */
#define NES_NTSC_OUT_DEPTH 32

/* Type of input pixel values. You'll probably use unsigned short
if you enable emphasis above. */
#define NES_NTSC_IN_T unsigned short

/* Each raw pixel input value is passed through this. You might want to mask
the pixel index if you use the high bits as flags, etc. */
#define NES_NTSC_ADJ_IN( in ) in

/* For each pixel, this is the basic operation:
output_color = color_palette [NES_NTSC_ADJ_IN( NES_NTSC_IN_T )] */

/* Uncomment if you are defining your own blitter and don't need the built-in
one. */
/* #define NES_NTSC_NO_BLITTERS 1 */

#endif
