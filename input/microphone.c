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

static int microphone_connect(struct io_device *dev);
static void microphone_disconnect(struct io_device *dev);
static int microphone_set_button(void *, uint32_t, uint32_t);

static uint8_t microphone_read(struct io_device *, int port,
			     int mode, uint32_t cycles);

struct microphone_state {
	uint32_t counter;
	int pressed;
};

static struct input_event_handler microphone_handlers[] = {
	{ ACTION_MICROPHONE, microphone_set_button},
	{ ACTION_NONE },
};

struct io_device microphone_device = {
	.name = "Famicom Microphone",
	.id = IO_DEVICE_FAMICOM_MICROPHONE,
	.config_id = "microphone",
	.connect = microphone_connect,
	.disconnect = microphone_disconnect,
	.read = microphone_read,
	.handlers = microphone_handlers,
	.port = PORT_EXP,
	.removable = 0,
};

/*
  This implementation has been tested with Zelda no Densetsu (FDS) and
  Bokosuka Wars (uses the mic for a secret code when resetting the game).
  This works for both, and may work for others.
*/
static uint8_t microphone_read(struct io_device *dev, int port, int mode,
			       uint32_t cycles)
{
	struct microphone_state *state;
	uint8_t data;

	state = dev->private;

	if (state->counter)
		state->counter--;
	else
		state->counter = 4;

	if (state->pressed && state->counter)
		data = 0x04;
	else
		data = 0;

	return data;
}

static int microphone_connect(struct io_device *dev)
{
	struct microphone_state *state;

	state = malloc(sizeof(*state));
	if (!state)
		return -1;

	memset(state, 0, sizeof(*state));
	dev->private = state;

	return 0;
}

void microphone_disconnect(struct io_device *dev)
{
	if (dev->private)
		free(dev->private);
	dev->private = NULL;
}

int microphone_set_button(void *data, uint32_t pressed, uint32_t button)
{
	struct io_device *dev;
	struct microphone_state *state;

	dev = data;
	state = dev->private;

	if (pressed) {
		state->pressed = 1;
		state->counter = 50;
	} else {
		state->pressed = 0;
		state->counter = 0;
	}

	return 0;
}
