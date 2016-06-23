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

static uint8_t volume_table[32] = {
	0x00, 0x01, 0x01, 0x02, 0x02, 0x03, 0x03, 0x04,
	0x05, 0x06, 0x07, 0x09, 0x0b, 0x0d, 0x0f, 0x12,
	0x16, 0x1a, 0x1f, 0x25, 0x2d, 0x35, 0x3f, 0x4c,
	0x5a, 0x6a, 0x7f, 0x97, 0xb4, 0xd6, 0xeb, 0xff
};

struct sunsoft5b_tone {
	int period;
	int volume;
	int next_clock;
	int step;
	int amplitude;
};

struct sunsoft5b_envelope {
	int period;
	int cont;
	int attack;
	int alternate;
	int hold;
	int direction;
	int pause;
	int step;
	int next_clock;
};

struct sunsoft5b_noise {
	int period;
	int seed;
	int next_clock;
};

struct sunsoft5b_audio_state {
	struct sunsoft5b_tone tone[3];
	struct sunsoft5b_envelope envelope;
	struct sunsoft5b_noise noise;
	int tone_enabled[3];
	int noise_enabled[3];
	int envelope_enabled[3];
	int register_select;
	int last_amplitude;
	struct emu *emu;
};

struct state_item sunsoft5b_audio_state_items[] = {
	STATE_8BIT(sunsoft5b_audio_state, tone_enabled[0]), /* BOOLEAN */
	STATE_8BIT(sunsoft5b_audio_state, tone_enabled[1]), /* BOOLEAN */
	STATE_8BIT(sunsoft5b_audio_state, tone_enabled[2]), /* BOOLEAN */
	STATE_8BIT(sunsoft5b_audio_state, noise_enabled[0]), /* BOOLEAN */
	STATE_8BIT(sunsoft5b_audio_state, noise_enabled[1]), /* BOOLEAN */
	STATE_8BIT(sunsoft5b_audio_state, noise_enabled[2]), /* BOOLEAN */
	STATE_8BIT(sunsoft5b_audio_state, envelope_enabled[0]), /* BOOLEAN */
	STATE_8BIT(sunsoft5b_audio_state, envelope_enabled[1]), /* BOOLEAN */
	STATE_8BIT(sunsoft5b_audio_state, envelope_enabled[2]), /* BOOLEAN */
	STATE_8BIT(sunsoft5b_audio_state, register_select),
	STATE_ITEM_END(),
};

struct state_item sunsoft5b_tone_state_items[] = {
	STATE_16BIT(sunsoft5b_tone, period),
	STATE_8BIT(sunsoft5b_tone, volume),
	STATE_8BIT(sunsoft5b_tone, step),
	STATE_8BIT(sunsoft5b_tone, amplitude),
	STATE_32BIT(sunsoft5b_tone, next_clock),
	STATE_ITEM_END(),
};

struct state_item sunsoft5b_envelope_state_items[] = {
	STATE_16BIT(sunsoft5b_envelope, period),
	STATE_8BIT(sunsoft5b_envelope, cont), /* BOOLEAN */
	STATE_8BIT(sunsoft5b_envelope, attack), /* BOOLEAN */
	STATE_8BIT(sunsoft5b_envelope, alternate), /* BOOLEAN */
	STATE_8BIT(sunsoft5b_envelope, hold), /* BOOLEAN */
	STATE_8BIT(sunsoft5b_envelope, direction), /* BOOLEAN */
	STATE_8BIT(sunsoft5b_envelope, pause), /* BOOLEAN */
	STATE_8BIT(sunsoft5b_envelope, step),
	STATE_32BIT(sunsoft5b_envelope, next_clock),
	STATE_ITEM_END(),
};

struct state_item sunsoft5b_noise_state_items[] = {
	STATE_8BIT(sunsoft5b_noise, period),
	STATE_32BIT(sunsoft5b_noise, seed),
	STATE_32BIT(sunsoft5b_noise, next_clock),
	STATE_ITEM_END(),
};

static CPU_WRITE_HANDLER(sunsoft5b_audio_write_handler);

void sunsoft5b_audio_run(struct sunsoft5b_audio_state *audio, uint32_t cycles);

