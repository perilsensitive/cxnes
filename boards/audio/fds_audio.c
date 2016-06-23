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

static CPU_WRITE_HANDLER(fds_audio_write_handler);
static CPU_READ_HANDLER(fds_audio_read_handler);

struct volume_unit {
	uint32_t timestamp;
	int enabled;
	int increase;
	int speed;
	int gain;
	int period;
	int counter;
	int last_amplitude;
};

struct sweep_unit {
	uint32_t timestamp;
	int enabled;
	int increase;
	int speed;
	int gain;
	int period;
	int counter;
};

struct modulator_unit {
	uint32_t timestamp;
	uint8_t table[64];
	int accumulator;
	int pitch;
	int step;
	int enabled;
	int sweep_bias;
};

struct wave_unit {
	uint32_t timestamp;
	uint8_t table[64];
	int accumulator;
	int pitch;
	int step;
	int enabled;
	int writable;
	int master_volume;
	int unmodulated_pitch;
	uint8_t last_value;
};

struct fds_audio_state {
	struct volume_unit volume;
	struct sweep_unit sweep;
	struct modulator_unit modulator;
	struct wave_unit wave;
	int envelopes_enabled;
	int envelopes_speed;
	int enabled;

	struct emu *emu;
};

static struct state_item fds_volume_unit_state_items[] = {
	STATE_32BIT(volume_unit, timestamp),
	STATE_8BIT(volume_unit, enabled), /* BOOLEAN */
	STATE_8BIT(volume_unit, increase), /* BOOLEAN */
	STATE_8BIT(volume_unit, speed),
	STATE_8BIT(volume_unit, gain),
	STATE_32BIT(volume_unit, period),
	STATE_32BIT(volume_unit, counter),
};

static struct state_item fds_sweep_unit_state_items[] = {
	STATE_32BIT(sweep_unit, timestamp),
	STATE_8BIT(sweep_unit, enabled), /* BOOLEAN */
	STATE_8BIT(sweep_unit, increase), /* BOOLEAN */
	STATE_8BIT(sweep_unit, speed),
	STATE_8BIT(sweep_unit, gain),
	STATE_32BIT(sweep_unit, period),
	STATE_32BIT(sweep_unit, counter),
};

static struct state_item fds_modulator_unit_state_items[] = {
	STATE_32BIT(modulator_unit, timestamp),
	STATE_8BIT_ARRAY(modulator_unit, table),
	STATE_16BIT(modulator_unit, accumulator),
	STATE_16BIT(modulator_unit, pitch),
	STATE_8BIT(modulator_unit, step),
	STATE_8BIT(modulator_unit, enabled), /* BOOLEAN */
	STATE_8BIT(modulator_unit, sweep_bias),
	STATE_ITEM_END(),
};

static struct state_item fds_wave_unit_state_items[] = {
	STATE_32BIT(wave_unit, timestamp),
	STATE_8BIT_ARRAY(wave_unit, table),
	STATE_16BIT(wave_unit, accumulator),
	STATE_16BIT(wave_unit, unmodulated_pitch),
	STATE_16BIT(wave_unit, pitch),
	STATE_8BIT(wave_unit, step),
	STATE_8BIT(wave_unit, enabled), /* BOOLEAN */
	STATE_8BIT(wave_unit, writable), /* BOOLEAN */
	STATE_8BIT(wave_unit, master_volume),
	STATE_8BIT(wave_unit, last_value),
	STATE_ITEM_END(),
};

static struct state_item fds_audio_state_items[] = {
	STATE_8BIT(fds_audio_state, envelopes_enabled),
	STATE_8BIT(fds_audio_state, envelopes_speed),
	STATE_8BIT(fds_audio_state, enabled), /* BOOLEAN */
	STATE_ITEM_END(),
};

int fds_audio_init(struct emu *emu);
void fds_audio_reset(struct fds_audio_state *audio, int);
void fds_audio_end_frame(struct fds_audio_state *audio, uint32_t cycles);

static void update_mod(struct fds_audio_state *audio, uint32_t cycles);
static void sweep_run(struct fds_audio_state *audio, uint32_t cycles);
static void volume_run(struct fds_audio_state *audio, uint32_t cycles);
static void wave_run(struct fds_audio_state *audio, uint32_t cycles);
static void modulator_run(struct fds_audio_state *audio, uint32_t cycles);
static void update_amplitude(struct fds_audio_state *audio, uint32_t cycles);

