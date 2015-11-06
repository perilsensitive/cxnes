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

/* KANA */
/* @ */
/* CLR */
/* : */
/* GRPH */
/* DEL */
/* STOP */
/* ^ */

#define F(x,y) (((x) << 8)| (y))

static uint16_t subor_keyboard_translation[72] = {
	F(0x05,0x02), F(0x08,0x04), F(0x09,0x04), F(0x08,0x02),
	F(0xff,0xff), F(0x0f,0x10), F(0x09,0x08), F(0xff,0xff),
	F(0x09,0x02), F(0xff,0xff), F(0xff,0xff), F(0x0e,0x04),
	F(0xff,0xff), F(0x0e,0x10), F(0x0e,0x02), F(0xff,0xff),
	F(0x0f,0x02), F(0x07,0x04), F(0x06,0x08), F(0x0c,0x08),
	F(0x07,0x10), F(0x06,0x10), F(0x0f,0x04), F(0x07,0x08),
	F(0x07,0x02), F(0x06,0x04), F(0x0d,0x04), F(0x0d,0x10),
	F(0x0c,0x10), F(0x10,0x08), F(0x06,0x02), F(0x0d,0x08),
	F(0x0d,0x02), F(0x0c,0x04), F(0x00,0x04), F(0x10,0x04),
	F(0x11,0x10), F(0x01,0x10), F(0x0c,0x02), F(0x11,0x08),
	F(0x11,0x02), F(0x10,0x02), F(0x11,0x04), F(0x02,0x04),
	F(0x00,0x08), F(0x00,0x10), F(0x01,0x08), F(0x00,0x02),
	F(0x01,0x02), F(0x03,0x04), F(0x02,0x08), F(0x0b,0x04),
	F(0x03,0x10), F(0x0a,0x08), F(0x01,0x04), F(0x03,0x08),
	F(0x03,0x02), F(0x0b,0x02), F(0x0a,0x02), F(0x0b,0x10),
	F(0x0f,0x10), F(0xff,0xff), F(0x0b,0x08), F(0x02,0x02),
	F(0xff,0xff), F(0x08,0x08), F(0x04,0x10), F(0x08,0x10),
	F(0x09,0x10), F(0x10,0x10), F(0x05,0x08), F(0x04,0x02),
};