int sunsoft5b_audio_init(struct emu *emu);
void sunsoft5b_audio_cleanup(struct emu *emu);
void sunsoft5b_audio_reset(struct sunsoft5b_audio_state *audio, int hard);
void sunsoft5b_audio_end_frame(struct sunsoft5b_audio_state *audio, uint32_t cycles);
void sunsoft5b_audio_install_handlers(struct emu *emu, int multi_chip_nsf);

int sunsoft5b_audio_init(struct emu *emu)
{
	struct sunsoft5b_audio_state *audio;

	audio = malloc(sizeof(*audio));
	if (!audio)
		return 1;

	memset(audio, 0, sizeof(*audio));

	emu->sunsoft5b_audio = audio;
	audio->emu = emu;

	return 0;
}

void sunsoft5b_audio_cleanup(struct emu *emu)
{
	if (emu->sunsoft5b_audio)
		free(emu->sunsoft5b_audio);

	emu->sunsoft5b_audio = NULL;
}

void sunsoft5b_audio_reset(struct sunsoft5b_audio_state *audio, int hard)
{
	if (hard) {
		struct emu *emu = audio->emu;

		memset(audio, 0, sizeof(*audio));
		audio->emu = emu;
		audio->noise.seed = 0xffff;
	}

	audio->envelope.next_clock = 0;
	audio->noise.next_clock = 0;
	audio->tone[0].next_clock = 0;
	audio->tone[1].next_clock = 0;
	audio->tone[2].next_clock = 0;
}

void sunsoft5b_audio_end_frame(struct sunsoft5b_audio_state *audio, uint32_t cycles)
{
	/* if (audio->envelope.next_clock != ~0) */
		audio->envelope.next_clock -= cycles;
	audio->noise.next_clock -= cycles;
	audio->tone[0].next_clock -= cycles;
	audio->tone[1].next_clock -= cycles;
	audio->tone[2].next_clock -= cycles;
}

void sunsoft5b_audio_install_handlers(struct emu *emu, int multi_chip_nsf)
{
	if (multi_chip_nsf) {
		cpu_set_write_handler(emu->cpu, 0xc000, 1, 0,
				      sunsoft5b_audio_write_handler);
		cpu_set_write_handler(emu->cpu, 0xe000, 1, 0,
				      sunsoft5b_audio_write_handler);
	} else {
		cpu_set_write_handler(emu->cpu, 0xc000, 0x2000, 0,
				      sunsoft5b_audio_write_handler);
		cpu_set_write_handler(emu->cpu, 0xe000, 0x2000, 0,
				      sunsoft5b_audio_write_handler);
	}
		
}

static void sunsoft5b_envelope_run(struct sunsoft5b_audio_state *audio,
				   uint32_t cycles)
{
	struct sunsoft5b_envelope *envelope;
	int period_cycles;

	envelope = &audio->envelope;

	/* if (envelope->period == 0) */
	/* 	return; */

	period_cycles = (envelope->period + 1) * 16 * audio->emu->apu_clock_divider;

	if (!envelope->pause) {
		int incr;

		incr = envelope->direction ? 1 : 63;
		
		envelope->step = (envelope->step + incr) & 0x3f;
	}

	if (envelope->step >= 32) { /* if carry or borrow */
		if (envelope->cont) {
			if (envelope->alternate ^ envelope->hold)
				envelope->direction ^= 1;

			if (envelope->hold)
				envelope->pause = 1;

			envelope->step = envelope->direction ? 0 : 31;
		} else {
			envelope->pause = 1;
		        envelope->step = 0;
		}
	}

	envelope->next_clock += period_cycles;
}

static void sunsoft5b_noise_run(struct sunsoft5b_audio_state *audio,
				uint32_t cycles)
{
	int period_cycles;

	period_cycles = (audio->noise.period + 1) * 16 * audio->emu->apu_clock_divider;

	if (audio->noise.seed & 0x01)
		audio->noise.seed ^= 0x24000;

	audio->noise.seed >>= 1;

	audio->noise.next_clock += period_cycles;
}

