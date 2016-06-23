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

#include "emu.h"
#include "m2_timer.h"

struct m2_timer
{
	uint32_t timestamp;
	int flags;
	int mask;
	int size;
	int counter;
	int reload;
	int reload_flag;
	int force_reload_delay;
	int prescaler;
	int prescaler_reload;
	int prescaler_mask;
	int prescaler_decrement;
	int prescaler_size;
	int irq_enabled;
	int counter_enabled;
	int irq;
	int wrapped;
	int delay;
	struct emu *emu;
};

static struct state_item m2_timer_state_items[] = {
	STATE_32BIT(m2_timer, timestamp),
	STATE_32BIT(m2_timer, flags),
	STATE_8BIT(m2_timer, reload_flag), /* BOOLEAN */
	STATE_8BIT(m2_timer, size),
	STATE_16BIT(m2_timer, counter),
	STATE_16BIT(m2_timer, reload),
	STATE_8BIT(m2_timer, force_reload_delay),
	STATE_16BIT(m2_timer, prescaler),
	STATE_16BIT(m2_timer, prescaler_reload),
	STATE_8BIT(m2_timer, prescaler_decrement),
	STATE_8BIT(m2_timer, prescaler_size),
	STATE_8BIT(m2_timer, irq_enabled), /* BOOLEAN */
	STATE_8BIT(m2_timer, counter_enabled), /* BOOLEAN */
	STATE_8BIT(m2_timer, irq), /* BOOLEAN */
	STATE_8BIT(m2_timer, wrapped), /* BOOLEAN */
	STATE_8BIT(m2_timer, delay),
};

int m2_timer_save_state(struct m2_timer *timer, struct save_state *state)
{
	size_t size;
	uint8_t *buf;
	int rc;

	size = pack_state(timer, m2_timer_state_items, NULL);

	buf = malloc(size);
	if (!buf)
		return -1;

	pack_state(timer, m2_timer_state_items, buf);

	rc = save_state_add_chunk(state, "M2 ", buf, size);
	free(buf);

	return rc;
}

int m2_timer_load_state(struct m2_timer *timer, struct save_state *state)
{
	size_t size;
	uint8_t *buf;

	if (save_state_find_chunk(state, "M2 ", &buf, &size) < 0)
		return -1;

	unpack_state(timer, m2_timer_state_items, buf);

	timer->prescaler_mask = (1 << timer->prescaler_size) - 1;
	timer->mask = (1 << timer->size) - 1;

	return 0;
}

void m2_timer_schedule_irq(struct m2_timer *timer, uint32_t cycles)
{
	int remaining;
	int counter;
	int limit;

	cpu_interrupt_ack(timer->emu->cpu, IRQ_M2_TIMER);
	cpu_interrupt_cancel(timer->emu->cpu, IRQ_M2_TIMER);

	if (!timer->counter_enabled || !timer->irq_enabled)
		return;

	if (timer->flags & M2_TIMER_FLAG_COUNT_UP)
		limit = timer->mask;
	else
		limit = 0;

	if (timer->reload_flag) {
		if (timer->flags & M2_TIMER_FLAG_RELOAD) {
			counter = (timer->reload & timer->mask);
			if (timer->flags & M2_TIMER_FLAG_COUNT_UP)
				counter -= timer->force_reload_delay;
			else
				counter += timer->force_reload_delay;
		} else if (timer->flags & M2_TIMER_FLAG_COUNT_UP) {
			counter = 0;
		} else {
			counter = timer->mask;
		}

		counter++;
	} else {
		counter = timer->counter & timer->mask;
	}

	if (counter == limit) {
		if (timer->flags & M2_TIMER_FLAG_RELOAD)
			remaining = timer->reload + 1;
		else if (timer->flags & M2_TIMER_FLAG_ONE_SHOT)
			return;
		else
			remaining = timer->mask + 1;

		if (timer->flags & M2_TIMER_FLAG_IRQ_ON_RELOAD)
			remaining = 0;
	} else if (timer->flags & M2_TIMER_FLAG_COUNT_UP) {
		remaining = timer->mask - counter;
	} else {
		remaining = counter;
	}

	if (timer->flags & M2_TIMER_FLAG_IRQ_ON_RELOAD)
		remaining++;

	if (timer->flags & M2_TIMER_FLAG_PRESCALER) {
		int cpu_clocks = 0;
		int clocks;
		int prescaler;
		int prescaler_reload;

		prescaler = timer->prescaler & timer->prescaler_mask;

		if (timer->flags & M2_TIMER_FLAG_PRESCALER_RELOAD) {
			prescaler_reload = timer->prescaler_reload;
		} else {
			prescaler_reload = timer->prescaler_mask;
		}

		if (prescaler < prescaler_reload) {
			clocks = (prescaler / timer->prescaler_decrement) + 1;

			cpu_clocks += clocks;
			remaining--;
			prescaler = prescaler - (clocks * timer->prescaler_decrement) +
				(prescaler_reload + 1);
		}

		cpu_clocks += (remaining / timer->prescaler_decrement) *
			(prescaler_reload + 1);
		remaining %= timer->prescaler_decrement;

		while (remaining > 0) {
			clocks = (prescaler / timer->prescaler_decrement) + 1;

			cpu_clocks += clocks;
			prescaler = prescaler - (clocks * timer->prescaler_decrement) +
				(prescaler_reload + 1);

			remaining--;
		}

		remaining = cpu_clocks;
	}

	remaining += timer->delay;

	cycles += remaining * timer->emu->cpu_clock_divider;

	cpu_interrupt_schedule(timer->emu->cpu, IRQ_M2_TIMER, cycles);
}

