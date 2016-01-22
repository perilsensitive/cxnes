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

#ifndef __IO_H__
#define __IO_H__

#include "emu.h"
#include "input.h"

enum {
	PORT_1,
	PORT_2,
	PORT_3,
	PORT_4,
	PORT_EXP,
};

enum {
	FOUR_PLAYER_MODE_AUTO,
	FOUR_PLAYER_MODE_NONE,
	FOUR_PLAYER_MODE_NES,
	FOUR_PLAYER_MODE_FC
};

enum {
	VS_CONTROLLER_MODE_AUTO,
	VS_CONTROLLER_MODE_STANDARD,
	VS_CONTROLLER_MODE_SWAPPED,
	VS_CONTROLLER_MODE_VSSUPERSKYKID,
	VS_CONTROLLER_MODE_VSPINBALLJ,
	VS_CONTROLLER_MODE_BUNGELINGBAY,
};

enum {
IO_DEVICE_NONE,
IO_DEVICE_AUTO,
IO_DEVICE_CONTROLLER_1,
IO_DEVICE_CONTROLLER_2,
IO_DEVICE_CONTROLLER_3,
IO_DEVICE_CONTROLLER_4,
IO_DEVICE_CONTROLLER_COMMON,
IO_DEVICE_FOURSCORE,
IO_DEVICE_ZAPPER_1,
IO_DEVICE_ZAPPER_2,
IO_DEVICE_POWERPAD_A1,
IO_DEVICE_POWERPAD_B1,
IO_DEVICE_POWERPAD_A2,
IO_DEVICE_POWERPAD_B2,
IO_DEVICE_ARKANOID_NES_1,
IO_DEVICE_ARKANOID_NES_2,
IO_DEVICE_SNES_MOUSE_1,
IO_DEVICE_SNES_MOUSE_2,
IO_DEVICE_SNES_MOUSE_3,
IO_DEVICE_SNES_MOUSE_4,
IO_DEVICE_ARKANOID_FC,
IO_DEVICE_ARKANOID_II,
IO_DEVICE_VS_ZAPPER,
IO_DEVICE_KEYBOARD,
IO_DEVICE_VS_UNISYSTEM,
IO_DEVICE_VS_UNISYSTEM_BANKSWITCH,
IO_DEVICE_FTRAINER_A,
IO_DEVICE_FTRAINER_B,
IO_DEVICE_FAMICOM_MICROPHONE,
IO_DEVICE_SUBOR_KEYBOARD,
};


struct controller_common_state {
	int buttons[4];
	int current_state[4];
	int turbo_buttons[4];
	int turbo_toggles[4];
	int turbo_mask[4];
	int turbo_cycle_length;
	int turbo_pressed_frames;
	int turbo_counter;
	int port_mapping[4];
	int vs_controller_mode;
};

struct io_device {
	int (*connect) (struct io_device *);
	void (*disconnect) (struct io_device *);
	void (*reset) (struct io_device *, int hard);
	void (*end_frame) (struct io_device *, uint32_t cycles);
	void (*run) (struct io_device *, uint32_t cycles);
	uint8_t(*read) (struct io_device *, int port, int mode,
			uint32_t cycles);
	void (*write) (struct io_device *, uint8_t value, int mode,
		       uint32_t cycles);
	int (*apply_config) (struct io_device *);
	int (*save_state) (struct io_device *, int, struct save_state *);
	int (*load_state) (struct io_device *, int, struct save_state *);
	struct input_event_handler *handlers;
	const char *name;
	const char *config_id;
	int id;
	int connected;
	int port;
	int removable;
	struct io_device *next;
	void *private;
	struct emu *emu;
};

int io_init(struct emu *emu);
void io_cleanup(struct io_state *io);
void io_reset(struct io_state *io, int hard);
int io_apply_config(struct io_state *io);
void io_end_frame(struct io_state *io, uint32_t cycles);
void io_run(struct io_state *io, uint32_t cycles);
struct io_device *io_register_device(struct io_state *,
				     struct io_device *device,
				     int port);
void io_ui_device_select(struct io_state *, int);
void io_ui_device_connect(struct io_state *, int);
void io_connect_device(struct io_device *);
void io_disconnect_device(struct io_device *);
void io_set_four_player_mode(struct io_state *io, int mode,
			     int display);
void io_set_auto_four_player_mode(struct io_state *io, int mode);
void io_set_auto_device(struct io_state *io, int port, int id);
void io_set_auto_vs_controller_mode(struct io_state *io, int mode);
int io_get_auto_vs_controller_mode(struct io_state *io);

int io_get_four_player_mode(struct io_state *io);
int io_get_auto_four_player_mode(struct io_state *io);

struct controller_common_state *io_get_controller_common_state(struct io_state *io);
int io_get_selected_device_id(struct io_state *io, int port);
struct io_device *io_find_device(struct io_state *io, int port, int id);
struct io_device *io_find_auto_device(struct io_state *io, int port);
struct io_device *io_find_device_by_config_id(struct io_state *io, int port, const char *config_id);
void io_device_connect(struct io_state *io, int port, int connected);
void io_device_select(struct io_state *io, int port, int id);
int io_device_connected(struct io_state *io, int port);
void io_set_remember_input_devices(struct io_state *io, int enabled);

int io_save_state(struct io_state *io, struct save_state *state);
int io_load_state(struct io_state *io, struct save_state *state);

#endif				/* __IO_H__ */
