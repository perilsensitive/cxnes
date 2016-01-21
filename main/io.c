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

/* These device connect to the controller ports */
extern struct io_device controller1_device;
extern struct io_device controller2_device;
extern struct io_device controller3_device;
extern struct io_device controller4_device;
extern struct io_device zapper1_device;
extern struct io_device zapper2_device;
extern struct io_device powerpad_a1_device;
extern struct io_device powerpad_b1_device;
extern struct io_device powerpad_a2_device;
extern struct io_device powerpad_b2_device;
extern struct io_device arkanoid_nes1_device;
extern struct io_device arkanoid_nes2_device;
extern struct io_device snes_mouse1_device;
extern struct io_device snes_mouse2_device;
extern struct io_device snes_mouse3_device;
extern struct io_device snes_mouse4_device;

/* These devices use the expansion port */
extern struct io_device arkanoid_fc_device;
extern struct io_device arkanoidII_device;
extern struct io_device vs_zapper_device;
extern struct io_device keyboard_device;
extern struct io_device subor_keyboard_device;
extern struct io_device vs_unisystem_device;
extern struct io_device ftrainer_a_device;
extern struct io_device ftrainer_b_device;
extern struct io_device microphone_device;

static int fourscore_connect(struct io_device *dev);
static void fourscore_disconnect(struct io_device *dev);
static uint8_t fourscore_read(struct io_device *dev, int port,
		      int mode, uint32_t cycles);
static void fourscore_write(struct io_device *dev, uint8_t value,
		    int mode, uint32_t cycles);

static int controller_common_connect(struct io_device *dev);
static void controller_common_disconnect(struct io_device *dev);
static void controller_common_end_frame(struct io_device *dev, uint32_t cycles);
static int controller_common_set_button(void *data, uint32_t pressed, uint32_t button);
static int controller_common_apply_config(struct io_device *dev);

#define BUTTON_A 1
#define BUTTON_B 2
#define BUTTON_UP 16
#define BUTTON_DN 32
#define BUTTON_L  64
#define BUTTON_R  128

static uint8_t turbo_speeds[] = { 1, 10, 8, 6, 5, 4, 3, 2 };

struct fourscore_state {
	int latch[2];
	int strobe;
};

static struct input_event_handler controller_common_handlers[] = {
	{ ACTION_CONTROLLER_1_A & ACTION_PREFIX_MASK, controller_common_set_button},
	{ ACTION_NONE },
};


static struct io_device null_device = {
	.name = "None",
	.config_id = "none",
	.id = IO_DEVICE_NONE,
	.removable = 1,
};

static struct io_device controller_common_device = {
	.name = "Controller State",
	.config_id = "controller_common",
	.id = IO_DEVICE_CONTROLLER_COMMON,
	.connect = controller_common_connect,
	.disconnect = controller_common_disconnect,
	.handlers = controller_common_handlers,
	.end_frame = controller_common_end_frame,
	.apply_config = controller_common_apply_config,
	.port = PORT_EXP,
	.removable = 0,
};

static struct io_device fourscore_device = {
	.name = "NES Four Score",
	.config_id = "fourscore",
	.id = IO_DEVICE_FOURSCORE,
	.connect = fourscore_connect,
	.disconnect = fourscore_disconnect,
	.read = fourscore_read,
	.write = fourscore_write,
	.port = PORT_EXP,
	.removable = 0,
};

struct io_state {
	int device_id[5];
	int auto_device_id[5];
	struct io_device *port[5];
	struct io_device *selected_device[5];
	int read_mask;
	int four_player_mode;
	int auto_four_player_mode;
	int auto_vs_controller_mode;
	int initialized;
	int queue_processed;
	struct emu *emu;
};

