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

#ifndef __NSF_H__
#define __NSF_H__

#include "emu.h"

int nsf_load(struct emu *emu, struct rom *rom);
const char *nsf_get_title(struct rom *rom);
const char *nsf_get_artist(struct rom *rom);
const char *nsf_get_copyright(struct rom *rom);
const char *nsf_get_region(struct rom *rom);
int nsf_get_song_count(struct rom *rom);
int nsf_get_first_song(struct rom *rom);
int nsf_get_expansion_sound(struct rom *rom);
extern const char *nsf_expansion_audio_chips[];

#endif				/* __NSF_H__ */
