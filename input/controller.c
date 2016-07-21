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
#include "actions.h"
#include "input.h"
#include "controller.h"

/* BUTTON_A is 'B' on SNES controller */
/* BUTTON_B is 'Y' on SNES controller */
#define BUTTON_A      (1 <<  0)
#define BUTTON_B      (1 <<  1)
#define BUTTON_SEL    (1 <<  2)
#define BUTTON_ST     (1 <<  3)
#define BUTTON_UP     (1 <<  4) 
#define BUTTON_DOWN   (1 <<  5) 
#define BUTTON_LEFT   (1 <<  6) 
#define BUTTON_RIGHT  (1 <<  7) 
#define BUTTON_SNES_A (1 <<  8)
#define BUTTON_SNES_X (1 <<  9)
#define BUTTON_SNES_L (1 << 10)
#define BUTTON_SENS_R (1 << 11)
#define VS_BUTTON_MASK (BUTTON_SEL|BUTTON_ST)

/* Implementation of standard NES/Famicom controllers,
   FourScore and Famicom-style 4-player adapters. */

static int controller_connect(struct io_device *dev);
static void controller_disconnect(struct io_device *dev);
static uint8_t controller_read(struct io_device *, int port, int mode,
			       uint32_t cycles);
static void controller_write(struct io_device *, uint8_t value,
			     int mode, uint32_t cycles);
static void controller_end_frame(struct io_device *, uint32_t cycles);
static int controller_apply_config(struct io_device *dev);
static int controller_save_state(struct io_device *dev, int port,
				 struct save_state *state);
static int controller_load_state(struct io_device *dev, int port,
				 struct save_state *state);
static int controller_movie_get_latch(struct io_device *dev);
static void controller_close_movie(struct io_device *dev);
static void controller_play_movie(struct io_device *dev);
static void controller_stop_movie(struct io_device *dev);
static void controller_record_movie(struct io_device *dev);
static int controller_save_movie(struct io_device *dev);
static void controller_movie_add_data(struct io_device *dev, int data);
static void controller_movie_encode_latch(struct io_device *dev);
static void controller_movie_decode_latch(struct io_device *dev);

struct controller_state {
	int latch;
	int strobe;
	int index;
	int is_snes;
	struct controller_common_state *common_state;
	int runlength;
	int old_latch;
	int latched;
	uint8_t *movie_buffer;
	size_t movie_size;
	int movie_offset;
	int movie_length;
	int movie_latch_data;
	int movie_latch_count;
};

static struct state_item controller_state_items[] = {
	STATE_16BIT(controller_state, latch),
	STATE_8BIT(controller_state, strobe), /* BOOLEAN */
	STATE_ITEM_END(),
};

struct io_device controller1_device = {
	.name = "Controller 1",
	.id = IO_DEVICE_CONTROLLER_1,
	.config_id = "controller1",
	.connect = controller_connect,
	.disconnect = controller_disconnect,
	.read = controller_read,
	.write = controller_write,
	.save_state = controller_save_state,
	.load_state = controller_load_state,
	.apply_config = controller_apply_config,
	.end_frame = controller_end_frame,
	.record_movie = controller_record_movie,
	.play_movie = controller_play_movie,
	.stop_movie = controller_stop_movie,
	.save_movie = controller_save_movie,
	.close_movie = controller_close_movie,
	.port = PORT_1,
	.removable = 1,
};

struct io_device controller2_device = {
	.name = "Controller 2",
	.id = IO_DEVICE_CONTROLLER_2,
	.config_id = "controller2",
	.connect = controller_connect,
	.disconnect = controller_disconnect,
	.read = controller_read,
	.write = controller_write,
	.save_state = controller_save_state,
	.load_state = controller_load_state,
	.apply_config = controller_apply_config,
	.end_frame = controller_end_frame,
	.record_movie = controller_record_movie,
	.play_movie = controller_play_movie,
	.stop_movie = controller_stop_movie,
	.save_movie = controller_save_movie,
	.close_movie = controller_close_movie,
	.port = PORT_2,
	.removable = 1,
};