void fds_audio_run(struct fds_audio_state *audio, uint32_t cycles)
{
	if (audio->emu->overclocking)
		return;

	modulator_run(audio, cycles);
}

void fds_audio_enable(struct fds_audio_state *audio, uint32_t cycles,
		      int enabled)
{
	fds_audio_run(audio, cycles);
	audio->enabled = enabled;
}

static CPU_WRITE_HANDLER(fds_audio_write_handler)
{
	struct fds_audio_state *audio;

	audio = emu->fds_audio;
	fds_audio_run(audio, cycles);

	if (!audio->enabled)
		return;

	if (addr >= 0x4040 && addr < 0x4080) {
		if (audio->wave.writable)
			audio->wave.table[addr - 0x4040] = value & 0x3f;

		return;
	}

	switch (addr) {
	case 0x4080:
		audio->volume.increase = value & 0x40;
		audio->volume.enabled = !(value & 0x80);
		audio->volume.speed = value & 0x3f;
		if (!audio->volume.enabled)
			audio->volume.gain = value & 0x3f;
		audio->volume.counter = 0;
		audio->volume.period = (audio->volume.speed + 1) *
			8 * audio->envelopes_speed;
		update_amplitude(audio, cycles);
		break;
	case 0x4082:
		audio->wave.unmodulated_pitch &= 0xff00;
		audio->wave.unmodulated_pitch |= value;
		update_mod(audio, cycles);
		break;
	case 0x4083:
		audio->wave.unmodulated_pitch &= 0x00ff;
		audio->wave.unmodulated_pitch |= (value & 0x0f) << 8;
		audio->envelopes_enabled = !(value & 0x40);
		if (!audio->envelopes_enabled) {
			audio->volume.counter = 0;
			audio->sweep.counter = 0;
		}
		if (value & 0x80) {
			audio->wave.step = 0;
			audio->wave.accumulator = 0;
		}
		audio->wave.enabled = !(value & 0x80);

		update_amplitude(audio, cycles);
		update_mod(audio, cycles);
		break;
	case 0x4084:
		audio->sweep.increase = value & 0x40;
		audio->sweep.enabled = !(value & 0x80);
		audio->sweep.speed = value & 0x3f;
		if (!audio->sweep.enabled)
			audio->sweep.gain = value & 0x3f;
		audio->sweep.counter = 0;
		audio->sweep.period = (audio->sweep.speed + 1) *
			8 * audio->envelopes_speed;
		update_mod(audio, cycles);
		break;
	case 0x4085:
		audio->modulator.sweep_bias = value & 0x7f;
		if (audio->modulator.sweep_bias >= 0x40)
			audio->modulator.sweep_bias -= 127;
		update_mod(audio, cycles);
		break;
	case 0x4086:
		audio->modulator.pitch &= 0xff00;
		audio->modulator.pitch |= value;
		update_mod(audio, cycles);
		break;
	case 0x4087:
		audio->modulator.pitch &= 0x00ff;
		audio->modulator.pitch |= (value & 0x0f) << 8;
		audio->modulator.enabled = !(value & 0x80);
		if ((value & 0x80)) {
			audio->modulator.accumulator = 0;
		}
		update_mod(audio, cycles);
		break;
	case 0x4088:
		if (!audio->modulator.enabled) {
			int index;

			value &= 0x07;
			index = audio->modulator.step;
			
			audio->modulator.table[index] = value;
			index = (index + 1) & 0x3f;
			audio->modulator.table[index] = value;
			index = (index + 1) & 0x3f;
			audio->modulator.step = index;
		}
		break;
	case 0x4089:
		audio->wave.writable = value & 0x80;
		audio->wave.last_value = audio->wave.table[audio->wave.step];
		audio->wave.master_volume = value & 0x03;
		update_amplitude(audio, cycles);
		break;
	case 0x408a:
		audio->envelopes_speed = value;
		audio->volume.counter = 0;
		audio->sweep.counter = 0;
		audio->volume.period = (audio->volume.speed + 1) *
			8 * value;
		audio->sweep.period = (audio->sweep.speed + 1) *
			8 * value;
		break;
	}
}

