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
#include "audio.h"

/* FIXME this is what the APU uses on NTSC; what about PAL? */
/* looks NTSC is 7457 93/240, pal 6927 127/240 */
#define FRAME_COUNTER_PERIOD_NTSC 7458
#define FRAME_COUNTER_PERIOD_PAL 6928
#define FRAME_COUNTER_PERIOD_DENDY 7390

static CPU_WRITE_HANDLER(mmc5_audio_write_handler);
static CPU_READ_HANDLER(mmc5_audio_read_handler);

struct mmc5_audio_state;

int mmc5_audio_init(struct emu *emu);
void mmc5_audio_cleanup(struct emu *);
void mmc5_audio_reset(struct mmc5_audio_state *, int);
void mmc5_audio_end_frame(struct mmc5_audio_state *, uint32_t);

static const uint8_t length_table[0x20] = {
	0x0A, 0xFE, 0x14, 0x02, 0x28, 0x04, 0x50, 0x06,
	0xA0, 0x08, 0x3C, 0x0A, 0x0E, 0x0C, 0x1A, 0x0E,
	0x0C, 0x10, 0x18, 0x12, 0x30, 0x14, 0x60, 0x16,
	0xC0, 0x18, 0x48, 0x1A, 0x10, 0x1C, 0x20, 0x1E
};

struct mmc5_pulse {
	int counter;
	int envelope_period;
	int envelope_delay;
	int envelope_counter;
	int period;
	int duty_cycle;
	int phase;
	int amplitude;
	int enabled;
	int counter_halt_flag;
	int loop_flag;
	int start_flag;
	int constant_flag;
	uint32_t next_clock;
};

struct mmc5_audio_state {
	struct mmc5_pulse mmc5_pulse[2];
	uint32_t next_frame_clock;
	int frame_counter_period;
	int pcm_irq_enabled;
	int pcm_irq_fired;
	int pcm_read_mode;
	int pcm;
	int last_amplitude;

	struct emu *emu;
};

static struct state_item mmc5_pulse_state_items[] = {
	STATE_8BIT(mmc5_pulse, counter),
	STATE_8BIT(mmc5_pulse, envelope_period),
	STATE_8BIT(mmc5_pulse, envelope_delay),
	STATE_8BIT(mmc5_pulse, envelope_counter),
	STATE_16BIT(mmc5_pulse, period),
	STATE_8BIT(mmc5_pulse, duty_cycle),
	STATE_8BIT(mmc5_pulse, phase),
	STATE_8BIT(mmc5_pulse, amplitude),
	STATE_8BIT(mmc5_pulse, enabled), /* BOOLEAN */
	STATE_8BIT(mmc5_pulse, counter_halt_flag), /* BOOLEAN */
	STATE_8BIT(mmc5_pulse, loop_flag), /* BOOLEAN */
	STATE_8BIT(mmc5_pulse, start_flag), /* BOOLEAN */
	STATE_8BIT(mmc5_pulse, constant_flag), /* BOOLEAN */
	STATE_32BIT(mmc5_pulse, next_clock),
	STATE_ITEM_END(),
	
};

static struct state_item mmc5_audio_state_items[] = {
	STATE_32BIT(mmc5_audio_state, next_frame_clock),
	STATE_8BIT(mmc5_audio_state, pcm_irq_enabled), /* BOOLEAN */
	STATE_8BIT(mmc5_audio_state, pcm_irq_fired), /* BOOLEAN */
	STATE_8BIT(mmc5_audio_state, pcm_read_mode), /* BOOLEAN */
	STATE_8BIT(mmc5_audio_state, pcm),
	STATE_ITEM_END(),
};

static void mmc5_audio_update_amplitude(struct mmc5_audio_state *audio,
					uint32_t cycles)
{
	uint32_t pulse_out, pulse_tmp;
	uint32_t pcm_out, pcm_tmp;
	uint32_t out;
	int delta;
	struct config *config;

	config = audio->emu->config;

	pulse_tmp  = audio->mmc5_pulse[0].amplitude * config->mmc5_pulse0_volume;
	pulse_tmp += audio->mmc5_pulse[1].amplitude * config->mmc5_pulse1_volume;

	pcm_tmp = audio->pcm * 64 * config->mmc5_pcm_volume;

	if (pulse_tmp)
		pulse_out = (65536) * 9552 / (100 * 812800 / pulse_tmp + 10000);
	else
		pulse_out = 0;

	/* PCM audio volume is apparently closer to linear than the 2A03's DMC
	   channel.
	*/
	if (pcm_tmp)
		pcm_out = pcm_tmp / 100;
	else
		pcm_out = 0;


	out = pulse_out + pcm_out;

	delta = -(out - audio->last_amplitude);
	if (delta) {
		audio_add_delta(cycles, delta);
		audio->last_amplitude = out;
	}
}