static struct input_event_handler keyboard_handlers[] = {
	{ ACTION_KEYBOARD_F1, keyboard_set_key},
	{ ACTION_KEYBOARD_F2, keyboard_set_key},
	{ ACTION_KEYBOARD_F3, keyboard_set_key},
	{ ACTION_KEYBOARD_F4, keyboard_set_key},
	{ ACTION_KEYBOARD_F5, keyboard_set_key},
	{ ACTION_KEYBOARD_F6, keyboard_set_key},
	{ ACTION_KEYBOARD_F7, keyboard_set_key},
	{ ACTION_KEYBOARD_F8, keyboard_set_key},
	{ ACTION_KEYBOARD_F9, keyboard_set_key},
	{ ACTION_KEYBOARD_F10, keyboard_set_key},
	{ ACTION_KEYBOARD_F11, keyboard_set_key},
	{ ACTION_KEYBOARD_F12, keyboard_set_key},
	{ ACTION_KEYBOARD_1, keyboard_set_key},
	{ ACTION_KEYBOARD_2, keyboard_set_key},
	{ ACTION_KEYBOARD_3, keyboard_set_key},
	{ ACTION_KEYBOARD_4, keyboard_set_key},
	{ ACTION_KEYBOARD_5, keyboard_set_key},
	{ ACTION_KEYBOARD_6, keyboard_set_key},
	{ ACTION_KEYBOARD_7, keyboard_set_key},
	{ ACTION_KEYBOARD_8, keyboard_set_key},
	{ ACTION_KEYBOARD_9, keyboard_set_key},
	{ ACTION_KEYBOARD_0, keyboard_set_key},
	{ ACTION_KEYBOARD_MINUS, keyboard_set_key},
	{ ACTION_KEYBOARD_YEN, keyboard_set_key},
	{ ACTION_KEYBOARD_STOP, keyboard_set_key},
	{ ACTION_KEYBOARD_ESCAPE, keyboard_set_key},
	{ ACTION_KEYBOARD_q, keyboard_set_key},
	{ ACTION_KEYBOARD_w, keyboard_set_key},
	{ ACTION_KEYBOARD_e, keyboard_set_key},
	{ ACTION_KEYBOARD_r, keyboard_set_key},
	{ ACTION_KEYBOARD_t, keyboard_set_key},
	{ ACTION_KEYBOARD_y, keyboard_set_key},
	{ ACTION_KEYBOARD_u, keyboard_set_key},
	{ ACTION_KEYBOARD_i, keyboard_set_key},
	{ ACTION_KEYBOARD_o, keyboard_set_key},
	{ ACTION_KEYBOARD_p, keyboard_set_key},
	{ ACTION_KEYBOARD_AT, keyboard_set_key},
	{ ACTION_KEYBOARD_LEFTBRACKET, keyboard_set_key},
	{ ACTION_KEYBOARD_ENTER, keyboard_set_key},
	{ ACTION_KEYBOARD_CTRL, keyboard_set_key},
	{ ACTION_KEYBOARD_a, keyboard_set_key},
	{ ACTION_KEYBOARD_s, keyboard_set_key},
	{ ACTION_KEYBOARD_d, keyboard_set_key},
	{ ACTION_KEYBOARD_f, keyboard_set_key},
	{ ACTION_KEYBOARD_g, keyboard_set_key},
	{ ACTION_KEYBOARD_h, keyboard_set_key},
	{ ACTION_KEYBOARD_j, keyboard_set_key},
	{ ACTION_KEYBOARD_k, keyboard_set_key},
	{ ACTION_KEYBOARD_l, keyboard_set_key},
	{ ACTION_KEYBOARD_SEMICOLON, keyboard_set_key},
	{ ACTION_KEYBOARD_COLON, keyboard_set_key},
	{ ACTION_KEYBOARD_RIGHTBRACKET, keyboard_set_key},
	{ ACTION_KEYBOARD_KANA, keyboard_set_key},
	{ ACTION_KEYBOARD_LSHIFT, keyboard_set_key},
	{ ACTION_KEYBOARD_z, keyboard_set_key},
	{ ACTION_KEYBOARD_x, keyboard_set_key},
	{ ACTION_KEYBOARD_c, keyboard_set_key},
	{ ACTION_KEYBOARD_v, keyboard_set_key},
	{ ACTION_KEYBOARD_b, keyboard_set_key},
	{ ACTION_KEYBOARD_n, keyboard_set_key},
	{ ACTION_KEYBOARD_m, keyboard_set_key},
	{ ACTION_KEYBOARD_COMMA, keyboard_set_key},
	{ ACTION_KEYBOARD_PERIOD, keyboard_set_key},
	{ ACTION_KEYBOARD_SLASH, keyboard_set_key},
	{ ACTION_KEYBOARD_UNDERSCORE, keyboard_set_key},
	{ ACTION_KEYBOARD_RSHIFT, keyboard_set_key},
	{ ACTION_KEYBOARD_GRPH, keyboard_set_key},
	{ ACTION_KEYBOARD_SPACE, keyboard_set_key},
	{ ACTION_KEYBOARD_CLR, keyboard_set_key},
	{ ACTION_KEYBOARD_INS, keyboard_set_key},
	{ ACTION_KEYBOARD_DEL, keyboard_set_key},
	{ ACTION_KEYBOARD_UP, keyboard_set_key},
	{ ACTION_KEYBOARD_DOWN, keyboard_set_key},
	{ ACTION_KEYBOARD_LEFT, keyboard_set_key},
	{ ACTION_KEYBOARD_RIGHT, keyboard_set_key},
	{ ACTION_KEYBOARD_BS, keyboard_set_key},
	{ ACTION_KEYBOARD_TAB, keyboard_set_key},
 	{ ACTION_KEYBOARD_CAPS, keyboard_set_key},
	{ ACTION_KEYBOARD_PGUP, keyboard_set_key},
	{ ACTION_KEYBOARD_PGDN, keyboard_set_key},
	{ ACTION_KEYBOARD_HOME, keyboard_set_key},
	{ ACTION_KEYBOARD_END, keyboard_set_key},
	{ ACTION_KEYBOARD_ALT, keyboard_set_key},
	{ ACTION_KEYBOARD_TILDE, keyboard_set_key},
	{ ACTION_KEYBOARD_APOSTROPHE, keyboard_set_key},
	{ ACTION_KEYBOARD_EQUALS, keyboard_set_key},
	{ ACTION_KEYBOARD_PAUSE, keyboard_set_key},
	{ ACTION_KEYBOARD_BREAK, keyboard_set_key},
	{ ACTION_KEYBOARD_RESET, keyboard_set_key},
	{ ACTION_KEYBOARD_NUMLOCK, keyboard_set_key},
	{ ACTION_KEYBOARD_BACKSLASH, keyboard_set_key},
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

