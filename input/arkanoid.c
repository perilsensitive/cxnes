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
#include "actions.h"
#include "input.h"
#include "controller.h"

#define OLD_CONTROLLER_CENTER 0xa4
#define NEW_CONTROLLER_CENTER 0x98

#define TYPE_OLD_FC 0
#define TYPE_OLD_NES 1
#define TYPE_NEW_FC 2

#define ACTION_DIAL 0x01
#define ACTION_DIAL_MOUSE 0x02
#define ACTION_BUTTON 0x04

static int arkanoid_apply_config(struct io_device *dev);
static int arkanoid_nes_connect(struct io_device *dev);
static int arkanoid_fc_connect(struct io_device *dev);
static int arkanoid2_connect(struct io_device *dev);
static void arkanoid_disconnect(struct io_device *dev);
static uint8_t arkanoid_read(struct io_device *, int port, int mode,
			     uint32_t cycles);
static void arkanoid_write(struct io_device *, uint8_t value, int mode,
			   uint32_t cycles);
static int arkanoid_set_button(void *, uint32_t, uint32_t);

struct arkanoid_state {
	int latch[2];
	int dial[2];
	int strobe;
	int button[2];
	int type;
	int center;
	int paddle2_connected;
	int relx;
	int rely;
};

static struct input_event_handler arkanoid1_handlers[] = {
	{ ACTION_ARKANOID_1_DIAL_MOUSE,
	 arkanoid_set_button},
	{ ACTION_ARKANOID_1_DIAL, arkanoid_set_button},
	{ ACTION_ARKANOID_1, arkanoid_set_button},
	{ ACTION_NONE },
};

static struct input_event_handler arkanoid2_handlers[] = {
	{ ACTION_ARKANOID_2_DIAL_MOUSE,
	 arkanoid_set_button},
	{ ACTION_ARKANOID_2_DIAL, arkanoid_set_button},
	{ ACTION_ARKANOID_2, arkanoid_set_button},
	{ ACTION_NONE },
};

static struct input_event_handler arkanoid_fc_handlers[] = {
	{ ACTION_ARKANOID_2_DIAL_MOUSE,
	 arkanoid_set_button},
	{ ACTION_ARKANOID_2_DIAL, arkanoid_set_button},
	{ ACTION_ARKANOID_2, arkanoid_set_button},

	/* The "Port 1" handlers are used for the second paddle. */
	{ ACTION_ARKANOID_1_DIAL_MOUSE,
	 arkanoid_set_button},
	{ ACTION_ARKANOID_1_DIAL, arkanoid_set_button},
	{ ACTION_ARKANOID_1, arkanoid_set_button},
	{ ACTION_NONE },
};

struct io_device arkanoid_nes1_device = {
	.name = "Arkanoid Controller (NES)",
	.config_id = "nes_arkanoid",
	.id = IO_DEVICE_ARKANOID_NES_1,
	.connect = arkanoid_nes_connect,
	.disconnect = arkanoid_disconnect,
	.read = arkanoid_read,
	.write = arkanoid_write,
	.apply_config = arkanoid_apply_config,
	.handlers = arkanoid1_handlers,
	.port = PORT_1,
	.removable = 1,
};

struct io_device arkanoid_nes2_device = {
	.name = "Arkanoid Controller (NES)",
	.config_id = "nes_arkanoid",
	.id = IO_DEVICE_ARKANOID_NES_2,
	.connect = arkanoid_nes_connect,
	.disconnect = arkanoid_disconnect,
	.read = arkanoid_read,
	.write = arkanoid_write,
	.apply_config = arkanoid_apply_config,
	.handlers = arkanoid2_handlers,
	.port = PORT_2,
	.removable = 1,
};

struct io_device arkanoid_fc_device = {
	.name = "Arkanoid Controller (Famicom)",
	.config_id = "fc_arkanoid",
	.id = IO_DEVICE_ARKANOID_FC,
	.connect = arkanoid_fc_connect,
	.disconnect = arkanoid_disconnect,
	.read = arkanoid_read,
	.write = arkanoid_write,
	.apply_config = arkanoid_apply_config,
	.handlers = arkanoid_fc_handlers,
	.port = PORT_EXP,
	.removable = 1,
};

struct io_device arkanoidII_device = {
	.name = "Arkanoid II Controller",
	.config_id = "arkanoid2",
	.id = IO_DEVICE_ARKANOID_II,
	.connect = arkanoid2_connect,
	.disconnect = arkanoid_disconnect,
	.read = arkanoid_read,
	.write = arkanoid_write,
	.apply_config = arkanoid_apply_config,
	.handlers = arkanoid_fc_handlers,
	.port = PORT_EXP,
	.removable = 1,
};