struct io_device controller3_device = {
	.name = "Controller 3",
	.id = IO_DEVICE_CONTROLLER_3,
	.config_id = "controller3",
	.connect = controller_connect,
	.disconnect = controller_disconnect,
	.read = controller_read,
	.write = controller_write,
	.save_state = controller_save_state,
	.load_state = controller_load_state,
	.apply_config = controller_apply_config,
	.end_frame = controller_end_frame,
	.record_movie = controller_record_movie,
	.play_movie = controller_play_movie,
	.stop_movie = controller_stop_movie,
	.save_movie = controller_save_movie,
	.close_movie = controller_close_movie,
	.port = PORT_3,
	.removable = 1,
};

struct io_device controller4_device = {
	.name = "Controller 4",
	.id = IO_DEVICE_CONTROLLER_4,
	.config_id = "controller4",
	.connect = controller_connect,
	.disconnect = controller_disconnect,
	.read = controller_read,
	.write = controller_write,
	.save_state = controller_save_state,
	.load_state = controller_load_state,
	.apply_config = controller_apply_config,
	.end_frame = controller_end_frame,
	.record_movie = controller_record_movie,
	.play_movie = controller_play_movie,
	.stop_movie = controller_stop_movie,
	.save_movie = controller_save_movie,
	.close_movie = controller_close_movie,
	.port = PORT_4,
	.removable = 1,
};

struct io_device bandai_hyper_shot_controller_device = {
	.name = "Bandai HyperShot Controller",
	.id = IO_DEVICE_BANDAI_HYPER_SHOT_CONTROLLER,
	.config_id = "bandai_hyper_shot_controller",
	.connect = controller_connect,
	.disconnect = controller_disconnect,
	.read = controller_read,
	.write = controller_write,
	/* .save_state = controller_save_state, */
	/* .load_state = controller_load_state, */
	.apply_config = controller_apply_config,
	.end_frame = controller_end_frame,
	.port = PORT_EXP,
	.removable = 1,
};

struct io_device snes_controller1_device = {
	.name = "SNES Controller 1",
	.id = IO_DEVICE_SNES_CONTROLLER_1,
	.config_id = "snes_controller1",
	.connect = controller_connect,
	.disconnect = controller_disconnect,
	.read = controller_read,
	.write = controller_write,
	.save_state = controller_save_state,
	.load_state = controller_load_state,
	.apply_config = controller_apply_config,
	.end_frame = controller_end_frame,
	.record_movie = controller_record_movie,
	.play_movie = controller_play_movie,
	.stop_movie = controller_stop_movie,
	.save_movie = controller_save_movie,
	.close_movie = controller_close_movie,
	.port = PORT_1,
	.removable = 1,
};

struct io_device snes_controller2_device = {
	.name = "SNES Controller 2",
	.id = IO_DEVICE_SNES_CONTROLLER_2,
	.config_id = "snes_controller2",
	.connect = controller_connect,
	.disconnect = controller_disconnect,
	.read = controller_read,
	.write = controller_write,
	.save_state = controller_save_state,
	.load_state = controller_load_state,
	.apply_config = controller_apply_config,
	.record_movie = controller_record_movie,
	.play_movie = controller_play_movie,
	.stop_movie = controller_stop_movie,
	.save_movie = controller_save_movie,
	.close_movie = controller_close_movie,
	.port = PORT_2,
	.removable = 1,
};

struct io_device snes_controller3_device = {
	.name = "SNES Controller 3",
	.id = IO_DEVICE_SNES_CONTROLLER_3,
	.config_id = "snes_controller3",
	.connect = controller_connect,
	.disconnect = controller_disconnect,
	.read = controller_read,
	.write = controller_write,
	.save_state = controller_save_state,
	.load_state = controller_load_state,
	.apply_config = controller_apply_config,
	.record_movie = controller_record_movie,
	.play_movie = controller_play_movie,
	.stop_movie = controller_stop_movie,
	.save_movie = controller_save_movie,
	.close_movie = controller_close_movie,
	.port = PORT_3,
	.removable = 1,
};

struct io_device snes_controller4_device = {
	.name = "SNES Controller 4",
	.id = IO_DEVICE_SNES_CONTROLLER_4,
	.config_id = "snes_controller4",
	.connect = controller_connect,
	.disconnect = controller_disconnect,
	.read = controller_read,
	.write = controller_write,
	.save_state = controller_save_state,
	.load_state = controller_load_state,
	.apply_config = controller_apply_config,
	.record_movie = controller_record_movie,
	.play_movie = controller_play_movie,
	.stop_movie = controller_stop_movie,
	.save_movie = controller_save_movie,
	.close_movie = controller_close_movie,
	.port = PORT_4,
	.removable = 1,
};

