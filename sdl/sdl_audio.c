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

#include <SDL.h>
#include <SDL_thread.h>

#include "emu.h"
#include "blip_buf.h"

int old_diff;
static double sample_rate;
static int sdl_audio_buffer_size;
static int dynamic_rate_enabled;
static int force_stereo;

/* Audio state */
int sdl_audio_device;
static blip_buffer_t* blip = NULL;
static int audio_muted = -1;
static int audio_buffer_size;
static int samples_per_frame;
static int samples_per_nes_frame;
static int samples_per_display_frame;
static int samples_per_larger_frame;
static int samples_available;
static int sample_reserve;
static int channels;
static int playing;
static int counter;
static int last_counter;
static int old_samples;
static int frame_count;
static int high_target; 
static int low_target;
static double adjusted_sample_rate;
static double min_adjusted_sample_rate;
static double max_adjusted_sample_rate;
static double safe_min_adjusted_sample_rate;
static double safe_max_adjusted_sample_rate;
static int previous_difference;
static double prev_adjustment;
static double nes_framerate;

static int dynamic_rate_adjustment_delay;

static SDL_mutex *audiobuf_lock;
static SDL_cond *audiobuf_cond;

extern struct emu *emu;

static void audio_callback(void* unused, uint8_t* out, int byte_count);
extern void update_clock(void);

void audio_add_delta(unsigned time, int delta)
{
	if (!blip)
		return;

	SDL_LockMutex(audiobuf_lock);
	if (audio_muted > 0)
		delta = 0;
	else if (!audio_muted)
		delta = (delta * emu->config->master_volume) / 100;
	blip_add_delta(blip, time, delta);
	SDL_UnlockMutex(audiobuf_lock);
}

static int audio_setup(struct emu *emu)
{
	SDL_AudioSpec wanted, returned;
	const char *driver_name;
	int flags;

	if (!emu_loaded(emu))
		return 0;

	if (!audiobuf_lock)
		audiobuf_lock = SDL_CreateMutex();

	if (!audiobuf_cond)
		audiobuf_cond = SDL_CreateCond();

	sample_rate = emu->config->sample_rate;
	sdl_audio_buffer_size = emu->config->audio_buffer_size;
	force_stereo = emu->config->force_stereo;

	/* FIXME what if display framerate changes? */
	samples_per_nes_frame = sample_rate / emu->user_framerate;
	nes_framerate = emu->user_framerate;

	samples_per_display_frame = sample_rate / emu->display_framerate;

	if (emu->config->vsync)
		samples_per_frame = samples_per_display_frame;
	else
		samples_per_frame = samples_per_nes_frame;

	if (samples_per_display_frame > samples_per_nes_frame)
		samples_per_larger_frame = samples_per_display_frame;
	else
		samples_per_larger_frame = samples_per_nes_frame;

	if (force_stereo)
		channels = 2;
	else
		channels = 1;

	wanted.freq = sample_rate;
	wanted.format = AUDIO_S16SYS;
	wanted.channels = channels;
	wanted.callback = audio_callback;
	wanted.samples = sdl_audio_buffer_size;

	flags = SDL_AUDIO_ALLOW_FREQUENCY_CHANGE |
		SDL_AUDIO_ALLOW_CHANNELS_CHANGE;

	/* Try mono first, since it saves us a small amount of
	   work over stereo.  Some devices don't support less
	   than two channels though.
	*/
	sdl_audio_device = SDL_OpenAudioDevice(NULL, 0, &wanted, &returned,
					       flags );
	if (sdl_audio_device <= 0 && channels == 1) {
		/* Try again with two channels */
		channels = 2;
		wanted.channels = 2;
		sdl_audio_device = SDL_OpenAudioDevice(NULL, 0, &wanted, &returned,
						       flags );
		if (sdl_audio_device <= 0) {
			log_err("failed to open SDL audio: %s\n",
		                SDL_GetError());
			return 0;
		}
	}

	driver_name = SDL_GetCurrentAudioDriver();

	if (strcmp(driver_name, "dummy") == 0)
		return 0;

	log_dbg("Using SDL audio driver %s\n", driver_name);

	sample_rate = returned.freq;
	adjusted_sample_rate = returned.freq;
	max_adjusted_sample_rate = sample_rate *
	    (1.0 + emu->config->dynamic_rate_adjustment_max);
	min_adjusted_sample_rate = sample_rate * 
	    (1.0 - emu->config->dynamic_rate_adjustment_max);
	safe_max_adjusted_sample_rate = sample_rate *
	    (1.0 + 0.005);
	safe_min_adjusted_sample_rate = sample_rate * 
	    (1.0 - 0.005);
	channels = returned.channels;

	if (channels < 1 || channels > 2) {
		log_err("Unsupported number of channels %d\n", channels);
		return 1;
	}

	/* Allocate a bit more storage than requested to handle larger
	   frames due to A) dynamic sample rate adjustment and B)
	   frames that go past the desired frame length due to DMA or
	   instruction length of the last executed instruction.
	*/
	/* printf("returned: %d, samples %d\n", returned.samples, samples_per_nes_frame); */
	audio_buffer_size = (2 * (returned.samples /
				  samples_per_larger_frame + 1) + 1)* samples_per_larger_frame;
	

	blip = blip_new(audio_buffer_size * 1.03);
	if ( blip == NULL )
		return 1;

	log_dbg("sample_rate: %f\n", sample_rate);
	blip_set_rates(blip, emu->current_clock_rate, sample_rate);

	low_target = sdl_audio_buffer_size % samples_per_frame;
	low_target = low_target +
		(emu->config->dynamic_rate_adjustment_low_watermark *
		 samples_per_frame);
	high_target = low_target +
		(emu->config->dynamic_rate_adjustment_buffer_range *
		 samples_per_frame);

        dynamic_rate_enabled =
		emu->config->dynamic_rate_adjustment_enabled;
	if (blip && dynamic_rate_enabled) {
               frame_count = 0;
               adjusted_sample_rate = sample_rate;
               max_adjusted_sample_rate = sample_rate *
                   (1.0 + emu->config->dynamic_rate_adjustment_max);
               min_adjusted_sample_rate = sample_rate *
                   (1.0 - emu->config->dynamic_rate_adjustment_max);
               last_counter = 0;
               previous_difference = 0;
               old_samples = 0;
               counter = 0;
       }
	

	return 0;
}