static CPU_READ_HANDLER(fds_audio_read_handler)
{
	uint8_t data;
	struct fds_audio_state *audio;

	audio = emu->fds_audio;

	data = value;
	fds_audio_run(audio, cycles);

	if (!audio->enabled)
		return 0;

	if (addr >= 0x4040 && addr < 0x4080) {
		data = audio->wave.table[addr - 0x4040];
	} else {
		switch (addr) {
		case 0x4090:
			data = audio->volume.gain;
			break;
		case 0x4092:
			data = audio->sweep.gain;
			break;
		}
	}

	return (value & 0xc0) | data;
}

int fds_audio_init(struct emu *emu)
{
	struct fds_audio_state *audio;

	audio = malloc(sizeof(*audio));
	if (!audio)
		return 1;

	memset(audio, 0, sizeof(*audio));

	audio->emu = emu;
	emu->fds_audio = audio;

	return 0;
}

void fds_audio_cleanup(struct emu *emu)
{
	if (emu->fds_audio)
		free(emu->fds_audio);

	emu->fds_audio = NULL;
}

void fds_audio_reset(struct fds_audio_state *audio, int hard)
{
	audio->modulator.timestamp = 0;
	audio->wave.timestamp = 0;
	audio->sweep.timestamp = 0;
	audio->volume.timestamp = 0;

	if (hard) {
		memset(&audio->volume, 0, sizeof(audio->volume));
		memset(&audio->sweep, 0, sizeof(audio->sweep));
		memset(&audio->modulator, 0, sizeof(audio->modulator));
		memset(&audio->wave, 0, sizeof(audio->wave));
		audio->envelopes_enabled = 0;
		audio->envelopes_speed = 0;

		if (board_get_type(audio->emu->board) == BOARD_TYPE_NSF)
			audio->enabled = 1;
	}

	/* FIXME Should anything happen on a soft reset? */
}

void fds_audio_end_frame(struct fds_audio_state *audio, uint32_t cycles)
{
	audio->modulator.timestamp -= cycles;
	audio->wave.timestamp -= cycles;
	audio->sweep.timestamp -= cycles;
	audio->volume.timestamp -= cycles;
}

void fds_audio_install_handlers(struct emu *emu, int multi_chip_nsf)
{
	/* FDS maps easily into a multi-chip setup, so no hacks required */
	(void)multi_chip_nsf;

	cpu_set_read_handler(emu->cpu, 0x4040, 64, 0, fds_audio_read_handler);
	cpu_set_write_handler(emu->cpu, 0x4040, 64, 0, fds_audio_write_handler);
	cpu_set_write_handler(emu->cpu, 0x4080, 1, 0, fds_audio_write_handler);
	cpu_set_write_handler(emu->cpu, 0x4082, 9, 0, fds_audio_write_handler);
	cpu_set_read_handler(emu->cpu, 0x4090, 1, 0, fds_audio_read_handler);
	cpu_set_read_handler(emu->cpu, 0x4092, 1, 0, fds_audio_read_handler);
}


static void update_amplitude(struct fds_audio_state *audio, uint32_t cycles)
{
	int amp;
	int delta;
	const double max_output = 63.0 * 32.0;
	const double master_volume = 2.4 * 9752.0;
	const int master_vol[4] = {
		(master_volume / max_output) * 256.0 * 2.0 / 2.0,
		(master_volume / max_output) * 256.0 * 2.0 / 3.0,
		(master_volume / max_output) * 256.0 * 2.0 / 4.0,
		(master_volume / max_output) * 256.0 * 2.0 / 5.0,
	};

	if (audio->volume.gain) {
		amp = audio->volume.gain;

		if (amp > 32)
			amp = 32;

		amp *= master_vol[audio->wave.master_volume];

		if (audio->wave.writable)
			amp *= audio->wave.last_value;
		else
			amp *= audio->wave.table[audio->wave.step];

		amp *= audio->emu->config->fds_volume;
		amp /= 100;

		amp >>= 8;
	} else {
		amp = 0;
	}

	delta = amp - audio->volume.last_amplitude;
	if (delta) {
		audio_add_delta(cycles, delta);
		audio->volume.last_amplitude = amp;
	}
}

