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

#include <errno.h>
#include <SDL.h>

#include "emu.h"
#include "video.h"
#include "input.h"
#include "file_io.h"
#include "actions.h"

struct keyname_map {
	const char *keyname;
	const SDL_Keycode keycode;
};

static struct keyname_map keyname_to_keycode[] = {
	{"Backslash", SDLK_BACKSLASH},

	/* For some bizarre reason, the windows port of SDL2
	   renames Left/Right GUI to Left/Right Windows.  Try
	   to handle that here by mapping both versions to
	   the same keycodes.
	*/
	{"Left GUI", SDLK_LGUI},
	{"Right GUI", SDLK_RGUI},
	{"Left Windows", SDLK_LGUI},
	{"Right Windows", SDLK_RGUI},

	/* All of these names normally contain characters with special meaning
	   for either the config parser or the binding parser, so they've been
	   munged here to avoid those characters.  These values must be used
	   in the configuration file if you want to bind functions to these
	   keys. */

	{"Equals", SDLK_EQUALS,},
	{"Keypad Equals", SDLK_KP_EQUALS},
	{"Keypad Equals (AS400)", SDLK_KP_EQUALSAS400},
	{NULL, SDLK_UNKNOWN}
};

extern struct emu *emu;
extern int running;
extern int center_x, center_y;
extern int mouse_grabbed;

#if GUI_ENABLED
extern int gui_enabled;
extern struct input_event_node grabbed_event;
extern int sdl_grab_event;
extern uint32_t binding_config_action_id;
#endif

struct axis_status {
	int value;
	int mapped;
};

struct hat_status {
	uint8_t value;
	int mapped;
};

struct button_status {
	int mapped;
};

struct joystick_list_node {
	SDL_Joystick *joystick;
	SDL_GameController *controller;
	SDL_JoystickID instance_id;
	struct axis_status *axis_status;
	struct hat_status *hat_status;
	struct button_status *button_status;
	int index;
	int axis_count;
	int hat_count;
	int button_count;
};

static struct joystick_list_node **joystick_list;
static int joystick_list_size;

const char *input_lookup_keyname_from_code(uint32_t keycode)
{
	const char *keyname;
	int i;

	for (i = 0; keyname_to_keycode[i].keyname; i++) {
		if (keyname_to_keycode[i].keycode == keycode)
			break;
	}

	if (keyname_to_keycode[i].keyname) {
		keyname = keyname_to_keycode[i].keyname;
	} else {
		keyname = SDL_GetKeyName(keycode);
	}

	if (!keyname || !keyname[0])
		return NULL;

	return keyname;
}

uint32_t input_lookup_keycode_from_name(const char *value)
{
	SDL_Keycode keycode;

	keycode = SDL_GetKeyFromName(value);
	if (!keycode) {
		int i;

		for (i = 0; keyname_to_keycode[i].keyname; i++) {
			if (strcasecmp(keyname_to_keycode[i].keyname, value) == 0)
				break;
		}

		if (keyname_to_keycode[i].keyname)
			keycode = keyname_to_keycode[i].keycode;
	}

	return (uint32_t)keycode;
}

static int find_unused_joystick_id(void)
{
	int i;
	struct joystick_list_node **tmp;

	for (i = 0; i < joystick_list_size; i++) {
		if (!joystick_list[i])
			break;
	}

	if (i == joystick_list_size) {
		tmp = realloc(joystick_list, (joystick_list_size + 1) *
			      sizeof(struct joystick_list_node *));

		if (!tmp)
			return -1;

		joystick_list = tmp;
		joystick_list_size++;
	}

	return i;
}

static struct joystick_list_node *find_joystick(int instance_id)
{
	int i;

	for (i = 0; i < joystick_list_size; i++) {
		if (joystick_list[i] &&
		    (joystick_list[i]->instance_id == instance_id)) {
			return joystick_list[i];
		}

	}

	return NULL;
}

