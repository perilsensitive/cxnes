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

#ifndef __M2_TIMER_H__
#define __M2_TIMER_H__

#include <stdint.h>

#include "emu.h"
#include "cpu.h"

struct m2_timer;

#define M2_TIMER_FLAG_AUTO_IRQ_DISABLE 0x01
#define M2_TIMER_FLAG_ONE_SHOT 0x02
#define M2_TIMER_FLAG_COUNT_UP 0x04
#define M2_TIMER_FLAG_RELOAD   0x08
#define M2_TIMER_FLAG_IRQ_ON_RELOAD 0x10
#define M2_TIMER_FLAG_IRQ_ON_WRAP 0x10
#define M2_TIMER_FLAG_DELAYED_RELOAD 0x20
#define M2_TIMER_FLAG_PRESCALER 0x40
#define M2_TIMER_FLAG_PRESCALER_RELOAD 0x80

void m2_timer_run(struct m2_timer *timer, uint32_t cycles);

int m2_timer_init(struct emu *emu);
void m2_timer_cleanup(struct emu *emu);
void m2_timer_reset(struct m2_timer *timer, int hard);
void m2_timer_end_frame(struct m2_timer *timer, uint32_t cycles);
void m2_timer_set_enabled(struct m2_timer *timer, int enabled, uint32_t cycles);
void m2_timer_set_irq_enabled(struct m2_timer *timer, int enabled, uint32_t cycles);
int m2_timer_get_irq_enabled(struct m2_timer *timer);
void m2_timer_set_counter_enabled(struct m2_timer *timer, int enabled, uint32_t cycles);
int m2_timer_get_counter_enabled(struct m2_timer *timer);
void m2_timer_set_flags(struct m2_timer *timer, int flags, uint32_t cycles);
int m2_timer_get_flags(struct m2_timer *timer);
int m2_timer_get_counter(struct m2_timer *timer, uint32_t cycles);
void m2_timer_set_counter(struct m2_timer *timer, int counter, uint32_t cycles);
int m2_timer_get_reload(struct m2_timer *timer);
void m2_timer_set_reload(struct m2_timer *timer, int reload, uint32_t cycles);
void m2_timer_set_reload_lo(struct m2_timer *timer, int reload_hi, uint32_t cycles);
void m2_timer_set_reload_hi(struct m2_timer *timer, int reload_hi, uint32_t cycles);
void m2_timer_set_counter_lo(struct m2_timer *timer, int counter_hi , uint32_t cycles);
void m2_timer_set_counter_hi(struct m2_timer *timer, int counter_hi, uint32_t cycles);
void m2_timer_set_size(struct m2_timer *timer, int size, uint32_t cycles);
void m2_timer_set_prescaler_size(struct m2_timer *timer, int size, uint32_t cycles);
int m2_timer_get_size(struct m2_timer *timer);
int m2_timer_get_prescaler_size(struct m2_timer *timer);
void m2_timer_ack(struct m2_timer *timer, uint32_t cycles);
void m2_timer_cancel(struct m2_timer *timer, uint32_t cycles);
void m2_timer_force_reload(struct m2_timer *timer, uint32_t cycles);
int m2_timer_get_irq_status(struct m2_timer *timer, uint32_t cycles);
int m2_timer_get_counter_status(struct m2_timer *timer, uint32_t cycles);
void m2_timer_set_prescaler(struct m2_timer *timer, int prescaler, uint32_t cycles);
void m2_timer_set_prescaler_reload(struct m2_timer *timer, int value, uint32_t cycles);
void m2_timer_set_prescaler_decrement(struct m2_timer *timer, int value, uint32_t cycles);
void m2_timer_schedule_irq(struct m2_timer *timer, uint32_t cycles);
void m2_timer_set_irq_delay(struct m2_timer *timer, int value, uint32_t cycles);
void m2_timer_set_force_reload_delay(struct m2_timer *timer, int value, uint32_t cycles);
int m2_timer_load_state(struct m2_timer *timer, struct save_state *state);
int m2_timer_save_state(struct m2_timer *timer, struct save_state *state);
#endif /* __M2_TIMER_H__ */