static void update_mod(struct fds_audio_state *audio, uint32_t cycles)
{
	struct sweep_unit *sweep;
	struct modulator_unit *modulator;
	struct wave_unit *wave;
	int tmp;
	int remainder;

	sweep = &audio->sweep;
	modulator = &audio->modulator;
	wave = &audio->wave;

	tmp = modulator->sweep_bias * sweep->gain;
	remainder = tmp & 0x0f;
	tmp >>= 4;

	if (remainder && !(tmp & 0x80)) {
		if (modulator->sweep_bias < 0)
			tmp--;
		else
			tmp += 2;
	}

	if (tmp >= 192)
		tmp -= 256;
	else if (tmp < -64)
		tmp += 256;

	tmp = wave->unmodulated_pitch * tmp;
	remainder = tmp & 0x3f;
	tmp >>= 6;
	if (remainder >= 32)
		tmp++;

	wave->pitch = wave->unmodulated_pitch + tmp;
}

static void sweep_run(struct fds_audio_state *audio, uint32_t cycles)
{
	struct sweep_unit *sweep;
	int remaining;

	sweep = &audio->sweep;

	if (!audio->wave.enabled || !audio->envelopes_enabled || !sweep->enabled ||
	    !sweep->period) {
		sweep->timestamp = cycles;
		wave_run(audio, cycles);
		return;
	}

	remaining = (cycles - sweep->timestamp) / audio->emu->apu_clock_divider;;

	while (remaining) {
		int ticks_to_next_clock;
		int ticks;

		ticks_to_next_clock = sweep->period - sweep->counter;

		ticks = ticks_to_next_clock;
		if (ticks > remaining)
			ticks = remaining;

		sweep->timestamp += ticks * audio->emu->apu_clock_divider;;
		
		sweep->counter += ticks;
		wave_run(audio, sweep->timestamp);
		if (sweep->counter == sweep->period) {
			sweep->counter = 0;

			if (sweep->increase && sweep->gain < 32)
				sweep->gain++;
			else if (!sweep->increase && sweep->gain > 0)
				sweep->gain--;

			update_mod(audio, sweep->timestamp);
		}

		remaining -= ticks;
	}
}

static void volume_run(struct fds_audio_state *audio, uint32_t cycles)
{
	struct volume_unit *volume;
	int remaining;

	volume = &audio->volume;

	if (!audio->wave.enabled || !audio->envelopes_enabled || !volume->enabled ||
	    !volume->period) {
		volume->timestamp = cycles;
		/* FIXME update amplitude ? */
		return;
	}

	remaining = (cycles - volume->timestamp) / audio->emu->apu_clock_divider;;

	while (remaining) {
		int ticks_to_next_clock;
		int ticks;

		ticks_to_next_clock = volume->period - volume->counter;

		ticks = ticks_to_next_clock;
		if (ticks > remaining)
			ticks = remaining;

		volume->timestamp += ticks * audio->emu->apu_clock_divider;;
		
		volume->counter += ticks;
		if (volume->counter == volume->period) {
			volume->counter = 0;

			if (volume->increase && volume->gain < 32)
				volume->gain++;
			else if (!volume->increase && volume->gain > 0)
				volume->gain--;
			
			update_amplitude(audio, volume->timestamp);
		}

		remaining -= ticks;
	}
}

static void wave_run(struct fds_audio_state *audio, uint32_t cycles)
{
	struct wave_unit *wave;
	int remaining;

	wave = &audio->wave;

	if (!wave->enabled || !wave->pitch) {
		wave->timestamp = cycles;
		volume_run(audio, cycles);
		return;
	}

	remaining = (cycles - wave->timestamp) /
		audio->emu->apu_clock_divider;;

	while (remaining) {
		int acc_remaining;
		int ticks_to_next_clock;
		int clocks;

		acc_remaining = 65536 - wave->accumulator;
		ticks_to_next_clock = acc_remaining / wave->pitch;
		if (acc_remaining % wave->pitch)
			ticks_to_next_clock++;

		clocks = ticks_to_next_clock;
		if (clocks > remaining)
			clocks = remaining;
			
		wave->accumulator += clocks * wave->pitch;
		wave->timestamp += clocks * audio->emu->apu_clock_divider;;
		remaining -= clocks;
		volume_run(audio, wave->timestamp);
		if (wave->accumulator >= 65536) {
			wave->accumulator &= 0xffff;

			wave->step = (wave->step + 1) & 0x3f;

			update_amplitude(audio, wave->timestamp);
		}
	}
}