static void check_controller_mappings(struct joystick_list_node *joystick)
{
	SDL_GameControllerButtonBind bind;
	int i;

	if (!joystick->controller)
		return;

	for (i = SDL_CONTROLLER_AXIS_INVALID + 1;
	     i < SDL_CONTROLLER_AXIS_MAX; i++) {

		bind = SDL_GameControllerGetBindForAxis(joystick->controller,
							i);

		switch (bind.bindType) {
		case SDL_CONTROLLER_BINDTYPE_AXIS:
			joystick->axis_status[bind.value.axis].mapped = 1;
			break;
		case SDL_CONTROLLER_BINDTYPE_BUTTON:
			joystick->button_status[bind.value.button].mapped = 1;
			break;
		case SDL_CONTROLLER_BINDTYPE_HAT:
			joystick->hat_status[bind.value.hat.hat].mapped = 1;
			break;
		default:
			break;
		}
	}

	for (i = SDL_CONTROLLER_BUTTON_INVALID + 1;
	     i < SDL_CONTROLLER_BUTTON_MAX; i++) {
		bind = SDL_GameControllerGetBindForButton(joystick->controller,
							  i);

		switch (bind.bindType) {
		case SDL_CONTROLLER_BINDTYPE_AXIS:
			joystick->axis_status[bind.value.axis].mapped = 1;
			break;
		case SDL_CONTROLLER_BINDTYPE_BUTTON:
			joystick->button_status[bind.value.button].mapped = 1;
			break;
		case SDL_CONTROLLER_BINDTYPE_HAT:
			joystick->hat_status[bind.value.hat.hat].mapped = 1;
			break;
		default:
			break;
		}
	}
}

static void add_joystick(int index)
{
	struct joystick_list_node *node;
	SDL_Joystick *joystick;
	SDL_GameController *controller;
	SDL_JoystickGUID guid;
	char guid_string[33];
	struct axis_status *axis_status;
	struct hat_status *hat_status;
	struct button_status *button_status;
	int axis_count;
	int hat_count;
	int button_count;
	int instance_id;

	if (SDL_IsGameController(index)) {
		controller = SDL_GameControllerOpen(index);
		if (!controller)
			return;

		joystick = SDL_GameControllerGetJoystick(controller);
	} else {
		controller = NULL;
		joystick = SDL_JoystickOpen(index);
		if (!joystick)
			return;
	}

	/* Don't add this joystick if it already exists in the list */
	instance_id = SDL_JoystickInstanceID(joystick);
	if (find_joystick(instance_id)) {
		if (controller)
			SDL_GameControllerClose(controller);
		else
			SDL_JoystickClose(joystick);

		return;
	}

	axis_count = SDL_JoystickNumAxes(joystick);
	hat_count = SDL_JoystickNumHats(joystick);
	button_count = SDL_JoystickNumButtons(joystick);

	if (controller) {
		axis_count += SDL_CONTROLLER_AXIS_MAX;
		button_count += SDL_CONTROLLER_BUTTON_MAX;
	}

	axis_status = NULL;
	if (axis_count) {
		axis_status = malloc(axis_count * sizeof(*axis_status));
		if (!axis_status) {
			log_err("add_joystick: malloc() failed\n");
			if (controller)
				SDL_GameControllerClose(controller);
			else
				SDL_JoystickClose(joystick);
			return;
		}

		memset(axis_status, 0, axis_count * sizeof(*axis_status));
	}

	if (controller)
		axis_count -= SDL_CONTROLLER_AXIS_MAX;

	button_status = NULL;
	if (button_count) {
		button_status = malloc(button_count * sizeof(*button_status));
		if (!button_status) {
			log_err("add_joystick: malloc() failed\n");
			if (controller)
				SDL_GameControllerClose(controller);
			else
				SDL_JoystickClose(joystick);
			return;
		}

		memset(button_status, 0, button_count * sizeof(*button_status));
	}

	if (controller)
		button_count -= SDL_CONTROLLER_BUTTON_MAX;

	hat_status = NULL;
	if (hat_count) {
		hat_status = malloc(hat_count * sizeof(*hat_status));
		if (!hat_status) {
			log_err("add_joystick: malloc() failed\n");
			if (controller)
				SDL_GameControllerClose(controller);
			else
				SDL_JoystickClose(joystick);
			return;
		}

		memset(hat_status, 0, hat_count * sizeof(*hat_status));
	}

	node = malloc(sizeof(*node));
	if (!node) {
		free(axis_status);
		free(hat_status);
		free(button_status);
		if (controller)
			SDL_GameControllerClose(controller);
		else
			SDL_JoystickClose(joystick);
		return;
	}

	/* Stick this joystick in the first available slot */
	index = find_unused_joystick_id();

	node->axis_count = axis_count;
	node->axis_status = axis_status;
	node->button_count = button_count;
	node->button_status = button_status;
	node->hat_count = hat_count;
	node->hat_status = hat_status;
	node->joystick = joystick;
	node->controller = controller;
	node->index = index;
	node->instance_id = instance_id;

	joystick_list[index] = node;

	check_controller_mappings(node);

	guid = SDL_JoystickGetGUID(joystick);
	SDL_JoystickGetGUIDString(guid, guid_string, 33);
	printf("Joystick %d%s: %s (%s)\n",
	       index,
	       controller ? " (GameController)" : "",
	       controller ? SDL_GameControllerName(controller) :
	       SDL_JoystickName(joystick),
	       guid_string);
}

