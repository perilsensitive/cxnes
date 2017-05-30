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
#include "input.h"
#include "actions.h"

#define KEYBOARD_ENABLE 0x04
#define KEYBOARD_COLUMN 0x02
#define KEYBOARD_RESET  0x01

#define SUBOR_KEYBOARD_ROWS   26
#define FAMILY_BASIC_KEYBOARD_ROWS   20

static int keyboard_connect(struct io_device *);
static void keyboard_disconnect(struct io_device *);
static uint8_t keyboard_read(struct io_device *, int port, int mode,
			     uint32_t cycles);
static void keyboard_write(struct io_device *, uint8_t value, int mode,
			   uint32_t cycles);
static int keyboard_set_key(void *, uint32_t pressed, uint32_t key);

struct keyboard_state {
	uint8_t key_state[SUBOR_KEYBOARD_ROWS];
	int enabled;
	int index;
	uint8_t prev_write;
	int connected;
};

#define KEY(x,y) (((x) << 8)| (y))

static uint16_t subor_keyboard_translation[72] = {
	KEY(0x05,0x02), KEY(0x08,0x04), KEY(0x09,0x04), KEY(0x08,0x02),
	KEY(0xff,0xff), KEY(0x0f,0x10), KEY(0x09,0x08), KEY(0xff,0xff),
	KEY(0x09,0x02), KEY(0xff,0xff), KEY(0xff,0xff), KEY(0x0e,0x04),
	KEY(0xff,0xff), KEY(0x0e,0x10), KEY(0x0e,0x02), KEY(0xff,0xff),
	KEY(0x0f,0x02), KEY(0x07,0x04), KEY(0x06,0x08), KEY(0x0c,0x08),
	KEY(0x07,0x10), KEY(0x06,0x10), KEY(0x0f,0x04), KEY(0x07,0x08),
	KEY(0x07,0x02), KEY(0x06,0x04), KEY(0x0d,0x04), KEY(0x0d,0x10),
	KEY(0x0c,0x10), KEY(0x10,0x08), KEY(0x06,0x02), KEY(0x0d,0x08),
	KEY(0x0d,0x02), KEY(0x0c,0x04), KEY(0x00,0x04), KEY(0x10,0x04),
	KEY(0x11,0x10), KEY(0x01,0x10), KEY(0x0c,0x02), KEY(0x11,0x08),
	KEY(0x11,0x02), KEY(0x10,0x02), KEY(0x11,0x04), KEY(0x02,0x04),
	KEY(0x00,0x08), KEY(0x00,0x10), KEY(0x01,0x08), KEY(0x00,0x02),
	KEY(0x01,0x02), KEY(0x03,0x04), KEY(0x02,0x08), KEY(0x0b,0x04),
	KEY(0x03,0x10), KEY(0x0a,0x08), KEY(0x01,0x04), KEY(0x03,0x08),
	KEY(0x03,0x02), KEY(0x0b,0x02), KEY(0x0a,0x02), KEY(0x0b,0x10),
	KEY(0x0f,0x10), KEY(0xff,0xff), KEY(0x0b,0x08), KEY(0x02,0x02),
	KEY(0x05,0x10), KEY(0x08,0x08), KEY(0x04,0x10), KEY(0x08,0x10),
	KEY(0x09,0x10), KEY(0x10,0x10), KEY(0x05,0x08), KEY(0x04,0x02),
};

static struct input_event_handler keyboard_handlers[] = {
	{ ACTION_KEYBOARD_F1 & ACTION_PREFIX_MASK, keyboard_set_key },
	{ ACTION_NONE },
};

struct io_device keyboard_device = {
	.name = "Famicom Keyboard",
	.id = IO_DEVICE_KEYBOARD,
	.config_id = "familykeyboard",
	.connect = keyboard_connect,
	.disconnect = keyboard_disconnect,
	.read = keyboard_read,
	.write = keyboard_write,
	.handlers = keyboard_handlers,
	.port = PORT_EXP,
	.removable = 1,
};

struct io_device subor_keyboard_device = {
	.name = "Subor Keyboard",
	.id = IO_DEVICE_SUBOR_KEYBOARD,
	.config_id = "suborkeyboard",
	.connect = keyboard_connect,
	.disconnect = keyboard_disconnect,
	.read = keyboard_read,
	.write = keyboard_write,
	.handlers = keyboard_handlers,
	.port = PORT_EXP,
	.removable = 1,
};

