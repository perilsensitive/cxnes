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

#define active_channels() (((audio->ram[0x7f] >> 4) & 0x07))

#define channel_length(x) (256 - (audio->ram[(x << 3) | 0x44] & 0xfc))
#define channel_volume(x) (audio->ram[(x << 3) | 0x47] & 0x0f)
#define channel_frequency(x) (((audio->ram[(x << 3) | 0x40]) |	   \
	                       (audio->ram[(x << 3) | 0x42] << 8) | \
			       (audio->ram[(x << 3) | 0x44] << 16)) & 0x3ffff)

#define channel_phase(x) (((audio->ram[(x << 3) | 0x41]) |	   \
	                   (audio->ram[(x << 3) | 0x43] << 8) | \
		           (audio->ram[(x << 3) | 0x45] << 16)) & 0xffffff)

static const int gain = 6.0 * 9752.0 /	(15.0 * 15.0 * 256.0) * 256.0;
static const int mixer_adjust[9] = { 256, 256, 128, 256 / 3, 64, 256 / 5,
				     256 / 6, 256 / 6, 256 / 6 };

struct namco163_audio_state {
	uint32_t timestamp;
	int current_address;
	int auto_increment;
	int enabled;
	uint8_t *ram;
	int last_amp[8];
	int next_channel;
	struct emu *emu;
};

static struct state_item namco163_audio_state_items[] = {
	STATE_32BIT(namco163_audio_state, timestamp),
	STATE_8BIT(namco163_audio_state, current_address),
	STATE_8BIT(namco163_audio_state, auto_increment), /* BOOLEAN */
	STATE_8BIT(namco163_audio_state, enabled), /* BOOLEAN */
	STATE_8BIT(namco163_audio_state, next_channel),
};

static void set_channel_phase(struct namco163_audio_state *audio, int channel,
			      int phase)
{
	audio->ram[(channel << 3) | 0x41] = phase & 0xff;
	audio->ram[(channel << 3) | 0x43] = (phase >>  8) & 0xff;
	audio->ram[(channel << 3) | 0x45] = (phase >> 16) & 0xff;
}

static int channel_amp(struct namco163_audio_state *audio, int channel, int phase)
{
	int volume;
	int addr;
	int amp;
	int enabled;

	enabled = active_channels();
	volume = channel_volume(channel);
	addr = audio->ram[0x46 | (channel << 3)];

	addr += phase >> 16;
	addr &= 0xff;

	amp = audio->ram[addr >> 1];
	if (addr & 1)
		amp >>= 4;


	amp &= 0x0f;
	amp = 8 - amp;
	amp *= volume;
	amp *= mixer_adjust[enabled + 1];
	amp *= audio->emu->config->namco163_channel_volume[channel];
	amp *= gain;
	amp /= 100 * 256;

	return amp;
}

void namco163_audio_run(struct namco163_audio_state *audio,
		       uint32_t cycles)
{
	uint32_t clocks_elapsed;
	int channel;
	int min_channel;
	int length;
	int multiplier;

	if (audio->emu->overclocking)
		return;

	if (!audio->enabled) {
		audio->timestamp = cycles;
		return;
	}

	clocks_elapsed = (cycles - audio->timestamp) /
		(audio->emu->apu_clock_divider * 15);

	channel = audio->next_channel;
	min_channel = 7 - active_channels();

	if (channel < min_channel)
		channel = 7;

	multiplier = 15 * audio->emu->apu_clock_divider;

	while (clocks_elapsed) {
		int old_phase, phase;
		int frequency;
		int tmp;
		int amp, delta;

		old_phase = channel_phase(channel);
		length = channel_length(channel);
		frequency = channel_frequency(channel);
		phase = (old_phase + frequency) & 0x00ffffff;
		tmp = length << 16;

		while (phase >= tmp)
			phase -= tmp;

		set_channel_phase(audio, channel, phase);
		audio->timestamp += multiplier;
		amp = channel_amp(audio, channel, phase);
		delta = amp - audio->last_amp[channel];

		if (delta) {
			audio_add_delta(audio->timestamp, delta);
			audio->last_amp[channel] = amp;
		}

		clocks_elapsed--;
		channel--;
		if (channel < min_channel)
			channel = 7;
	}

	audio->next_channel = channel;
}

