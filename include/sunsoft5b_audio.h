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

#ifndef __SUNSOFT5B_AUDIO_H__
#define __SUNSOFT5B_AUDIO_H__

#include "emu.h"

extern int sunsoft5b_audio_init(struct emu *emu);
extern void sunsoft5b_audio_cleanup(struct emu *emu);
extern void sunsoft5b_audio_reset(struct sunsoft5b_audio_state *, int);
extern void sunsoft5b_audio_run(struct sunsoft5b_audio_state *, uint32_t cycles);
extern void sunsoft5b_audio_end_frame(struct sunsoft5b_audio_state *, uint32_t cycles);
extern void sunsoft5b_audio_install_handlers(struct emu *, int);
extern int sunsoft5b_audio_load_state(struct emu *emu, struct save_state *state);
extern int sunsoft5b_audio_save_state(struct emu *emu, struct save_state *state);

#endif /* __SUNSOFT5B_AUDIO_H__ */
