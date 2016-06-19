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

#include <string.h>

#include "audio.h"
#include "emu.h"

#define PULSE0_IDX   0
#define PULSE1_IDX   1
#define TRIANGLE_IDX 2
#define NOISE_IDX    3

#define FRAME_INTERRUPT_DISABLED 0x40

#define sched_next_frame_step(y) (apu->next_frame_step += \
                                  apu->emu->cpu_clock_divider * (y))

#define CPU_CYCLES(x) ((x) / apu->emu->cpu_clock_divider)

struct length_counter {
	int counter;
	int halt;
};

struct linear_counter {
	int counter;
	int reload;
	int halt_flag;
	int control_flag;
};

struct sweep {
	int period;
	int delay;
	int shift;
	int negate_flag;
	int negative_adjust;
	int enabled_flag;
	int reload_flag;
	int *parent_period;	/* Points to parent channel's period */
};

struct envelope {
	int period;
	int delay;
	int counter;
	int constant_flag;
	int constant_volume;
	int loop_flag;
	int start_flag;
};

struct pulse {
	struct length_counter length;
	struct envelope envelope;
	struct sweep sweep;
	int period;
	int duty_cycle;
	int phase;
	int amplitude;
	int enabled;
	uint32_t next_clock;
};

struct triangle {
	struct length_counter length;
	struct linear_counter linear;
	int period;
	int phase;
	int amplitude;
	int enabled;
	uint32_t next_clock;
};

struct noise {
	struct length_counter length;
	struct envelope envelope;
	int period;
	int shift;
	int mode;
	int amplitude;
	int delay;
	int enabled;
	uint32_t next_clock;
};

struct dmc {
	int empty;
	int enabled;
	int silent;
	int period;
	int amplitude;
	uint8_t dma_buf;
	int dac;
	uint8_t shift;
	int shift_bits;
	uint32_t next_clock;
	int addr;
	int addr_current;
	int length;
	int bytes_remaining;
	uint32_t dma_timestamp;
	int loop;
	int irq;
};

struct apu_state {
	int frame_counter_mode;
	int frame_counter_step;
	int frame_irq_flag;

	struct pulse pulse[2];
	struct triangle triangle;
	struct noise noise;
	struct dmc dmc;

	uint32_t next_frame_step;
	uint32_t last_time;
	uint32_t frame_step_delay;
	int next_frame_irq;
	int frame_irq_delay;
	uint32_t dmc_irq_flag;
	int last_amplitude;

	int swap_duty_cycles;
	int raw_pcm_filter;

	/* FIXME is there any reason that these can't
	   be ints vs uint16_ts?
	 */
	const uint16_t *noise_period_table;
	const uint16_t *dmc_rate_table;

	int odd_cycle;
	uint8_t frame_counter_register;
	uint32_t frame_counter_register_timestamp;
	int apu_clock_divider;

	struct emu *emu;
};

static struct state_item apu_state_items[] = {
	STATE_8BIT(apu_state, frame_counter_mode),
	STATE_8BIT(apu_state, frame_counter_step),
	STATE_8BIT(apu_state, frame_irq_flag), /* BOOLEAN */
	STATE_32BIT(apu_state, next_frame_step),
	STATE_32BIT(apu_state, last_time),
	STATE_32BIT(apu_state, frame_step_delay),
	STATE_32BIT(apu_state, next_frame_irq), /* FIXME check type */
	STATE_32BIT(apu_state, frame_irq_delay), /* FIXME check type */
	STATE_8BIT(apu_state, dmc_irq_flag), /* BOOLEAN */
	STATE_8BIT(apu_state, odd_cycle), /* BOOLEAN */
	STATE_8BIT(apu_state, frame_counter_register),
	STATE_32BIT(apu_state, frame_counter_register_timestamp),
	STATE_ITEM_END(),
};

static struct state_item length_counter_state_items[] = {
	STATE_32BIT(length_counter, counter),
	STATE_8BIT(length_counter, halt),
	STATE_ITEM_END(),
};

static struct state_item linear_counter_state_items[] = {
	STATE_32BIT(linear_counter, counter),
	STATE_8BIT(linear_counter, reload), /* BOOLEAN */
	STATE_8BIT(linear_counter, halt_flag), /* BOOLEAN */
	STATE_8BIT(linear_counter, control_flag), /* BOOLEAN */
	STATE_ITEM_END(),
};

static struct state_item sweep_state_items[] = {
	STATE_8BIT(sweep, period),
	STATE_8BIT(sweep, delay),
	STATE_8BIT(sweep, reload_flag), /* BOOLEAN */
	STATE_8BIT(sweep, enabled_flag), /* BOOLEAN */
	STATE_8BIT(sweep, negate_flag), /* BOOLEAN */
	STATE_8BIT(sweep, shift),
	STATE_ITEM_END(),
};

static struct state_item envelope_state_items[] = {
	STATE_8BIT(envelope, period),
	STATE_8BIT(envelope, delay),
	STATE_8BIT(envelope, counter),
	STATE_8BIT(envelope, constant_volume),
	STATE_8BIT(envelope, constant_flag), /* BOOLEAN */
	STATE_8BIT(envelope, loop_flag), /* BOOLEAN */
	STATE_8BIT(envelope, start_flag), /* BOOLEAN */
	STATE_8BIT(envelope, constant_flag), /* BOOLEAN */
	STATE_ITEM_END(),
};

static struct state_item pulse_state_items[] = {
	STATE_16BIT(pulse, period),
	STATE_8BIT(pulse, duty_cycle),
	STATE_8BIT(pulse, phase),
	STATE_8BIT(pulse, enabled), /* BOOLEAN */
	STATE_32BIT(pulse, next_clock),
	STATE_ITEM_END(),
};

static struct state_item triangle_state_items[] = {
	STATE_16BIT(triangle, period),
	STATE_8BIT(triangle, phase),
	STATE_8BIT(triangle, enabled), /* BOOLEAN */
	STATE_32BIT(triangle, next_clock),
	STATE_ITEM_END(),
};

static struct state_item noise_state_items[] = {
	STATE_16BIT(noise, period),
	STATE_16BIT(noise, shift), /* FIXME check type */
	STATE_8BIT(noise, mode), /* BOOLEAN */
	STATE_8BIT(noise, enabled), /* BOOLEAN */
	STATE_32BIT(noise, next_clock),
	STATE_ITEM_END(),
};

static struct state_item dmc_state_items[] = {
	STATE_8BIT(dmc, empty), /* BOOLEAN */
	STATE_8BIT(dmc, enabled), /* BOOLEAN */
	STATE_8BIT(dmc, silent), /* BOOLEAN */
	STATE_8BIT(dmc, loop), /* BOOLEAN */
	STATE_8BIT(dmc, irq), /* BOOLEAN */

