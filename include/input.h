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

#ifndef __INPUT_H__
#define __INPUT_H__

#include "emu.h"

#define EVENT_HASH_SIZE 8

#define INPUT_MOD_COUNT                 8
#define INPUT_MOD_MOD1                  0
#define INPUT_MOD_MOD2                  1
#define INPUT_MOD_MOD3                  2
#define INPUT_MOD_KBD                   3
#define INPUT_MOD_CTRL                  4
#define INPUT_MOD_ALT                   5
#define INPUT_MOD_SHIFT                 6
#define INPUT_MOD_GUI                   7
#define INPUT_MOD_BITS_MOD1          (1 << INPUT_MOD_MOD1)
#define INPUT_MOD_BITS_MOD2          (1 << INPUT_MOD_MOD2)
#define INPUT_MOD_BITS_MOD3          (1 << INPUT_MOD_MOD3)
#define INPUT_MOD_BITS_KBD           (1 << INPUT_MOD_KBD)
#define INPUT_MOD_BITS_CTRL          (1 << INPUT_MOD_CTRL)
#define INPUT_MOD_BITS_ALT           (1 << INPUT_MOD_ALT)
#define INPUT_MOD_BITS_SHIFT         (1 << INPUT_MOD_SHIFT)
#define INPUT_MOD_BITS_GUI           (1 << INPUT_MOD_GUI)
#define INPUT_MOD_BITS_ALL           (INPUT_MOD_BITS_MOD1 | INPUT_MOD_BITS_MOD2 | \
				INPUT_MOD_BITS_MOD3 | INPUT_MOD_BITS_KBD)

struct input_event_handler {
	uint32_t id;
	int (*handler) (void *, uint32_t, uint32_t);
};

enum input_event_type {
	INPUT_EVENT_TYPE_NONE = 0,
	INPUT_EVENT_TYPE_KEYBOARD = 1,
	INPUT_EVENT_TYPE_MOUSEMOTION = 2,
	INPUT_EVENT_TYPE_MOUSEBUTTON = 3,
	INPUT_EVENT_TYPE_JOYAXIS = 4,
	INPUT_EVENT_TYPE_JOYBUTTON = 5,
	INPUT_EVENT_TYPE_JOYAXISBUTTON = 6,
	INPUT_EVENT_TYPE_JOYHAT = 7,
};

enum joystick_hat_state {
	INPUT_JOYHAT_CENTERED = 0x00,
	INPUT_JOYHAT_UP = 0x01,
	INPUT_JOYHAT_RIGHT = 0x02,
	INPUT_JOYHAT_DOWN = 0x04,
	INPUT_JOYHAT_LEFT = 0x08,
	INPUT_JOYHAT_RIGHTUP = 0x03,
	INPUT_JOYHAT_RIGHTDOWN = 0x06,
	INPUT_JOYHAT_LEFTUP = 0x09,
	INPUT_JOYHAT_LEFTDOWN = 0x0c,
};

enum action_category {
	ACTION_CATEGORY_CONTROLLER1,
	ACTION_CATEGORY_CONTROLLER2,
	ACTION_CATEGORY_CONTROLLER3,
	ACTION_CATEGORY_CONTROLLER4,
	ACTION_CATEGORY_ARKANOID1,
	ACTION_CATEGORY_ARKANOID2,
	ACTION_CATEGORY_POWERPAD1,
	ACTION_CATEGORY_POWERPAD2,
	ACTION_CATEGORY_ZAPPER1,
	ACTION_CATEGORY_ZAPPER2,
	ACTION_CATEGORY_MOUSE1,
	ACTION_CATEGORY_MOUSE2,
	ACTION_CATEGORY_MOUSE3,
	ACTION_CATEGORY_MOUSE4,
	ACTION_CATEGORY_KEYBOARD,
	ACTION_CATEGORY_EMULATOR,
	ACTION_CATEGORY_VIDEO,
	ACTION_CATEGORY_INPUT,
	ACTION_CATEGORY_PPU,
	ACTION_CATEGORY_VS,
	ACTION_CATEGORY_SAVESTATE,
	ACTION_CATEGORY_MISC,
	ACTION_CATEGORY_COUNT,
};

struct input_common_event {
	uint32_t type;
	uint32_t device;
	uint32_t index;
	uint32_t misc;
	uint32_t padding;
	int processed;
};