struct io_device *io_register_device(struct io_state *io,
				     struct io_device *device,
				     int port)
{
	struct io_device *ptr, *last;

	if (port < 0)
		port = device->port;

	last = NULL;
	ptr = io->port[port];

	while (ptr) {
		/* Device already added */
		if ((strcmp(ptr->name, device->name) == 0) && (ptr->port == port))
			return ptr;

		last = ptr;
		ptr = ptr->next;
	}

	ptr = malloc(sizeof(*device));
	if (!ptr)
		return NULL;

	memcpy(ptr, device, sizeof(*device));
	ptr->connected = 0;
	ptr->port = port;
	ptr->emu = io->emu;
	ptr->private = NULL;
	ptr->next = NULL;

	if (last)
		last->next = ptr;
	else
		io->port[port] = ptr;


	return ptr;
}

static CPU_WRITE_HANDLER(io_write_handler)
{
	struct io_device *device;
	int port;
	int mask;
	int four_player_mode;
	struct io_state *io;

	io = emu->io;
	four_player_mode = emu->io->four_player_mode;

	if (four_player_mode == FOUR_PLAYER_MODE_AUTO)
		four_player_mode = emu->io->auto_four_player_mode;

	if (!io->queue_processed) {
		input_poll_events();
		input_process_queue(0);
		io->queue_processed = 1;
	}

	/* Write to each of the 5 ports. 1-4 get only bit 0 of the
	   value, since that's all that's connected. Port 5 is the
	   expansion port, so it gets all 3 bits.
	*/
	
	for (port = 0; port < 5; port++) {
		if ((port == 2 || port == 3) &&
		    (four_player_mode == FOUR_PLAYER_MODE_NONE)) {
			continue;
		}

		for (device = emu->io->port[port]; device;
		     device = device->next) {
			mask = 0x01;
			if (port == 4)
				mask = 0x07;
			if (device->write && (device->connected))
				device->write(device, value & mask,
					      four_player_mode,
					      cycles);
		}
	}
}

static CPU_READ_HANDLER(io_read_handler)
{
	struct io_device *device;
	struct io_state *io;
	uint8_t result;
	int port;
	int mask;
	int four_player_mode;

	io = emu->io;
	four_player_mode = io->four_player_mode;

	if (four_player_mode == FOUR_PLAYER_MODE_AUTO)
		four_player_mode = io->auto_four_player_mode;

	port = addr - 0x4016;

	if (!io->queue_processed) {
		input_poll_events();
		input_process_queue(0);
		io->queue_processed = 1;
	}

	result = 0;

	/* Read from standard controller port */
	for (device = io->port[port]; device; device = device->next) {
		if (device->connected && device->read) {
			mask = 0x19;
			result |= device->read(device, port,
					       four_player_mode,
					       cycles) & mask;
		}
	}

	/* Read from expansion port */
	for (device = io->port[PORT_EXP]; device; device = device->next) {
		if (device->connected && device->read) {
			mask = io->read_mask;
			result |= (device->read(device, port,
						four_player_mode,
						cycles) & mask);
		}
	}

	if (four_player_mode != FOUR_PLAYER_MODE_NONE) {
		int shift = 0;

		if (four_player_mode == FOUR_PLAYER_MODE_FC)
			shift = 1;

		/* Read from player 3 or 4 */
		for (device = io->port[port + 2]; device;
		     device = device->next) {
			if (device->connected && device->read) {
				mask = 0x01;
				result |= (device->read(device, port,
							four_player_mode,
							cycles)
					   & mask) << shift;
			}
		}
	}

	return result | (value & ~io->read_mask);
}

void io_connect_device(struct io_device *device)
{
	if (!device)
		return;

	if (device->connect)
		device->connect(device);

	if (device->connected)
		return;

	device->connected = 1;
	input_connect_handlers(device->handlers, device);

	if (device->apply_config)
		device->apply_config(device);

}

void io_disconnect_device(struct io_device *device)
{
	if (!device->connected)
		return;

	if (device->disconnect)
		device->disconnect(device);

	device->connected = 0;

	input_disconnect_handlers(device->handlers);
}