	STATE_16BIT(dmc, period), /* FIXME check type */
	STATE_8BIT(dmc, dma_buf),
	STATE_8BIT(dmc, dac),
	STATE_8BIT(dmc, shift),
	STATE_8BIT(dmc, shift_bits),
	STATE_16BIT(dmc, length),
	STATE_16BIT(dmc, bytes_remaining),
	STATE_16BIT(dmc, addr),
	STATE_16BIT(dmc, addr_current),
	STATE_32BIT(dmc, dma_timestamp),
	STATE_32BIT(dmc, next_clock),
	STATE_ITEM_END(),
};

static void apu_update_amplitude(struct apu_state *apu, uint32_t cycles);
static uint32_t apu_dmc_set_period(struct apu_state *apu, int period, uint32_t cycles);
static uint32_t apu_dmc_calc_dma_time(struct apu_state *apu, uint32_t cycles);

static void clock_length_counters(struct apu_state *apu);
static void clock_envelopes(struct apu_state *apu);
static void clock_sweeps(struct apu_state *apu);
static void clock_linear_counter(struct linear_counter *l);
CPU_WRITE_HANDLER(oam_dma);
static CPU_WRITE_HANDLER(pulse_write_handler);
static CPU_WRITE_HANDLER(triangle_write_handler);
static CPU_WRITE_HANDLER(noise_write_handler);
static CPU_WRITE_HANDLER(status_write_handler);
static CPU_WRITE_HANDLER(frame_counter_write_handler);
static CPU_READ_HANDLER(status_read_handler);
void apu_run(struct apu_state *apu, uint32_t cycles);
static void clock_frame_counter(struct apu_state *apu);
//static void apu_clock_mode1(void);

void apu_end_frame(struct apu_state *apu, uint32_t cycles);

#define channel_volume(x) ((x)->length.counter == 0 ? 0 :		\
		((x)->envelope.constant_flag ? (x)->envelope.constant_volume : \
		 (x)->envelope.counter))

static const uint8_t length_table[0x20] = {
	0x0A, 0xFE, 0x14, 0x02, 0x28, 0x04, 0x50, 0x06,
	0xA0, 0x08, 0x3C, 0x0A, 0x0E, 0x0C, 0x1A, 0x0E,
	0x0C, 0x10, 0x18, 0x12, 0x30, 0x14, 0x60, 0x16,
	0xC0, 0x18, 0x48, 0x1A, 0x10, 0x1C, 0x20, 0x1E
};

static const uint16_t ntsc_dmc_rate_table[16] = {
	428, 380, 340, 320, 286, 254, 226, 214,
	190, 160, 142, 128, 106, 84, 72, 54
};

static const uint16_t pal_dmc_rate_table[16] = {
	398, 354, 316, 298, 276, 236, 210, 198,
	176, 148, 132, 118, 98, 78, 66, 50
};

static const uint16_t ntsc_noise_period_table[16] = {
	4, 8, 16, 32, 64, 96, 128, 160,
	202, 254, 380, 508, 762, 1016, 2034, 4068
};

static const uint16_t pal_noise_period_table[16] = {
	4, 7, 14, 30, 60, 88, 118, 148,
	188, 236, 354, 472, 708, 944, 1890, 3778
};

static void clock_length(struct length_counter *l)
{
	if (l->counter && !l->halt)
		l->counter--;
}

static void clock_envelope(struct envelope *env)
{
	if (env->start_flag) {
		env->start_flag = 0;
		env->delay = env->period;
		env->counter = 15;
	} else if (--env->delay == 0) {
		env->delay = env->period;
		if (env->counter)
			env->counter--;
		else if (env->loop_flag)
			env->counter = 15;
	}
}

static void clock_sweep(struct sweep *sweep)
{
	if (--sweep->delay == 0) {
		sweep->reload_flag = 1;

		int period = *sweep->parent_period;
		if (sweep->shift && sweep->enabled_flag && period >= 8) {
			int offset = period >> sweep->shift;

			if (sweep->negate_flag)
				offset = sweep->negative_adjust - offset;

			if (period + offset < 0x800) {
				period += offset;
				*sweep->parent_period = period;
			}
		}
	}

	if (sweep->reload_flag) {
		sweep->reload_flag = 0;
		sweep->delay = sweep->period;
	}
}

static void clock_linear_counter(struct linear_counter *l)
{
	if (l->halt_flag)
		l->counter = l->reload;
	else if (l->counter)
		l->counter--;

	if (!l->control_flag)
		l->halt_flag = 0;
}

static void clock_length_counters(struct apu_state *apu)
{
	clock_length(&apu->pulse[0].length);
	clock_length(&apu->pulse[1].length);
	clock_length(&apu->triangle.length);
	clock_length(&apu->noise.length);
}

static void clock_envelopes(struct apu_state *apu)
{
	clock_envelope(&apu->pulse[0].envelope);
	clock_envelope(&apu->pulse[1].envelope);
	clock_envelope(&apu->noise.envelope);
}

static void clock_sweeps(struct apu_state *apu)
{
	clock_sweep(&apu->pulse[0].sweep);
	clock_sweep(&apu->pulse[1].sweep);
}

static void pulse_update_volume(struct apu_state *apu, int c, uint32_t cycles)
{
	struct pulse *pulse;
	int volume;
	int new_amplitude;
	int duty;
	int offset;

	pulse = &apu->pulse[c];
	volume = channel_volume(pulse);
	offset = pulse->period >> pulse->sweep.shift;
	if (pulse->sweep.negate_flag)
		offset = 0;

	new_amplitude = 0;

	duty = 1 << pulse->duty_cycle;
	if (pulse->duty_cycle == 3) {
		duty = 2;
		new_amplitude = volume;
	}

	if (pulse->phase < duty)
		new_amplitude ^= volume;

	/* See if the channel has been muted */
	if (!volume || pulse->period < 8 || (pulse->period + offset) >= 0x800) {
		new_amplitude = 0;
	}

	if (new_amplitude != pulse->amplitude) {
		pulse->amplitude = new_amplitude;
		apu_update_amplitude(apu, cycles);
	}
}

static inline void pulse_run(struct apu_state *apu, int c,
			     uint32_t cycles) __attribute((always_inline));