struct input_keyboard_event {
	uint32_t type;
	uint32_t device;
	uint32_t key;
	uint32_t padding;
	uint32_t state;
	int processed;
};

struct input_mouse_motion_event {
	uint32_t type;
	uint32_t device;
	uint32_t padding;
	uint32_t padding2;
	uint32_t button_state;
	int processed;
	int16_t x;
	int16_t y;
	int16_t xrel;
	int16_t yrel;
};

struct input_mouse_button_event {
	uint32_t type;
	uint32_t device;
	uint32_t button;
	uint32_t padding;
	uint32_t state;
	int processed;
	int16_t x;
	int16_t y;
};

struct input_joystick_axis_event {
	uint32_t type;
	uint32_t device;
	uint32_t axis;
	uint32_t padding;
	uint32_t padding2;
	int processed;
	int16_t value;
};

struct input_joystick_button_event {
	uint32_t type;
	uint32_t device;
	uint32_t button;
	uint32_t padding;
	uint32_t state;
	int processed;
};

struct input_joystick_axis_button_event {
	uint32_t type;
	uint32_t device;
	uint32_t axis;
	int32_t direction;
	uint32_t state;
	int processed;
};

struct input_joystick_hat_event {
	uint32_t type;
	uint32_t device;
	uint32_t hat;
	uint32_t value;
	uint32_t state;
	int processed;
};

union input_new_event {
	uint32_t type;
	struct input_common_event common;
	struct input_keyboard_event key;
	struct input_mouse_motion_event motion;
	struct input_mouse_button_event mbutton;
	struct input_joystick_axis_event jaxis;
	struct input_joystick_button_event jbutton;
	struct input_joystick_axis_button_event jaxisbutton;
	struct input_joystick_hat_event jhat;
};

struct emu_action {
	uint32_t id;
	void *data;
	int count;
	int (*handler)(void *, uint32_t, uint32_t);
	struct emu_action *next;
};

struct input_event_mapping {
	int mod_bits;
	struct emu_action *emu_action;
};

struct input_event_node {
	union input_new_event event;

	int modifier;
	int pressed;
	int mapping_count;
	int mapping_max;
	struct input_event_mapping *mappings;

	struct input_event_node *next;
};

struct emu_action_id_map {
	const char *name;
	const char *description;
	enum action_category category;
	uint32_t emu_action_id;
};

const char *modifier_names[INPUT_MOD_COUNT];

extern struct input_event_node *event_hash[EVENT_HASH_SIZE];
extern struct emu_action_id_map emu_action_id_map[];
extern const char *category_names[];

int input_bind(const char *binding, const char *actions);
void input_ignore_events(int ignore);
void input_release_all(void);
void input_get_mouse_state(int *x, int *y, int *xrel, int *yrel);
int input_validate_binding_name(const char *);
int input_validate_modifier(const char *);
int input_init(struct emu *emu);
void input_set_context(int context);
void input_configure_modifier(const char *name, const char *value);
int input_connect_handlers(const struct input_event_handler *, void *);
int input_disconnect_handlers(const struct input_event_handler *);
void input_get_binding_config(char **config_data, size_t *config_data_size);
int input_shutdown(void);
const char *input_lookup_keyname_from_code(uint32_t keycode);
uint32_t input_lookup_keycode_from_name(const char *value);
int get_binding_name(char *buffer, int size, struct input_event_node *e);
void get_modifier_string(char *buffer, int size, int modbits);
void get_event_name_and_category(uint32_t event_id, char **name, char **category);
void input_reset_bindings(void);
struct emu_action *input_lookup_emu_action(uint32_t id);
struct emu_action *input_insert_emu_action(uint32_t id);
int input_bindings_loaded(void);
void input_configure_keyboard_modifiers(void);
int input_add_modifier(union input_new_event *event, int mod);
struct input_event_node *input_insert_event(union input_new_event *event,
				       int mod,
				       struct emu_action *emu_action);
int input_queue_event(union input_new_event *event);
int input_process_queue(int force);
void input_poll_events(void);
int input_get_num_joysticks(void);
const char *input_get_joystick_name(int index);
int input_get_joystick_guid(int index, char *buffer, size_t size);
int input_joystick_is_gamecontroller(int index);
int input_parse_binding(const char *string, union input_new_event *event,
			int *modp);
int emu_action_lookup_by_name(const char *name, uint32_t *ptr);
#endif				/* __INPUT_H__ */
