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

#include "emu.h"
#include "audio.h"
#include "emu2413.h"

#define DEFAULT_RATE 48000

struct vrc7_audio_state {
	OPLL *opll;
	uint32_t timestamp;
	int last_amplitude;
	int cpu_clock_divider;
	int muted;
	struct emu *emu;
};

static struct state_item vrc7_opll_slot_state_items[] = {
	STATE_32BIT(__OPLL_SLOT, patch.TL),
	STATE_32BIT(__OPLL_SLOT, patch.FB),
	STATE_32BIT(__OPLL_SLOT, patch.EG),
	STATE_32BIT(__OPLL_SLOT, patch.ML),
	STATE_32BIT(__OPLL_SLOT, patch.AR),
	STATE_32BIT(__OPLL_SLOT, patch.DR),
	STATE_32BIT(__OPLL_SLOT, patch.SL),
	STATE_32BIT(__OPLL_SLOT, patch.RR),
	STATE_32BIT(__OPLL_SLOT, patch.KR),
	STATE_32BIT(__OPLL_SLOT, patch.KL),
	STATE_32BIT(__OPLL_SLOT, patch.AM),
	STATE_32BIT(__OPLL_SLOT, patch.PM),
	STATE_32BIT(__OPLL_SLOT, patch.WF),

	STATE_32BIT(__OPLL_SLOT, type),
	STATE_32BIT(__OPLL_SLOT, feedback),
	STATE_32BIT_ARRAY(__OPLL_SLOT, output),

	STATE_32BIT(__OPLL_SLOT, phase),
	/* STATE_32BIT(__OPLL_SLOT, dphase), */
	STATE_32BIT(__OPLL_SLOT, pgout),
	STATE_32BIT(__OPLL_SLOT, fnum),
	STATE_32BIT(__OPLL_SLOT, block),
	STATE_32BIT(__OPLL_SLOT, volume),
	STATE_32BIT(__OPLL_SLOT, sustine),
	/* STATE_32BIT(__OPLL_SLOT, tll), */
	/* STATE_32BIT(__OPLL_SLOT, rks), */
	STATE_32BIT(__OPLL_SLOT, eg_mode),
	STATE_32BIT(__OPLL_SLOT, eg_phase),
	/* STATE_32BIT(__OPLL_SLOT, eg_dphase), */
	STATE_32BIT(__OPLL_SLOT, egout),
	STATE_ITEM_END(),
};

static struct state_item vrc7_opll_state_items[] = {
	STATE_32BIT(__OPLL, adr),
	STATE_32BIT(__OPLL, out),
	STATE_32BIT(__OPLL, realstep),
	STATE_32BIT(__OPLL, oplltime),
	STATE_32BIT(__OPLL, opllstep),
	STATE_32BIT(__OPLL, prev),
	STATE_32BIT(__OPLL, next),
	STATE_8BIT_ARRAY(__OPLL, LowFreq),
	STATE_8BIT_ARRAY(__OPLL, HiFreq),
	STATE_8BIT_ARRAY(__OPLL, InstVol),
	STATE_8BIT_ARRAY(__OPLL, CustInst),
	STATE_32BIT_ARRAY(__OPLL, slot_on_flag),
	STATE_32BIT(__OPLL, pm_phase),
	STATE_32BIT(__OPLL, lfo_pm),
	STATE_32BIT(__OPLL, am_phase),
	STATE_32BIT(__OPLL, lfo_pm),
	STATE_32BIT(__OPLL, quality),
	STATE_32BIT_ARRAY(__OPLL, patch_number),
	STATE_32BIT_ARRAY(__OPLL, key_status),
	STATE_32BIT(__OPLL, mask),
	STATE_ITEM_END(),
};

static struct state_item vrc7_audio_state_items[] = {
	STATE_32BIT(vrc7_audio_state, timestamp),
	STATE_ITEM_END(),
};

CPU_WRITE_HANDLER(vrc7_audio_write_handler);
void vrc7_audio_run(struct vrc7_audio_state *audio, uint32_t cycles);

int vrc7_audio_init(struct emu *emu);
void vrc7_audio_cleanup(struct emu *emu);
void vrc7_audio_reset(struct vrc7_audio_state *, int);
void vrc7_audio_end_frame(struct vrc7_audio_state *, uint32_t cycles);

int vrc7_audio_init(struct emu *emu)
{
	struct vrc7_audio_state *audio;

	audio = malloc(sizeof(*audio));
	if (!audio)
		return 1;

	memset(audio, 0, sizeof(*audio));
	audio->emu = emu;
	emu->vrc7_audio = audio;
	
	audio->opll = OPLL_new(3579545, DEFAULT_RATE);

	OPLL_set_quality(audio->opll, 1);
	OPLL_set_rate(audio->opll, 49716);

	return 0;
}