static inline void pulse_run(struct apu_state *apu, int c, uint32_t cycles)
{
	struct pulse *pulse;
	int timer_period;
	int volume;
	int offset = 0;
	int delta;
	int duty;
	int muted = 0;
	uint32_t limit;


	pulse = &apu->pulse[c];
	limit = apu->next_frame_step;

	if (limit > cycles)
		limit = cycles;

	timer_period = (pulse->period << 1) + 2;
	volume = channel_volume(pulse);
	offset = pulse->period >> pulse->sweep.shift;
	if (pulse->sweep.negate_flag)
		offset = 0;

	if (!volume || pulse->period < 8 || (pulse->period + offset) >= 0x800) {
		muted = 1;
	}

	duty = 1 << pulse->duty_cycle;
	if (pulse->duty_cycle == 3)
		duty = 2;

	delta = pulse->amplitude ? volume : -volume;

	if (muted) {
		int cycles_to_run;
		int period;
		int count;

		period = timer_period * apu->apu_clock_divider;
		cycles_to_run = limit - pulse->next_clock;

		count = cycles_to_run / period;
		if (cycles_to_run % period)
			count++;

		pulse->phase = (pulse->phase + count) % 8;
		pulse->next_clock += count * period;

		return;
	}

	/* We need to clock the channel only once if it isn't silent.  But if
	   it is, we can clock it until it is no longer silent (probably the
	   next time the frame counter is clocked. */
	pulse->phase = (pulse->phase + 1) % 8;
	if (!muted && (pulse->phase == 0 || pulse->phase == duty)) {
		delta = -delta;
		pulse->amplitude += delta;
	}
	pulse->next_clock += timer_period * apu->apu_clock_divider;
}

static inline void triangle_run(struct apu_state *apu,
				uint32_t cycles) __attribute((always_inline));
static inline void triangle_run(struct apu_state *apu, uint32_t cycles)
{
	struct triangle *triangle;
	int timer_period;
	uint32_t limit;
	int muted;

	triangle = &apu->triangle;

	timer_period = triangle->period + 1;

	limit = apu->next_frame_step;
	if (limit > cycles)
		limit = cycles;

	muted = 0;
	if (!triangle->length.counter || !triangle->linear.counter ||
	    timer_period < 3) {
		muted = 1;
	}

	if (muted) {
		int cycles_to_run;
		int period;
		int count;

		period = timer_period * apu->apu_clock_divider;
		cycles_to_run = limit - triangle->next_clock;

		count = cycles_to_run / period;
		if (cycles_to_run % period)
			count++;

		triangle->next_clock += count * period;

		return;
	}

	triangle->phase = (triangle->phase + 1) % 32;
	triangle->amplitude = 15 - triangle->phase;

	if (triangle->amplitude < 0)
		triangle->amplitude = triangle->phase - 16;

	triangle->next_clock +=
		timer_period * apu->apu_clock_divider;
}

static void noise_update_volume(struct apu_state *apu, uint32_t cycles)
{
	struct noise *noise;
	int volume;

	noise = &apu->noise;
	volume = channel_volume(noise) * ((noise->shift & 1) ^ 1);

	if (volume != noise->amplitude) {
		noise->amplitude = volume;
		apu_update_amplitude(apu, cycles);
	}
}

static inline void noise_run(struct apu_state *apu,
			     uint32_t cycles) __attribute((always_inline));
static inline void noise_run(struct apu_state *apu, uint32_t cycles)
{
	struct noise *noise;
	int volume;
	int bit_to_xor;
	int feedback;
	uint32_t limit;

	noise = &apu->noise;

	limit = apu->next_frame_step;
	if (limit > cycles)
		limit = cycles;

	bit_to_xor = noise->mode ? 6 : 1;
	volume = channel_volume(noise);

	do {
		feedback =
		    ((noise->shift) ^ (noise->shift >> bit_to_xor)) & 0x1;
		noise->shift = (feedback << 14) | (noise->shift >> 1);
		noise->amplitude = (((noise->shift & 1) ^ 1) * volume);
		noise->next_clock += noise->period * apu->apu_clock_divider;
	} while (!volume && noise->next_clock < limit);
}

static inline void dmc_run(struct apu_state *apu,
			   uint32_t cycles) __attribute((always_inline));
static inline void dmc_run(struct apu_state *apu, uint32_t cycles)
{
	struct dmc *dmc;

	dmc = &apu->dmc;

	if (!dmc->silent || !dmc->empty) {
		int delta;

		if (dmc->shift & 1)
			delta = 2;
		else
			delta = -2;

		if (!((dmc->dac + delta) & 0x80)) {
			dmc->dac += delta;
			dmc->amplitude = dmc->dac;
		}
	}

	dmc->shift >>= 1;

	dmc->shift_bits--;

	if (dmc->shift_bits == 0) {
		dmc->shift_bits = 8;
		if (dmc->empty) {
			dmc->silent = 1;
		} else {
			dmc->silent = 0;
			dmc->shift = dmc->dma_buf;
			dmc->empty = 1;
//                              printf("empty buffer at %d\n", dmc->next_clock);
		}
	}
	dmc->next_clock += dmc->period * apu->apu_clock_divider;
}

CPU_WRITE_HANDLER(oam_dma)
{
	struct apu_state *apu;

	apu = emu->apu;
	apu_run(apu, cycles);

	/* All of the code for accessing memory is in the CPU core,
	   so it needs to do most of the work.
	*/
	cpu_oam_dma(emu->cpu, value << 8, apu->odd_cycle);
}

static CPU_WRITE_HANDLER(pulse_write_handler)
{
	struct pulse *pulse;
	struct apu_state *apu;
	int c;

	apu = emu->apu;
	apu_run(apu, cycles);

	c = (addr >= 0x4004) ? 1 : 0;
	pulse = &apu->pulse[c];

//      printf("called from %x, wrote %x\n", cpu_get_pc(), value);

	switch (addr & 3) {
	case 0:
		pulse->duty_cycle = (value >> 6) & 0x03;
		if (apu->swap_duty_cycles) {
			if (pulse->duty_cycle == 1)
				pulse->duty_cycle = 2;
			else if (pulse->duty_cycle == 2)
				pulse->duty_cycle = 1;
		}
		pulse->envelope.loop_flag = (value & 0x20);
		pulse->envelope.constant_flag = (value & 0x10);
		pulse->envelope.period = (value & 0x0f) + 1;
		pulse->envelope.constant_volume = value & 0x0f;
		pulse_update_volume(apu, c, cycles);
		/* apu_run(apu, cycles + apu_clock_divider); */
		pulse->length.halt = (value & 0x20);
		break;
	case 1:
		pulse->sweep.enabled_flag = (value & 0x80);
		pulse->sweep.period = ((value >> 4) & 0x07) + 1;
		pulse->sweep.negate_flag = (value & 0x08);
		pulse->sweep.shift = (value & 0x07);
		pulse->sweep.reload_flag = 1;
		pulse_update_volume(apu, c, cycles);
		break;
	case 2:
		pulse->period = (pulse->period & 0x700) | value;
		pulse_update_volume(apu, c, cycles);
		break;
	case 3:
		pulse->period = (pulse->period & 0xff) | ((value & 0x07) << 8);
		pulse->envelope.start_flag = 1;
		pulse->phase = 7;	/* Will be on first phase when next clocked */
		pulse_update_volume(apu, c, cycles);
		if (pulse->enabled) {
			pulse->length.counter =
				length_table[(value >> 3) & 0x1f];
			pulse_update_volume(apu, c,
					    cycles +
					    apu->apu_clock_divider);
		}
		break;
	}
}

