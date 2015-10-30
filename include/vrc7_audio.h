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

#ifndef __VRC7_AUDIO_H__
#define __VRC7_AUDIO_H__

#include "emu.h"

extern int vrc7_audio_init(struct emu *emu);
extern void vrc7_audio_cleanup(struct emu *emu);
extern void vrc7_audio_reset(struct vrc7_audio_state *audio, int);
extern void vrc7_audio_end_frame(struct vrc7_audio_state *audio, uint32_t);
extern void vrc7_audio_install_handlers(struct emu *emu, int multi_chip_nsf);
extern int vrc7_audio_load_state(struct emu *emu, struct save_state *state);
extern int vrc7_audio_save_state(struct emu *emu, struct save_state *state);
extern void vrc7_audio_run(struct vrc7_audio_state *audio, uint32_t cycles);

#endif /* __VRC7_AUDIO_H__ */
