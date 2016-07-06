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

static int mat_connect(struct io_device *dev);
static void mat_disconnect(struct io_device *dev);
static int mat_set_button(void *, uint32_t, uint32_t);

static uint8_t powerpad_read(struct io_device *, int port,
			     int mode, uint32_t cycles);
static void powerpad_write(struct io_device *, uint8_t value,
			   int mode, uint32_t cycles);
static uint8_t ftrainer_read(struct io_device *, int port,
			     int mode, uint32_t cycles);
static void ftrainer_write(struct io_device *, uint8_t value,
			   int mode, uint32_t cycles);

struct mat_state {
	uint32_t latch;
	uint32_t buttons;
	const uint16_t *lookup;
	int xor;
	int strobe;
};

static const uint16_t powerpad_lookup[] = {
	0x0004, 0x0001, 0x0008, 0x0002,
	0x0010, 0x0100, 0x4000, 0x0080,
	0x0040, 0x0400, 0x1000, 0x0020,
};

static const uint16_t family_trainer_lookup[] = {
	0x0008, 0x0004, 0x0002, 0x0001,
	0x0080, 0x0040, 0x0020, 0x0010,
	0x0800, 0x0400, 0x0200, 0x0100,
};

static struct input_event_handler powerpad_1_handlers[] = {
	{ ACTION_MAT_1_1 & ACTION_PREFIX_MASK, mat_set_button},
	{ ACTION_NONE },
};

static struct input_event_handler powerpad_2_handlers[] = {
	{ ACTION_MAT_2_1 & ACTION_PREFIX_MASK, mat_set_button},
	{ ACTION_NONE },
};

struct io_device powerpad_a1_device = {
	.name = "Power Pad Side A",
	.id = IO_DEVICE_POWERPAD_A1,
	.config_id = "powerpad_a",
	.connect = mat_connect,
	.disconnect = mat_disconnect,
	.read = powerpad_read,
	.write = powerpad_write,
	.handlers = powerpad_1_handlers,
	.port = PORT_1,
	.removable = 1,
};

struct io_device powerpad_b1_device = {
	.name = "Power Pad Side B",
	.id = IO_DEVICE_POWERPAD_B1,
	.config_id = "powerpad_b",
	.connect = mat_connect,
	.disconnect = mat_disconnect,
	.read = powerpad_read,
	.write = powerpad_write,
	.handlers = powerpad_1_handlers,
	.port = PORT_1,
	.removable = 1,
};

struct io_device powerpad_a2_device = {
	.name = "Power Pad Side A",
	.id = IO_DEVICE_POWERPAD_A2,
	.config_id = "powerpad_a",
	.connect = mat_connect,
	.disconnect = mat_disconnect,
	.read = powerpad_read,
	.write = powerpad_write,
	.handlers = powerpad_2_handlers,
	.port = PORT_2,
	.removable = 1,
};

struct io_device powerpad_b2_device = {
	.name = "Power Pad Side B",
	.id = IO_DEVICE_POWERPAD_B2,
	.config_id = "powerpad_b",
	.connect = mat_connect,
	.disconnect = mat_disconnect,
	.read = powerpad_read,
	.write = powerpad_write,
	.handlers = powerpad_2_handlers,
	.port = PORT_2,
	.removable = 1,
};

struct io_device ftrainer_a_device = {
	.name = "Family Trainer Side A",
	.id = IO_DEVICE_FTRAINER_A,
	.config_id = "ftrainer_a",
	.connect = mat_connect,
	.disconnect = mat_disconnect,
	.read = ftrainer_read,
	.write = ftrainer_write,
	.handlers = powerpad_2_handlers,
	.port = PORT_EXP,
	.removable = 1,
};

struct io_device ftrainer_b_device = {
	.name = "Family Trainer Side B",
	.id = IO_DEVICE_FTRAINER_B,
	.config_id = "ftrainer_b",
	.connect = mat_connect,
	.disconnect = mat_disconnect,
	.read = ftrainer_read,
	.write = ftrainer_write,
	.handlers = powerpad_2_handlers,
	.port = PORT_EXP,
	.removable = 1,
};

static void powerpad_write(struct io_device *dev, uint8_t data, int mode,
			   uint32_t cycles)
{
	struct mat_state *state;

	state = dev->private;

	if (!state->strobe && (data & 0x01)) {
		state->strobe = 1;
	} else if (state->strobe && !(data & 0x01)) {
		state->strobe = 0;

		state->latch = 0xffffaa00 | state->buttons;
	}
}

static uint8_t powerpad_read(struct io_device *dev, int port, int mode,
			     uint32_t cycles)
{
	struct mat_state *state;
	uint8_t data;

	state = dev->private;

	data = (state->latch & 0x03) << 3;
	state->latch >>= 2;
	state->latch |= (3 << 30);

	return data;
}

static void ftrainer_write(struct io_device *dev, uint8_t data, int mode,
			   uint32_t cycles)
{
	struct mat_state *state;
	int shift = 0;

	state = dev->private;
	data ^= 0x07;

	if (data & 0x1)
		shift = 8;
	else if (data & 0x2)
		shift = 4;
	else if (data & 0x4)
		shift = 0;

	state->latch = (state->buttons >> shift) & 0xf;
}

static uint8_t ftrainer_read(struct io_device *dev, int port, int mode,
			     uint32_t cycles)
{
	struct mat_state *state;

	if (port == 0)
		return 0;

	state = dev->private;
	return (state->latch ^ 0x0f) << 1;
}

static int mat_connect(struct io_device *dev)
{
	struct mat_state *state;

	state = malloc(sizeof(*state));
	if (!state)
		return -1;

	memset(state, 0, sizeof(*state));
	dev->private = state;

	switch (dev->id) {
	case IO_DEVICE_POWERPAD_A1:
		state->xor = 0x3;
		state->lookup = powerpad_lookup;
		break;
	case IO_DEVICE_POWERPAD_A2:
		state->xor = 0x3;
		state->lookup = powerpad_lookup;
		break;
	case IO_DEVICE_FTRAINER_A:
		state->xor = 0x3;
		state->lookup = family_trainer_lookup;
		break;
	case IO_DEVICE_POWERPAD_B1:
		state->xor = 0;
		state->lookup = powerpad_lookup;
		break;
	case IO_DEVICE_POWERPAD_B2:
		state->xor = 0;
		state->lookup = powerpad_lookup;
		break;
	case IO_DEVICE_FTRAINER_B:
		state->xor = 0;
		state->lookup = family_trainer_lookup;
		break;
	}

	return 0;
}

void mat_disconnect(struct io_device *dev)
{
	if (dev->private)
		free(dev->private);
	dev->private = NULL;
}

int mat_set_button(void *data, uint32_t pressed, uint32_t button)
{
	struct io_device *dev;
	struct mat_state *state;

	dev = data;
	state = dev->private;
	button &= 0xffff;
	button -= 1;

	button ^= state->xor;
	button = state->lookup[button];

	if (pressed)
		state->buttons |= button;
	else
		state->buttons &= ~button;

	return 0;
}