static CPU_WRITE_HANDLER(triangle_write_handler)
{
	struct triangle *triangle;
	struct apu_state *apu;
	int tmp;

	apu = emu->apu;

	apu_run(apu, cycles);

	triangle = &apu->triangle;

	switch (addr & 3) {
	case 0:
		triangle->linear.reload = (value & 0x7f);
		triangle->linear.control_flag = (value & 0x80);
		/* apu_run(apu, cycles + apu_clock_divider); */
		triangle->length.halt = (value & 0x80);
		break;
	case 1:
		break;
	case 2:
		triangle->period = (triangle->period & 0x700) | value;
		break;
	case 3:
		triangle->period = (triangle->period & 0xff) |
		    ((value & 0x7) << 8);
		if (triangle->enabled) {
			tmp = triangle->length.counter;
			/* apu_run(apu, cycles + apu_clock_divider); */
			if (tmp == triangle->length.counter)
				triangle->length.counter =
				    length_table[(value >> 3) & 0x1f];
		}
		triangle->linear.halt_flag = 1;
		break;
	}
}

static CPU_WRITE_HANDLER(noise_write_handler)
{
	struct noise *noise;
	struct apu_state *apu;
	int tmp;

	apu = emu->apu;

	apu_run(apu, cycles);

	noise = &apu->noise;

	switch (addr & 3) {
	case 0:
		noise->envelope.loop_flag = (value & 0x20);
		noise->envelope.constant_flag = (value & 0x10);
		noise->envelope.period = (value & 0x0f) + 1;
		noise->envelope.constant_volume = value & 0x0f;
		noise_update_volume(apu, cycles);
		/* apu_run(apu, cycles + apu_clock_divider); */
		noise->length.halt = (value & 0x20);
		break;
	case 1:
		break;
	case 2:
		noise->mode = (value & 0x80);
		noise->period = apu->noise_period_table[value & 0xf];
		break;
	case 3:
		if (noise->enabled) {
			tmp = noise->length.counter;
			if (tmp == noise->length.counter) {
				noise->length.counter =
				    length_table[(value >> 3) & 0x1f];
				noise_update_volume(apu,
						    cycles +
						    apu->apu_clock_divider);
			}
		}
		noise->envelope.start_flag = 1;
		break;
	}
}

static CPU_WRITE_HANDLER(dmc_write_handler)
{
	struct dmc *dmc;
	struct apu_state *apu;
	uint32_t new_dma;

	apu = emu->apu;

	apu_run(apu, cycles);

	dmc = &apu->dmc;

	switch (addr & 3) {
	case 0:
		/* NSF doesn't support DMC interrupts */
		if (board_get_type(emu->board) == BOARD_TYPE_NSF)
			value &= 0x7f;

		apu->dmc.loop = value & 0x40;
		apu->dmc.irq = apu->dmc.loop ? 0 : value & 0x80;

		if (!apu->dmc.irq)
			apu->dmc_irq_flag = 0;

		new_dma = apu_dmc_set_period(apu, value & 0x0f, cycles);
		if (apu->dmc.bytes_remaining) {
			cpu_set_dmc_dma_timestamp(emu->cpu, new_dma,
					  apu->dmc.addr_current, 0);
		}
		cpu_interrupt_ack(emu->cpu, IRQ_APU_DMC);
		cpu_interrupt_cancel(emu->cpu, IRQ_APU_DMC);
		break;
	case 1:
		/* FIXME Pop-reducing hack
		   Ignore the write if we're currently playing a sample.  This doesn't
		   eliminate all pops, but it reduces them without breaking raw PCM
		   playback.
		*/

		if (apu->raw_pcm_filter == 2 ||
		    (apu->raw_pcm_filter &&(!dmc->empty || !dmc->silent))) {
			return;
		}

		dmc->dac = value & 0x7f;
		if (dmc->dac != dmc->amplitude) {
			dmc->amplitude = dmc->dac;
			apu_update_amplitude(apu, cycles);
		}
		break;
	case 2:
		apu->dmc.addr = 0xc000 | (value << 6);
		break;
	case 3:
		apu->dmc.length = (value << 4) | 1;
		break;
	}
}

static CPU_WRITE_HANDLER(status_write_handler)
{
	struct apu_state *apu;
	uint32_t next_dma;

	apu = emu->apu;

	apu_run(apu, cycles);

	apu->pulse[0].enabled = value & 1;
	apu->pulse[1].enabled = value & 2;
	apu->triangle.enabled = value & 4;
	apu->noise.enabled = value & 8;
	apu->dmc.enabled = value & 16;

	if (!apu->pulse[0].enabled)
		apu->pulse[0].length.counter = 0;
	if (!apu->pulse[1].enabled)
		apu->pulse[1].length.counter = 0;
	if (!apu->triangle.enabled)
		apu->triangle.length.counter = 0;
	if (!apu->noise.enabled)
		apu->noise.length.counter = 0;

	if (apu->dmc_irq_flag) {
		apu->dmc_irq_flag = 0;
		cpu_interrupt_ack(apu->emu->cpu, IRQ_APU_DMC);
	}

	if (!apu->dmc.enabled) {
		apu->dmc_irq_flag = 0;
		next_dma = ~0;
		apu->dmc.bytes_remaining = 0;
		cpu_set_dmc_dma_timestamp(apu->emu->cpu, next_dma,
				  apu->dmc.addr, 0);
		return;
	}

	/* DMC enabled */
	if (apu->dmc.bytes_remaining == 0 && apu->dmc.length > 0) {
		apu->dmc.bytes_remaining = apu->dmc.length;
		apu->dmc.addr_current = apu->dmc.addr;
		if (apu->dmc.empty) {
			/* FIXME previously set wait_cycles to 2 */
			next_dma = cycles;
			apu->dmc.dma_timestamp = next_dma;
			cpu_set_dmc_dma_timestamp(emu->cpu, next_dma,
					  apu->dmc.addr_current, 1);
		} else {
			next_dma = apu_dmc_calc_dma_time(apu, cycles);

			/* Tell the cpu which byte to dma next and
			   when to do it.
			*/
			apu->dmc.dma_timestamp = next_dma;
			cpu_set_dmc_dma_timestamp(emu->cpu, next_dma,
					  apu->dmc.addr_current, 0);
		}
	}
}