int io_init(struct emu *emu)
{
	struct io_state *io;

	io = malloc(sizeof(*io));
	if (!io)
		return -1;

	memset(io, 0, sizeof(*io));
	io->emu = emu;
	emu->io = io;

	io->initialized = 0;

	io->selected_device[0] = NULL;
	io->selected_device[1] = NULL;
	io->selected_device[2] = NULL;
	io->selected_device[3] = NULL;
	io->selected_device[4] = NULL;

	io->auto_device_id[0] = IO_DEVICE_CONTROLLER_1;
	io->auto_device_id[1] = IO_DEVICE_CONTROLLER_2;
	io->auto_device_id[2] = IO_DEVICE_CONTROLLER_3;
	io->auto_device_id[3] = IO_DEVICE_CONTROLLER_4;
	io->auto_device_id[4] = IO_DEVICE_NONE;

	io->four_player_mode = -1;
	io->auto_four_player_mode = FOUR_PLAYER_MODE_NONE;
	io->auto_vs_controller_mode = VS_CONTROLLER_MODE_STANDARD;

	cpu_set_write_handler(emu->cpu, 0x4016, 1, 0, io_write_handler);
	cpu_set_read_handler(emu->cpu, 0x4016, 2, 0, io_read_handler);

	return 0;
}

void io_cleanup(struct io_state *io)
{
	struct io_device **dev, *tmp;
	int port;

	for (port = 0; port < 5; port++) {
		dev = &io->port[port];
		while (*dev) {
			tmp = *dev;
			io_disconnect_device(*dev);
			*dev = (*dev)->next;
			free(tmp);
		}
	}

	io->emu = NULL;
	free(io);
}

int io_apply_config(struct io_state *io)
{
	const char *four_player_mode;
	struct emu *emu;
	int i;

	emu = io->emu;

	four_player_mode = NULL;
	if (io->four_player_mode < 0) {
		four_player_mode = emu->config->four_player_mode;
		if (strcasecmp(four_player_mode, "fourscore") == 0)
			io->four_player_mode = FOUR_PLAYER_MODE_NES;
		else if (strcasecmp(four_player_mode, "nes") == 0)
			io->four_player_mode = FOUR_PLAYER_MODE_NES;
		else if (strcasecmp(four_player_mode, "famicom") == 0)
			io->four_player_mode = FOUR_PLAYER_MODE_FC;
		else if (strcasecmp(four_player_mode, "none") == 0)
			io->four_player_mode = FOUR_PLAYER_MODE_NONE;
		else if (strcasecmp(four_player_mode, "auto") == 0)
			io->four_player_mode = FOUR_PLAYER_MODE_AUTO;
	}

	for (i = 0; i <= PORT_EXP; i++) {
		struct io_device *device;

		for (device = io->port[i]; device; device = device->next) {
			if (device->connected && device->apply_config) {
				device->apply_config(device);
			}
		}
	}


	return 0;
}

