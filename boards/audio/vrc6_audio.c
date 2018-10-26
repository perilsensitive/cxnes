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

struct vrc6_pulse {
	int counter;
	int period;
	int duty;
	int step;
	int volume;
	int mode;
	int enabled;
	int amplitude;
};

struct vrc6_sawtooth {
	int counter;
	int period;
	int rate;
	int step;
	int accumulator;
	int enabled;
	int amplitude;
};

struct vrc6_audio_state {
	struct vrc6_pulse pulse[2];
	struct vrc6_sawtooth sawtooth;
	uint32_t timestamp;
	int halt;
	int period_shift;
	int swap_lines;
	int last_amplitude;
	struct emu *emu;
};

static struct state_item vrc6_audio_state_items[] = {
	STATE_32BIT(vrc6_audio_state, timestamp),
	STATE_8BIT(vrc6_audio_state, halt), /* BOOLEAN */
	STATE_8BIT(vrc6_audio_state, period_shift),
	STATE_ITEM_END(),
};

static struct state_item vrc6_pulse_state_items[] = {
	STATE_32BIT(vrc6_pulse, counter),
	STATE_16BIT(vrc6_pulse, period),
	STATE_8BIT(vrc6_pulse, duty),
	STATE_8BIT(vrc6_pulse, step),
	STATE_8BIT(vrc6_pulse, volume),
	STATE_8BIT(vrc6_pulse, mode),
	STATE_8BIT(vrc6_pulse, enabled), /* BOOLEAN */
	STATE_ITEM_END(),
};

static struct state_item vrc6_sawtooth_state_items[] = {
	STATE_32BIT(vrc6_sawtooth, counter),
	STATE_16BIT(vrc6_sawtooth, period),
	STATE_8BIT(vrc6_sawtooth, rate),
	STATE_8BIT(vrc6_sawtooth, step),
	STATE_8BIT(vrc6_sawtooth, accumulator),
	STATE_8BIT(vrc6_sawtooth, enabled), /* BOOLEAN */
	STATE_ITEM_END(),
};

static CPU_WRITE_HANDLER(vrc6_audio_write_handler);
void vrc6_audio_run(struct vrc6_audio_state *audio, uint32_t cycles);

int vrc6_audio_init(struct emu *emu);
void vrc6_audio_cleanup(struct emu *emu);
void vrc6_audio_reset(struct vrc6_audio_state *, int);
void vrc6_audio_end_frame(struct vrc6_audio_state *, uint32_t cycles);

int vrc6_audio_init(struct emu *emu)
{
	struct vrc6_audio_state *audio;

	audio = malloc(sizeof(*audio));
	if (!audio)
		return 1;

	memset(audio, 0, sizeof(*audio));
	audio->emu = emu;
	emu->vrc6_audio = audio;

	if (board_get_type(emu->board) == BOARD_TYPE_VRC6B)
		audio->swap_lines = 1;

	return 0;
}

void vrc6_audio_reset(struct vrc6_audio_state *audio, int hard)
{
	if (hard) {
		memset(&audio->pulse[0], 0, sizeof(audio->pulse[0]));
		memset(&audio->pulse[1], 0, sizeof(audio->pulse[1]));
		memset(&audio->sawtooth, 0, sizeof(audio->sawtooth));
		audio->halt = 0;
		audio->period_shift = 0;
	}

	audio->timestamp = 0;
}

void vrc6_audio_end_frame(struct vrc6_audio_state *audio, uint32_t cycles)
{
	audio->timestamp -= cycles;
}

static void update_amplitude(struct vrc6_audio_state *audio, uint32_t cycles)
{
	int amplitude;
	const unsigned int master = 9752 * 256.0 / 15.0;

	amplitude  = audio->pulse[0].amplitude;
	amplitude += audio->pulse[1].amplitude;
	amplitude += audio->sawtooth.amplitude;

	amplitude *= master;
	amplitude >>= 8;

	if (amplitude != audio->last_amplitude) {
		int delta = amplitude - audio->last_amplitude;
		audio_add_delta(audio->emu->audio,
		                audio->timestamp, delta);
		audio->last_amplitude = amplitude;
	}
}

static void pulse_enable(struct vrc6_audio_state *audio, int channel,
			 int enabled, uint32_t cycles)
{
	struct vrc6_pulse *pulse;
	int period_cycles;

	pulse = &audio->pulse[channel];

	if (pulse->enabled == enabled)
		return;

	period_cycles = ((pulse->period >> audio->period_shift) + 1) *
		audio->emu->apu_clock_divider;

	if (enabled) {
		pulse->counter = period_cycles;
	} else {
		pulse->step = 15;
		pulse->amplitude = 0;

		update_amplitude(audio, cycles);
	}

	pulse->enabled = enabled;
}