static CPU_READ_HANDLER(mmc5_audio_pcm_read_handler)
{
	struct mmc5_audio_state *audio;
	uint8_t v;

	audio = emu->mmc5_audio;
	v = cpu_peek(emu->cpu, addr);

	if (v) {
		audio->pcm = v;
		mmc5_audio_update_amplitude(audio, cycles);
	} else if (audio->pcm_irq_enabled) {
		cpu_interrupt_schedule(emu->cpu, IRQ_MMC5_PCM, cycles);
		audio->pcm_irq_fired = 0x80;
	}

	return v;
}

void mmc5_audio_pcm_read_mode_enable(struct mmc5_audio_state *audio, int enable)
{
	audio->pcm_read_mode = enable;

	/* Set up read handlers */
	if (audio->pcm_read_mode) {
		cpu_set_read_handler(audio->emu->cpu, 0x8000, 0x4000, 0,
				     mmc5_audio_pcm_read_handler);
		/* printf("configured read handler\n"); */
	} else {
		cpu_set_read_handler(audio->emu->cpu, 0x8000, 0x4000, 0, NULL);
		/* printf("deconfigured read handler\n"); */
	}
}

static void mmc5_pulse_run(struct mmc5_audio_state *audio, int c,
			   uint32_t cycles)
{
	struct mmc5_pulse *pulse;
	int timer_period;
	int volume;
	int duty;
	int delta;
	uint32_t limit;

	pulse = &audio->mmc5_pulse[c];

	limit = audio->next_frame_clock;
	if (limit > cycles)
		limit = cycles;

	timer_period = (pulse->period << 1) + 2;
	timer_period *= audio->emu->apu_clock_divider;

	if (pulse->counter == 0 || pulse->period < 8)
		volume = 0;
	else if (pulse->constant_flag)
		volume = pulse->envelope_period;
	else
		volume = pulse->envelope_counter;

	duty = 1 << pulse->duty_cycle;
	if (pulse->duty_cycle == 3)
		duty = 2;

	delta = pulse->amplitude ? volume : -volume;

	if (!volume) {
		int cycles_to_run;
		int count;

		cycles_to_run = limit - pulse->next_clock;
		count = cycles_to_run / timer_period;
		if (cycles_to_run % timer_period)
			count++;

		pulse->next_clock += count * timer_period;
		pulse->phase = (pulse->phase + count) % 8;

		return;
	}

	pulse->phase = (pulse->phase + 1) % 8;
	if (pulse->phase == 0 || pulse->phase == duty) {
		delta = -delta;
		pulse->amplitude += delta;
	}
	pulse->next_clock += timer_period;
}

static void mmc5_pulse_update(struct mmc5_audio_state *audio, int c,
			      uint32_t cycles)
{
	struct mmc5_pulse *pulse;
	int volume;
	int new_amplitude;
	int duty;

	pulse = &audio->mmc5_pulse[c];

	if (pulse->counter == 0)
		volume = 0;
	else if (pulse->constant_flag)
		volume = pulse->envelope_period;
	else
		volume = pulse->envelope_counter;

	new_amplitude = 0;

	duty = 1 << pulse->duty_cycle;
	if (pulse->duty_cycle == 3) {
		duty = 2;
		new_amplitude = volume;
	}

	if (pulse->phase < duty)
		new_amplitude ^= volume;

	if (!volume || pulse->period < 8)
		new_amplitude = 0;

	if (new_amplitude != pulse->amplitude) {
		pulse->amplitude = new_amplitude;
		mmc5_audio_update_amplitude(audio, cycles);
	}
}

static void mmc5_pulse_frame_clock(struct mmc5_audio_state *audio, int c,
				   uint32_t cycles)
{
	struct mmc5_pulse *pulse;

	pulse = &audio->mmc5_pulse[c];

	if (pulse->counter && !pulse->counter_halt_flag)
		pulse->counter--;

	if (pulse->start_flag) {
		pulse->start_flag = 0;
		pulse->envelope_delay = pulse->envelope_period + 1;
		pulse->envelope_counter = 15;
	} else if (--pulse->envelope_delay == 0) {
		pulse->envelope_delay = pulse->envelope_period + 1;
		if (pulse->envelope_counter)
			pulse->envelope_counter--;
		else if (pulse->loop_flag)
			pulse->envelope_counter = 15;
	}

	mmc5_pulse_update(audio, c, cycles);
}