static CPU_READ_HANDLER(status_read_handler)
{
	struct apu_state *apu;
	int result;

	apu = emu->apu;

	apu_run(apu, cycles);

	result = 0;
	if (apu->pulse[0].length.counter)
		result |= 1;
	if (apu->pulse[1].length.counter)
		result |= 2;
	if (apu->triangle.length.counter)
		result |= 4;
	if (apu->noise.length.counter)
		result |= 8;

	if (apu->frame_irq_flag) {
		result |= 0x40;
		apu->frame_irq_flag = 0;
		cpu_interrupt_ack(apu->emu->cpu, IRQ_APU_FRAME);
		if (!(apu->frame_counter_mode & 0xc0))
			cpu_interrupt_schedule(apu->emu->cpu, IRQ_APU_FRAME,
					       apu->next_frame_irq);
	}

	if (apu->dmc_irq_flag) {
		result |= 0x80;
	}

	if (apu->dmc.bytes_remaining) {
//              printf("apu status read, bytes left\n");
		result |= 0x10;
	}

	//printf("got result %x from 4015, %d %d\n", result, cycles, apu->odd_cycle);

	return result;
}

static CPU_WRITE_HANDLER(frame_counter_write_handler)
{
	struct apu_state *apu;

	apu = emu->apu;
	apu_run(apu, cycles);

	/* Calculate the timestamp when the write will take effect */
	if (apu->odd_cycle)
		cycles += apu->apu_clock_divider;
	cycles += apu->apu_clock_divider;

	/* NSF doesn't support frame interrupts */
	if (board_get_type(emu->board) == BOARD_TYPE_NSF)
		value |= 0x40;

	apu->frame_counter_register = value;
	apu->frame_counter_register_timestamp = cycles;

	/* If write completes before next frame step,
	   then we just go straight for the reset step.
	 */
	if (cycles < apu->next_frame_step) {
		apu->next_frame_step = cycles;
		apu->frame_counter_step = 255;
	}

	if (!(apu->frame_counter_mode & 0xc0) /*&&
	    (apu->next_frame_irq > cycles)*/) {
		cpu_interrupt_ack(apu->emu->cpu, IRQ_APU_FRAME);
		cpu_interrupt_cancel(apu->emu->cpu, IRQ_APU_FRAME);
		apu->next_frame_irq = ~0;
	}

	/* Deal with IRQ (if IRQ will occur after the write takes effect).
	   If the IRQ will occur before the write takes effect, there's nothing
	   to do. */
	if (!(value & 0xc0) /*&& (apu->next_frame_irq > cycles)*/) {
		apu->next_frame_irq  = cycles + apu->apu_clock_divider +
		                       apu->frame_irq_delay;
		cpu_interrupt_schedule(apu->emu->cpu, IRQ_APU_FRAME,
				       apu->next_frame_irq);
	}
}

int apu_init(struct emu *emu)
{
	struct apu_state *apu;
	int i;

	apu = malloc(sizeof(*emu->apu));
	if (!apu)
		return 0;

	apu->emu = emu;
	emu->apu = apu;

	for (i = 0; i < 4; i++) {
		cpu_set_write_handler(apu->emu->cpu, 0x4000 + i, 1, 0,
				      pulse_write_handler);
		cpu_set_write_handler(apu->emu->cpu, 0x4004 + i, 1, 0,
				      pulse_write_handler);
		cpu_set_write_handler(apu->emu->cpu, 0x4008 + i, 1, 0,
				      triangle_write_handler);
		cpu_set_write_handler(apu->emu->cpu, 0x400c + i, 1, 0,
				      noise_write_handler);
	}

	cpu_set_write_handler(apu->emu->cpu, 0x4014, 1, 0,
			      oam_dma);
	cpu_set_write_handler(apu->emu->cpu, 0x4015, 1, 0,
			      status_write_handler);
	cpu_set_write_handler(apu->emu->cpu, 0x4017, 1, 0,
			      frame_counter_write_handler);
	cpu_set_read_handler(apu->emu->cpu, 0x4015, 1, 0, status_read_handler);
	cpu_set_write_handler(apu->emu->cpu, 0x4010, 4, 0, dmc_write_handler);

	return 1;
}

void apu_cleanup(struct apu_state *apu)
{
	apu->emu->apu = NULL;
	free(apu);
}