static int controller_load_movie(struct io_device *dev)
{
	int rc;
	struct controller_state *state;
	char *id;
	uint8_t *buffer;
	size_t size;

	state = dev->private;

	if ((dev->port < PORT_1) || (dev->port > PORT_4))
		return -1;

	switch (dev->port) {
	case PORT_1:
		if (state->is_snes)
			id = "PSM0";
		else
			id = "PDM0";
		break;
	case PORT_2:
		if (state->is_snes)
			id = "PSM1";
		else
			id = "PDM1";
		break;
	case PORT_3:
		if (state->is_snes)
			id = "PSM2";
		else
			id = "PDM2";
		break;
	case PORT_4:
		if (state->is_snes)
			id = "PSM3";
		else
			id = "PDM3";
		break;
	}

	if (!state->movie_size) {
		rc = save_state_find_chunk(dev->emu->movie_save_state, id,
					   &buffer, &size);

		/* Chunk not present, but that's not necessarily a problem. */
		if (rc < 0)
			return 0;

		if (size) {
			state->movie_buffer = malloc(size);
			if (!state->movie_buffer)
				return -1;

			memcpy(state->movie_buffer, buffer, size);
			state->movie_size = size;
			state->movie_length = size;
		}
	}

	emu_increment_movie_chunk_count(dev->emu);

	return 0;
}

static void controller_play_movie(struct io_device *dev)
{
	struct controller_state *state;

	state = dev->private;

	controller_load_movie(dev);

	state->movie_offset = 0;
	state->movie_latch_count = 0;
}

static void controller_record_movie(struct io_device *dev)
{
	struct controller_state *state;

	state = dev->private;

	state->movie_offset = 0;
}


static void controller_close_movie(struct io_device *dev)
{
	struct controller_state *state;

	state = dev->private;

	if (state->movie_buffer)
		free(state->movie_buffer);

	state->movie_buffer = NULL;
	state->movie_size = 0;
	state->movie_length = 0;
	state->movie_offset = 0;
	state->movie_latch_data = 0;
	state->movie_latch_count = 0;
}

static void controller_stop_movie(struct io_device *dev)
{
	if (dev->emu->recording_movie) {
		controller_movie_encode_latch(dev);
	}
}

static int controller_save_movie(struct io_device *dev)
{
	struct controller_state *state;
	int rc;
	char *id;

	state = dev->private;

	if ((dev->port < PORT_1) || (dev->port > PORT_4))
		return -1;

	switch (dev->port) {
	case PORT_1:
		if (state->is_snes)
			id = "PSM0";
		else
			id = "PDM0";
		break;
	case PORT_2:
		if (state->is_snes)
			id = "PSM1";
		else
			id = "PDM1";
		break;
	case PORT_3:
		if (state->is_snes)
			id = "PSM2";
		else
			id = "PDM2";
		break;
	case PORT_4:
		if (state->is_snes)
			id = "PSM3";
		else
			id = "PDM3";
		break;
	}
	
	rc = save_state_replace_chunk(dev->emu->movie_save_state, id,
	                              state->movie_buffer,
	                              state->movie_length);

	return rc;
}

static void controller_end_frame(struct io_device *dev, uint32_t cycles)
{
	struct controller_state *state;

	state = dev->private;

	state->latched = 0;
}