void mmc5_audio_run(struct mmc5_audio_state *audio, uint32_t cycles)
{
	uint32_t limit;

	if (audio->emu->overclocking)
		return;

	while (1) {
		limit = ~0;
		if (audio->mmc5_pulse[0].next_clock < limit)
			limit = audio->mmc5_pulse[0].next_clock;
		if (audio->mmc5_pulse[1].next_clock < limit)
			limit = audio->mmc5_pulse[1].next_clock;
		if (audio->next_frame_clock < limit)
			limit = audio->next_frame_clock;

		if (limit >= cycles) {
			break;
		}

		if (limit == audio->next_frame_clock) {
			mmc5_pulse_frame_clock(audio, 0, limit);
			mmc5_pulse_frame_clock(audio, 1, limit);
			audio->next_frame_clock += audio->frame_counter_period *
				audio->emu->apu_clock_divider;
		}

		if (audio->mmc5_pulse[0].next_clock <= limit)
			mmc5_pulse_run(audio, 0, cycles);
		if (audio->mmc5_pulse[1].next_clock <= limit)
			mmc5_pulse_run(audio, 1, cycles);

		mmc5_audio_update_amplitude(audio, limit);
	}
}

static void mmc5_pulse_write_handler(struct mmc5_audio_state *audio,
				     int addr, uint8_t value,
				     uint32_t cycles)
{
	struct mmc5_pulse *pulse;
	int c;

	mmc5_audio_run(audio, cycles);
	c = (addr & 0x04) >> 2;
	pulse = &audio->mmc5_pulse[c];

	switch (addr & 0x03) {
	case 0:
		pulse->duty_cycle = (value >> 6) & 0x03;
		pulse->loop_flag = value & 0x20;
		pulse->constant_flag = value & 0x10;
		pulse->envelope_period = value & 0x0f;
		mmc5_pulse_update(audio, c, cycles);
		pulse->counter_halt_flag = value & 0x20;
		break;
	case 1:
		/* Register 1 does nothing (normally sweep on
		   APU) */
		break;
	case 2:
		pulse->period = (pulse->period & 0x700) | value;
		mmc5_pulse_update(audio, c, cycles);
		break;
	case 3:
		pulse->period = (pulse->period & 0xff) | ((value & 0x07) << 8);
		pulse->start_flag = 1;
		pulse->phase = 7;	/* Will be on first phase when next clocked */
		if (pulse->enabled) {
			pulse->counter =
				length_table[(value >> 3) & 0x1f];
		}
		mmc5_pulse_update(audio, c, cycles);
		break;
	}
}

static CPU_WRITE_HANDLER(mmc5_audio_write_handler)
{
	struct mmc5_audio_state *audio;

	audio = emu->mmc5_audio;
	mmc5_audio_run(audio, cycles);

	switch (addr) {
	case 0x5000:
	case 0x5001:
	case 0x5002:
	case 0x5003:
	case 0x5004:
	case 0x5005:
	case 0x5006:
	case 0x5007:
		mmc5_pulse_write_handler(audio, addr, value, cycles);
		break;
	case 0x5010:
		mmc5_audio_pcm_read_mode_enable(audio, value & 0x01);
		audio->pcm_irq_enabled = value & 0x80;
		break;
	case 0x5011:
		if (!audio->pcm_read_mode && value) {
			if (audio->pcm != value) {
				audio->pcm = value;
				mmc5_audio_update_amplitude(audio, cycles);
			}
		}
		break;
	case 0x5015:
		audio->mmc5_pulse[0].enabled = value & 0x01;
		if (!(value & 0x01))
			audio->mmc5_pulse[0].counter = 0;

		audio->mmc5_pulse[1].enabled = value & 0x02;
		if (!(value & 0x02))
			audio->mmc5_pulse[1].counter = 0;
		break;
	}
}

static CPU_READ_HANDLER(mmc5_audio_read_handler)
{
	struct mmc5_audio_state *audio;

	audio = emu->mmc5_audio;
	mmc5_audio_run(audio, cycles);

	value = 0;

	switch (addr) {
	case 0x5010:
		cpu_interrupt_ack(audio->emu->cpu, IRQ_MMC5_PCM);
		/* FIXME are bits 0-6 open bus? */
		value &= 0x7f;
		value |= audio->pcm_irq_fired;
		audio->pcm_irq_fired = 0;
		break;
	case 0x5015:
		if (audio->mmc5_pulse[0].counter)
			value |= 0x01;

		if (audio->mmc5_pulse[1].counter)
			value |= 0x02;
		break;
	}

	return value;
}

int mmc5_audio_init(struct emu *emu)
{
	struct mmc5_audio_state *audio;

	audio = malloc(sizeof(*audio));
	if (!audio)
		return 1;

	memset(audio, 0, sizeof(*audio));

	emu->mmc5_audio = audio;
	audio->emu = emu;

	return 0;
}

void mmc5_audio_cleanup(struct emu *emu)
{
	if (emu->mmc5_audio)
		free(emu->mmc5_audio);

	emu->mmc5_audio = NULL;
}