static void modulator_run(struct fds_audio_state *audio, uint32_t cycles)
{
	struct modulator_unit *modulator;
	int remaining;
	int8_t adjust[] = { 0, 1, 2, 4, 0, -4, -2, -1 };

	modulator = &audio->modulator;

	if (!modulator->enabled || !modulator->pitch) {
		modulator->timestamp = cycles;
		sweep_run(audio, cycles);
		return;
	}

	remaining = (cycles - modulator->timestamp) /
		audio->emu->apu_clock_divider;;

	while (remaining) {
		int acc_remaining;
		int ticks_to_next_clock;
		int clocks;

		acc_remaining = 65536 - modulator->accumulator;
		ticks_to_next_clock = acc_remaining / modulator->pitch;
		if (acc_remaining % modulator->pitch)
			ticks_to_next_clock++;

		clocks = ticks_to_next_clock;
		if (clocks > remaining)
			clocks = remaining;
			
		modulator->accumulator += clocks * modulator->pitch;
		modulator->timestamp += clocks * audio->emu->apu_clock_divider;;
		remaining -= clocks;
		sweep_run(audio, modulator->timestamp);
		if (modulator->accumulator >= 65536) {
			int tmp;
			modulator->accumulator &= 0xffff;

			tmp = modulator->table[modulator->step];
			modulator->step = (modulator->step + 1) & 0x3f;

			if (tmp == 4)
				modulator->sweep_bias = 0;
			else
				modulator->sweep_bias += adjust[tmp];

			if (modulator->sweep_bias > 63)
				modulator->sweep_bias -= 128;
			else if (modulator->sweep_bias < -64)
				modulator->sweep_bias += 128;

			update_mod(audio, modulator->timestamp);
		}
	}
}

int fds_audio_save_state(struct emu *emu, struct save_state *state)
{
	struct fds_audio_state *audio;
	size_t size;
	uint8_t *buf, *ptr;
	int rc;

	audio = emu->fds_audio;

	size = pack_state(audio, fds_audio_state_items, NULL);
	size += pack_state(&audio->volume, fds_volume_unit_state_items, NULL);
	size += pack_state(&audio->sweep, fds_sweep_unit_state_items, NULL);
	size += pack_state(&audio->wave, fds_wave_unit_state_items, NULL);
	size += pack_state(&audio->modulator, fds_modulator_unit_state_items,
			   NULL);

	buf = malloc(size);
	if (!buf)
		return -1;

	ptr = buf;
	ptr += pack_state(audio, fds_audio_state_items, ptr);
	ptr += pack_state(&audio->volume, fds_volume_unit_state_items, ptr);
	ptr += pack_state(&audio->sweep, fds_sweep_unit_state_items, ptr);
	ptr += pack_state(&audio->wave, fds_wave_unit_state_items, ptr);
	ptr += pack_state(&audio->modulator, fds_modulator_unit_state_items,
			  ptr);

	rc = save_state_add_chunk(state, "FDSS", buf, size);
	free(buf);

	return rc;
}

int fds_audio_load_state(struct emu *emu, struct save_state *state)
{
	struct fds_audio_state *audio;
	size_t size;
	uint8_t *buf;

	audio = emu->fds_audio;

	if (save_state_find_chunk(state, "FDSS", &buf, &size) < 0)
		return -1;

	buf += unpack_state(audio, fds_audio_state_items, buf);
	buf += unpack_state(&audio->volume, fds_volume_unit_state_items, buf);
	buf += unpack_state(&audio->sweep, fds_sweep_unit_state_items, buf);
	buf += unpack_state(&audio->wave, fds_wave_unit_state_items, buf);
	buf += unpack_state(&audio->modulator, fds_modulator_unit_state_items,
			    buf);

	return 0;
}