static void controller_write(struct io_device *dev, uint8_t data, int mode,
			     uint32_t cycles)
{
	struct controller_state *state;
	int swap_start_select;
	int controller;

	state = dev->private;
	controller = state->index;

	if (!state->strobe && (data & 0x01)) {
		state->strobe = 1;
	} else if (state->strobe && !(data & 0x01)) {
		state->strobe = 0;

		if (dev->emu->playing_movie) {
			if (!state->latched) {
				state->latch = controller_movie_get_latch(dev);
				state->old_latch = state->latch;
			} else {
				state->latch = state->old_latch;
			}
			goto done;
		}

		if (state->common_state)
			state->latch = state->common_state->current_state[controller];

		swap_start_select = dev->emu->config->swap_start_select;
		if (emu_system_is_vs(dev->emu) &&
		    dev->emu->config->vs_swap_start_select) {
			swap_start_select = 1;
		}

		if (swap_start_select) {
			int tmp = state->latch & (BUTTON_SEL | BUTTON_ST);
			state->latch &= ~(BUTTON_SEL | BUTTON_ST);
			state->latch |= (tmp & BUTTON_SEL) << 1;
			state->latch |= (tmp & BUTTON_ST) >> 1;
		}

		if (emu_system_is_vs(dev->emu)) {
			int c, tmp;
			int vs_standard_controls;

			vs_standard_controls = 1;

			switch (state->common_state->vs_controller_mode) {
			case VS_CONTROLLER_MODE_STANDARD:
				break;
			case VS_CONTROLLER_MODE_BUNGELINGBAY:
				state->latch |= BUTTON_ST;
				break;
			case VS_CONTROLLER_MODE_VSSUPERSKYKID:
				if (dev->port == 0) {
					tmp = state->common_state->port_mapping[1];
					if (tmp >= 0) {
						tmp = state->common_state->current_state[tmp];
						if (swap_start_select)
							tmp &= BUTTON_ST;
						else
							tmp &= BUTTON_SEL;

						if (tmp)
							state->latch |= BUTTON_ST;
					}
				}
				break;
			case VS_CONTROLLER_MODE_SWAPPED:
				vs_standard_controls = 0;
				break;
			case VS_CONTROLLER_MODE_VSPINBALLJ:
				if (dev->port == 1) {
					tmp = state->common_state->port_mapping[0];
					if (tmp >= 0) {
						tmp = state->common_state->current_state[tmp];
						state->latch &= ~BUTTON_A;
						state->latch |= tmp & BUTTON_B;
					}
				} else {
					tmp = state->common_state->port_mapping[1];
					if (tmp >= 0) {
						tmp = state->common_state->current_state[tmp];
						state->latch &= ~BUTTON_B;
						state->latch |= tmp & BUTTON_A;
					}
				}
				vs_standard_controls = 0;
				break;
			}

			if (vs_standard_controls) {
				c = dev->port ^ 1;
				tmp = state->common_state->port_mapping[c];
				state->latch &= VS_BUTTON_MASK;
				if (tmp >= 0) {
					tmp = state->common_state->current_state[tmp];
					tmp &= ~VS_BUTTON_MASK;
					state->latch |= tmp;
				}
			}
		} else if (dev->id == IO_DEVICE_BANDAI_HYPER_SHOT_CONTROLLER) {
			int tmp = state->common_state->port_mapping[0];

			if (tmp < 0)
				tmp = 0;

			state->latch = state->common_state->current_state[tmp];
		}

		if (dev->emu->config->swap_a_b) {
			int tmp = state->latch & (BUTTON_A | BUTTON_B);
			state->latch &= ~(BUTTON_A | BUTTON_B);
			state->latch |= (tmp & BUTTON_A) << 1;
			state->latch |= (tmp & BUTTON_B) >> 1;
		}

		if (dev->emu->config->mask_opposite_directions) {
			if ((state->latch & (BUTTON_LEFT|BUTTON_RIGHT)) ==
			    (BUTTON_LEFT|BUTTON_RIGHT)) {
				state->latch ^= BUTTON_LEFT|BUTTON_RIGHT;
			}
				
			if ((state->latch & (BUTTON_UP|BUTTON_DOWN)) ==
			    (BUTTON_UP|BUTTON_DOWN)) {
				state->latch ^= BUTTON_UP|BUTTON_DOWN;
			}
		}

done:

		if (state->is_snes) {
			state->latch &= 0x0fff;
		} else {
			state->latch |= ~0xff;
		}

		if (mode == FOUR_PLAYER_MODE_NES) {
			state->latch &= 0xff;
			if (dev->port >= 2) {
				state->latch <<= 8;
			}
		}

		if (dev->emu->recording_movie && !state->latched) {
			controller_movie_add_data(dev, state->latch);
		}

		state->latched = 1;
	}

}

