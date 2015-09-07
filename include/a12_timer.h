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

#ifndef __A12_TIMER_H__
#define __A12_TIMER_H__

#include <stdint.h>

#include "emu.h"
#include "cpu.h"

#define A12_TIMER_VARIANT_MMC3_STD         0
#define A12_TIMER_VARIANT_MMC3_ALT         1
#define A12_TIMER_VARIANT_RAMBO1           2
#define A12_TIMER_VARIANT_TAITO_TC0190FMC  3
#define A12_TIMER_VARIANT_ACCLAIM_MC_ACC   4

#define A12_TIMER_FLAG_WRAP 0x01
#define A12_TIMER_FLAG_IRQ_ON_WRAP 0x02
#define A12_TIMER_FLAG_COUNT_UP 0x04
#define A12_TIMER_FLAG_DELAYED_RELOAD 0x08

void a12_timer_set_counter_enabled(struct a12_timer *timer, int enabled, uint32_t cycles);
void a12_timer_set_irq_enabled(struct a12_timer *timer, int enabled, uint32_t cycles);
int a12_timer_init(struct emu *emu, int variant);
void a12_timer_reset(struct a12_timer *timer, int);
void a12_timer_end_frame(struct a12_timer *timer, uint32_t cycles);
void a12_timer_cleanup(struct emu *emu);
int a12_timer_save_state(struct emu *emu, struct save_state *state);
int a12_timer_load_state(struct emu *emu, struct save_state *state);
void a12_timer_reset_hook(struct emu *emu, uint32_t cycles);
void a12_timer_hook(struct emu *emu, int address, int scanline,
			   int scanline_cycle, int rendering);
void a12_timer_run(struct a12_timer *timer, uint32_t cycles);
void a12_timer_set_delta(struct a12_timer *timer, int delta, uint32_t cycles);
void a12_timer_set_flags(struct a12_timer *timer, int flags, uint32_t cycles);
int a12_timer_get_flags(struct a12_timer *timer);
int a12_timer_get_counter(struct a12_timer *timer, uint32_t cycles);
void a12_timer_set_counter(struct a12_timer *timer, int value, uint32_t cycles);
void a12_timer_set_prescaler_size(struct a12_timer *timer, int size, uint32_t cycles);
int a12_timer_get_prescaler_size(struct a12_timer *timer);
void a12_timer_set_prescaler(struct a12_timer *timer, int prescaler, uint32_t cycles);
int a12_timer_get_prescaler(struct a12_timer *timer);
void a12_timer_set_force_reload_delay(struct a12_timer *timer, int value, uint32_t cycles);
void a12_timer_set_reload(struct a12_timer *timer, int reload, uint32_t cycles);
void a12_timer_force_reload(struct a12_timer *timer, uint32_t cycles);

CPU_WRITE_HANDLER(a12_timer_irq_latch);
CPU_WRITE_HANDLER(a12_timer_irq_reload);
CPU_WRITE_HANDLER(a12_timer_irq_enable);
CPU_WRITE_HANDLER(a12_timer_irq_disable);

#endif				/* __A12_TIMER_H__ */