void m2_timer_run(struct m2_timer *timer, uint32_t cycles)
{
	int divider;
	int reload;
	int limit;
	int cycle_length;
	int remaining;
	int counter;
	int elapsed;

	if (!timer)
		return;

	if (timer->emu->overclocking)
		return;

	if (!timer->counter_enabled)
		goto done;

	divider = timer->emu->cpu_clock_divider;
	elapsed = (cycles - timer->timestamp) / divider;
	if (!elapsed)
		return;

	if (timer->flags & M2_TIMER_FLAG_RELOAD)
		reload = timer->reload;
	else if (timer->flags & M2_TIMER_FLAG_COUNT_UP)
		reload = 0;
	else
		reload = timer->mask;

	if (timer->reload_flag) {
		counter = (reload & timer->mask);
		if (timer->flags & M2_TIMER_FLAG_COUNT_UP)
			counter -= timer->force_reload_delay;
		else
			counter += timer->force_reload_delay;
		timer->reload_flag = 0;
	} else {
		counter = timer->counter & timer->mask;
	}

	if (timer->flags & M2_TIMER_FLAG_COUNT_UP) {
		limit = timer->mask;
		cycle_length = limit - reload + 1;
		remaining = limit - counter;
	} else {
		limit = 0;
		cycle_length = reload + 1;
		remaining = counter;
	}

	/* if ((timer->flags & M2_TIMER_FLAG_ONE_SHOT) && */
	/*     (timer->counter == limit)) { */
	/* 	return; */
	/* } */

	if (timer->flags & M2_TIMER_FLAG_PRESCALER) {
		int counter_clocks = 0;
		int prescaler_reload;
		int prescaler;

		prescaler = timer->prescaler & timer->prescaler_mask;

		if (timer->flags & M2_TIMER_FLAG_PRESCALER_RELOAD) {
			prescaler_reload = timer->prescaler_reload;
		} else {
			prescaler_reload = timer->prescaler_mask;
		}

		if (prescaler < prescaler_reload) {
			int clocks = (prescaler /
				      timer->prescaler_decrement) + 1;

			if (clocks > elapsed) {
				prescaler -= elapsed *
					timer->prescaler_decrement;
				timer->prescaler = prescaler &
					timer->prescaler_mask;
				goto done;
			}

			elapsed -= clocks;
			prescaler = prescaler -
				(clocks * timer->prescaler_decrement) +
				(prescaler_reload + 1);
			counter_clocks++;
		}

		counter_clocks += (elapsed / (prescaler_reload + 1)) *
			timer->prescaler_decrement;
		elapsed %= prescaler_reload + 1;

		while (elapsed) {
			int clocks = (prescaler /
				      timer->prescaler_decrement) + 1;

			if (clocks > elapsed)
				break;

			elapsed -= clocks;
			counter_clocks++;
			prescaler = prescaler -
				(clocks * timer->prescaler_decrement) +
				(prescaler_reload + 1);
		}
		
		prescaler -= elapsed * timer->prescaler_decrement;
		timer->prescaler = prescaler & timer->prescaler_mask;
		elapsed = counter_clocks;
	}

	if (elapsed > remaining) {
		int irq_on_reload = timer->flags & M2_TIMER_FLAG_IRQ_ON_RELOAD;

		if (!(timer->flags & M2_TIMER_FLAG_ONE_SHOT)) {
			timer->wrapped = 1;
		}

		elapsed -= remaining + 1;
		if (timer->flags & M2_TIMER_FLAG_ONE_SHOT)
			elapsed = 0;

		if (remaining && (!irq_on_reload || (irq_on_reload && elapsed))) {
			if (timer->irq_enabled)
				timer->irq = 1;
		}

		/* if (remaining) */
		/* 	timer->ended = 1; */

		if (timer->flags & M2_TIMER_FLAG_AUTO_IRQ_DISABLE) {
			timer->irq_enabled = 0;
		}

		elapsed %= cycle_length;
		counter = reload;
	}

	if (timer->flags & M2_TIMER_FLAG_COUNT_UP)
		counter += elapsed;
	else
		counter -= elapsed;

	/* FIXME one shot? */

	timer->counter &= ~timer->mask;
	timer->counter |= counter;

done:
	timer->timestamp = cycles;
}