void mmc5_audio_reset(struct mmc5_audio_state *audio, int hard)
{
	if (!hard) {
		/*
		audio->mmc5_pulse[0].enabled = 0;
		audio->mmc5_pulse[0].counter = 0;
		audio->mmc5_pulse[1].enabled = 0;
		audio->mmc5_pulse[1].counter = 0;
		*/

		audio->mmc5_pulse[0].next_clock = 0;
		audio->mmc5_pulse[1].next_clock = 0;

		return;
	}

	memset(&audio->mmc5_pulse[0], 0, sizeof(audio->mmc5_pulse[0]));
	memset(&audio->mmc5_pulse[1], 0, sizeof(audio->mmc5_pulse[1]));

	audio->mmc5_pulse[0].envelope_delay = 1;
	audio->mmc5_pulse[0].envelope_period = 1;
	audio->mmc5_pulse[0].enabled = 0;
	audio->mmc5_pulse[0].counter_halt_flag = 0;
	audio->mmc5_pulse[1].envelope_delay = 1;
	audio->mmc5_pulse[1].envelope_period = 1;
	audio->mmc5_pulse[1].enabled = 0;
	audio->mmc5_pulse[1].counter_halt_flag = 0;

	audio->pcm_irq_enabled = 0;
	audio->pcm_irq_fired = 0;
	audio->pcm_read_mode = 0;
	audio->pcm = 0;
	switch (cpu_get_type(audio->emu->cpu)) {
	case CPU_TYPE_RP2A03:
		audio->frame_counter_period = FRAME_COUNTER_PERIOD_NTSC;
		break;
	case CPU_TYPE_RP2A07:
		audio->frame_counter_period = FRAME_COUNTER_PERIOD_PAL;
		break;
	case CPU_TYPE_DENDY:
		audio->frame_counter_period = FRAME_COUNTER_PERIOD_DENDY;
		break;
	}
	audio->next_frame_clock = audio->frame_counter_period *
	    audio->emu->apu_clock_divider;
}

void mmc5_audio_end_frame(struct mmc5_audio_state *audio, uint32_t cycles)
{
	audio->mmc5_pulse[0].next_clock -= cycles;
	audio->mmc5_pulse[1].next_clock -= cycles;
	audio->next_frame_clock -= cycles;
}

void mmc5_audio_install_handlers(struct emu *emu, int multi_chip_nsf)
{
	if (multi_chip_nsf) {
		cpu_set_write_handler(emu->cpu, 0x5205, 2, 0, mmc5_audio_write_handler);
		cpu_set_read_handler(emu->cpu, 0x5205, 2, 0, mmc5_audio_read_handler);
		/* No PCM, since IRQs aren't really supported for NSFs */
	} else {
		cpu_set_write_handler(emu->cpu, 0x5000, 8, 0, mmc5_audio_write_handler);
		cpu_set_write_handler(emu->cpu, 0x5010, 2, 0, mmc5_audio_write_handler);
		cpu_set_write_handler(emu->cpu, 0x5015, 1, 0, mmc5_audio_write_handler);
		cpu_set_read_handler(emu->cpu, 0x5010, 2, 0, mmc5_audio_read_handler);
	}

}

int mmc5_audio_save_state(struct emu *emu, struct save_state *state)
{
	struct mmc5_audio_state *audio;
	size_t size;
	uint8_t *buf, *ptr;
	int rc;

	audio = emu->mmc5_audio;

	size = pack_state(audio, mmc5_audio_state_items, NULL);
	size += 2 * pack_state(&audio->mmc5_pulse[0], mmc5_pulse_state_items,
			       NULL);

	buf = malloc(size);
	if (!buf)
		return -1;

	ptr = buf;
	ptr += pack_state(audio, mmc5_audio_state_items, ptr);
	ptr += pack_state(&audio->mmc5_pulse[0], mmc5_pulse_state_items,
			   ptr);
	ptr += pack_state(&audio->mmc5_pulse[1], mmc5_pulse_state_items,
			   ptr);


	rc = save_state_add_chunk(state, "MC5S", buf, size);
	free(buf);

	return rc;
}

int mmc5_audio_load_state(struct emu *emu, struct save_state *state)
{
	struct mmc5_audio_state *audio;
	size_t size;
	uint8_t *buf;

	audio = emu->mmc5_audio;

	if (save_state_find_chunk(state, "MC5S", &buf, &size) < 0)
		return -1;

	buf += unpack_state(audio, mmc5_audio_state_items, buf);
	buf += unpack_state(&audio->mmc5_pulse[0], mmc5_pulse_state_items, buf);
	buf += unpack_state(&audio->mmc5_pulse[1], mmc5_pulse_state_items, buf);

	return 0;
}