void io_reset(struct io_state *io, int hard)
{
	struct io_device *device;
	int port;
	int i;

	io->queue_processed = 0;

	if (hard) {
		/* The fourscore device is always connected, but simply
		   does nothing if Four Score mode isn't enabled.
		*/

		device = io_register_device(io, &controller_common_device, -1);
		io_connect_device(device);
		
		if (emu_system_is_vs(io->emu)) {
			io_register_device(io, &null_device,PORT_1);
			io_register_device(io, &null_device,PORT_2);
			io_register_device(io, &null_device,PORT_EXP);
			io_register_device(io, &controller1_device, PORT_1);
			io_register_device(io, &controller2_device, PORT_1);
			io_register_device(io, &controller3_device, PORT_1);
			io_register_device(io, &controller4_device, PORT_1);
			io_register_device(io, &controller1_device, PORT_2);
			io_register_device(io, &controller2_device, PORT_2);
			io_register_device(io, &controller3_device, PORT_2);
			io_register_device(io, &controller4_device, PORT_2);
			io_register_device(io, &vs_zapper_device, -1);
			device = io_register_device(io,
						    &vs_unisystem_device, -1);
			io_connect_device(device);
			io->read_mask = 0xff;
		} else {
			io_register_device(io, &null_device,PORT_1);
			io_register_device(io, &null_device,PORT_2);
			io_register_device(io, &null_device,PORT_3);
			io_register_device(io, &null_device,PORT_4);
			io_register_device(io, &null_device,PORT_EXP);
			io_register_device(io, &controller1_device, PORT_1);
			io_register_device(io, &controller1_device, PORT_2);
			io_register_device(io, &controller1_device, PORT_3);
			io_register_device(io, &controller1_device, PORT_4);
			io_register_device(io, &controller2_device, PORT_1);
			io_register_device(io, &controller2_device, PORT_2);
			io_register_device(io, &controller2_device, PORT_3);
			io_register_device(io, &controller2_device, PORT_4);
			io_register_device(io, &controller3_device, PORT_1);
			io_register_device(io, &controller3_device, PORT_2);
			io_register_device(io, &controller3_device, PORT_3);
			io_register_device(io, &controller3_device, PORT_4);
			io_register_device(io, &controller4_device, PORT_1);
			io_register_device(io, &controller4_device, PORT_2);
			io_register_device(io, &controller4_device, PORT_3);
			io_register_device(io, &controller4_device, PORT_4);
			io_register_device(io, &zapper1_device, -1);
			io_register_device(io, &zapper2_device, -1);
			io_register_device(io, &powerpad_a1_device, -1);
			io_register_device(io, &powerpad_b1_device, -1);
			io_register_device(io, &powerpad_a2_device, -1);
			io_register_device(io, &powerpad_b2_device, -1);
			io_register_device(io, &arkanoid_nes1_device, -1);
			io_register_device(io, &arkanoid_nes2_device, -1);
			io_register_device(io, &snes_mouse1_device, -1);
			io_register_device(io, &snes_mouse2_device, -1);
			io_register_device(io, &snes_mouse3_device, -1);
			io_register_device(io, &snes_mouse4_device, -1);

			io_register_device(io, &keyboard_device, -1);
			io_register_device(io, &subor_keyboard_device, -1);
			io_register_device(io, &ftrainer_a_device, -1);
			io_register_device(io, &ftrainer_b_device, -1);
			io_register_device(io, &arkanoid_fc_device, -1);
			io_register_device(io, &arkanoidII_device, -1);
			io_register_device(io, &microphone_device, -1);

			
			device = io_register_device(io,
						    &fourscore_device, -1);
			io_connect_device(device);
			device = io_register_device(io,
						    &microphone_device, -1);
			io_connect_device(device);
			io->read_mask = 0x1f;
		}

		if (!io->initialized) {
			struct io_device *device;
			struct config *config = io->emu->config;
			int id = IO_DEVICE_NONE;

			io->device_id[PORT_1] = IO_DEVICE_NONE; 
			io->device_id[PORT_2] = IO_DEVICE_NONE; 
			io->device_id[PORT_3] = IO_DEVICE_NONE; 
			io->device_id[PORT_4] = IO_DEVICE_NONE; 
			io->device_id[PORT_EXP] = IO_DEVICE_NONE; 

			if (strcasecmp(config->default_port1_device, "auto") == 0) {
				id = IO_DEVICE_AUTO;
			} else {
				device = io_find_device_by_config_id(io, PORT_1,
						     config->default_port1_device);
				if (device)
					id = device->id;
			}
			io->device_id[PORT_1] = id;

			if (strcasecmp(config->default_port2_device, "auto") == 0) {
				id = IO_DEVICE_AUTO;
			} else {
				device = io_find_device_by_config_id(io, PORT_2,
						     config->default_port2_device);
				if (device)
					id = device->id;
			}
			io->device_id[PORT_2] = id;

			if (strcasecmp(config->default_port3_device, "auto") == 0) {
				id = IO_DEVICE_AUTO;
			} else {
				device = io_find_device_by_config_id(io, PORT_3,
						     config->default_port3_device);
				if (device)
					id = device->id;
			}
			io->device_id[PORT_3] = id;

			if (strcasecmp(config->default_port4_device, "auto") == 0) {
				id = IO_DEVICE_AUTO;
			} else {
				device = io_find_device_by_config_id(io, PORT_4,
						     config->default_port4_device);
				if (device)
					id = device->id;
			}
			io->device_id[PORT_4] = id;

			if (strcasecmp(config->default_exp_device, "auto") == 0) {
				id = IO_DEVICE_AUTO;
			} else {
				device = io_find_device_by_config_id(io, PORT_EXP,
						     config->default_exp_device);
				if (device)
					id = device->id;
			}
			io->device_id[PORT_EXP] = id;

			for (i = 0; i <= PORT_EXP; i++) {
				struct io_device *device;

				if (io->selected_device[i])
					io_device_connect(io, i, 0);
				io_device_select(io, i, io->device_id[i]);
				io_device_connect(io, i, 1);

				for (device = io->port[i]; device; device = device->next) {
					if (device->connected && device->apply_config) {
						device->apply_config(device);
					}
				}
			}

			io->initialized = 1;
		}

		io_apply_config(io);

	}

	for (port = 0; port < 5; port++) {
		for (device = io->port[port]; device; device = device->next) {
			if (device->connected && device->reset) {
				device->reset(device, hard);
			}
		}
	}
}