static void remove_joystick(SDL_JoystickID instance_id)
{
	struct joystick_list_node *tmp;

	tmp = find_joystick(instance_id);
	if (!tmp)
		return;

	joystick_list[tmp->index] = NULL;

	if (tmp->controller)
		SDL_GameControllerClose(tmp->controller);
	else
		SDL_JoystickClose(tmp->joystick);

	if (tmp->axis_status)
		free(tmp->axis_status);

	if (tmp->hat_status)
		free(tmp->hat_status);

	if (tmp->button_status)
		free(tmp->button_status);

	free(tmp);
}

static int load_gamecontroller_mappings(struct config *config)
{
	char *filename;
	char *system_db_path;
	SDL_RWops *file;

	system_db_path = config_get_path(config,
					     CONFIG_DATA_FILE_GAMECONTROLLER_DB,
					     NULL, 0);

	file = SDL_RWFromFile(system_db_path, "rb");
	if (file) {
		if (SDL_GameControllerAddMappingsFromRW(file, 0) < 0) {
			log_err("failed to load %s: %s\n",
			        system_db_path, SDL_GetError());
		}

		file->close(file);
	}

	free(system_db_path);


	filename = config_get_path(config,
				       CONFIG_DATA_FILE_GAMECONTROLLER_DB,
				       NULL, 1);

	file = SDL_RWFromFile(filename, "rb");
	if (file) {
		if (SDL_GameControllerAddMappingsFromRW(file, 0) < 0) {
			log_err("failed to load %s: %s\n",
			        filename, SDL_GetError());
		}

		file->close(file);
	}

	if (filename)
		free(filename);

	return 0;
}

static int joystick_init(struct emu *emu)
{
	int joystick_count;
	int i;

	joystick_list = NULL;
	joystick_list_size = 0;
	
	SDL_JoystickEventState(SDL_ENABLE);
	SDL_GameControllerEventState(SDL_ENABLE);
	SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
	/* Load any user-defined mappings */
	load_gamecontroller_mappings(emu->config);

	/* FIXME This will go away in the future. SDL is supposed to emit
	   JOYDEVICEADDED events for all joysticks present when
	   SDL_InitSubSystem() is called, but the Linux port does not.
	   A patch was recently committed that fixes this, but for
	   now they will get added here to ensure the joysticks do get
	   added.  add_joystick() will return early if the joystick has 
	   already been added, so there's no chance of the stick being
	   opened twice once the events do start getting sent.
	*/
	joystick_count = SDL_NumJoysticks();
	for (i = 0; i < joystick_count; i++) {
		add_joystick(i);
	}

	return 0;
}

int input_native_init(struct emu *emu)
{
	joystick_init(emu);

	return 0;
}

int input_native_shutdown(void)
{
	int i;

	for (i = 0; i < joystick_list_size; i++) {
		if (joystick_list[i])
			remove_joystick(joystick_list[i]->instance_id);
	}

	free(joystick_list);

	return 0;
}

void handle_joy_hat_event(struct joystick_list_node *joystick,
			  union input_new_event *new_event)
{
	int hat;
	uint8_t old_state, new_state;
	int up_released, down_released, left_released, right_released;
	int up_pressed, down_pressed, left_pressed, right_pressed;
	struct config *config;
	int queue_event;

	config = emu->config;
	hat = new_event->common.index;
	old_state = joystick->hat_status[hat].value;
	new_state = new_event->jhat.value;
	joystick->hat_status[hat].value = new_state;

	if (old_state == new_state)
		return;

	queue_event = 1;
	if (joystick->hat_status[hat].mapped && config->gamecontroller)
		queue_event = 0;

	left_released = right_released = up_released = down_released = 0;
	left_pressed = right_pressed = up_pressed = down_pressed = 0;