CPU_WRITE_HANDLER(namco163_audio_write_handler)
{
	struct namco163_audio_state *audio;

	audio = emu->namco163_audio;

	namco163_audio_run(audio, cycles);

	if (addr >= 0xf800) {
		audio->current_address = value & 0x7f;
		audio->auto_increment = value & 0x80;
	} else if ((addr >= 0xe000) && (addr < 0xe800)) {
		audio->enabled = !(value & 0x40);
	} else if (addr >= 0x4800) {
		/* we've already caught up by this point */
		audio->ram[audio->current_address] = value;
		if (audio->auto_increment) {
			audio->current_address =
			    (audio->current_address + 1) & 0x7f;
		}
	}
}

CPU_READ_HANDLER(namco163_audio_read_handler)
{
	struct namco163_audio_state *audio;

	audio = emu->namco163_audio;

	namco163_audio_run(audio, cycles);

	if (addr >= 0x4800) {
		value = audio->ram[audio->current_address];
		if (audio->auto_increment) {
			audio->current_address =
			    (audio->current_address + 1) & 0x7f;
		}
	}

	return value;
}

int namco163_audio_init(struct emu *emu)
{
	struct namco163_audio_state *audio;

	audio = malloc(sizeof(*audio));
	if (!audio)
		return 1;

	memset(audio, 0, sizeof(*audio));

	audio->emu = emu;
	emu->namco163_audio = audio;

	return 0;
}

void namco163_audio_cleanup(struct emu *emu)
{
	if (emu->namco163_audio)
		free(emu->namco163_audio);

	emu->namco163_audio = NULL;
}

void namco163_audio_reset(struct namco163_audio_state *audio, int hard)
{
	size_t size;

	/* FIXME is there a difference between hard and soft reset? */
	if (hard) {
		board_get_mapper_ram(audio->emu->board, &audio->ram, &size);

		/* Hack for NSF: if the NSF uses both MMC5 and N163
		   then the mapper RAM will have an extra 128 bytes.
		   Always use the last 128 bytes in this case.
		*/
		if (size > 128) {
			audio->ram += (size - 128);
			size = 128;
		}

		memset(audio->ram, 0, size);
		audio->current_address = 0;
		audio->auto_increment = 0;
		audio->enabled = 1;
		audio->next_channel = 7;
	}

	audio->timestamp = 0;
}

void namco163_audio_end_frame(struct namco163_audio_state *audio, uint32_t cycles)
{
	audio->timestamp -= cycles;
}

void namco163_audio_install_handlers(struct emu *emu, int multi_chip_nsf)
{
	cpu_set_write_handler(emu->cpu, 0xf800, 1, 0,
			      namco163_audio_write_handler);
	cpu_set_write_handler(emu->cpu, 0x4800, 1, 0,
			      namco163_audio_write_handler);
	cpu_set_read_handler(emu->cpu, 0x4800, 1, 0,
			     namco163_audio_read_handler);
}

int namco163_audio_save_state(struct emu *emu, struct save_state *state)
{
	struct namco163_audio_state *audio;
	size_t size;
	uint8_t *buf, *ptr;
	int rc;

	audio = emu->namco163_audio;

	size = pack_state(audio, namco163_audio_state_items, NULL);

	buf = malloc(size);
	if (!buf)
		return -1;

	ptr = buf;
	ptr += pack_state(audio, namco163_audio_state_items, ptr);

	rc = save_state_add_chunk(state, "N163", buf, size);
	free(buf);

	return rc;
}

int namco163_audio_load_state(struct emu *emu, struct save_state *state)
{
	size_t size;
	uint8_t *buf;

	if (save_state_find_chunk(state, "N163", &buf, &size) < 0)
		return -1;

	return 0;
}