void apu_reset(struct apu_state *apu, int hard)
{
	if (!hard) {
		apu->dmc_irq_flag = 0;
		apu->pulse[0].enabled = 0;
		apu->pulse[1].enabled = 0;
		apu->triangle.enabled = 0;
		apu->noise.enabled = 0;
		apu->frame_irq_flag = 0;
		apu->dmc.dma_timestamp = ~0;

		/* I'm not sure if all of this should
		   be done for DMC or not, but it keeps
		   the timestamps sane between soft resets.
		 */
		apu->dmc.empty = 1;
		apu->dmc.silent = 1;
		apu->dmc.next_clock = 0;

		/* not sure about these either */
		apu->pulse[0].next_clock = 0;
		apu->pulse[1].next_clock = 0;
		apu->triangle.next_clock = 0;
		apu->noise.next_clock = 0;

		apu->pulse[0].length.counter = 0;
		apu->pulse[1].length.counter = 0;
		apu->triangle.length.counter = 0;
		apu->noise.length.counter = 0;

		cpu_set_dmc_dma_timestamp(apu->emu->cpu, ~0, 0, 0);
		cpu_interrupt_ack(apu->emu->cpu, IRQ_APU_DMC);
		cpu_interrupt_cancel(apu->emu->cpu, IRQ_APU_DMC);
		apu->dmc_irq_flag = 0;

		apu->last_amplitude = 0.0;
		frame_counter_write_handler(apu->emu, 0x4017,
					    apu->frame_counter_mode,
					    cpu_get_cycles(apu->emu->cpu) +
			apu->apu_clock_divider);

		return;
	}

	memset(&apu->pulse[0], 0, sizeof(apu->pulse[0]));
	memset(&apu->pulse[1], 0, sizeof(apu->pulse[1]));
	memset(&apu->triangle, 0, sizeof(apu->triangle));
	memset(&apu->noise, 0, sizeof(apu->noise));
	memset(&apu->dmc, 0, sizeof(apu->dmc));

	apu->odd_cycle = 0;

	apu->next_frame_step = 0;
	apu->last_time = 0;
	apu->next_frame_irq = 0;
	apu->dmc_irq_flag = 0;
	apu->last_amplitude = 0;
	apu->dmc.addr = 0;
	apu->dmc.addr_current = 0;
	apu->dmc.length = 0;
	apu->dmc.bytes_remaining = 0;
	apu->dmc.loop = 0;
	apu->dmc.irq = 0;

	apu->pulse[0].sweep.parent_period = &apu->pulse[0].period;
	apu->pulse[1].sweep.parent_period = &apu->pulse[1].period;
	apu->pulse[0].sweep.negative_adjust = -1;
	apu->pulse[1].sweep.negative_adjust = 0;
	apu->pulse[0].envelope.delay = 1;
	apu->pulse[1].envelope.delay = 1;
	apu->noise.envelope.delay = 1;
	apu->pulse[0].envelope.period = 1;
	apu->pulse[1].envelope.period = 1;
	apu->noise.envelope.period = 1;
	apu->pulse[0].sweep.period = 1;
	apu->pulse[1].sweep.period = 1;
	apu->pulse[0].sweep.delay = 1;
	apu->pulse[1].sweep.delay = 1;
	apu->dmc.period = apu->dmc_rate_table[0];
	apu->dmc.empty = 1;
	apu->dmc.silent = 1;
	apu->dmc.shift_bits = 8;
	apu->dmc_irq_flag = 0;
	apu->dmc.dma_timestamp = ~0;

	apu->frame_counter_step = 0;
	apu->frame_irq_flag = 0;
	apu->frame_counter_register = 0;
	apu->frame_counter_register_timestamp = ~0;

	/* Clear any pending frame and DMC interrupt */
	cpu_interrupt_ack(apu->emu->cpu, IRQ_APU_FRAME);
	cpu_interrupt_cancel(apu->emu->cpu, IRQ_APU_FRAME);
	cpu_interrupt_ack(apu->emu->cpu, IRQ_APU_DMC);
	cpu_interrupt_cancel(apu->emu->cpu, IRQ_APU_DMC);

	/* Reset frame counter */
	apu->frame_counter_step = 0;
	apu->next_frame_irq = apu->frame_irq_delay;
	apu->next_frame_step = (apu->frame_step_delay + 1) *
	    apu->apu_clock_divider;

	/* Always disable frame interrupts for NSF */
	if (board_get_type(apu->emu->board) == BOARD_TYPE_NSF) {
		apu->frame_counter_mode = 0x40;
	} else {
		apu->frame_counter_mode = 0;
		cpu_interrupt_schedule(apu->emu->cpu, IRQ_APU_FRAME,
				       apu->next_frame_irq);
	}

//      apu->next_frame_step = apu->apu_clock_divider;

     apu->pulse[0].phase = 7;
     apu->pulse[1].phase = 7;
//      apu->pulse[0].next_clock = (apu->pulse[0].period + 1) * 2 * apu->apu_clock_divider;
//      apu->pulse[1].next_clock = (apu->pulse[1].period + 1) * 2 * apu->apu_clock_divider;
//      apu->triangle.next_clock = (apu->triangle.period + 1) * apu->apu_clock_divider;
//      apu->noise.next_clock = apu->noise.period * apu->apu_clock_divider;
	apu->noise.period = apu->noise_period_table[0];
	/* Reset length counters */
	apu->pulse[0].enabled = 0;
	apu->pulse[1].enabled = 0;
	apu->triangle.enabled = 0;
	apu->noise.enabled = 0;
	apu->pulse[0].length.halt = 0;
	apu->pulse[1].length.halt = 0;
	apu->triangle.length.halt = 0;
	apu->triangle.phase = 0;
	apu->noise.length.halt = 0;

	apu->noise.shift = 1;
}

static void set_frame_irq_flag(struct emu *emu)
{
	struct apu_state *apu = emu->apu;

	if (!apu->frame_irq_flag
	    && !(apu->frame_counter_mode & FRAME_INTERRUPT_DISABLED)) {
		apu->frame_irq_flag = 1;
		cpu_interrupt_schedule(apu->emu->cpu, IRQ_APU_FRAME,
				       apu->next_frame_irq);
	}
}

void apu_end_frame(struct apu_state *apu, uint32_t cycles)
{
	apu->pulse[0].next_clock -= cycles;
	apu->pulse[1].next_clock -= cycles;
	apu->triangle.next_clock -= cycles;
	apu->noise.next_clock -= cycles;
	apu->dmc.next_clock -= cycles;

	apu->next_frame_step -= cycles;

	apu->last_time -= cycles;

	if (!(apu->frame_counter_mode & 0xc0)) {
		apu->next_frame_irq -= cycles;
	}
}

static void clock_frame_counter(struct apu_state *apu)
{
	uint32_t cycles = apu->next_frame_step;
	int do_quarter_frame, do_half_frame;
	int frame_counter_mode;
	int frame_counter_reset;

	frame_counter_reset = 0;
	do_quarter_frame = 0;
	do_half_frame = 0;

	if (cycles == apu->frame_counter_register_timestamp) {
		apu->frame_counter_mode = apu->frame_counter_register;
		apu->frame_counter_register_timestamp = ~0;
		frame_counter_reset = 1;
	}

	frame_counter_mode = apu->frame_counter_mode & 0x80;

	switch (apu->frame_counter_step) {
	case 0x00:
		do_quarter_frame = 1;
		sched_next_frame_step(apu->frame_step_delay);
		break;
	case 0x01:
		do_half_frame = 1;
		sched_next_frame_step(apu->frame_step_delay + 2);
		break;
	case 0x02:
		do_quarter_frame = 1;
		sched_next_frame_step(apu->frame_step_delay + 1);
		break;
	case 0x03:
		do_half_frame = -1;
		do_quarter_frame = -1;
		set_frame_irq_flag(apu->emu);
		apu->next_frame_irq += apu->apu_clock_divider;
		sched_next_frame_step(1);
		break;
	case 0x04:
		do_half_frame = 1;
		set_frame_irq_flag(apu->emu);
		apu->next_frame_irq += apu->apu_clock_divider;
		sched_next_frame_step(1);
		break;
	case 0x05:
		do_half_frame = -1;
		do_quarter_frame = -1;
		set_frame_irq_flag(apu->emu);
		apu->next_frame_irq = apu->next_frame_step +
		    apu->frame_irq_delay;
		sched_next_frame_step(apu->frame_step_delay + 1);
		break;
	case 0x80:
		do_quarter_frame = 1;
		sched_next_frame_step(apu->frame_step_delay);
		break;
	case 0x81:
		do_half_frame = 1;
		sched_next_frame_step(apu->frame_step_delay + 2);
		break;
	case 0x82:
		do_quarter_frame = 1;
		sched_next_frame_step(2 * apu->frame_step_delay - 2);
		break;
	case 0x83:
		do_half_frame = 1;
		sched_next_frame_step(apu->frame_step_delay + 2);
		break;
	case 255:
		frame_counter_reset = 1;
		break;
	}

	if (frame_counter_reset &&
	    (apu->frame_counter_mode & FRAME_INTERRUPT_DISABLED)) {
		apu->frame_irq_flag = 0;
	}

	if (frame_counter_reset && frame_counter_mode &&
	    (!do_quarter_frame && !do_half_frame)) {
		do_quarter_frame = 1;
		do_half_frame = 1;
	}

	if (do_half_frame > 0) {
		clock_length_counters(apu);
		clock_sweeps(apu);
	}

	if (do_quarter_frame > 0) {
		clock_linear_counter(&apu->triangle.linear);
		clock_envelopes(apu);
	}

	if (apu->frame_counter_register_timestamp < apu->next_frame_step) {
		apu->next_frame_step = apu->frame_counter_register_timestamp;
		apu->frame_counter_step = 255;
	} else {
		if (frame_counter_reset) {
			apu->frame_counter_step = 0;
			sched_next_frame_step(apu->frame_step_delay + 2);
		} else {
			apu->frame_counter_step = (apu->frame_counter_step + 1);
		}

		if (frame_counter_mode)
			apu->frame_counter_step %= 4;
		else
			apu->frame_counter_step %= 6;

		apu->frame_counter_step |= frame_counter_mode;
	}

	pulse_update_volume(apu, 0, cycles);
	pulse_update_volume(apu, 1, cycles);
	noise_update_volume(apu, cycles);
}
static void apu_update_amplitude(struct apu_state *apu, uint32_t cycles)
{
	uint32_t pulse_out, tnd_out;
	int delta;
	uint32_t pulse_tmp, tnd_tmp;
	uint32_t out;
	struct config *config;

	config = apu->emu->config;

	pulse_tmp  = apu->pulse[0].amplitude * config->apu_pulse0_volume;
	pulse_tmp += apu->pulse[1].amplitude * config->apu_pulse1_volume;

	tnd_tmp  = 3 * apu->triangle.amplitude * config->apu_triangle_volume;
	tnd_tmp += 2 * apu->noise.amplitude * config->apu_noise_volume;
	tnd_tmp +=     apu->dmc.amplitude * config->apu_dmc_volume;

	if (pulse_tmp)
		pulse_out = (65536) * 9552 / (100 * 812800 / pulse_tmp + 10000);
	else
		pulse_out = 0;

	if (tnd_tmp)
		tnd_out = (65536) * 16367  / (100 * 2432900 / tnd_tmp + 10000);
	else
		tnd_out = 0;

	out = pulse_out + tnd_out;

	delta = out - apu->last_amplitude;
	if (delta) {
		audio_add_delta(cycles, delta);
		apu->last_amplitude = out;
	}

}