static void arkanoid_write(struct io_device *dev, uint8_t data, int mode,
			   uint32_t cycles)
{
	struct arkanoid_state *state;

	state = dev->private;

	/* if (state->type == TYPE_OLD_FC && (data & 0x02)) { */
	/* 	state->type = TYPE_NEW_FC; */
	/* 	state->center = NEW_CONTROLLER_CENTER; */
	/* 	state->dial[0] = state->center; */
	/* 	state->dial[1] = state->center; */
	/* } */

	if (!state->strobe && (data & 0x01)) {
		state->strobe = 1;
	} else if (state->strobe && !(data & 0x01)) {
		int relx;
		int controller;

		controller = 0;

		relx = state->relx;

		state->dial[controller] += relx;

		if (state->dial[controller] < state->center - 0x50) {
			state->dial[controller] = state->center - 0x50;
		} else if (state->dial[controller] > state->center + 0x50) {
			state->dial[controller] = state->center + 0x50;
		}

		state->strobe = 0;
		state->relx = 0;
		state->rely = 0;
		state->latch[0] = state->dial[0] ^ 0xff;
		if (state->paddle2_connected)
			state->latch[1] = state->dial[1] ^ 0xff;
	}
}

static uint8_t arkanoid_read(struct io_device *dev, int port, int mode,
			     uint32_t cycles)
{
	struct arkanoid_state *state;
	uint8_t data;

	state = dev->private;

	data = 0;

	switch (state->type) {
	case TYPE_OLD_NES:
		data  = (state->latch[0] & 0x80) >> 3;
		data |= state->button[0] << 3;
		state->latch[0] = (state->latch[0] << 1) & 0xff;
		break;
	case TYPE_OLD_FC:
	case TYPE_NEW_FC:
		if (port) {
			data = (state->latch[0] & 0x80) >> 6;
			state->latch[0] = (state->latch[0] << 1) & 0xff;

			if (state->paddle2_connected) {
				data |= (state->latch[1] & 0x80) >> 3;
				data |= state->button[1] << 3;
				state->latch[1] = (state->latch[1] << 1) & 0xff;
			}
		} else {
			data = state->button[0] << 1;
		}
		break;
	}

	return data;
}

static int arkanoid_fc_connect(struct io_device *dev)
{
	struct arkanoid_state *state;

	state = malloc(sizeof(*state));
	if (!state)
		return -1;

	memset(state, 0, sizeof(*state));
	dev->private = state;
	state->dial[0] = OLD_CONTROLLER_CENTER;
	state->dial[1] = OLD_CONTROLLER_CENTER;
	state->type = TYPE_OLD_FC;
	state->center = OLD_CONTROLLER_CENTER;
	state->paddle2_connected =
		dev->emu->config->arkanoid_paddle2_connected;

	return 0;
}

static int arkanoid_nes_connect(struct io_device *dev)
{
	struct arkanoid_state *state;

	state = malloc(sizeof(*state));
	if (!state)
		return -1;

	memset(state, 0, sizeof(*state));
	dev->private = state;
	state->dial[0] = OLD_CONTROLLER_CENTER;
	state->dial[1] = OLD_CONTROLLER_CENTER;
	state->type = TYPE_OLD_NES;
	state->center = OLD_CONTROLLER_CENTER;

	return 0;
}

static int arkanoid2_connect(struct io_device *dev)
{
	struct arkanoid_state *state;

	state = malloc(sizeof(*state));
	if (!state)
		return -1;

	memset(state, 0, sizeof(*state));
	dev->private = state;
	state->dial[0] = NEW_CONTROLLER_CENTER;
	state->dial[1] = NEW_CONTROLLER_CENTER;
	state->type = TYPE_NEW_FC;
	state->center = NEW_CONTROLLER_CENTER;

	return 0;
}

static void arkanoid_disconnect(struct io_device *dev)
{
	if (dev->private)
		free(dev->private);
	dev->private = NULL;
}

static int arkanoid_set_button(void *data, uint32_t pressed, uint32_t button)
{
	struct io_device *dev;
	struct arkanoid_state *state;
	int value;
	int paddle;

	value = pressed;

	dev = data;
	state = dev->private;

	/* For the FC variants, use the buttons for "Port 1" as
	   the handlers for the second paddle, and the "Port 2"
	   handlers as the handlers for the first paddle.

	   This seems odd, but it allows us to share handlers
	   for the NES version in Port 2 and the first paddle
	   in both FC versions, meaning that if the user only
	   needs to change one set of handlers for all extant
	   games.
	*/
	paddle = ((button & 0xf0) >> 4) ^ 1;
	button &= 0x0f;

	if (state->type == TYPE_OLD_NES)
		paddle = 0;
	else if (paddle && !state->paddle2_connected)
		return 0;


	if (button == ACTION_DIAL_MOUSE) {
		int relx, rely;

		input_get_mouse_state(NULL, NULL, &relx, &rely);
		state->relx += relx;
		state->rely += rely;
	} else if (button == ACTION_DIAL) {
		state->dial[paddle] = state->center + value / 409;
	} else if (button == ACTION_BUTTON) {
		state->button[paddle] = pressed;
	}

	return 0;
}

static int arkanoid_apply_config(struct io_device *dev)
{
	struct arkanoid_state *state;

	state = dev->private;
	state->paddle2_connected =
		dev->emu->config->arkanoid_paddle2_connected;

	return 0;
}
