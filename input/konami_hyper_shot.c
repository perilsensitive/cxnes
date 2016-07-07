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

static int konami_hyper_shot_connect(struct io_device *dev);
static void konami_hyper_shot_disconnect(struct io_device *dev);
static int konami_hyper_shot_set_button(void *, uint32_t, uint32_t);

static uint8_t konami_hyper_shot_read(struct io_device *, int port,
			     int mode, uint32_t cycles);
static void konami_hyper_shot_write(struct io_device *, uint8_t value,
			   int mode, uint32_t cycles);

struct konami_hyper_shot_state {
	int disabled;
	int buttons;
};

static struct input_event_handler konami_hyper_shot_handlers[] = {
	{ ACTION_KONAMI_HYPER_SHOT_P1_RUN & ACTION_PREFIX_MASK,
	  konami_hyper_shot_set_button},
	{ ACTION_NONE },
};

struct io_device konami_hyper_shot_device = {
	.name = "Konami Hyper Shot",
	.id = IO_DEVICE_KONAMI_HYPER_SHOT,
	.config_id = "konami_hyper_shot",
	.connect = konami_hyper_shot_connect,
	.disconnect = konami_hyper_shot_disconnect,
	.read = konami_hyper_shot_read,
	.write = konami_hyper_shot_write,
	.handlers = konami_hyper_shot_handlers,
	.port = PORT_EXP,
	.removable = 1,
};

static void konami_hyper_shot_write(struct io_device *dev, uint8_t data, int mode,
			   uint32_t cycles)
{
	struct konami_hyper_shot_state *state;

	state = dev->private;

	state->disabled = data & 0x06;
}

static uint8_t konami_hyper_shot_read(struct io_device *dev, int port, int mode,
			     uint32_t cycles)
{
	struct konami_hyper_shot_state *state;
	uint8_t data;

	state = dev->private;

	if (!port)
		return 0;

	data = state->buttons & 0x1e;

	if (state->disabled & 0x02)
		data &= 0x06;

	if (state->disabled & 0x04)
		data &= 0x18;

	return data;
}

static int konami_hyper_shot_connect(struct io_device *dev)
{
	struct konami_hyper_shot_state *state;

	state = malloc(sizeof(*state));
	if (!state)
		return -1;

	memset(state, 0, sizeof(*state));
	dev->private = state;

	return 0;
}

void konami_hyper_shot_disconnect(struct io_device *dev)
{
	if (dev->private)
		free(dev->private);
	dev->private = NULL;
}

int konami_hyper_shot_set_button(void *data, uint32_t pressed, uint32_t button)
{
	struct io_device *dev;
	struct konami_hyper_shot_state *state;

	dev = data;
	state = dev->private;
	button &= 0x1e;

	if (pressed)
		state->buttons |= button;
	else
		state->buttons &= ~button;

	return 0;
}