int audio_init(struct emu *emu)
{

	sdl_audio_device = -1;
	dynamic_rate_enabled = 0;
	force_stereo = 0;
	sample_rate = 0;
	sdl_audio_buffer_size = 0;
	blip = NULL;
	dynamic_rate_adjustment_delay = -1;
	samples_available = 0;
	sample_reserve = 0;
	return 0;
}

int audio_shutdown(void)
{
	SDL_PauseAudioDevice(sdl_audio_device, 1 );
	SDL_CloseAudioDevice(sdl_audio_device);
	playing = 0;
	blip_delete( blip );
	sdl_audio_device = -1;
	dynamic_rate_enabled = 0;
	force_stereo = 0;
	sample_rate = 0;
	sdl_audio_buffer_size = 0;
	blip = NULL;

	return 0;
}

int audio_apply_config(struct emu *emu)
{
	if (sdl_audio_device >= 0) {
		/* FIXME flush existing audio data */
		audio_shutdown();
	}
	audio_init(emu);
	audio_setup(emu);

	return 0;
}

static void audio_callback(void* unused, uint8_t* out, int byte_count)
{
	int samples_requested, samples;
	int actual_samples_available;

	SDL_LockMutex(audiobuf_lock);
	samples_requested = byte_count / (2 * channels);
	actual_samples_available = blip_samples_avail(blip);

//	printf("consume\n");

	samples = samples_requested;
	if (samples_available < samples_requested) {
		log_dbg("%d samples requested, only %d buffered: %d\n",
			samples_requested, samples_available, previous_difference);
		samples = samples_available;
		memset(out, 0, byte_count);
	}

	samples_available -= samples;

	blip_read_samples(blip, (short *)out, samples, channels >> 1);

	SDL_UnlockMutex(audiobuf_lock);

	if (audio_buffer_size - (actual_samples_available -
				 (byte_count / 2)) >
	    samples_per_larger_frame * 2) {
		SDL_CondBroadcast(audiobuf_cond);
	}
}