static void controller_movie_decode_latch(struct io_device *dev)
{
	struct controller_state *state;
	uint8_t *buffer;
	int offset;

	state = dev->private;

	buffer = state->movie_buffer;
	offset = state->movie_offset;

	if (buffer && (offset < state->movie_length)) {
		int magic;

		if ((buffer[offset] == 0xff) || (buffer[offset] == 0xfe)) {
			magic = buffer[offset];
			offset++;
			state->movie_latch_data = buffer[offset];
			offset++;

			if (state->is_snes) {
				state->movie_latch_data |= buffer[offset] << 8;
				offset++;
			}

			state->movie_latch_count = buffer[offset];
			offset++;
			state->movie_latch_count |= (buffer[offset] << 8);
			offset++;

			if (magic == 0xff) {
				state->movie_latch_count |= (buffer[offset] << 16);
				offset++;
			}

			state->movie_latch_count++;
			state->movie_offset = offset;
		} else {
			state->movie_latch_data = buffer[offset];
			offset++;

			if (state->is_snes) {
				state->movie_latch_data |= buffer[offset] << 8;
				offset++;
			}

			state->movie_latch_count = buffer[offset] + 1;
			offset++;

			state->movie_offset = offset;
		}
	} else {
		state->movie_latch_data = 0;
		state->movie_latch_count = 0;
	}
}

static int controller_movie_get_latch(struct io_device *dev)
{
	struct controller_state *state;
	uint8_t value;

	state = dev->private;
	value = 0;

	if (state->movie_latch_count <= 0)
		controller_movie_decode_latch(dev);

	if (state->movie_latch_count > 0) {
		state->movie_latch_count--;
		value = state->movie_latch_data;
	}

	if ((state->movie_offset == state->movie_length) &&
	    !state->movie_latch_count) {
		emu_decrement_movie_chunk_count(dev->emu);
	}

	return value;
}

static void controller_movie_encode_latch(struct io_device *dev)
{
	struct controller_state *state;
	uint8_t data;
	int available;
	int count;
	int needed;
	int max;
	uint8_t magic;

	state = dev->private;
	if (state->is_snes)
		data = state->movie_latch_data & 0x0fff;
	else
		data = state->movie_latch_data & 0x00ff;

	count = state->movie_latch_count;

	if (count > 65536) {
		needed = 5;
		max = 1 << 24;
		magic = 0xff;
	} else if (count > 512) {
		needed = 4;
		max = 1 << 16;
		magic = 0xfe;
	} else if ((data & 0xf0) == 0xf0) {
		needed = 4;
		max = 1 << 16;
		magic = 0xfe;
	} else {
		needed = 2;
		max = 256;
		magic = 0x00;
	}

	if (state->is_snes)
		needed++;

	while (count) {
		int c;

		available = state->movie_size - state->movie_offset;

		if (available < needed) {
			uint8_t *tmp;
			int new_size;

			new_size = state->movie_size + 1024;
			tmp = realloc(state->movie_buffer, new_size);
			if (!tmp)
				return;

			state->movie_buffer = tmp;
			state->movie_size = new_size;
		}

		if (count > max)
			c = max - 1;
		else
			c = count - 1;
			   
		if (magic) {
			state->movie_buffer[state->movie_offset] = magic;
			state->movie_offset++;
		}

		state->movie_buffer[state->movie_offset] = data & 0xff;
		state->movie_offset++;

		if (state->is_snes) {
			state->movie_buffer[state->movie_offset] = (data >> 8) & 0xff;
			state->movie_offset++;
		}

		state->movie_buffer[state->movie_offset] = c & 0xff; 
		state->movie_offset++;

		if (needed >= 4) {
			state->movie_buffer[state->movie_offset] =
			    (c >> 8) & 0xff; 
			state->movie_offset++;
		}

		if (needed == 5) {
			state->movie_buffer[state->movie_offset] =
			    (c >> 16) & 0xff; 
			state->movie_offset++;
		}

		count -= (c + 1);
	}

	state->movie_length = state->movie_offset;
}

static void controller_movie_add_data(struct io_device *dev, int data)
{
	struct controller_state *state;

	state = dev->private;
	if (state->is_snes)
		data &= 0xfff;
	else
		data &= 0xff;

	if ((data != state->movie_latch_data) || (!state->movie_latch_count)) {
		controller_movie_encode_latch(dev);
		state->movie_latch_data = data;
		state->movie_latch_count = 1;
	} else {
		state->movie_latch_count++;
	}
}