void apu_run(struct apu_state *apu, uint32_t cycles)
{
	while (1) {
		uint32_t time = -1;
//              uint32_t time = apu->next_frame_step;
		/* if (time > cycles) */
		/*      time = cycles; */

		if (apu->pulse[0].next_clock < time)
			time = apu->pulse[0].next_clock;
		if (apu->pulse[1].next_clock < time)
			time = apu->pulse[1].next_clock;
		if (apu->triangle.next_clock < time)
			time = apu->triangle.next_clock;
		if (apu->noise.next_clock < time)
			time = apu->noise.next_clock;
		if (apu->dmc.next_clock < time)
			time = apu->dmc.next_clock;
		if (apu->next_frame_step < time)
			time = apu->next_frame_step;

		if (time >= cycles) {
			if (cycles > apu->last_time) {
				if (((cycles - apu->last_time) /
				     apu->apu_clock_divider) & 1) {
					apu->odd_cycle ^= 1;
				}
				apu->last_time = cycles;
			}
			break;
		}

		if (time > apu->last_time) {
			if ( ((time - apu->last_time) /
			     apu->apu_clock_divider) & 1) {
				apu->odd_cycle ^= 1;
			}
			apu->last_time = time;
		}

		if (time == apu->next_frame_step)
			clock_frame_counter(apu);
		if (apu->pulse[0].next_clock <= time)
			pulse_run(apu, 0, cycles);
		if (apu->pulse[1].next_clock <= time)
			pulse_run(apu, 1, cycles);
		if (apu->triangle.next_clock <= time)
			triangle_run(apu, cycles);
		if (apu->noise.next_clock <= time)
			noise_run(apu, cycles);
		if (apu->dmc.next_clock <= time)
			dmc_run(apu, time);

		apu_update_amplitude(apu, time);

	}

//      printf("done\n");
}

static uint32_t apu_dmc_calc_dma_time(struct apu_state *apu, uint32_t cycles)
{
	uint32_t next_dma;
	struct dmc *dmc;
	int diff;

	apu_run(apu, cycles);

	dmc = &apu->dmc;

	if (!dmc->bytes_remaining)
		return ~0;

	next_dma = dmc->next_clock + apu->apu_clock_divider;
	diff = (dmc->shift_bits - 1) *
	       (dmc->period * apu->apu_clock_divider);
	next_dma += diff;

	/*
	   FIXME HACK ALERT Occasionally a dma restart via $4015 write
	   will happen on the same cycle as the last clock before
	   refilling the shift register.  I'm not sure of what is
	   correct in this case.  There are two possibilities: A)
	   clock, and load sample into shift register on this cycle B)
	   clock, load another 8 bits of silence into the register
	   and load the sample byte on the next transfer.

	   Case 'A' is the easiest, so that's what I'm doing here.  It
	   may not be correct.  The problem with it is that it will
	   cause the sample to finish playing too early if case B is
	   supposed to be the correct one.

	   Regardless, the next DMA time is 8 periods from right now
	   if we have only one bit left.
	 */
	if (next_dma == cycles) {
		next_dma += 8 * dmc->period * apu->apu_clock_divider;
	}

	return next_dma;
}

void apu_dmc_load_buf(struct apu_state * apu, uint8_t data, uint32_t *dma_time_ptr,
		      int *dma_addr_ptr, uint32_t cycles)
{
	struct dmc *dmc;

	dmc = &apu->dmc;

	/* FIXME should this run forward one tick? */
	apu_run(apu, cycles);

	dmc->dma_buf = data;
	dmc->bytes_remaining--;
	dmc->addr_current = ((dmc->addr_current + 1) & 0x3fff) | 0xc000;
	dmc->empty = 0;

	if (!dmc->bytes_remaining) {
		if (dmc->loop) {
			dmc->bytes_remaining = dmc->length;
			dmc->addr_current = dmc->addr;
		} else if (dmc->irq) {
			apu->dmc_irq_flag = 1;
			cpu_interrupt_schedule(apu->emu->cpu, IRQ_APU_DMC,
					       cycles);
		}
	}

	apu->dmc_irq_flag = (dmc->irq && !dmc->bytes_remaining);

	dmc->dma_timestamp = apu_dmc_calc_dma_time(apu, cycles);
	*dma_addr_ptr = dmc->addr_current;
	*dma_time_ptr = dmc->dma_timestamp;
}