void audio_pause(int paused)
{
	if (sdl_audio_device >= 0)
		SDL_PauseAudioDevice(sdl_audio_device, paused);
}

int audio_buffer_check(void)
{
	int samples;
	int buffer_remaining;
	int rc;
	int skip_watermark;

	if (!playing)
		return 1;

	SDL_LockMutex(audiobuf_lock);
	samples = samples_available + sample_reserve;

	buffer_remaining = audio_buffer_size - samples;
	rc = 1;

	/* FIXME HACK ALERT!
	   The following code *seems* to work, but I
	   can't make any guarantees at the moment.

	   If we have less than two frames' worth of samples, don't
	   delay at all.  This is to avoid buffer underruns.

	   To avoid overruns, if there is less than one frame's
	   samples buffer_remaining to be filled, then wait for the audio
	   consumer thread to use at least one frames' worth of audio.
	 */
	if (buffer_remaining < samples_per_nes_frame) {
		while (buffer_remaining < samples_per_nes_frame) {
			SDL_CondWait(audiobuf_cond, audiobuf_lock);
			samples = samples_available + sample_reserve;
			buffer_remaining = audio_buffer_size - samples;
		}
		SDL_UnlockMutex(audiobuf_lock);
		update_clock();
		return 1;
	}

	skip_watermark = (sdl_audio_buffer_size % samples_per_frame) / 2;
	/* Audio buffer is running low; skip the next frame to stuff more
	   audio data into the buffer before the callback runs again. */
	if (samples_available < skip_watermark) {
		rc = 0;
		dynamic_rate_adjustment_delay = 10;
		log_dbg("skipping: %d < %d\n", samples_available, skip_watermark);
	}

	SDL_UnlockMutex(audiobuf_lock);

	return rc;
}