void io_run(struct io_state *io, uint32_t cycles)
{
	struct io_device *device;
	int port;

	for (port = 0; port < 5; port++) {
		for (device = io->port[port]; device; device = device->next) {
			if (device->connected && device->run)
				device->run(device, cycles);
		}
	}
}

void io_end_frame(struct io_state *io, uint32_t cycles)
{
	struct io_device *device;
	int port;

	/* if (!io->queue_processed) */
	/* 	input_process_queue(0); */

	for (port = 0; port < 5; port++) {
		for (device = io->port[port]; device; device = device->next) {
			if (device->connected && device->end_frame)
				device->end_frame(device, cycles);
		}
	}

	io->queue_processed = 0;
}

struct io_device *io_find_device_by_config_id(struct io_state *io, int port, const char *config_id)
{
	struct io_device *device;

	if (!io || !config_id || (port < 0) || (port > 5))
		return NULL;

	if (strcasecmp(config_id, "auto") == 0) {
		device = io_find_device(io, port, io->auto_device_id[port]);
		return device;
	}

	for (device = io->port[port]; device; device = device->next) {
		if ((strcasecmp(config_id,  device->config_id) == 0) &&
		    (port == device->port)) {
			break;
		}
	}

	return device;
}

struct io_device *io_find_device(struct io_state *io, int port, int id)
{
	struct io_device *device;

	if (!io || (port < 0) || (port > 5))
		return 0;

	for (device = io->port[port]; device; device = device->next) {
		if ((id == device->id) &&
		    (port == device->port)) {
			break;
		}
	}

	return device;
}

struct io_device *io_find_auto_device(struct io_state *io, int port)
{
	struct io_device *device;
	int id;

	if (!io || (port < 0) || (port > 5))
		return 0;

	id = io->auto_device_id[port];

	device = NULL;
	for (device = io->port[port]; device; device = device->next) {
		if ((id == device->id) &&
		    (port == device->port)) {
			break;
		}
	}

	return device;
}

void io_device_select(struct io_state *io, int port, int id)
{
	struct io_device *device;
	int real_id;
	const char *config_param;

	if (io->selected_device[port] && io->selected_device[port]->connected)
		return;

	config_param = NULL;
	switch (port) {
	case PORT_1:
		config_param = "default_port1_device";
		break;
	case PORT_2:
		config_param = "default_port2_device";
		break;
	case PORT_3:
		config_param = "default_port3_device";
		break;
	case PORT_4:
		config_param = "default_port4_device";
		break;
	case PORT_EXP:
		config_param = "default_exp_device";
		break;
	}

	device = io_find_device(io, port, id);

	if (io->initialized && device && config_param) {
		rom_config_set(io->emu->config, config_param,
			(id == IO_DEVICE_AUTO) ? "auto" : device->config_id);
		emu_save_rom_config(io->emu);
	}

	real_id = id;
	if (id == IO_DEVICE_AUTO)
		real_id = io->auto_device_id[port];

	device = io_find_device(io, port, real_id);

	if (device) {
		io->device_id[port] = id;
		io->selected_device[port] = device;
	} else {
		io->device_id[port] = IO_DEVICE_NONE;
	}
}

void io_device_connect(struct io_state *io, int port, int connected)
{
	if (!io)
		return;

	if (!connected) {
		io_disconnect_device(io->selected_device[port]);
	} else {
		io_connect_device(io->selected_device[port]);
	}
}

