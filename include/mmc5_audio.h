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

#ifndef __MMC5_AUDIO_H__
#define __MMC5_AUDIO_H__

#include "emu.h"

extern int mmc5_audio_init(struct emu *emu);
extern void mmc5_audio_cleanup(struct emu *emu);
extern void mmc5_audio_reset(struct mmc5_audio_state *audio, int);
extern void mmc5_audio_run(struct mmc5_audio_state *audio, uint32_t cycles);
extern void mmc5_audio_end_frame(struct mmc5_audio_state *audio, uint32_t cycles);
extern void mmc5_audio_install_handlers(struct emu *emu, int multi_chip_nsf);
extern int mmc5_audio_load_state(struct emu *emu, struct save_state *state);
extern int mmc5_audio_save_state(struct emu *emu, struct save_state *state);

#endif /* __MMC5_AUDIO_H__ */