static uint32_t apu_dmc_set_period(struct apu_state *apu, int period, uint32_t cycles)
{
	apu_run(apu, cycles);
	apu->dmc.period = apu->dmc_rate_table[period & 0x0f];
	return apu_dmc_calc_dma_time(apu, cycles);
}

void apu_set_type(struct apu_state *apu, int type)
{
	apu->swap_duty_cycles = 0;

	if (type == APU_TYPE_RP2A07) {
		apu->noise_period_table = pal_noise_period_table;
		apu->dmc_rate_table = pal_dmc_rate_table;
		apu->frame_step_delay = 8312;
		apu->apu_clock_divider = 15;
	} else if (type == APU_TYPE_DENDY) {
		apu->noise_period_table = ntsc_noise_period_table;
		apu->dmc_rate_table = ntsc_dmc_rate_table;
		apu->frame_step_delay = 7456;
		apu->swap_duty_cycles = 1;
		apu->apu_clock_divider = 16;
	} else {
		apu->noise_period_table = ntsc_noise_period_table;
		apu->dmc_rate_table = ntsc_dmc_rate_table;
		apu->frame_step_delay = 7456;
		apu->apu_clock_divider = 12;
	}
	apu->frame_irq_delay = (4 * apu->frame_step_delay + 4) *
	    apu->apu_clock_divider;
	apu->emu->apu_clock_divider = apu->apu_clock_divider;

}

int apu_apply_config(struct apu_state *apu)
{
	const char *filter;

//	apu->swap_duty_cycles = apu->emu->config->swap_pulse_duty_cycles;
	filter = apu->emu->config->raw_pcm_filter;

	if (strcasecmp(filter, "never") == 0) {
		apu->raw_pcm_filter = 0;
	} else if (strcasecmp(filter, "playback") == 0) {
		apu->raw_pcm_filter = 1;
	} else if (strcasecmp(filter, "always") == 0) {
		apu->raw_pcm_filter = 2;
	}

	return 0;
}

int apu_save_state(struct apu_state *apu, struct save_state *state)
{
	uint8_t *buf, *ptr;
	size_t size;
	int rc;

	size = 0;
	size += pack_state(apu, apu_state_items, NULL);

	size += pack_state(&apu->pulse[0], pulse_state_items, NULL);
	size += pack_state(&apu->pulse[0].length,
			   length_counter_state_items, NULL);
	size += pack_state(&apu->pulse[0].envelope,
			   envelope_state_items, NULL);
	size += pack_state(&apu->pulse[0].sweep, sweep_state_items, NULL);

	size += pack_state(&apu->pulse[1], pulse_state_items, NULL);
	size += pack_state(&apu->pulse[1].length,
			   length_counter_state_items, NULL);
	size += pack_state(&apu->pulse[1].envelope,
			   envelope_state_items, NULL);
	size += pack_state(&apu->pulse[1].sweep, sweep_state_items, NULL);

	size += pack_state(&apu->triangle, triangle_state_items, NULL);
	size += pack_state(&apu->triangle.length,
			   length_counter_state_items, NULL);
	size += pack_state(&apu->triangle.linear,
			   linear_counter_state_items, NULL);

	size += pack_state(&apu->noise, noise_state_items, NULL);
	size += pack_state(&apu->noise.length,
			   length_counter_state_items, NULL);
	size += pack_state(&apu->noise.envelope,
			   envelope_state_items, NULL);

	size += pack_state(&apu->dmc, dmc_state_items, NULL);

	buf = malloc(size);
	if (!buf)
		return -1;

	ptr = buf;
	ptr += pack_state(apu, apu_state_items, ptr);

	ptr += pack_state(&apu->pulse[0], pulse_state_items, ptr);
	ptr += pack_state(&apu->pulse[0].length,
			   length_counter_state_items, ptr);
	ptr += pack_state(&apu->pulse[0].envelope,
			   envelope_state_items, ptr);
	ptr += pack_state(&apu->pulse[0].sweep, sweep_state_items, ptr);

	ptr += pack_state(&apu->pulse[1], pulse_state_items, ptr);
	ptr += pack_state(&apu->pulse[1].length,
			   length_counter_state_items, ptr);
	ptr += pack_state(&apu->pulse[1].envelope,
			   envelope_state_items, ptr);
	ptr += pack_state(&apu->pulse[1].sweep, sweep_state_items, ptr);

	ptr += pack_state(&apu->triangle, triangle_state_items, ptr);
	ptr += pack_state(&apu->triangle.length,
			   length_counter_state_items, ptr);
	ptr += pack_state(&apu->triangle.linear,
			   linear_counter_state_items, ptr);

	ptr += pack_state(&apu->noise, noise_state_items, ptr);
	ptr += pack_state(&apu->noise.length,
			   length_counter_state_items, ptr);
	ptr += pack_state(&apu->noise.envelope,
			   envelope_state_items, ptr);
	
	ptr += pack_state(&apu->dmc, dmc_state_items, ptr);

	rc = save_state_add_chunk(state, "APU ", buf, size);
	free(buf);

	if (rc < 0)
		return -1;

	return 0;
}

int apu_load_state(struct apu_state *apu, struct save_state *state)
{
	uint8_t *buf;
	size_t size;

	if (save_state_find_chunk(state, "APU ", &buf, &size) < 0)
		return -1;

	buf += unpack_state(apu, apu_state_items, buf);

	buf += unpack_state(&apu->pulse[0], pulse_state_items, buf);
	buf += unpack_state(&apu->pulse[0].length,
			   length_counter_state_items, buf);
	buf += unpack_state(&apu->pulse[0].envelope,
			   envelope_state_items, buf);
	buf += unpack_state(&apu->pulse[0].sweep, sweep_state_items, buf);

	buf += unpack_state(&apu->pulse[1], pulse_state_items, buf);
	buf += unpack_state(&apu->pulse[1].length,
			   length_counter_state_items, buf);
	buf += unpack_state(&apu->pulse[1].envelope,
			   envelope_state_items, buf);
	buf += unpack_state(&apu->pulse[1].sweep, sweep_state_items, buf);

	buf += unpack_state(&apu->triangle, triangle_state_items, buf);
	buf += unpack_state(&apu->triangle.length,
			   length_counter_state_items, buf);
	buf += unpack_state(&apu->triangle.linear,
			   linear_counter_state_items, buf);

	buf += unpack_state(&apu->noise, noise_state_items, buf);
	buf += unpack_state(&apu->noise.length,
			   length_counter_state_items, buf);
	buf += unpack_state(&apu->noise.envelope,
			   envelope_state_items, buf);

	buf += unpack_state(&apu->dmc, dmc_state_items, buf);

	return 0;
}
