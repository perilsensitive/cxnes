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

#ifndef __APU_H__
#define __APU_H__

#define APU_TYPE_RP2A03 0
#define APU_TYPE_RP2A07 1
#define APU_TYPE_DENDY  2

#include "emu.h"

struct apu_state;

void apu_run(struct apu_state *apu, uint32_t cycles);
void apu_end_frame(struct apu_state *apu, uint32_t cycles);
void apu_reset(struct apu_state *apu, int hard);
int apu_init(struct emu *emu);
void apu_cleanup(struct apu_state *apu);
void apu_dmc_load_buf(struct apu_state * apu, uint8_t data,
		      uint32_t *dma_time_ptr,
		      int *dma_addr_ptr, uint32_t cycles);
void apu_set_type(struct apu_state *apu, int type);
int apu_apply_config(struct apu_state *apu);
int apu_save_state(struct apu_state *apu, struct save_state *state);
int apu_load_state(struct apu_state *apu, struct save_state *state);
extern CPU_WRITE_HANDLER(oam_dma);

#endif				/* __APU_H__ */