static void keyboard_write(struct io_device *dev, uint8_t data, int mode,
			   uint32_t cycles)
{
	struct keyboard_state *state;
	uint8_t prev, changed;
	int rows;

	state = dev->private;

	if (!(data & KEYBOARD_ENABLE))
		return;

	if (dev->id == IO_DEVICE_SUBOR_KEYBOARD)
		rows = SUBOR_KEYBOARD_ROWS;
	else
		rows = FAMILY_BASIC_KEYBOARD_ROWS;

	prev = state->prev_write;
	changed = prev ^ data;

	if (changed & KEYBOARD_COLUMN)
		state->index = (state->index + 1) % rows;

	if (data & KEYBOARD_RESET)
		state->index = 0;

	state->prev_write = data;
}

static uint8_t keyboard_read(struct io_device *dev, int port, int mode,
			     uint32_t cycles)
{
	struct keyboard_state *state;
	uint8_t data;

	state = dev->private;

	if (port == 0)
		data = 0x00;
	else
		data = state->key_state[state->index];

	/* if (state->enabled) */
	/*      printf("keystate[%d] = 0x%02x\n", state->index, data); */

	return data;
}

static int keyboard_connect(struct io_device *dev)
{
	struct keyboard_state *state;

	state = malloc(sizeof(*state));
	if (!state)
		return -1;

	memset(state, 0, sizeof(*state));
	dev->private = state;

	memset(state->key_state, 0x1e, sizeof(state->key_state));

	state->prev_write = 0;
	state->enabled = 1;
	state->index = 0;

	return 0;
}

static void keyboard_disconnect(struct io_device *dev)
{
	if (dev->private)
		free(dev->private);
	dev->private = NULL;
}

static int keyboard_set_key(void *data, uint32_t pressed, uint32_t key)
{
	struct io_device *dev;
	struct keyboard_state *state;
	int offset, mask;
	int rows;

	dev = data;
	state = dev->private;

	/* Keep gcc happy */
	mask = 0;
	offset = 0;

	if (key == ACTION_KEYBOARD_DISABLE) {
		if (pressed)
			input_set_keyboard_mode(0);
		return 0;
	}

	if (dev->id == IO_DEVICE_SUBOR_KEYBOARD) {
		rows = SUBOR_KEYBOARD_ROWS;

		if (key == ACTION_KEYBOARD_RCTRL)
			key = ACTION_KEYBOARD_LCTRL;

		switch (key) {
		case ACTION_KEYBOARD_BS:
			key = KEY(4,0x04); break;
		case ACTION_KEYBOARD_CAPS:
			key = KEY(10,0x04); break;
		case ACTION_KEYBOARD_PGUP:
			key = KEY(5,0x04); break;
		case ACTION_KEYBOARD_PGDN:
			key = KEY(4,0x08); break;
		case ACTION_KEYBOARD_END:
			key = KEY(2,0x10); break;
		case ACTION_KEYBOARD_APOSTROPHE:
			key = KEY(14,0x08); break;
		case ACTION_KEYBOARD_EQUALS:
			key = KEY(15,0x08); break;
		case ACTION_KEYBOARD_PAUSE:
			printf("here: pressed=%d\n", pressed);
			key = KEY(10,0x10); break;
		case ACTION_KEYBOARD_BACKSLASH:
			key = KEY(9,0x08); break;
		case ACTION_KEYBOARD_TAB:
		case ACTION_KEYBOARD_BACKQUOTE:
		case ACTION_KEYBOARD_BREAK:
		case ACTION_KEYBOARD_RESET:
		case ACTION_KEYBOARD_NUMLOCK:
		case ACTION_KEYBOARD_LALT:
		case ACTION_KEYBOARD_RALT:
		case ACTION_KEYBOARD_F9:
		case ACTION_KEYBOARD_F10:
		case ACTION_KEYBOARD_F11:
		case ACTION_KEYBOARD_F12:
			break;
		default:
			key &= 0xffff;
			offset = key >> 8;
			mask = (key & 0x1e);

			switch (mask) {
			case 0x02: mask = 0; break;
			case 0x04: mask = 1; break;
			case 0x08: mask = 2; break;
			case 0x10: mask = 3; break;
			}

			key = subor_keyboard_translation[(offset << 2) + mask];
		}

		key &= 0xffff;
		offset = key >> 8;
		mask = (key & 0x1e);

	} else {
		rows = FAMILY_BASIC_KEYBOARD_ROWS;

		switch (key) {
		case ACTION_KEYBOARD_RCTRL:
			key = ACTION_KEYBOARD_LCTRL;
			break;
		case ACTION_KEYBOARD_BS:
			key = ACTION_KEYBOARD_DEL;
			break;
		}

		key &= 0xffff;

		offset = key >> 8;
		mask = (key & 0x1e);
	}


	if (offset >= rows)
		return 0;

	if (pressed)
		state->key_state[offset] &= ~mask;
	else
		state->key_state[offset] |= mask;

	return 0;
}