int io_device_connected(struct io_state *io, int port)
{
	if (port < 0 || port > 4)
		return -1;

	return (io->selected_device[port] && io->selected_device[port]->connected);
}

void io_ui_device_connect(struct io_state *io, int port)
{
	const char *status;

	if (!io)
		return;

	if (io->selected_device[port]) {
		if (io->selected_device[port]->connected) {
			status = "disconnected";
			io_disconnect_device(io->selected_device[port]);
		} else {
			status = "connected";
			io_connect_device(io->selected_device[port]);
		}

		osdprintf("Port %d %s", port + 1, status);
	} else {
		osdprintf("Port %d: No device selected", port + 1);
		return;
	}
}

void io_ui_device_select(struct io_state *io, int port)
{
	if (!io)
		return;

	if (io->selected_device[port] &&
	    io->selected_device[port]->connected) {
		return;
	}


	while (1) {
		if (io->device_id[port] == IO_DEVICE_AUTO) {
			io->selected_device[port] = io->port[port];
			io->device_id[port] = io->selected_device[port]->id;
		} else if (!io->selected_device[port]) {
			io->device_id[port] = IO_DEVICE_AUTO;
			io->selected_device[port] =
				io_find_device(io, port,
						io->auto_device_id[port]);
		} else {
			io->selected_device[port] =
				io->selected_device[port]->next;
			if (!io->selected_device[port])
				continue;

			io->device_id[port] = io->selected_device[port]->id;
		}


		if (!io->selected_device[port])
			break;

		if (io->selected_device[port]->port != port)
			continue;

		if (!io->selected_device[port]->removable)
			continue;

		break;
	}

	if (io->device_id[port] == IO_DEVICE_AUTO) {
		if (io->selected_device[port]) {
			osdprintf("Port %d: Auto (%s)\n", port + 1,
				  io->selected_device[port]->name);
		} else {
			osdprintf("Port %d: Auto (Unconnected)\n", port + 1);
		}
	} else if (io->selected_device[port]) {
		osdprintf("Port %d: %s\n", port + 1,
			  io->selected_device[port]->name);
	} else {
		osdprintf("Port %d: None", port + 1);
	}
}

int io_get_four_player_mode(struct io_state *io)
{
	return io->four_player_mode;
}

int io_get_auto_four_player_mode(struct io_state *io)
{
	return io->auto_four_player_mode;
}

void io_set_four_player_mode(struct io_state *io, int mode, int display)
{
	const char *auto_mode_desc;
	const char *mode_str;

	if (!io)
		return;

	if (mode < 0)
		mode = (io->four_player_mode + 1) % 4;
	else if (mode > 3)
		return;

	io->four_player_mode = mode;

	switch (mode) {
	case FOUR_PLAYER_MODE_NONE:
		mode_str = "none";
		break;
	case FOUR_PLAYER_MODE_AUTO:
		mode_str = "auto";
		break;
	case FOUR_PLAYER_MODE_NES:
		mode_str = "nes";
		break;
	case FOUR_PLAYER_MODE_FC:
		mode_str = "famicom";
		break;
	default:
		mode_str = "";
	}

	rom_config_set(io->emu->config, "four_player_mode", mode_str);
	emu_save_rom_config(io->emu);

	if (!display)
		return;

	switch (io->auto_four_player_mode) {
	case FOUR_PLAYER_MODE_NONE:
		auto_mode_desc = "Disabled";
		break;
	case FOUR_PLAYER_MODE_NES:
		auto_mode_desc = "Four Score";
		break;
	case FOUR_PLAYER_MODE_FC:
		auto_mode_desc = "Famicom";
		break;
	default:
		auto_mode_desc = "";
	}

	switch (io->four_player_mode) {
	case FOUR_PLAYER_MODE_NONE:
		osdprintf("Four-player mode: Disabled");
		break;
	case FOUR_PLAYER_MODE_NES:
		osdprintf("Four-player mode: Four Score");
		break;
	case FOUR_PLAYER_MODE_FC:
		osdprintf("Four-player mode: Famicom");
		break;
	case FOUR_PLAYER_MODE_AUTO:
		osdprintf("Four-player mode: Auto (%s)", auto_mode_desc);
		break;
	}
}

