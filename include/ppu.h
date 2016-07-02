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

#ifndef __PPU_H__
#define __PPU_H__

#include "emu.h"

struct ppu_state;

int ppu_run(struct ppu_state *ppu, int cycles);
void ppu_begin_frame(struct ppu_state *, uint32_t * buf, uint32_t *nes_pixel_buf);
uint32_t ppu_end_frame(struct ppu_state *, uint32_t cycles);
int ppu_apply_config(struct ppu_state *ppu);
void ppu_reset(struct ppu_state *, int hard);
int ppu_init(struct emu *emu);
void ppu_cleanup(struct ppu_state *ppu);
void ppu_map_nametable(struct ppu_state *ppu, int nametable, uint8_t * data,
		       int rw, uint32_t cycles);
void ppu_set_pagemap_entry(struct ppu_state *ppu, int map, int page,
			   int size, uint8_t * data, int rw, uint32_t cycles);
void ppu_select_bg_pagemap(struct ppu_state *ppu, int map, uint32_t cycles);
void ppu_select_spr_pagemap(struct ppu_state *ppu, int map, uint32_t cycles);
uint8_t ppu_get_register(struct ppu_state *, int);
void ppu_set_type(struct ppu_state *, int type);
int ppu_get_type(struct ppu_state *);
void ppu_set_reset_connected(struct ppu_state *ppu, int connected);
int ppu_get_reset_connected(struct ppu_state *ppu);
void ppu_set_read_hook(void (*hook) (struct board *, int));
void ppu_enable_a12_timer(struct ppu_state *ppu, int);
uint8_t *ppu_get_oam_ptr(struct ppu_state *ppu);
void ppu_use_exram(struct ppu_state *ppu, int mode, uint32_t cycles);
uint32_t ppu_get_cycles(struct ppu_state *ppu, int *, int *, int *, int *);
int ppu_get_burst_phase(struct ppu_state *ppu);

/* Mirroring types */
#define PPU_PAGE_READ 1
#define PPU_PAGE_WRITE 2
#define PPU_PAGE_READWRITE 3

#define PPU_TYPE_RP2C03B       0
#define PPU_TYPE_RP2C03G       1
#define PPU_TYPE_RP2C04_0001   2
#define PPU_TYPE_RP2C04_0002   3
#define PPU_TYPE_RP2C04_0003   4
#define PPU_TYPE_RP2C04_0004   5
#define PPU_TYPE_RC2C03B       6
#define PPU_TYPE_RC2C03C       7
#define PPU_TYPE_RC2C05_01     8
#define PPU_TYPE_RC2C05_02     9
#define PPU_TYPE_RC2C05_03    10
#define PPU_TYPE_RC2C05_04    11
#define PPU_TYPE_RC2C05_05    12
/* Leave types 13-15 undefined */
#define PPU_TYPE_RP2C02       16
#define PPU_TYPE_RP2C07       17
#define PPU_TYPE_DENDY        18

void ppu_set_split_screen(struct ppu_state *, int enabled, int right_side,
			  int startstop, uint32_t cycles);
void ppu_set_split_screen_scroll(struct ppu_state *, int value,
				 uint32_t cycles);
void ppu_set_split_screen_bank(struct ppu_state *, int value, uint32_t cycles);
void ppu_set_sprite_limit(struct ppu_state *, int);
void ppu_set_sprite_hiding(struct ppu_state *, int);
void ppu_set_scanline_renderer(struct ppu_state *, int);
void ppu_toggle_bg(struct ppu_state *);
void ppu_toggle_sprites(struct ppu_state *);
int ppu_save_state(struct ppu_state *ppu, struct save_state *state);
int ppu_load_state(struct ppu_state *ppu, struct save_state *state);
uint8_t ppu_peek(struct ppu_state *ppu, int addr);
void ppu_poke(struct ppu_state *ppu, int address, uint8_t value);
void ppu_set_overclock_mode(struct ppu_state *ppu, int mode, int scanlines);
void ppu_end_overclock(struct ppu_state *ppu, int cycles);

#endif				/* __PPU_H__ */