	switch (old_state) {
	case SDL_HAT_LEFT:      left_released = 1; break;
	case SDL_HAT_RIGHT:     right_released = 1; break;
	case SDL_HAT_UP:        up_released = 1; break;
	case SDL_HAT_DOWN:      down_released = 1; break;
	case SDL_HAT_LEFTUP:    left_released = 1; up_released = 1; break;
	case SDL_HAT_RIGHTUP:   right_released = 1; up_released = 1; break;
	case SDL_HAT_LEFTDOWN:  left_released = 1; down_released = 1; break;
	case SDL_HAT_RIGHTDOWN: right_released = 1; down_released = 1; break;
	}

	switch (new_state) {
	case SDL_HAT_LEFT:      left_pressed = 1; break;
	case SDL_HAT_RIGHT:     right_pressed = 1; break;
	case SDL_HAT_UP:        up_pressed = 1; break;
	case SDL_HAT_DOWN:      down_pressed = 1; break;
	case SDL_HAT_LEFTUP:    left_pressed = 1; up_pressed = 1; break;
	case SDL_HAT_RIGHTUP:   right_pressed = 1; up_pressed = 1; break;
	case SDL_HAT_LEFTDOWN:  left_pressed = 1; down_pressed = 1; break;
	case SDL_HAT_RIGHTDOWN: right_pressed = 1; down_pressed = 1; break;
	}

	if (left_pressed && left_released)
		left_pressed = left_released = 0;
	if (right_pressed && right_released)
		right_pressed = right_released = 0;
	if (up_pressed && up_released)
		up_pressed = up_released = 0;
	if (down_pressed && down_released)
		down_pressed = down_released = 0;

	/* Handle release events */
	new_event->jhat.state = 0;

	if (left_released) {
		new_event->jhat.value = INPUT_JOYHAT_LEFT;
		if (queue_event)
			input_queue_event(new_event);
	}

	if (right_released) {
		new_event->jhat.value = INPUT_JOYHAT_RIGHT;
		if (queue_event)
			input_queue_event(new_event);
	}

	if (up_released) {
		new_event->jhat.value = INPUT_JOYHAT_UP;
		if (queue_event)
			input_queue_event(new_event);
	}

	if (down_released) {
		new_event->jhat.value = INPUT_JOYHAT_DOWN;
		if (queue_event)
			input_queue_event(new_event);
	}

	/* Handle press events */
	new_event->jhat.type = INPUT_EVENT_TYPE_JOYHAT;
	new_event->jhat.state = 1;

	if (left_pressed) {
		new_event->jhat.value = INPUT_JOYHAT_LEFT;
		if (queue_event)
			input_queue_event(new_event);
	}

	if (right_pressed) {
		new_event->jhat.value = INPUT_JOYHAT_RIGHT;
		if (queue_event)
			input_queue_event(new_event);
	}

	if (up_pressed) {
		new_event->jhat.value = INPUT_JOYHAT_UP;
		if (queue_event)
			input_queue_event(new_event);
	}

	if (down_pressed) {
		new_event->jhat.value = INPUT_JOYHAT_DOWN;
		if (queue_event)
			input_queue_event(new_event);
	}

}

void handle_joy_axis_event(struct joystick_list_node *joystick,
			   union input_new_event *new_event)
{
	int old_direction;
	int new_direction;
	int axis;
	int axis_index;
	int queue_event;

	axis = new_event->common.index;

	if (axis >= 0x100)
		axis_index = joystick->axis_count + axis - 0x100;
	else
		axis_index = axis;

	if (joystick->controller && emu->config->gamecontroller &&
	    (axis_index >= joystick->axis_count)) {
		queue_event = 1;
	} else if (!joystick->controller ||
		   (!joystick->axis_status[axis_index].mapped) ||
		   (!emu->config->gamecontroller &&
		    (axis < joystick->axis_count))) {
		queue_event = 1;
	} else {
		queue_event = 0;
	}

	/* Handle standard axis event */
	if (queue_event)
		input_queue_event(new_event);

	new_event->type = INPUT_EVENT_TYPE_JOYAXISBUTTON;


	old_direction = joystick->axis_status[axis_index].value;

	/* FIXME should threshold for simulating "button-style"
	   events be configurable? */
	new_direction = new_event->jaxis.value;
	if (new_direction < (-32768 / 2)) {
		new_direction = -1;
	} else if (new_direction > (32768 / 2)) {
		new_direction = 1;
	} else {
		new_direction = 0;
	}