static int fourscore_connect(struct io_device *dev)
{
	struct fourscore_state *state;

	state = malloc(sizeof(*state));
	if (!state)
		return -1;

	memset(state, 0, sizeof(*state));
	dev->private = state;

	return 0;
}

static void fourscore_disconnect(struct io_device *dev)
{
	if (dev->private)
		free(dev->private);
	dev->private = NULL;
}

static uint8_t fourscore_read(struct io_device *dev, int port,
			      int mode, uint32_t cycles)
{
	struct fourscore_state *state;
	uint8_t data;

	state = dev->private;

	if (mode != FOUR_PLAYER_MODE_NES)
		return 0;

	data = state->latch[port] & 1;
	state->latch[port] >>= 1;
	state->latch[port] |= (1 << 31);

	return data;
}

static void fourscore_write(struct io_device *dev, uint8_t value,
			    int mode, uint32_t cycles)
{
	struct fourscore_state *state;

	if (mode != FOUR_PLAYER_MODE_NES)
		return;

	state = dev->private;

	if (!state->strobe && (value & 0x01)) {
		state->strobe = 1;
	} else if (state->strobe && !(value & 0x01)) {

		/* The hardware returns bits MSB first,
		   this code does it backward as I found
		   it easier to work with.  The signatures
		   are really 0x10 for port 1 and 0x20
		   for port 2, but I have to reverse the
		   bits here to return them LSB first.
		*/
		state->latch[0] = 0x08 << 16;
		state->latch[1] = 0x04 << 16;
		state->latch[0] |= ~0xffffff;
		state->latch[1] |= ~0xffffff;
	}
}

int io_get_selected_device_id(struct io_state *io, int port)
{
	if (port < 0 || port > 4)
		return IO_DEVICE_NONE;

	return io->device_id[port];
}


static int controller_common_connect(struct io_device *dev)
{
	struct controller_common_state *state;

	if (!dev->private) {
		state = malloc(sizeof(*state));
		if (!state)
			return -1;

		memset(state, 0, sizeof(*state));

		state->port_mapping[0] = -1;
		state->port_mapping[1] = -1;
		state->port_mapping[2] = -1;
		state->port_mapping[3] = -1;

		state->turbo_cycle_length = 3;
		state->turbo_pressed_frames = 2;
		state->turbo_counter = 0;

		dev->private = state;
	}

	return 0;
}

static void controller_common_disconnect(struct io_device *dev)
{
	struct controller_state *state;

	state = dev->private;
	if (state) {
		free(dev->private);
		dev->private = NULL;
	}
}

static void controller_update_button_state(struct io_device *dev, int controller)
{
	struct controller_common_state *state;
	int tmp;

	/* Find existing device */
	state = dev->private;

	/* Update the effective button state of this controller based
	   on the current turbo settings */
	tmp = state->buttons[controller] &
		state->turbo_toggles[controller];
	tmp &= state->turbo_mask[controller];

	state->current_state[controller] = state->buttons[controller] &
		~state->turbo_toggles[controller];
	state->current_state[controller] |= tmp;
	state->current_state[controller] |= state->turbo_buttons[controller] &
		state->turbo_mask[controller];
}

static void controller_common_end_frame(struct io_device *dev, uint32_t cycles)
{
	struct controller_common_state *state;
	int i;

	state = dev->private;
	state->turbo_counter = (state->turbo_counter + 1) %
		state->turbo_cycle_length;

	for (i = 0; i < 4; i++) {
		if (state->turbo_counter < state->turbo_pressed_frames)
			state->turbo_mask[i] = ~0;
		else if (state->turbo_counter >= state->turbo_pressed_frames)
			state->turbo_mask[i] = 0;

		controller_update_button_state(dev, i);
	}
}