static void sunsoft5b_tone_run(struct sunsoft5b_audio_state *audio, int c,
			     uint32_t cycles)
{
	struct sunsoft5b_tone *tone;
	int amplitude;

	tone = &audio->tone[c];

	if (!audio->tone_enabled[c] && (tone->next_clock < cycles)) {
		int period_cycles;
		int cycles_to_run;
		int count;

		cycles_to_run = cycles - tone->next_clock;
		period_cycles = (tone->period + 1) * 16 * audio->emu->apu_clock_divider;

		count = cycles_to_run / period_cycles;
		if (cycles_to_run % period_cycles)
			count++;

		if (tone->amplitude)
			tone->amplitude = 0;

		if (count & 1)
			tone->step ^= 1;

		tone->next_clock += count * period_cycles;

		return;
	}

	
	amplitude = 0;

	if (tone->step)
		amplitude = 1;

	tone->amplitude = amplitude;
	tone->step ^= 1;
	tone->next_clock += (tone->period + 1) * 16 * audio->emu->apu_clock_divider;
}

void sunsoft5b_audio_update_amplitude(struct sunsoft5b_audio_state *audio,
				      uint32_t cycles)
{
	int out;
	int noise;
	int i;
	struct config *config;

	noise = audio->noise.seed & 0x01;
	out = 0;
	config = audio->emu->config;

	for (i = 0; i < 3; i++) {
		if ((!audio->noise_enabled[i] || noise) &&
		    (!audio->tone_enabled[i] || audio->tone[i].amplitude)) {
			int channel_out;
			int volume = config->sunsoft5b_channel_volume[i];

			if (audio->envelope_enabled[i]) {
				channel_out = volume_table[audio->envelope.step];
			} else {
				channel_out = volume_table[audio->tone[i].volume];
			}

			out += 128 * 64 * volume * channel_out / 10000;
		}
	}

	if (out != audio->last_amplitude) {
		audio_add_delta(cycles, out - audio->last_amplitude);
		audio->last_amplitude = out;
	}
}

void sunsoft5b_audio_run(struct sunsoft5b_audio_state *audio, uint32_t cycles)
{
	uint32_t limit;

	if (audio->emu->overclocking)
		return;

	while (1) {
		limit = -1;
		if (audio->envelope.next_clock < limit)
			limit = audio->envelope.next_clock;
		if (audio->noise.next_clock < limit)
			limit = audio->noise.next_clock;
		if (audio->tone[0].next_clock < limit)
			limit = audio->tone[0].next_clock;
		if (audio->tone[1].next_clock < limit)
			limit = audio->tone[1].next_clock;
		if (audio->tone[2].next_clock < limit)
			limit = audio->tone[2].next_clock;

		if (limit >= cycles)
			break;

		if (audio->envelope.next_clock <= limit)
			sunsoft5b_envelope_run(audio, cycles);
		if (audio->noise.next_clock <= limit)
			sunsoft5b_noise_run(audio, cycles);
		if (audio->tone[0].next_clock <= limit)
			sunsoft5b_tone_run(audio, 0, cycles);
		if (audio->tone[1].next_clock <= limit)
			sunsoft5b_tone_run(audio, 1, cycles);
		if (audio->tone[2].next_clock <= limit)
			sunsoft5b_tone_run(audio, 2, cycles);

		sunsoft5b_audio_update_amplitude(audio, limit);
	}
}