	if (dev->id == IO_DEVICE_SUBOR_KEYBOARD) {
		state->key_state[18] = 0x00;
		state->key_state[19] = 0x00;
	}

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

	dev = data;
	state = dev->private;

	/* Keep gcc happy */
	mask = 0;
	offset = 0;

	if (key == ACTION_KEYBOARD_ENABLE) {
		return 0;
	}

	if (dev->id == IO_DEVICE_SUBOR_KEYBOARD) {
		uint16_t map;

		printf("key: %x\n", key);

		switch (key) {
		case ACTION_KEYBOARD_BS:
			offset = 4; mask = 0x04; break;
		case ACTION_KEYBOARD_CAPS:
			offset = 10; mask = 0x04; break;
		case ACTION_KEYBOARD_PGUP:
			offset = 5; mask = 0x04; break;
		case ACTION_KEYBOARD_PGDN:
			offset = 4; mask = 0x08; break;
		case ACTION_KEYBOARD_END:
			offset = 2; mask = 0x10; break;
		case ACTION_KEYBOARD_APOSTROPHE:
			offset = 14; mask = 0x08; break;
		case ACTION_KEYBOARD_EQUALS:
			offset = 15; mask = 0x08; break;
		case ACTION_KEYBOARD_PAUSE:
			offset = 10; mask = 0x10; break;
		case ACTION_KEYBOARD_BACKSLASH:
			offset = 9; mask = 0x08; break;
		case ACTION_KEYBOARD_TAB:
			offset = 24; mask = 0x02; break;
		case ACTION_KEYBOARD_TILDE:
		case ACTION_KEYBOARD_BREAK:
		case ACTION_KEYBOARD_RESET:
		case ACTION_KEYBOARD_NUMLOCK:
		case ACTION_KEYBOARD_ALT:
		case ACTION_KEYBOARD_F9:
		case ACTION_KEYBOARD_F10:
		case ACTION_KEYBOARD_F11:
		case ACTION_KEYBOARD_F12:
			break;
		default:
			key &= 0xffff;

			offset = key >> 8;
			mask = (key & 0x1e);

			printf("old offset = %d, mask = %d\n", offset, mask);

			switch (mask) {
			case 0x02: mask = 0; break;
			case 0x04: mask = 1; break;
			case 0x08: mask = 2; break;
			case 0x10: mask = 3; break;
			}

			map = subor_keyboard_translation[(offset << 2) + mask];
			offset = map >> 8;
			mask = map & 0x1e;
			printf("new offset = %d, mask = %d\n", offset, mask);
		}

	} else {
		key &= 0xffff;

		offset = key >> 8;
		mask = (key & 0x1e);
	}


	if (pressed)
		state->key_state[offset] &= ~mask;
	else
		state->key_state[offset] |= mask;

	return 0;
}