void vrc7_audio_reset(struct vrc7_audio_state *audio, int hard)
{
	audio->cpu_clock_divider = audio->emu->cpu_clock_divider;

	if (hard) {
		int i;

		for (i = 0; i < 0x40; i++) {
			OPLL_writeIO(audio->opll, 0, i);
			OPLL_writeIO(audio->opll, 1, 0);
		}

		OPLL_reset(audio->opll);
		audio->timestamp = 0;
	}
}

void vrc7_audio_end_frame(struct vrc7_audio_state *audio, uint32_t cycles)
{
	audio->timestamp -= cycles;
}

static int update_amplitude(struct vrc7_audio_state *audio)
{
	int amplitude;
	const int master = 0.8f * 256.0;
	int delta;
	int i;

	amplitude = 0;

	for (i = 0; i < 6; i++) {
		int32_t val = audio->opll->slot[(i<<1)|1].output[1];
		amplitude += val * 128 * audio->emu->config->vrc7_channel_volume[i];
	}

	amplitude = amplitude * master / 100 >> 8;

	delta = amplitude - audio->last_amplitude;
	audio->last_amplitude = amplitude;

	if (audio->muted)
		return 0;
	else
		return delta;
}

CPU_WRITE_HANDLER(vrc7_audio_write_handler)
{
	struct vrc7_audio_state *audio;

	audio = emu->vrc7_audio;

	vrc7_audio_run(audio, cycles);

	if (addr == 0x9010) {
		OPLL_writeIO(audio->opll, 0, value);
	} else if (addr == 0x9030) {
		OPLL_writeIO(audio->opll, 1, value);
	} else if (addr == 0xe000) {
		int muted;
		int delta;

		muted = value & 0x40;

		if (!audio->muted && muted) {
			delta = -audio->last_amplitude;
			audio_add_delta(cycles, delta);
		} else if (audio->muted && !muted) {
			delta = audio->last_amplitude;
			audio_add_delta(cycles, delta);
		}

		audio->muted = muted;
	}
}

void vrc7_audio_run(struct vrc7_audio_state *audio, uint32_t cycles)
{
	int elapsed;
	int timestamp;
	int clocks;

	timestamp = audio->timestamp;
	elapsed = cycles - timestamp;
	clocks = elapsed / (36 * audio->cpu_clock_divider);

	while (clocks > 0) {
		int delta;

		timestamp += 36 * audio->cpu_clock_divider;
		OPLL_calc(audio->opll);
		delta = update_amplitude(audio);
		if (delta)
			audio_add_delta(timestamp, delta);
		clocks--;
	}

	audio->timestamp = timestamp;
}

void vrc7_audio_cleanup(struct emu *emu)
{
	if (emu->vrc7_audio) {
		OPLL_delete(emu->vrc7_audio->opll);
		free(emu->vrc7_audio);
	}

	emu->vrc7_audio = NULL;
}

void vrc7_audio_install_handlers(struct emu *emu, int multi_chip_nsf)
{
	cpu_set_write_handler(emu->cpu, 0x9010, 1, 0,
			      vrc7_audio_write_handler);
	cpu_set_write_handler(emu->cpu, 0x9030, 1, 0,
			      vrc7_audio_write_handler);
}

int vrc7_audio_save_state(struct emu *emu, struct save_state *state)
{

	struct vrc7_audio_state *audio;
	size_t size;
	uint8_t *buf, *ptr;
	int rc;
	int i;

	audio = emu->vrc7_audio;

	size  = pack_state(audio, vrc7_audio_state_items, NULL);
	size += pack_state(&audio->opll, vrc7_opll_state_items, NULL);
	size += 12 * pack_state(&audio->opll->slot[0],
				vrc7_opll_slot_state_items, NULL);

	buf = malloc(size);
	if (!buf)
		return -1;

	ptr = buf;
	ptr += pack_state(audio, vrc7_audio_state_items, ptr);
	ptr += pack_state(audio->opll, vrc7_opll_state_items, ptr);

	for (i = 0; i < 12; i++) {
		ptr += pack_state(&audio->opll->slot[i],
				  vrc7_opll_slot_state_items, ptr);
	}

	rc = save_state_add_chunk(state, "VC7S", buf, size);
	free(buf);

	return rc;
}

int vrc7_audio_load_state(struct emu *emu, struct save_state *state)
{
	struct vrc7_audio_state *audio;
	size_t size;
	uint8_t *buf;
	int i;

	audio = emu->vrc7_audio;

	if (save_state_find_chunk(state, "VC7S", &buf, &size) < 0)
		return -1;

	buf += unpack_state(audio, vrc7_audio_state_items, buf);
	buf += unpack_state(audio->opll, vrc7_opll_state_items, buf);

	for (i = 0; i < 12; i++) {
		buf += unpack_state(&audio->opll->slot[i],
				    vrc7_opll_slot_state_items, buf);
	}

	return 0;
}