static CPU_WRITE_HANDLER(sunsoft5b_audio_write_handler)
{
	struct sunsoft5b_audio_state *audio;
	int channel;
	int period;

	audio = emu->sunsoft5b_audio;

	sunsoft5b_audio_run(audio, cycles);

	if (addr < 0xe000) {
		audio->register_select = value & 0x0f;
		return;
	}

	switch (audio->register_select) {
	case 0x00:
	case 0x02:
	case 0x04:
		channel = audio->register_select >> 1;
		period = audio->tone[channel].period;
		period = (period & (0xf00)) | value;
		audio->tone[channel].period = period;	
	break;
	case 0x01:
	case 0x03:
	case 0x05:
		channel = audio->register_select >> 1;
		period = audio->tone[channel].period;
		period = (period & (0xff)) | ((value & 0x0f) << 8);
		audio->tone[channel].period = period;
		break;
	case 0x06:
		audio->noise.period = value & 0x1f;
		break;
	case 0x07:
		audio->noise_enabled[0] = !(value & 0x08);
		audio->noise_enabled[1] = !(value & 0x10);
		audio->noise_enabled[2] = !(value & 0x20);
		audio->tone_enabled[0] = !(value & 0x01);
		audio->tone_enabled[1] = !(value & 0x02);
		audio->tone_enabled[2] = !(value & 0x04);
		sunsoft5b_audio_update_amplitude(audio, cycles);
		break;
	case 0x08:
	case 0x09:
	case 0x0a:
		channel = audio->register_select & 0x03;
		audio->envelope_enabled[channel] = (value & 0x10);
		audio->tone[channel].volume = (value & 0x0f) << 1;
		sunsoft5b_audio_update_amplitude(audio, cycles);
		break;
	case 0x0b:
		audio->envelope.period &= 0xff00;
		audio->envelope.period |= value;
		/* if (audio->envelope.period == 0) */
		/* 	audio->envelope.next_clock = ~0; */
		/* else */
		/* 	audio->envelope.next_clock = cycles; /\* FIXME *\/ */
		break;
	case 0x0c:
		audio->envelope.period &= 0x00ff;
		audio->envelope.period |= (value << 8);
		/* if (audio->envelope.period == 0) */
		/* 	audio->envelope.next_clock = ~0; */
		/* else */
		/* 	audio->envelope.next_clock = cycles; /\* FIXME *\/ */
		break;
	case 0x0d:
		audio->envelope.cont = (value >> 3) & 0x01;
		audio->envelope.attack = (value >> 2) & 0x01;
		audio->envelope.alternate = (value >> 1) & 0x01;
		audio->envelope.hold = value & 0x01;
		audio->envelope.direction = audio->envelope.attack;
		audio->envelope.pause = 0;
		audio->envelope.step = audio->envelope.direction ? 0 : 31;
		/* if (audio->envelope.period == 0) */
		/* 	audio->envelope.next_clock = ~0; */
		/* else */
		/* 	audio->envelope.next_clock = cycles + ((audio->envelope.period + 1) * 16 * audio->emu->apu_clock_divider); /\* FIXME *\/ */
		break;
		sunsoft5b_audio_update_amplitude(audio, cycles);
		break;
	}
}

int sunsoft5b_audio_save_state(struct emu *emu, struct save_state *state)
{
	struct sunsoft5b_audio_state *audio;
	size_t size;
	uint8_t *buf, *ptr;
	int rc;

	audio = emu->sunsoft5b_audio;

	size = pack_state(audio, sunsoft5b_audio_state_items, NULL);
	size += 3 * pack_state(&audio->tone[0], sunsoft5b_tone_state_items,
			       NULL);
	size += pack_state(&audio->envelope, sunsoft5b_envelope_state_items,
			   NULL);
	size += pack_state(&audio->noise, sunsoft5b_noise_state_items,
			   NULL);

	buf = malloc(size);
	if (!buf)
		return -1;

	ptr = buf;
	ptr += pack_state(audio, sunsoft5b_audio_state_items, ptr);
	ptr += pack_state(&audio->tone[0], sunsoft5b_tone_state_items,
			   ptr);
	ptr += pack_state(&audio->tone[1], sunsoft5b_tone_state_items,
			   ptr);
	ptr += pack_state(&audio->tone[2], sunsoft5b_tone_state_items,
			   ptr);
	ptr += pack_state(&audio->envelope, sunsoft5b_envelope_state_items,
			   ptr);
	ptr += pack_state(&audio->noise, sunsoft5b_noise_state_items,
			   ptr);


	rc = save_state_add_chunk(state, "S5BS", buf, size);
	free(buf);

	return rc;
}

int sunsoft5b_audio_load_state(struct emu *emu, struct save_state *state)
{
	struct sunsoft5b_audio_state *audio;
	size_t size;
	uint8_t *buf;

	audio = emu->sunsoft5b_audio;

	if (save_state_find_chunk(state, "S5BS", &buf, &size) < 0)
		return -1;

	buf += unpack_state(audio, sunsoft5b_audio_state_items, buf);
	buf += unpack_state(&audio->tone[0], sunsoft5b_tone_state_items, buf);
	buf += unpack_state(&audio->tone[1], sunsoft5b_tone_state_items, buf);
	buf += unpack_state(&audio->tone[2], sunsoft5b_tone_state_items, buf);
	buf += unpack_state(&audio->envelope, sunsoft5b_envelope_state_items, buf);
	buf += unpack_state(&audio->noise, sunsoft5b_noise_state_items, buf);

	return 0;
}
