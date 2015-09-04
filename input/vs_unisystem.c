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

static uint8_t vs_unisystem_read(struct io_device *dev, int port,
				 int mode, uint32_t cycles);
static int vs_unisystem_connect(struct io_device *dev);
static void vs_unisystem_disconnect(struct io_device *dev);
static int vs_unisystem_buttons(void *data, uint32_t pressed, uint32_t button);
static void vs_unisystem_end_frame(struct io_device *dev, uint32_t cycles);

struct vs_unisystem_state {
	uint8_t coin_switch[2];
	uint8_t service_switch;
};

static struct input_event_handler vs_unisystem_handlers[] = {
	{ ACTION_VS_SERVICE_SWITCH, vs_unisystem_buttons},
	{ ACTION_VS_COIN_SWITCH_1, vs_unisystem_buttons},
	{ ACTION_VS_COIN_SWITCH_2, vs_unisystem_buttons},
	{ ACTION_DIP_SWITCH_1, vs_unisystem_buttons},
	{ ACTION_DIP_SWITCH_2, vs_unisystem_buttons},
	{ ACTION_DIP_SWITCH_3, vs_unisystem_buttons},
	{ ACTION_DIP_SWITCH_4, vs_unisystem_buttons},
	{ ACTION_DIP_SWITCH_5, vs_unisystem_buttons},
	{ ACTION_DIP_SWITCH_6, vs_unisystem_buttons},
	{ ACTION_DIP_SWITCH_7, vs_unisystem_buttons},
	{ ACTION_DIP_SWITCH_8, vs_unisystem_buttons},
	{ ACTION_NONE },
};

struct io_device vs_unisystem_device = {
	.name = "VS. Unisystem Switches",
	.id = IO_DEVICE_VS_UNISYSTEM,
	.config_id = "vssystem",
	.connect = vs_unisystem_connect,
	.disconnect = vs_unisystem_disconnect,
	.read = vs_unisystem_read,
	.end_frame = vs_unisystem_end_frame,
	.handlers = vs_unisystem_handlers,
	.port = PORT_EXP,
	.removable = 0,
};

static int vs_unisystem_buttons(void *data, uint32_t pressed, uint32_t button)
{
	struct io_device *dev;
	struct board *board;
	struct vs_unisystem_state *state;

	dev = data;
	board = dev->emu->board;
	state = dev->private;

	/* Coin switches should be "pressed" for approximately
	   3 frames.  If they're pressed too long some games
	   will ignore them as an anti-cheating measure.
	*/
	switch (button) {
	case ACTION_VS_COIN_SWITCH_1:
		if (pressed && !state->coin_switch[0])
			state->coin_switch[0] = 3;
		break;
	case ACTION_VS_COIN_SWITCH_2:
		if (pressed && !state->coin_switch[1])
			state->coin_switch[1] = 3;
		break;
	case ACTION_VS_SERVICE_SWITCH:
		state->service_switch = (pressed << 2);
		break;
	}

	if (!pressed)
		return 0;

	switch (button) {
	case ACTION_DIP_SWITCH_1:
		board_toggle_dip_switch(board, 1);
		break;
	case ACTION_DIP_SWITCH_2:
		board_toggle_dip_switch(board, 2);
		break;
	case ACTION_DIP_SWITCH_3:
		board_toggle_dip_switch(board, 3);
		break;
	case ACTION_DIP_SWITCH_4:
		board_toggle_dip_switch(board, 4);
		break;
	case ACTION_DIP_SWITCH_5:
		board_toggle_dip_switch(board, 5);
		break;
	case ACTION_DIP_SWITCH_6:
		board_toggle_dip_switch(board, 6);
		break;
	case ACTION_DIP_SWITCH_7:
		board_toggle_dip_switch(board, 7);
		break;
	case ACTION_DIP_SWITCH_8:
		board_toggle_dip_switch(board, 8);
		break;
	}

	return 0;
}

static int vs_unisystem_connect(struct io_device *dev)
{
	struct vs_unisystem_state *state;

	state = malloc(sizeof(*state));
	if (!state)
		return -1;

	board_set_num_dip_switches(dev->emu->board, 8);

	dev->private = state;
	memset(state, 0, sizeof(*state));

	return 0;
}

static void vs_unisystem_disconnect(struct io_device *dev)
{
	if (dev->private)
		free(dev->private);
	dev->private = NULL;
}

static uint8_t vs_unisystem_read(struct io_device *dev, int port,
				 int mode, uint32_t cycles)
{
	uint8_t result = 0;
	uint8_t dip_switches = board_get_dip_switches(dev->emu->board);
	struct vs_unisystem_state *state;
	state = dev->private;

	if (port == 0) {
		result = state->service_switch;
		result |= 0x80;
		result |= (dip_switches & 0x3) << 3;

		if (state->coin_switch[0])
			result |= 1 << 5;

		if (state->coin_switch[1])
			result |= 1 << 6;
	} else {
		result = dip_switches & 0xfc;
	}

	return result;
}

static void vs_unisystem_end_frame(struct io_device *dev, uint32_t cycles)
{
	struct vs_unisystem_state *state;

	state = dev->private;

	if (state->coin_switch[0])
		state->coin_switch[0]--;

	if (state->coin_switch[1])
		state->coin_switch[1]--;
}
	