static uint8_t controller_read(struct io_device *dev, int port, int mode,
			       uint32_t cycles)
{
	struct controller_state *state;
	uint8_t data;

	state = dev->private;

	if ((dev->id == IO_DEVICE_BANDAI_HYPER_SHOT_CONTROLLER) && port)
		return 0;

	data = state->latch & 0x01;
	state->latch >>= 1;

	if (mode != FOUR_PLAYER_MODE_NES) {
		if (state->is_snes)
			state->latch |= (1 << 15);
		else
			state->latch |= (1 << 7);
	}

	if (dev->id == IO_DEVICE_BANDAI_HYPER_SHOT_CONTROLLER)
		data <<= 1;

	return data;
}

static int controller_connect(struct io_device *dev)
{
	struct controller_state *state;

	state = malloc(sizeof(*state));
	if (!state)
		return -1;

	memset(state, 0, sizeof(*state));
	dev->private = state;

	switch (dev->id) {
	case IO_DEVICE_SNES_CONTROLLER_1:
		state->index = 0;
		state->is_snes = 1;
		break;
	case IO_DEVICE_SNES_CONTROLLER_2:
		state->index = 1;
		state->is_snes = 1;
		break;
	case IO_DEVICE_SNES_CONTROLLER_3:
		state->index = 2;
		state->is_snes = 1;
		break;
	case IO_DEVICE_SNES_CONTROLLER_4:
		state->index = 3;
		state->is_snes = 1;
		break;
	case IO_DEVICE_CONTROLLER_1:
		state->index = 0;
		state->is_snes = 0;
		break;
	case IO_DEVICE_CONTROLLER_2:
		state->index = 1;
		state->is_snes = 0;
		break;
	case IO_DEVICE_CONTROLLER_3:
		state->index = 2;
		state->is_snes = 0;
		break;
	case IO_DEVICE_CONTROLLER_4:
		state->index = 3;
		state->is_snes = 0;
		break;
	default:
		state->is_snes = 0;
		state->index = -1;
	}

	state->movie_buffer = NULL;
	state->movie_size = 0;
	state->movie_length = 0;
	state->movie_offset = 0;
	state->movie_latch_data = 0;
	state->movie_latch_count = 0;

	state->common_state =
		io_get_controller_common_state(dev->emu->io);

	state->common_state->port_mapping[dev->port] = state->index;

	return 0;
}

static void controller_disconnect(struct io_device *dev)
{
	struct controller_state *state;

	state = dev->private;

	if (state) {
		state->common_state->port_mapping[dev->port] = -1;
		free(dev->private);
		dev->private = NULL;
	}
}

static int controller_apply_config(struct io_device *dev)
{
	/* struct config *config; */

	/* config = dev->emu->config; */

	return 0;
}

static int controller_save_state(struct io_device *dev, int port,
				 struct save_state *state)
{
	struct controller_state *controller_state;
	uint8_t *buf, *ptr;
	char *chunk_name;
	size_t size;
	int rc;

	if (port < 0 || port > 3)
		return -1;

	controller_state = dev->private;

	size = pack_state(controller_state, controller_state_items, NULL);

	buf = malloc(size);
	if (!buf)
		return -1;

	ptr = buf;
	ptr += pack_state(controller_state, controller_state_items, ptr);

	switch(port) {
	case 0:
		chunk_name = "PAD0";
		break;
	case 1:
		chunk_name = "PAD1";
		break;
	case 2:
		chunk_name = "PAD2";
		break;
	case 3:
		chunk_name = "PAD3";
		break;
	}

	rc = save_state_add_chunk(state, chunk_name, buf, size);
	free(buf);

	if (rc < 0)
		return -1;

	return 0;
}

static int controller_load_state(struct io_device *dev, int port,
				 struct save_state *state)
{
	struct controller_state *controller_state;
	uint8_t *buf;
	char *chunk_name;
	size_t size;

	if (port < 0 || port > 3)
		return -1;

	controller_state = dev->private;

	switch(port) {
	case 0:
		chunk_name = "PAD0";
		break;
	case 1:
		chunk_name = "PAD1";
		break;
	case 2:
		chunk_name = "PAD2";
		break;
	case 3:
		chunk_name = "PAD3";
		break;
	}

	if (save_state_find_chunk(state, chunk_name, &buf, &size) < 0) {
		log_warn("Missing state chunk %s\n", chunk_name);
		return 0;
	}

	buf += unpack_state(controller_state, controller_state_items, buf);
	controller_state->latch |= 0xffff0000;

	return 0;
}