	if (new_direction == old_direction)
		return;

	if (old_direction) {
		new_event->jaxisbutton.direction = old_direction;
		new_event->jaxisbutton.state = 0;
		if (queue_event)
			input_queue_event(new_event);
	}

	if (new_direction) {
		new_event->jaxisbutton.direction = new_direction;
		new_event->jaxisbutton.state = 1;
		if (queue_event)
			input_queue_event(new_event);
	}

	joystick->axis_status[axis_index].value =
		new_direction;
}

int input_convert_event(SDL_Event *event, union input_new_event *new_event)
{
	struct joystick_list_node *joystick_node;
	int button_index;
	int rc;

	new_event->common.device = 0;
	new_event->common.index = 0;
	new_event->common.misc = 0;

	rc = 1;
	switch(event->type) {
	case SDL_MOUSEMOTION:
		new_event->type = INPUT_EVENT_TYPE_MOUSEMOTION;
		new_event->motion.x = event->motion.x;
		new_event->motion.y = event->motion.y;
		new_event->motion.button_state = event->motion.state;
		new_event->motion.xrel = event->motion.xrel;
		new_event->motion.yrel = event->motion.yrel;
		break;
	case SDL_MOUSEBUTTONDOWN:
	case SDL_MOUSEBUTTONUP:
		new_event->type = INPUT_EVENT_TYPE_MOUSEBUTTON;
		new_event->mbutton.device = event->button.which;
		new_event->mbutton.button = event->button.button;
		new_event->mbutton.x = event->button.x;
		new_event->mbutton.y = event->button.y;
		new_event->mbutton.state =
			(event->type == SDL_MOUSEBUTTONDOWN);
		break;
	case SDL_KEYDOWN:
	case SDL_KEYUP:
		new_event->type = INPUT_EVENT_TYPE_KEYBOARD;
		new_event->key.device = 0;
		new_event->key.key = event->key.keysym.sym;
		new_event->key.state = (event->type == SDL_KEYDOWN);
		break;
	case SDL_CONTROLLERBUTTONDOWN:
	case SDL_CONTROLLERBUTTONUP:
		if (emu->config->gamecontroller) {
			joystick_node = find_joystick(event->cbutton.which);
			new_event->type = INPUT_EVENT_TYPE_JOYBUTTON;
			new_event->jbutton.device = joystick_node->index;
			new_event->jbutton.button = event->cbutton.button + 0x100;
			new_event->jbutton.state =
				(event->type == SDL_CONTROLLERBUTTONDOWN);
		} else {
			rc = 0;
		}
		break;
	case SDL_JOYBUTTONDOWN:
	case SDL_JOYBUTTONUP:
		button_index = event->jbutton.button;
		joystick_node = find_joystick(event->jbutton.which);
		if (button_index >= 0x100)
			button_index = button_index - 0x100 + joystick_node->button_count;
		if (joystick_node->controller && emu->config->gamecontroller &&
		    joystick_node->button_status[button_index].mapped) {
			rc = 0;
			break;
		}

		new_event->type = INPUT_EVENT_TYPE_JOYBUTTON;
		new_event->jbutton.device = joystick_node->index;
		new_event->jbutton.button = event->jbutton.button;
		new_event->jbutton.state =
			(event->type == SDL_JOYBUTTONDOWN);
		break;
	case SDL_JOYHATMOTION:
		joystick_node =	find_joystick(event->jhat.which);
		if (joystick_node->controller && emu->config->gamecontroller) {
			rc = 0;
			break;
		}

		new_event->type = INPUT_EVENT_TYPE_JOYHAT;
		new_event->jhat.device = joystick_node->index;
		new_event->jhat.hat = event->jhat.hat;
		new_event->jhat.value = event->jhat.value;
		break;
	case SDL_JOYAXISMOTION:
		joystick_node =	find_joystick(event->jaxis.which);
		if (joystick_node->controller && emu->config->gamecontroller) {
			rc = 0;
			break;
		}

		new_event->type = INPUT_EVENT_TYPE_JOYAXIS;
		new_event->jaxis.device = joystick_node->index;
		new_event->jaxis.axis = event->jaxis.axis;
		new_event->jaxis.value = event->jaxis.value;
		new_event->common.misc = 0;
		break;
	case SDL_CONTROLLERAXISMOTION:
		joystick_node =	find_joystick(event->jaxis.which);
		new_event->type = INPUT_EVENT_TYPE_JOYAXIS;
		new_event->jaxis.device = joystick_node->index;
		new_event->jaxis.axis = event->jaxis.axis + 0x100;
		new_event->jaxis.value = event->jaxis.value;
		new_event->common.misc = 0;
		break;
	default:
		rc = 0;
	}

	return rc;
}