int m2_timer_init(struct emu *emu)
{
	struct m2_timer *timer;

	timer = malloc(sizeof(*timer));
	if (!timer)
		return 1;

	memset(timer, 0, sizeof(*timer));
	emu->m2_timer = timer;
	timer->emu = emu;

	return 0;
}

void m2_timer_cleanup(struct emu *emu)
{
	if (emu->m2_timer)
		free(emu->m2_timer);

	emu->m2_timer = NULL;
}

void m2_timer_reset(struct m2_timer *timer, int hard)
{
	struct emu *emu;

	if (!hard)
		return;

	emu = timer->emu;
	memset(timer, 0, sizeof(*timer));
	timer->emu = emu;

	timer->prescaler_decrement = 1;
	timer->irq_enabled = 0;
	timer->counter_enabled = 1;
	timer->size = 16;
	timer->mask = 0xffff;
	timer->prescaler_mask = 0xffff;
	timer->prescaler_size = 16;
	timer->reload_flag = 0;
	timer->force_reload_delay = 0;
}

void m2_timer_end_frame(struct m2_timer *timer, uint32_t cycles)
{
	timer->timestamp -= cycles;
}

int m2_timer_get_irq_enabled(struct m2_timer *timer)
{
	return timer->irq_enabled;
}

void m2_timer_force_reload(struct m2_timer *timer, uint32_t cycles)
{
	if (timer->flags & M2_TIMER_FLAG_DELAYED_RELOAD) {
		timer->reload_flag = 1;
	} else {
		m2_timer_set_counter(timer, timer->reload +
				     timer->force_reload_delay, cycles);
	}
}

void m2_timer_set_enabled(struct m2_timer *timer, int enabled, uint32_t cycles)
{
	m2_timer_set_counter_enabled(timer, enabled, cycles);
	m2_timer_set_irq_enabled(timer, enabled, cycles);
}

void m2_timer_set_irq_enabled(struct m2_timer *timer, int enabled, uint32_t cycles)
{
	if ((enabled && timer->irq_enabled) ||
	    (!enabled && !timer->irq_enabled)) {
		return;
	}

	m2_timer_run(timer, cycles);
	timer->irq_enabled = enabled;

	if (!timer->counter_enabled)
		return;

	m2_timer_schedule_irq(timer, cycles);
}

int m2_timer_get_counter_enabled(struct m2_timer *timer)
{
	return timer->counter_enabled;
}

void m2_timer_set_counter_enabled(struct m2_timer *timer, int enabled, uint32_t cycles)
{
	if ((enabled && timer->counter_enabled) ||
	    (!enabled && !timer->counter_enabled)) {
		return;
	}

	m2_timer_run(timer, cycles);
	timer->counter_enabled = enabled;

	if (!timer->irq_enabled)
		return;

	m2_timer_schedule_irq(timer, cycles);
}

void m2_timer_set_flags(struct m2_timer *timer, int flags, uint32_t cycles)
{
	m2_timer_run(timer, cycles);
	timer->flags = flags;

	m2_timer_schedule_irq(timer, cycles);
}

int m2_timer_get_flags(struct m2_timer *timer)
{
	return timer->flags;
}

int m2_timer_get_counter(struct m2_timer *timer, uint32_t cycles)
{
	m2_timer_run(timer, cycles);
	return timer->counter;
}

void m2_timer_set_counter(struct m2_timer *timer, int counter, uint32_t cycles)
{
	m2_timer_run(timer, cycles);
	timer->counter = counter;
	m2_timer_schedule_irq(timer, cycles);
}