void audio_fill_buffer(uint32_t cycles)
{
//	double level;
	int samples;
	int do_adjust;
	double adjustment;
	int difference;

	difference = 0;
	adjustment = 0;

	if (!blip)
		return;

//	printf("produce\n");

	counter++;
	samples = samples_available;
	SDL_LockMutex(audiobuf_lock);
	if (cycles) {
		int old_samples, new_samples, sample_count;
		int tmp;
		old_samples = blip_samples_avail(blip);
		blip_end_frame(blip, cycles);
		new_samples = blip_samples_avail(blip);
		sample_count = new_samples - old_samples;
		tmp = sample_count;
		if (emu->config->vsync && emu->frame_timer_reload) {
			if (emu->user_framerate < emu->display_framerate) {
				if (tmp < samples_per_display_frame) {
					int diff = samples_per_display_frame - tmp;

					if (diff > sample_reserve)
						diff = sample_reserve;

					tmp += diff;
				} else {
					tmp = samples_per_display_frame;
				}
			}
		}
		samples_available += tmp;
//		printf("adding %d samples %f (%d) (%d s %d)\n", tmp, adjusted_sample_rate, samples_per_display_frame, sample_count, samples_per_nes_frame);
		sample_reserve += sample_count - tmp;
	} else {
		if (emu->config->vsync && emu->frame_timer_reload) {
			if (emu->user_framerate < emu->display_framerate) {
				/* if (sample_reserve > samples_per_display_frame) { */
				/* 	samples_available += samples_per_display_frame; */
				/* 	sample_reserve -= samples_per_display_frame; */
				/* } else { */
//					printf("here: %d %d\n", sample_reserve, emu->frame_timer_reload);
					samples_available += sample_reserve;
					sample_reserve = 0;
				/* } */
			} else {
				samples_available += sample_reserve;
				sample_reserve = 0;
			}
		}
	}
//	level = (double)samples / audio_buffer_size;
	do_adjust = 0;

	if (!playing) {
//		if (samples >= sdl_audio_buffer_size) {
			playing = 1;
			SDL_PauseAudioDevice(sdl_audio_device, 0);
//		}
	} else if (dynamic_rate_enabled && (samples < old_samples) &&
		   (dynamic_rate_adjustment_delay < 0)) {
		if (samples < low_target)
			difference = samples - low_target;
		else if (samples > high_target)
			difference = samples - high_target;
		else
			difference = 0;

		if (difference) {
			do_adjust = 1;

			adjustment = (double)difference / counter;
			adjustment *= emu->current_framerate;
			adjustment *= -1.2;

			/* Limit this adjustment to +/- 0.1% */
			if (adjustment > sample_rate * 0.001) {
				adjustment = sample_rate * 0.001;
			} else if (adjustment < sample_rate * -0.001) {
				adjustment = sample_rate * -0.001;
			}

			if (!previous_difference) {
				/* Don't make any adjustments the first time through; just keep
				   track of the difference for the next pass.
				*/
				do_adjust = 0;
				previous_difference = difference;
			} else if (((difference > 0) && (previous_difference < 0)) ||
				   ((difference < 0) && (previous_difference > 0))) {
				/* we've switched direction from the last time we adjusted, so
				   update the count to indicate the number of frames that we've
				   run in this direction.
				*/
				last_counter = counter;
				counter = 0;
				/* printf("here\n"); */
			} else if (((difference < 0) && ((difference < previous_difference))) ||
							 ((previous_difference == 0) && (
								 (difference == 0))) ||
				   ((difference > 0) && (difference > previous_difference))) {
				/* we're going the same direction as the previous difference, and
				   it appears we're moving away from the target.
				*/
//				last_counter = counter;
			} else {
				do_adjust = 0;
			}


			if (do_adjust) {
				/* Save difference so we can remember the direction
				   we adjusted in */
				old_diff = previous_difference;
				previous_difference = difference;
				prev_adjustment = adjustment;
			}

		}
	} else if (dynamic_rate_enabled && (samples < old_samples) &&
		   (dynamic_rate_adjustment_delay >= 0)) {
		adjustment = sample_rate * 0.001;
		do_adjust = 1;
	} else if (dynamic_rate_enabled) {
	}
		frame_count++;

	if (do_adjust) {
		/* Limit resampled_rate to sample_rate +/- 0.05%.
		   This should prevent huge adjustments that would
		   result in audible pitch changes.
		*/
		adjusted_sample_rate += adjustment;
		if (adjusted_sample_rate > max_adjusted_sample_rate)
			adjusted_sample_rate = max_adjusted_sample_rate;
		else if (adjusted_sample_rate < min_adjusted_sample_rate)
			adjusted_sample_rate = min_adjusted_sample_rate;

		if (adjusted_sample_rate > safe_max_adjusted_sample_rate) {
			log_dbg("Adjustment exceeded safe limits: %f > %f\n",
				adjusted_sample_rate, safe_max_adjusted_sample_rate);
		} else if (adjusted_sample_rate < safe_min_adjusted_sample_rate) {
			log_dbg("Adjustment exceeded safe limits: %f < %f\n",
				adjusted_sample_rate, safe_min_adjusted_sample_rate);
		}

		/* printf("%d: new_rate: %f, prev: %d diff: %d low: %d high: %d level: %d\n", */
		/*        frame_count, adjusted_sample_rate, old_diff, difference, low_target, */
		/*        high_target, samples); */
		/* printf("last_counter: %d, counter: %d\n", last_counter, counter); */
		blip_set_rates(blip, emu->current_clock_rate, adjusted_sample_rate);
		samples_per_frame = adjusted_sample_rate / emu->current_framerate;
		low_target = (sdl_audio_buffer_size % samples_per_frame) / 2;
		low_target += emu->config->dynamic_rate_adjustment_low_watermark *
			samples_per_frame;
		high_target = emu->config->dynamic_rate_adjustment_buffer_range *
			samples_per_frame + low_target;

	}

	if (dynamic_rate_adjustment_delay >= 0)
		dynamic_rate_adjustment_delay--;

	old_samples = samples;

	SDL_UnlockMutex(audiobuf_lock);
}

void audio_mute(int muted)
{
	SDL_LockMutex(audiobuf_lock);
	audio_muted = !!muted;
	SDL_UnlockMutex(audiobuf_lock);
}