static int controller_common_set_button(void *data, uint32_t pressed, uint32_t button)
{
	struct io_device *dev;
	struct controller_common_state *state;
	int controller;
	int turbo, turbo_toggle;

	/* Find existing device */
	dev = data;
	state = dev->private;


	controller = (button & 0x300) >> 8;
	turbo = button & ACTION_CONTROLLER_TURBO_FLAG;
	turbo_toggle = button & ACTION_CONTROLLER_TURBO_TOGGLE_FLAG;
	button &= 0xff;

	if (turbo) {
		if (pressed)
			state->turbo_buttons[controller] |= button;
		else
			state->turbo_buttons[controller] &= ~button;
	} else if (turbo_toggle) {
		if (pressed)
			state->turbo_toggles[controller] ^= button;
		osdprintf("Controller %d Turbo %c %s",
			  controller + 1, button == BUTTON_A ? 'A' : 'B',
			  state->turbo_toggles[controller] & button ?
			  "on" : "off");
	} else {
		if (pressed)
			state->buttons[controller] |= button;
		else
			state->buttons[controller] &= ~button;
	}

	controller_update_button_state(dev, controller);

	return 0;
}

struct controller_common_state *io_get_controller_common_state(struct io_state *io)
{
	struct io_device *dev;

	dev = io_find_device(io, PORT_EXP, IO_DEVICE_CONTROLLER_COMMON);

	if (!dev)
		return NULL;

	return dev->private;
}

static int controller_common_apply_config(struct io_device *dev)
{
	struct config *config;
	struct controller_common_state *state;

	state = dev->private;
	config = dev->emu->config;
		
	state->turbo_cycle_length = turbo_speeds[config->turbo_speed];
	state->turbo_pressed_frames = state->turbo_cycle_length / 2;

	if (state->turbo_cycle_length & 1)
		state->turbo_pressed_frames++;

	if (!strcasecmp(config->vs_controller_mode, "auto")) {
		state->vs_controller_mode = dev->emu->io->auto_vs_controller_mode;
	} else if (!strcasecmp(config->vs_controller_mode, "standard")) {
		state->vs_controller_mode = VS_CONTROLLER_MODE_STANDARD;
	} else if (!strcasecmp(config->vs_controller_mode, "swapped")) {
		state->vs_controller_mode = VS_CONTROLLER_MODE_SWAPPED;
	} else if (!strcasecmp(config->vs_controller_mode, "vssuperskykid")) {
		state->vs_controller_mode = VS_CONTROLLER_MODE_VSSUPERSKYKID;
	} else if (!strcasecmp(config->vs_controller_mode, "vspinballj")) {
		state->vs_controller_mode = VS_CONTROLLER_MODE_VSPINBALLJ;
	} else if (!strcasecmp(config->vs_controller_mode, "vsbungelingbay")) {
		state->vs_controller_mode = VS_CONTROLLER_MODE_BUNGELINGBAY;
	}

	return 0;
}

void io_set_auto_four_player_mode(struct io_state *io, int mode)
{
	io->auto_four_player_mode = mode;
}

void io_set_auto_device(struct io_state *io, int port, int id)
{
	if (port >= 0 && port <= 4)
		io->auto_device_id[port] = id;
}

void io_set_auto_vs_controller_mode(struct io_state *io, int mode)
{
	io->auto_vs_controller_mode = mode;
}

int io_get_auto_vs_controller_mode(struct io_state *io)
{
	return io->auto_vs_controller_mode;
}

int io_save_state(struct io_state *io, struct save_state *state)
{
	struct io_device *device;
	int port;
	int rc;

	for (port = 0; port < PORT_EXP; port++) {
		for (device = io->port[port]; device; device = device->next) {
			if (!device->connected || !device->save_state)
				continue;

			rc = device->save_state(device, port, state);

			if (rc)
				return rc;
		}
	}

	return 0;
}

int io_load_state(struct io_state *io, struct save_state *state)
{
	struct io_device *device;
	int port;
	int rc;

	for (port = 0; port < PORT_EXP; port++) {
		for (device = io->port[port]; device; device = device->next) {
			if (!device->connected || !device->load_state)
				continue;

			rc = device->load_state(device, port, state);

			if (rc)
				return rc;
		}
	}

	return 0;
}
