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

#ifndef __NAMCO163_AUDIO_H__
#define __NAMCO163_AUDIO_H__

#include "emu.h"

extern CPU_READ_HANDLER(namco163_audio_read_handler);
extern CPU_WRITE_HANDLER(namco163_audio_write_handler);
extern int namco163_audio_init(struct emu *emu);
extern void namco163_audio_cleanup(struct emu *emu);
extern void namco163_audio_reset(struct namco163_audio_state *audio, int);
extern void namco163_audio_end_frame(struct namco163_audio_state *audio, uint32_t);
extern int namco163_audio_load_state(struct emu *emu, struct save_state *state);
extern int namco163_audio_save_state(struct emu *emu, struct save_state *state);
extern void namco163_audio_run(struct namco163_audio_state *audio,
			       uint32_t cycles);
extern void namco163_audio_install_handlers(struct emu *emu, int multi_chip_nsf);

#endif /* __NAMCO163_AUDIO_H__ */
