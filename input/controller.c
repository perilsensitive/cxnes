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
static int controller_apply_config(struct io_device *dev);
static int controller_save_state(struct io_device *dev, int port,
				 struct save_state *state);
static int controller_load_state(struct io_device *dev, int port,
				 struct save_state *state);

struct controller_state {
	int latch;
	int strobe;
	int index;
	int is_snes;
	struct controller_common_state *common_state;
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
	.port = PORT_4,
	.removable = 1,
};

struct io_device bandai_hypershot_controller_device = {
	.name = "Bandai HyperShot Controller",
	.id = IO_DEVICE_BANDAI_HYPERSHOT_CONTROLLER,
	.config_id = "bandai_hypershot_controller",
	.connect = controller_connect,
	.disconnect = controller_disconnect,
	.read = controller_read,
	.write = controller_write,
	/* .save_state = controller_save_state, */
	/* .load_state = controller_load_state, */
	.apply_config = controller_apply_config,
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
	.port = PORT_4,
	.removable = 1,
};

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
		} else if (dev->id == IO_DEVICE_BANDAI_HYPERSHOT_CONTROLLER) {
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

	}
}

static uint8_t controller_read(struct io_device *dev, int port, int mode,
			       uint32_t cycles)
{
	struct controller_state *state;
	uint8_t data;

	state = dev->private;

	if ((dev->id == IO_DEVICE_BANDAI_HYPERSHOT_CONTROLLER) && port)
		return 0;

	data = state->latch & 0x01;
	state->latch >>= 1;

	if (mode != FOUR_PLAYER_MODE_NES) {
		if (state->is_snes)
			state->latch |= (1 << 15);
		else
			state->latch |= (1 << 7);
	}

	if (dev->id == IO_DEVICE_BANDAI_HYPERSHOT_CONTROLLER)
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
