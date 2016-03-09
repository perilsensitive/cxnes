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
#include "video.h"
#include "input.h"
#include "actions.h"

#define TRIGGER_FRAMES 2
#define SENSOR_DELAY 25

#define UPDATE_LOCATION 1
#define TRIGGER         2
#define TRIGGER_OFFSCREEN 4

static int zapper_connect(struct io_device *);
static void zapper_disconnect(struct io_device *);
static void zapper_reset(struct io_device *, int hard);
static uint8_t zapper_read(struct io_device *, int port, int mode,
			   uint32_t cycles);
static uint8_t zapper_vs_read(struct io_device *, int port, int mode,
			      uint32_t cycles);
static void zapper_vs_write(struct io_device *, uint8_t data, int mode,
			    uint32_t cycles);
static void zapper_end_frame(struct io_device *, uint32_t cycles);
static int zapper_set_trigger(void *, uint32_t, uint32_t);

struct input_event_handler zapper1_handlers[] = {
	{ ACTION_ZAPPER_1_TRIGGER & ACTION_PREFIX_MASK, zapper_set_trigger},
	{ ACTION_NONE },
};

struct input_event_handler zapper2_handlers[] = {
	{ ACTION_ZAPPER_1_TRIGGER & ACTION_PREFIX_MASK, zapper_set_trigger},
	{ ACTION_NONE },
};

struct io_device zapper1_device = {
	.name = "Zapper",
	.id = IO_DEVICE_ZAPPER_1,
	.config_id = "zapper",
	.handlers = zapper1_handlers,
	.connect = zapper_connect,
	.disconnect = zapper_disconnect,
	.reset = zapper_reset,
	.end_frame = zapper_end_frame,
	.read = zapper_read,
	.port = PORT_1,
	.removable = 1,
};

struct io_device zapper2_device = {
	.name = "Zapper",
	.id = IO_DEVICE_ZAPPER_2,
	.config_id = "zapper",
	.handlers = zapper2_handlers,
	.connect = zapper_connect,
	.disconnect = zapper_disconnect,
	.reset = zapper_reset,
	.end_frame = zapper_end_frame,
	.read = zapper_read,
	.port = PORT_2,
	.removable = 1,
};

struct io_device vs_zapper_device = {
	.name = "VS. Zapper",
	.id = IO_DEVICE_VS_ZAPPER,
	.config_id = "vs_zapper",
	.handlers = zapper2_handlers,
	.connect = zapper_connect,
	.disconnect = zapper_disconnect,
	.reset = zapper_reset,
	.end_frame = zapper_end_frame,
	.read = zapper_vs_read,
	.write = zapper_vs_write,
	.port = PORT_EXP,
	.removable = 1,
};

struct zapper_state {
	int trigger_counter;
	int away_from_screen;
	int light;
	uint8_t latch;
	int strobe;
	int x;
	int y;
};

static int check_pixel(uint32_t pixel)
{
	int lit = 0;

	/* FIXME hack works on 2C02 only */
	if ((pixel & 0x0f) < 0x0e) {
		lit = 1;

		if ((pixel & 0x30) < 0x20)
			lit = 0;
	}

	return lit;
}

static void zapper_reset(struct io_device *dev, int hard)
{
}

static uint8_t zapper_read(struct io_device *dev, int port,
			   int mode, uint32_t cycles)
{
	uint8_t data;
	uint32_t color;
	int light;
	int scanline, cycle;
	int odd, short_frame;
	struct zapper_state *state;
	int x, y;

	state = dev->private;

	light = 0;
	ppu_run(dev->emu->ppu, cycles);
	ppu_get_cycles(dev->emu->ppu, &scanline, &cycle, &odd, &short_frame);
	x = state->x;
	y = state->y;
	video_calc_nes_coord(&x, &y);

	if (scanline < y ||
	    scanline > y + SENSOR_DELAY) {
		light = 0;
	} else if (scanline == y && cycle < x) {
		light = 0;
	} else if (scanline == y + SENSOR_DELAY &&
		   cycle > x) {
		light = 0;
	} else {
		if (!state->away_from_screen) {
			color =
			    video_get_pixel_color(x, y);
			light = check_pixel(color);
		}
	}

	data = (!light) << 3;

	if (state->trigger_counter >= 0)
		data |= (1 << 4);

	return data;
}

static void zapper_vs_write(struct io_device *dev, uint8_t data,
			    int mode, uint32_t cycles)
{
	struct zapper_state *state;

	state = dev->private;

	if (!state->strobe && (data & 0x01)) {
		state->strobe = 1;
	} else if (state->strobe && !(data & 0x01)) {
		state->strobe = 0;

		/* The VS. Unisystem light gun data is similar to the
		   zapper data, except it is latched by bits 6 and 7
		   instead of being returned in D3 and D4.  Also the
		   sense of the light detect bit is inverted (1 is
		   true, 0 is false).

		   Additionally, bit 4 is set to indicate the gun is
		   connected.  If it is not set, the game will play
		   back an "alarm" sound instead of normal audio.
		 */
		state->latch =
			((zapper_read(dev, 1, mode,
				      cycles)<< 3) ^ 0x40) | 0x10;
	}
}

static uint8_t zapper_vs_read(struct io_device *dev, int port,
			      int mode, uint32_t cycles)
{
	struct zapper_state *state;
	uint8_t data;

	if (port)
		return 0;

	state = dev->private;

	data = state->latch & 1;
	state->latch >>= 1;
	state->latch |= 0x80;

	return data;
}

static void zapper_disconnect(struct io_device *dev)
{
	video_show_cursor(0);
	video_enable_crosshairs(0);
	if (dev->private)
		free(dev->private);
	dev->private = NULL;
}

static int zapper_connect(struct io_device *dev)
{
	struct zapper_state *state;

	state = malloc(sizeof(*state));
	if (!state)
		return -1;

	dev->private = state;

	video_show_cursor(1);
	memset(state, 0, sizeof(*state));
	state->trigger_counter = -1;
	state->trigger_counter = -1;

	/* FIXME On the NES, it's possible to plug the zapper into
	   either port and read it, although most games expect it to
	   be plugged into the second port.  In fact, it's possible to
	   use two zappers at the same time.  Only one commercial
	   game, Chiller, supports this setup.  Zap Ruder, by Tepples,
	   is the only known homebrew to support two Zappers.

	   On the Famicom, the light gun plugs into the expansion port
	   and so can be read by reading $4017.

	   For now, only support one zapper and assume it's connected
	   to $4017.
	 */

	video_show_cursor(1);
	video_enable_crosshairs(1);

	return 0;
}

int zapper_set_trigger(void *data, uint32_t value, uint32_t button)
{
	int offscreen;
	struct zapper_state *state;
	struct io_device *dev;

	if (!value)
		return 0;

	/* This is a bit hacky.  io_register_device() will return
	   a pointer to the device if it already exists.
	 */
	dev = data;
	state = dev->private;

	offscreen = 0;
	switch (button & 0xf) {
	case UPDATE_LOCATION:
		input_get_mouse_state(&state->x, &state->y, NULL, NULL);
		return 0;
		break;
	case TRIGGER:
		offscreen = 0;
		break;
	case TRIGGER_OFFSCREEN:
		offscreen = 1;
		break;
	}

	if (state->trigger_counter >= 0)
		return 0;

	state->away_from_screen = offscreen;

	state->trigger_counter = TRIGGER_FRAMES;

	return 0;
}

static void zapper_end_frame(struct io_device *dev, uint32_t cycles)
{
	struct zapper_state *state = dev->private;

	if (state->trigger_counter >= 0)
		state->trigger_counter--;
}