#if GUI_ENABLED
static void grab_event(union input_new_event *new_event)
{
	uint32_t type;

	memcpy(&grabbed_event.event, new_event,
	       sizeof(*new_event));

	type = binding_config_action_id &
		ACTION_TYPE_MASK;

	switch (new_event->type) {
	case INPUT_EVENT_TYPE_JOYAXIS:
		if ((new_event->jaxis.axis >= 0x100) &&
		    !emu->config->gamecontroller) {
			return;
		}

		if (type == ACTION_TYPE_MOUSE) {
			return;
		}
		break;
	case INPUT_EVENT_TYPE_JOYBUTTON:
		if ((new_event->jbutton.button >= 0x100) &&
		    !emu->config->gamecontroller) {
			return;
		}

	case INPUT_EVENT_TYPE_JOYHAT:
		if (type != ACTION_TYPE_DIGITAL)
			return;
		break;
	}

	/* Require the user to have moved the stick at least
	   50% of the distance in one direction.  This should
	   filter out accidental movements and jitter.
	*/
	if (new_event->type == INPUT_EVENT_TYPE_JOYAXIS) {
		if ((new_event->jaxis.value <= (32768 / 2)) &&
		    (new_event->jaxis.value >= (-32768 / 2))) {
			return;
		}
	}
	sdl_grab_event = 0;
}
#endif

int input_process_event(SDL_Event *event)
{
	int rc;
	union input_new_event new_event;
	struct joystick_list_node *joystick_node;

	rc = input_convert_event(event, &new_event);
	if (rc) {
#if GUI_ENABLED
		if (sdl_grab_event)
			grab_event(&new_event);
#endif
		switch (new_event.type) {
		case INPUT_EVENT_TYPE_JOYAXIS:
			joystick_node = find_joystick(event->jaxis.which);
			handle_joy_axis_event(joystick_node, &new_event);
			break;
		case INPUT_EVENT_TYPE_JOYHAT:
			joystick_node = find_joystick(event->jhat.which);
			handle_joy_hat_event(joystick_node, &new_event);
			break;
		default:
			input_queue_event(&new_event);
		}
	} else {
		switch (event->type) {
		case SDL_JOYDEVICEADDED:
			add_joystick(event->jdevice.which);
			break;
		case SDL_CONTROLLERDEVICEADDED:
			add_joystick(event->cdevice.which);
			break;
		case SDL_JOYDEVICEREMOVED:
			remove_joystick(event->jdevice.which);
			break;
		case SDL_CONTROLLERDEVICEREMOVED:
			remove_joystick(event->cdevice.which);
			break;
		default:
			rc = 0;
		}
	}

	return rc;
}

int input_get_num_joysticks(void)
{
	return joystick_list_size;
}

const char *input_get_joystick_name(int index)
{
	if ((index < 0) || (index > joystick_list_size))
		return NULL;

	if (joystick_list[index] == NULL)
		return NULL;

	return SDL_JoystickName(joystick_list[index]->joystick);
}

int input_get_joystick_guid(int index, char *buffer, size_t size)
{
	SDL_JoystickGUID guid;

	if ((index < 0) || (index > joystick_list_size))
		return -1;

	if (joystick_list[index] == NULL)
		return -1;

	guid = SDL_JoystickGetGUID(joystick_list[index]->joystick);

	SDL_JoystickGetGUIDString(guid, buffer, size);

	return 0;
}

int input_joystick_is_gamecontroller(int index)
{
	if ((index < 0) || (index > joystick_list_size))
		return -1;

	if (joystick_list[index] == NULL)
		return -1;

	if (joystick_list[index]->controller)
		return 1;

	return 0;
}

void input_poll_events(void)
{
	SDL_Event event;

	SDL_PumpEvents();

	while (SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_KEYDOWN,
			      SDL_MULTIGESTURE) > 0) {
		input_process_event(&event);
	}
}
