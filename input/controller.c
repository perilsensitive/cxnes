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

#define BUTTON_A 1
#define BUTTON_B 2
#define BUTTON_SEL 4
#define BUTTON_ST 8
#define BUTTON_UP 16
#define BUTTON_DN 32
#define BUTTON_L  64
#define BUTTON_R  128
#define VS_BUTTON_MASK (BUTTON_SEL|BUTTON_ST)

/* Implementation of standard NES/Famicom controllers,
   FourScore and Famicom-style 4-player adapters. */

/* Compatibility notes:

   - All Famicom accessories are designed to work with the standard
     controllers still connected since the original FC had hardwired
     controllers.

   - FourScore mode should be OK for every game and accessory (unless
     the game doesn't like getting something other than all 0s or 1s
     after the first 8 reads or the accessory uses P1,D0).  The
     FourScore only uses D0 on both ports, leaving D1-D4 of both ports
     wide open.

   - All three American accessories (Zapper, Power Pad, and Arkanoid
     controller) use D3 and D4 for output, so they should work even
     with a controller also connected to the same port (in theory).
     They should also work in Famicom 4-player mode since the 3rd and
     4th controllers use P0,D1 and P1,D1.

   - Famicom 4-player mode isn't compatible with most FC accessories,
     as they tend to use P0,D1 and/or P1,D1 for output.  The Light Gun
     is an exception, since it uses P1,D3 and P1,D4.
*/

static int controller_connect(struct io_device *dev);
static void controller_disconnect(struct io_device *dev);
static uint8_t controller_read(struct io_device *, int port, int mode,
			       uint32_t cycles);
static void controller_write(struct io_device *, uint8_t value,
			     int mode, uint32_t cycles);
static int controller_apply_config(struct io_device *dev);

struct controller_state {
	int prev_latch;
	int latch;
	int strobe;
	int index;
	struct controller_common_state *common_state;
};

struct io_device controller1_device = {
	.name = "Controller 1",
	.id = IO_DEVICE_CONTROLLER_1,
	.config_id = "controller1",
	.connect = controller_connect,
	.disconnect = controller_disconnect,
	.read = controller_read,
	.write = controller_write,
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

		if (state->common_state->current_state)
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
		}

		if (dev->emu->config->swap_a_b) {
			int tmp = state->latch & (BUTTON_A | BUTTON_B);
			state->latch &= ~(BUTTON_A | BUTTON_B);
			state->latch |= (tmp & BUTTON_A) << 1;
			state->latch |= (tmp & BUTTON_B) >> 1;
		}

		if (dev->emu->config->mask_opposite_directions) {
			if ((state->latch & (BUTTON_L|BUTTON_R)) ==
			    (BUTTON_L|BUTTON_R)) {
				state->latch ^= BUTTON_L|BUTTON_R;
			}
				
			if ((state->latch & (BUTTON_UP|BUTTON_DN)) ==
			    (BUTTON_UP|BUTTON_DN)) {
				state->latch ^= BUTTON_UP|BUTTON_DN;
			}
		}

		state->latch |= ~0xff;

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

	data = state->latch & 0x01;
	state->latch >>= 1;

	if (mode != FOUR_PLAYER_MODE_NES)
		state->latch |= (1 << 7);

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
	case IO_DEVICE_CONTROLLER_1:
		state->index = 0;
		break;
	case IO_DEVICE_CONTROLLER_2:
		state->index = 1;
		break;
	case IO_DEVICE_CONTROLLER_3:
		state->index = 2;
		break;
	case IO_DEVICE_CONTROLLER_4:
		state->index = 3;
		break;
	default:
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