static void sawtooth_enable(struct vrc6_audio_state *audio, int enabled,
			    uint32_t cycles)
{
	struct vrc6_sawtooth *sawtooth;

	sawtooth = &audio->sawtooth;

	if (sawtooth->enabled == enabled)
		return;

	if (!enabled) {
		sawtooth->step = 0;
		sawtooth->amplitude = 0;
		sawtooth->accumulator = 0;

		update_amplitude(audio, cycles);
	}

	sawtooth->enabled = enabled;
}

static CPU_WRITE_HANDLER(vrc6_audio_write_handler)
{
	struct vrc6_audio_state *audio;

	audio = emu->vrc6_audio;

	addr &= 0xf003;

	if (audio->swap_lines) {
		int tmp;

		tmp = addr & 3;
		addr = (addr & 0xfffc) | (tmp >> 1) | ((tmp << 1) & 0x02);
	}

	vrc6_audio_run(audio, cycles);

	switch (addr) {
	case 0x9000:
		audio->pulse[0].volume = value & 0x0f;
		audio->pulse[0].duty = (value & 0x70) >> 4;
		audio->pulse[0].mode = value & 0x80;
		break;
	case 0x9001:
		audio->pulse[0].period &= 0xff00;
		audio->pulse[0].period |= value;
		break;
	case 0x9002:
		audio->pulse[0].period &= 0x00ff;
		audio->pulse[0].period |= ((value & 0x0f) << 8);
		pulse_enable(audio, 0, value & 0x80, cycles);
		break;
	case 0x9003:
		audio->halt = value & 0x01;
		audio->period_shift = (value & 0x06) << 1;
		if (audio->period_shift == 0x0c)
			audio->period_shift = 0x08;
		break;
	case 0xa000:
		audio->pulse[1].volume = value & 0x0f;
		audio->pulse[1].duty = (value & 0x70) >> 4;
		audio->pulse[1].mode = value & 0x80;
		break;
	case 0xa001:
		audio->pulse[1].period &= 0xff00;
		audio->pulse[1].period |= value;
		break;
	case 0xa002:
		audio->pulse[1].period &= 0x00ff;
		audio->pulse[1].period |= ((value & 0x0f) << 8);
		pulse_enable(audio, 1, value & 0x80, cycles);
		break;
	case 0xb000:
		audio->sawtooth.rate = value & 0x3f;
		break;
	case 0xb001:
		audio->sawtooth.period &= 0xff00;
		audio->sawtooth.period |= value;
		break;
	case 0xb002:
		audio->sawtooth.period &= 0x00ff;
		audio->sawtooth.period |= ((value & 0x0f) << 8);
		sawtooth_enable(audio, value & 0x80, cycles);
		break;
	}
}

static void pulse_run(struct vrc6_audio_state *audio, int channel,
		      int clocks)
{
	struct vrc6_pulse *pulse;
	struct config *config;
	int volume;
	int period_cycles;
	int amplitude;

	config = audio->emu->config;

	pulse = &audio->pulse[channel];

	if (channel)
		volume = config->vrc6_pulse1_volume;
	else
		volume = config->vrc6_pulse0_volume;

	volume *= pulse->volume;

	period_cycles = ((pulse->period >> audio->period_shift) + 1) *
		audio->emu->apu_clock_divider;

	if (clocks < pulse->counter) {
		pulse->counter -= clocks;
		return;
	}

	pulse->counter = period_cycles;

	if (pulse->mode || (pulse->step <= pulse->duty))
		amplitude = volume / 100;
	else
		amplitude = 0;

	pulse->amplitude = -amplitude;

	pulse->step--;
	if (pulse->step < 0)
		pulse->step = 15;
}

static void sawtooth_run(struct vrc6_audio_state *audio, int clocks)
{
	struct vrc6_sawtooth *sawtooth;
	struct config *config;
	int period_cycles;
	int amplitude;

	config = audio->emu->config;

	sawtooth = &audio->sawtooth;

	if (clocks < sawtooth->counter) {
		sawtooth->counter -= clocks;
		return;
	}

	period_cycles = ((sawtooth->period >> audio->period_shift) + 1) *
		audio->emu->apu_clock_divider;

	sawtooth->counter = period_cycles;

	if (!(sawtooth->step & 1)) {
		if (sawtooth->step == 0) {
			sawtooth->accumulator = 0;
		} else {
			sawtooth->accumulator =
				(sawtooth->accumulator +
				 sawtooth->rate) & 0xff;
		}

		amplitude = (sawtooth->accumulator >> 3) & 0x1f;
		amplitude = amplitude * config->vrc6_sawtooth_volume / 100;
		sawtooth->amplitude = -amplitude;
	}

	sawtooth->step = (sawtooth->step + 1) % 14;
}