int m2_timer_get_reload(struct m2_timer *timer)
{
	return timer->reload;
}

void m2_timer_set_reload(struct m2_timer *timer, int reload, uint32_t cycles)
{
	m2_timer_run(timer, cycles);
	timer->reload = reload;
}

void m2_timer_set_reload_lo(struct m2_timer *timer, int reload_lo, uint32_t cycles)
{
	m2_timer_run(timer, cycles);
	timer->reload &= 0xff00;
	timer->reload |= reload_lo & 0x00ff;
}

void m2_timer_set_reload_hi(struct m2_timer *timer, int reload_hi, uint32_t cycles)
{
	m2_timer_run(timer, cycles);
	timer->reload &= 0x00ff;
	timer->reload |= (reload_hi & 0x00ff) << 8;
}

void m2_timer_set_counter_lo(struct m2_timer *timer, int counter_lo, uint32_t cycles)
{
	int counter;

	m2_timer_run(timer, cycles);

	counter = (timer->counter & 0xff00) | (counter_lo & 0x00ff);
	m2_timer_set_counter(timer, counter, cycles);
}

void m2_timer_set_counter_hi(struct m2_timer *timer, int counter_hi, uint32_t cycles)
{
	int counter;

	m2_timer_run(timer, cycles);

	counter = (timer->counter & 0x00ff) | ((counter_hi & 0x00ff) << 8);
	m2_timer_set_counter(timer, counter, cycles);
}

void m2_timer_set_size(struct m2_timer *timer, int size, uint32_t cycles)
{
	if (size == timer->size)
		return;

	m2_timer_run(timer, cycles);

	if ((size > 0) && (size <= 32))
		timer->size = size;

	timer->mask = ~(~0 << size);

	m2_timer_schedule_irq(timer, cycles);
}

int m2_timer_get_size(struct m2_timer *timer)
{
	return timer->size;
}

void m2_timer_set_prescaler_size(struct m2_timer *timer, int size, uint32_t cycles)
{
	if (size == timer->prescaler_size)
		return;

	m2_timer_run(timer, cycles);

	if ((size > 0) && (size <= 32))
		timer->prescaler_size = size;

	timer->prescaler_mask = ~(~0 << size);

	m2_timer_schedule_irq(timer, cycles);
}

int m2_timer_get_prescaler_size(struct m2_timer *timer)
{
	return timer->prescaler_size;
}

void m2_timer_ack(struct m2_timer *timer, uint32_t cycles)
{
	m2_timer_run(timer, cycles);
	cpu_interrupt_ack(timer->emu->cpu, IRQ_M2_TIMER);
}

void m2_timer_cancel(struct m2_timer *timer, uint32_t cycles)
{
	m2_timer_run(timer, cycles);
	cpu_interrupt_cancel(timer->emu->cpu, IRQ_M2_TIMER);
}

int m2_timer_get_irq_status(struct m2_timer *timer, uint32_t cycles)
{
	int status;

	m2_timer_run(timer, cycles);
	status = timer->irq;
	timer->irq = 0;

	return status;
}

int m2_timer_get_counter_status(struct m2_timer *timer, uint32_t cycles)
{
	int status;

	m2_timer_run(timer, cycles);
	status = timer->wrapped;
	timer->wrapped = 0;

	return status;
}

void m2_timer_set_prescaler(struct m2_timer *timer, int prescaler, uint32_t cycles)
{
	m2_timer_run(timer, cycles);
	timer->prescaler = prescaler;
	m2_timer_schedule_irq(timer, cycles);
}

void m2_timer_set_prescaler_reload(struct m2_timer *timer, int value, uint32_t cycles)
{
	m2_timer_run(timer, cycles);
	timer->prescaler_reload = value;
	m2_timer_schedule_irq(timer, cycles);
}

void m2_timer_set_prescaler_decrement(struct m2_timer *timer, int value, uint32_t cycles)
{
	m2_timer_run(timer, cycles);
	timer->prescaler_decrement = value;
	m2_timer_schedule_irq(timer, cycles);
}

void m2_timer_set_irq_delay(struct m2_timer *timer, int value, uint32_t cycles)
{
	m2_timer_run(timer, cycles);
	timer->delay = value;
	m2_timer_schedule_irq(timer, cycles);
	
}

void m2_timer_set_force_reload_delay(struct m2_timer *timer, int value, uint32_t cycles)
{
	m2_timer_run(timer, cycles);
	timer->force_reload_delay = value;
}
