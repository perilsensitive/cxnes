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

#define ACTION_UPDATELOCATION 1
#define ACTION_LEFTBUTTON 2
#define ACTION_RIGHTBUTTON 4

static int snes_mouse_connect(struct io_device *dev);
static void snes_mouse_disconnect(struct io_device *dev);
static uint8_t snes_mouse_read(struct io_device *, int port,
			       int four_player_mode, uint32_t cycles);
static void snes_mouse_write(struct io_device *, uint8_t value,
			     int four_player_mode, uint32_t cycles);
static int snes_mouse_set_button(void *, uint32_t, uint32_t);

struct snes_mouse_state {
	uint32_t latch;
	int sensitivity;
	int buttons;
	int strobe;
	int relx;
	int rely;
};

static struct input_event_handler snes_mouse1_handlers[] = {
	{ ACTION_MOUSE_1_LEFTBUTTON, snes_mouse_set_button},
	{ ACTION_MOUSE_1_RIGHTBUTTON, snes_mouse_set_button},
	{ ACTION_MOUSE_1_UPDATE_LOCATION, snes_mouse_set_button},
	{ ACTION_NONE },
};

static struct input_event_handler snes_mouse2_handlers[] = {
	{ ACTION_MOUSE_2_LEFTBUTTON, snes_mouse_set_button},
	{ ACTION_MOUSE_2_RIGHTBUTTON, snes_mouse_set_button},
	{ ACTION_MOUSE_2_UPDATE_LOCATION, snes_mouse_set_button},
	{ ACTION_NONE },
};

static struct input_event_handler snes_mouse3_handlers[] = {
	{ ACTION_MOUSE_3_LEFTBUTTON, snes_mouse_set_button},
	{ ACTION_MOUSE_3_RIGHTBUTTON, snes_mouse_set_button},
	{ ACTION_MOUSE_3_UPDATE_LOCATION, snes_mouse_set_button},
	{ ACTION_NONE },
};

static struct input_event_handler snes_mouse4_handlers[] = {
	{ ACTION_MOUSE_4_LEFTBUTTON, snes_mouse_set_button},
	{ ACTION_MOUSE_4_RIGHTBUTTON, snes_mouse_set_button},
	{ ACTION_MOUSE_4_UPDATE_LOCATION, snes_mouse_set_button},
	{ ACTION_NONE },
};

struct io_device snes_mouse1_device = {
	.name = "SNES Mouse",
	.id = IO_DEVICE_SNES_MOUSE_1,
	.config_id = "snes_mouse",
	.connect = snes_mouse_connect,
	.disconnect = snes_mouse_disconnect,
	.read = snes_mouse_read,
	.write = snes_mouse_write,
	.handlers = snes_mouse1_handlers,
	.port = PORT_1,
	.removable = 1,
};

struct io_device snes_mouse2_device = {
	.name = "SNES Mouse",
	.id = IO_DEVICE_SNES_MOUSE_2,
	.config_id = "snes_mouse",
	.connect = snes_mouse_connect,
	.disconnect = snes_mouse_disconnect,
	.read = snes_mouse_read,
	.write = snes_mouse_write,
	.handlers = snes_mouse2_handlers,
	.port = PORT_2,
	.removable = 1,
};

struct io_device snes_mouse3_device = {
	.name = "SNES Mouse",
	.id = IO_DEVICE_SNES_MOUSE_3,
	.config_id = "snes_mouse",
	.connect = snes_mouse_connect,
	.disconnect = snes_mouse_disconnect,
	.read = snes_mouse_read,
	.write = snes_mouse_write,
	.handlers = snes_mouse3_handlers,
	.port = PORT_3,
	.removable = 1,
};

struct io_device snes_mouse4_device = {
	.name = "SNES Mouse",
	.id = IO_DEVICE_SNES_MOUSE_4,
	.config_id = "snes_mouse",
	.connect = snes_mouse_connect,
	.disconnect = snes_mouse_disconnect,
	.read = snes_mouse_read,
	.write = snes_mouse_write,
	.handlers = snes_mouse4_handlers,
	.port = PORT_4,
	.removable = 1,
};

static void snes_mouse_write(struct io_device *dev, uint8_t data,
			     int four_player_mode, uint32_t cycles)
{
	struct snes_mouse_state *state;

	state = dev->private;

	if (!state->strobe && (data & 0x01)) {
		state->strobe = 1;
	} else if (state->strobe && !(data & 0x01)) {
		int relx, xneg;
		int rely, yneg;

		relx = state->relx;
		rely = state->rely;

		xneg = yneg = 0;

		if (relx < 0) {
			xneg = 0x80;
			relx = -relx;
		}

		if (rely < 0) {
			yneg = 0x80;
			rely = -rely;
		}

		if (relx > 127)
			relx = 127;

		if (rely > 127)
			rely = 127;

		relx >>= (2 - state->sensitivity);
		rely >>= (2 - state->sensitivity);

		relx |= xneg;
		rely |= yneg;

		state->latch = 0x00010000;
		state->latch |= state->buttons << 22;
		state->latch |= state->sensitivity << 20;
		state->latch |= rely << 8;
		state->latch |= relx;

		if (four_player_mode == FOUR_PLAYER_MODE_NES)
			state->latch &= 0xff;

		state->strobe = 0;
		state->relx = 0;
		state->rely = 0;
	}
}

static uint8_t snes_mouse_read(struct io_device *dev, int port,
			       int four_player_mode, uint32_t cycles)
{
	struct snes_mouse_state *state;
	uint8_t data;

	state = dev->private;

	if (state->strobe) {
		state->sensitivity = (state->sensitivity + 1) % 3;
		return 0;
	}

	data = (state->latch >> 31) & 1;
	state->latch <<= 1;

	return data;
}

static int snes_mouse_connect(struct io_device *dev)
{
	struct snes_mouse_state *state;

	state = malloc(sizeof(*state));
	if (!state)
		return -1;

	memset(state, 0, sizeof(*state));
	dev->private = state;

	return 0;
}

static void snes_mouse_disconnect(struct io_device *dev)
{
	if (dev->private)
		free(dev->private);
	dev->private = NULL;
}

static int snes_mouse_set_button(void *data, uint32_t pressed, uint32_t button)
{
	struct io_device *dev;
	struct snes_mouse_state *state;

	dev = data;
	state = dev->private;

	button &= 0x03;

	if (button == ACTION_LEFTBUTTON) {
		if (pressed)
			state->buttons |= 0x1;
		else
			state->buttons &= 0x2;
	}
	else if (button == ACTION_RIGHTBUTTON) {
		if (pressed)
			state->buttons |= 0x2;
		else
			state->buttons &= 0x1;
	} else if (button == ACTION_UPDATELOCATION) {
		int relx, rely;

		input_get_mouse_state(NULL, NULL, &relx, &rely);
		state->relx += relx;
		state->rely += rely;
	}

	return 0;
}