void vrc6_audio_run(struct vrc6_audio_state *audio, uint32_t cycles)
{
	int elapsed;

	elapsed = cycles - audio->timestamp;

	if (audio->emu->overclocking)
		return;

	if (audio->halt) {
		audio->timestamp = cycles;
		return;
	}

	while (elapsed) {
		int clocks = -1;

		if (audio->pulse[0].enabled)
			clocks = audio->pulse[0].counter;

		if (audio->pulse[1].enabled &&
		    ((clocks < 0) || (audio->pulse[1].counter < clocks))) {
			    clocks = audio->pulse[1].counter;
		}
						
		if (audio->sawtooth.enabled &&
		    ((clocks < 0) || (audio->sawtooth.counter < clocks))) {
			    clocks = audio->sawtooth.counter;
		}

		if (clocks < 0) {
			audio->timestamp = cycles;
			return;
		}

		if (elapsed < clocks) {
			if (audio->pulse[0].enabled)
				audio->pulse[0].counter -= clocks;

			if (audio->pulse[1].enabled)
				audio->pulse[1].counter -= clocks;

			if (audio->sawtooth.enabled)
				audio->sawtooth.counter -= clocks;
			audio->timestamp += clocks;
			break;
		}

		audio->timestamp += clocks;
		elapsed -= clocks;

		if (audio->pulse[0].enabled)
			pulse_run(audio, 0, clocks);

		if (audio->pulse[1].enabled)
			pulse_run(audio, 1, clocks);

		if (audio->sawtooth.enabled)
			sawtooth_run(audio, clocks);

		update_amplitude(audio, audio->timestamp);
	}
}

void vrc6_audio_cleanup(struct emu *emu)
{
	if (emu->vrc6_audio)
		free(emu->vrc6_audio);

	emu->vrc6_audio = NULL;
}

void vrc6_audio_install_handlers(struct emu *emu, int multi_chip_nsf)
{
	if (multi_chip_nsf) {
		cpu_set_write_handler(emu->cpu, 0x9000, 4, 0,
				      vrc6_audio_write_handler);
		cpu_set_write_handler(emu->cpu, 0xa000, 3, 0,
				      vrc6_audio_write_handler);
		cpu_set_write_handler(emu->cpu, 0xb000, 3, 0,
				      vrc6_audio_write_handler);
	} else {
		cpu_set_write_handler(emu->cpu, 0x9000, 0x2003, 0,
				      vrc6_audio_write_handler);
	}

}

int vrc6_audio_save_state(struct emu *emu, struct save_state *state)
{
	struct vrc6_audio_state *audio;
	size_t size;
	uint8_t *buf, *ptr;
	int rc;

	audio = emu->vrc6_audio;

	size = pack_state(audio, vrc6_audio_state_items, NULL);
	size += 2 * pack_state(&audio->pulse[0], vrc6_pulse_state_items, NULL);
	size += pack_state(&audio->sawtooth, vrc6_sawtooth_state_items, NULL);

	buf = malloc(size);
	if (!buf)
		return -1;

	ptr = buf;
	ptr += pack_state(audio, vrc6_audio_state_items, ptr);
	ptr += pack_state(&audio->pulse[0], vrc6_pulse_state_items, ptr);
	ptr += pack_state(&audio->pulse[1], vrc6_pulse_state_items, ptr);
	ptr += pack_state(&audio->sawtooth, vrc6_sawtooth_state_items, ptr);

	rc = save_state_add_chunk(state, "VC6S", buf, size);
	free(buf);

	return rc;
}

int vrc6_audio_load_state(struct emu *emu, struct save_state *state)
{
	struct vrc6_audio_state *audio;
	size_t size;
	uint8_t *buf;

	audio = emu->vrc6_audio;

	if (save_state_find_chunk(state, "VC6S", &buf, &size) < 0)
		return -1;

	buf += unpack_state(audio, vrc6_audio_state_items, buf);
	buf += unpack_state(&audio->pulse[0], vrc6_pulse_state_items, buf);
	buf += unpack_state(&audio->pulse[1], vrc6_pulse_state_items, buf);
	buf += unpack_state(&audio->sawtooth, vrc6_sawtooth_state_items, buf);

	return 0;
}
