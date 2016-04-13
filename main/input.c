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

#include <errno.h>
#include <ctype.h>

#include "emu.h"
#include "controller.h"
#include "actions.h"
#if GUI_ENABLED
#include "gui.h"
#endif

#include "video.h"
#include "input.h"
#include "file_io.h"

#if _WIN32
char *strtok_r(char *str, const char *delim, char **saveptr)
{
	return strtok(str, delim);
}
#endif

#define BUF_SIZE 512

static union input_new_event *event_queue;
static int event_queue_size = 0;
static int event_queue_count = 0;

#define KEYBOARD_MODIFIER_COUNT 8
const char *keyboard_modifiers[] = {
	"Left Ctrl", "Right Ctrl",
	"Left Alt", "Right Alt",
	"Left Shift", "Right Shift",
	"Left GUI", "Right GUI",
};

const char *category_names[] = {
	"Controller 1",
	"Controller 2",
	"Controller 3",
	"Controller 4",
	"Arkanoid (Port 1)",
	"Arkanoid (Port 2)",
	"Power Pad (Port 1)",
	"Power Pad (Port 2) / Family Trainer",
	"Zapper (Port 1)",
	"Zapper (Port 2)",
	"Mouse (Port 1)",
	"Mouse (Port 2)",
	"Mouse (Port 3)",
	"Mouse (Port 4)",
	"Keyboard",
	"Emulator",
	"Video",
	"Input",
	"PPU",
	"VS",
	"Savestate",
	"Misc",
};

#define EMU_ACTION_IS_BUTTON(e) (((e)->id & ACTION_TYPE_MASK) == ACTION_TYPE_DIGITAL)

extern int input_native_init(struct emu *emu);
extern int input_native_shutdown(void);
extern int save_screenshot(void);
struct emu_action *input_insert_emu_action(uint32_t emu_action);

#define EMU_ACTION_ID_MAP(emu_action_name, cat, nice) { \
		.name = #emu_action_name, \
		.emu_action_id = ACTION_ ## emu_action_name,	\
		.category = ACTION_CATEGORY_ ## cat,	\
		.description = nice \
}

struct emu_action_id_map emu_action_id_map[] = {
	EMU_ACTION_ID_MAP(ALT_SPEED, EMULATOR, "Alternate Speed"),
	EMU_ACTION_ID_MAP(TOGGLE_CPU_TRACE, EMULATOR, "CPU Trace Toggle"),
	EMU_ACTION_ID_MAP(SWITCH_FOUR_PLAYER_MODE, MISC, "Four-Player Mode Switch"),
	EMU_ACTION_ID_MAP(TOGGLE_SCANLINE_RENDERER, PPU, "Scanline Renderer Toggle"),
	EMU_ACTION_ID_MAP(TOGGLE_FULLSCREEN, VIDEO, "Full-Screen Mode Toggle"),
	EMU_ACTION_ID_MAP(TOGGLE_MENUBAR, EMULATOR, "Menubar Toggle"),
	EMU_ACTION_ID_MAP(TOGGLE_FPS, EMULATOR, "FPS Display Toggle"),
	EMU_ACTION_ID_MAP(TOGGLE_SPRITE_LIMIT, PPU, "Sprite Limit Toggle"),
	EMU_ACTION_ID_MAP(TOGGLE_SPRITES, PPU, "Sprite Display Toggle"),
	EMU_ACTION_ID_MAP(TOGGLE_BG, PPU, "Background Display Toggle"),
	EMU_ACTION_ID_MAP(TOGGLE_MOUSE_GRAB, VIDEO, "Mouse Grab Toggle"),
	EMU_ACTION_ID_MAP(HARD_RESET, EMULATOR, "Hard Reset"),
	EMU_ACTION_ID_MAP(SOFT_RESET, EMULATOR, "Soft Reset"),
	EMU_ACTION_ID_MAP(QUIT, EMULATOR, "Quit"),
	EMU_ACTION_ID_MAP(TOGGLE_CHEATS, EMULATOR, "Cheats Toggle"),
	EMU_ACTION_ID_MAP(DEVICE_SELECT_PORT1, INPUT, "Port 1 Device Select"),
	EMU_ACTION_ID_MAP(DEVICE_CONNECT_PORT1, INPUT, "Port 1 Device Connect"),
	EMU_ACTION_ID_MAP(DEVICE_SELECT_PORT2, INPUT, "Port 2 Device Select"),
	EMU_ACTION_ID_MAP(DEVICE_CONNECT_PORT2, INPUT, "Port 2 Device Connect"),
	EMU_ACTION_ID_MAP(DEVICE_SELECT_PORT3, INPUT, "Port 3 Device Select"),
	EMU_ACTION_ID_MAP(DEVICE_CONNECT_PORT3, INPUT, "Port 3 Device Connect"),
	EMU_ACTION_ID_MAP(DEVICE_SELECT_PORT4, INPUT, "Port 4 Device Select"),
	EMU_ACTION_ID_MAP(DEVICE_CONNECT_PORT4, INPUT, "Port 4 Device Connect"),
	EMU_ACTION_ID_MAP(DEVICE_SELECT_EXP, INPUT, "Expansion Port Device Select"),
	EMU_ACTION_ID_MAP(DEVICE_CONNECT_EXP, INPUT, "Expansion Port Device Connect"),
	EMU_ACTION_ID_MAP(PAUSE, EMULATOR, "Pause/Resume Emulation"),

	EMU_ACTION_ID_MAP(ARKANOID_1_DIAL_MOUSE, ARKANOID1, "Dial (Mouse)"),
	EMU_ACTION_ID_MAP(ARKANOID_1_DIAL, ARKANOID1, "Dial"),
	EMU_ACTION_ID_MAP(ARKANOID_1, ARKANOID1, "Button"),
	EMU_ACTION_ID_MAP(ARKANOID_2_DIAL_MOUSE, ARKANOID2, "Dial (Mouse)"),
	EMU_ACTION_ID_MAP(ARKANOID_2_DIAL, ARKANOID2, "Dial"),
	EMU_ACTION_ID_MAP(ARKANOID_2, ARKANOID2, "Button"),

	EMU_ACTION_ID_MAP(CONTROLLER_1_A, CONTROLLER1, "A (SNES B)"),
	EMU_ACTION_ID_MAP(CONTROLLER_1_B, CONTROLLER1, "B (SNES Y)"),
	EMU_ACTION_ID_MAP(CONTROLLER_1_SELECT, CONTROLLER1, "Select"),
	EMU_ACTION_ID_MAP(CONTROLLER_1_START, CONTROLLER1, "Start"),
	EMU_ACTION_ID_MAP(CONTROLLER_1_UP, CONTROLLER1, "Up"),
	EMU_ACTION_ID_MAP(CONTROLLER_1_DOWN, CONTROLLER1, "Down"),
	EMU_ACTION_ID_MAP(CONTROLLER_1_LEFT, CONTROLLER1, "Left"),
	EMU_ACTION_ID_MAP(CONTROLLER_1_RIGHT, CONTROLLER1, "Right"),
	EMU_ACTION_ID_MAP(CONTROLLER_1_SNES_A, CONTROLLER1, "SNES A"),
	EMU_ACTION_ID_MAP(CONTROLLER_1_SNES_X, CONTROLLER1, "SNES X"),
	EMU_ACTION_ID_MAP(CONTROLLER_1_SNES_L, CONTROLLER1, "SNES L"),
	EMU_ACTION_ID_MAP(CONTROLLER_1_SNES_R, CONTROLLER1, "SNES R"),
	EMU_ACTION_ID_MAP(CONTROLLER_1_A_TURBO, CONTROLLER1, "A (SNES B) Turbo"),
	EMU_ACTION_ID_MAP(CONTROLLER_1_A_TURBO_TOGGLE, CONTROLLER1, "A (SNES B) Turbo Toggle"),
	EMU_ACTION_ID_MAP(CONTROLLER_1_B_TURBO, CONTROLLER1, "B (SNES Y) Turbo"),
	EMU_ACTION_ID_MAP(CONTROLLER_1_B_TURBO_TOGGLE, CONTROLLER1, "B (SNES Y) Turbo Toggle"),
	EMU_ACTION_ID_MAP(CONTROLLER_1_SNES_A_TURBO, CONTROLLER1, "SNES A Turbo"),
	EMU_ACTION_ID_MAP(CONTROLLER_1_SNES_A_TURBO_TOGGLE, CONTROLLER1, "SNES A Turbo Toggle"),
	EMU_ACTION_ID_MAP(CONTROLLER_1_SNES_X_TURBO, CONTROLLER1, "SNES X Turbo"),
	EMU_ACTION_ID_MAP(CONTROLLER_1_SNES_X_TURBO_TOGGLE, CONTROLLER1, "SNES X Turbo Toggle"),
	EMU_ACTION_ID_MAP(CONTROLLER_1_SNES_L_TURBO, CONTROLLER1, "SNES L Turbo"),
	EMU_ACTION_ID_MAP(CONTROLLER_1_SNES_L_TURBO_TOGGLE, CONTROLLER1, "SNES L Turbo Toggle"),
	EMU_ACTION_ID_MAP(CONTROLLER_1_SNES_R_TURBO, CONTROLLER1, "SNES R Turbo"),
	EMU_ACTION_ID_MAP(CONTROLLER_1_SNES_R_TURBO_TOGGLE, CONTROLLER1, "SNES R Turbo Toggle"),

	EMU_ACTION_ID_MAP(CONTROLLER_2_A, CONTROLLER2, "A (SNES B)"),
	EMU_ACTION_ID_MAP(CONTROLLER_2_B, CONTROLLER2, "B (SNES Y)"),
	EMU_ACTION_ID_MAP(CONTROLLER_2_SELECT, CONTROLLER2, "Select"),
	EMU_ACTION_ID_MAP(CONTROLLER_2_START, CONTROLLER2, "Start"),
	EMU_ACTION_ID_MAP(CONTROLLER_2_UP, CONTROLLER2, "Up"),
	EMU_ACTION_ID_MAP(CONTROLLER_2_DOWN, CONTROLLER2, "Down"),
	EMU_ACTION_ID_MAP(CONTROLLER_2_LEFT, CONTROLLER2, "Left"),
	EMU_ACTION_ID_MAP(CONTROLLER_2_RIGHT, CONTROLLER2, "Right"),
	EMU_ACTION_ID_MAP(CONTROLLER_2_SNES_A, CONTROLLER2, "SNES A"),
	EMU_ACTION_ID_MAP(CONTROLLER_2_SNES_X, CONTROLLER2, "SNES X"),
	EMU_ACTION_ID_MAP(CONTROLLER_2_SNES_L, CONTROLLER2, "SNES L"),
	EMU_ACTION_ID_MAP(CONTROLLER_2_SNES_R, CONTROLLER2, "SNES R"),
	EMU_ACTION_ID_MAP(CONTROLLER_2_A_TURBO, CONTROLLER2, "A (SNES B) Turbo"),
	EMU_ACTION_ID_MAP(CONTROLLER_2_A_TURBO_TOGGLE, CONTROLLER2, "A (SNES B) Turbo Toggle"),
	EMU_ACTION_ID_MAP(CONTROLLER_2_B_TURBO, CONTROLLER2, "B (SNES Y) Turbo"),
	EMU_ACTION_ID_MAP(CONTROLLER_2_B_TURBO_TOGGLE, CONTROLLER2, "B (SNES Y) Turbo Toggle"),
	EMU_ACTION_ID_MAP(CONTROLLER_2_SNES_A_TURBO, CONTROLLER1, "SNES A Turbo"),
	EMU_ACTION_ID_MAP(CONTROLLER_2_SNES_A_TURBO_TOGGLE, CONTROLLER1, "SNES A Turbo Toggle"),
	EMU_ACTION_ID_MAP(CONTROLLER_2_SNES_X_TURBO, CONTROLLER1, "SNES X Turbo"),
	EMU_ACTION_ID_MAP(CONTROLLER_2_SNES_X_TURBO_TOGGLE, CONTROLLER1, "SNES X Turbo Toggle"),
	EMU_ACTION_ID_MAP(CONTROLLER_2_SNES_L_TURBO, CONTROLLER1, "SNES L Turbo"),
	EMU_ACTION_ID_MAP(CONTROLLER_2_SNES_L_TURBO_TOGGLE, CONTROLLER1, "SNES L Turbo Toggle"),
	EMU_ACTION_ID_MAP(CONTROLLER_2_SNES_R_TURBO, CONTROLLER1, "SNES R Turbo"),
	EMU_ACTION_ID_MAP(CONTROLLER_2_SNES_R_TURBO_TOGGLE, CONTROLLER1, "SNES R Turbo Toggle"),

	EMU_ACTION_ID_MAP(CONTROLLER_3_A, CONTROLLER3, "A (SNES B)"),
	EMU_ACTION_ID_MAP(CONTROLLER_3_B, CONTROLLER3, "B (SNES Y)"),
	EMU_ACTION_ID_MAP(CONTROLLER_3_SELECT, CONTROLLER3, "Select"),
	EMU_ACTION_ID_MAP(CONTROLLER_3_START, CONTROLLER3, "Start"),
	EMU_ACTION_ID_MAP(CONTROLLER_3_UP, CONTROLLER3, "Up"),
	EMU_ACTION_ID_MAP(CONTROLLER_3_DOWN, CONTROLLER3, "Down"),
	EMU_ACTION_ID_MAP(CONTROLLER_3_LEFT, CONTROLLER3, "Left"),
	EMU_ACTION_ID_MAP(CONTROLLER_3_RIGHT, CONTROLLER3, "Right"),
	EMU_ACTION_ID_MAP(CONTROLLER_3_SNES_A, CONTROLLER3, "SNES A"),
	EMU_ACTION_ID_MAP(CONTROLLER_3_SNES_X, CONTROLLER3, "SNES X"),
	EMU_ACTION_ID_MAP(CONTROLLER_3_SNES_L, CONTROLLER3, "SNES L"),
	EMU_ACTION_ID_MAP(CONTROLLER_3_SNES_R, CONTROLLER3, "SNES R"),
	EMU_ACTION_ID_MAP(CONTROLLER_3_A_TURBO, CONTROLLER3, "A (SNES B) Turbo"),
	EMU_ACTION_ID_MAP(CONTROLLER_3_A_TURBO_TOGGLE, CONTROLLER3, "A (SNES B) Turbo Toggle"),
	EMU_ACTION_ID_MAP(CONTROLLER_3_B_TURBO, CONTROLLER3, "B (SNES Y) Turbo"),
	EMU_ACTION_ID_MAP(CONTROLLER_3_B_TURBO_TOGGLE, CONTROLLER3, "B (SNES Y) Turbo Toggle"),
	EMU_ACTION_ID_MAP(CONTROLLER_3_SNES_A_TURBO, CONTROLLER1, "SNES A Turbo"),
	EMU_ACTION_ID_MAP(CONTROLLER_3_SNES_A_TURBO_TOGGLE, CONTROLLER1, "SNES A Turbo Toggle"),
	EMU_ACTION_ID_MAP(CONTROLLER_3_SNES_X_TURBO, CONTROLLER1, "SNES X Turbo"),
	EMU_ACTION_ID_MAP(CONTROLLER_3_SNES_X_TURBO_TOGGLE, CONTROLLER1, "SNES X Turbo Toggle"),
	EMU_ACTION_ID_MAP(CONTROLLER_3_SNES_L_TURBO, CONTROLLER1, "SNES L Turbo"),
	EMU_ACTION_ID_MAP(CONTROLLER_3_SNES_L_TURBO_TOGGLE, CONTROLLER1, "SNES L Turbo Toggle"),
	EMU_ACTION_ID_MAP(CONTROLLER_3_SNES_R_TURBO, CONTROLLER1, "SNES R Turbo"),
	EMU_ACTION_ID_MAP(CONTROLLER_3_SNES_R_TURBO_TOGGLE, CONTROLLER1, "SNES R Turbo Toggle"),

	EMU_ACTION_ID_MAP(CONTROLLER_4_A, CONTROLLER4, "A (SNES B)"),
	EMU_ACTION_ID_MAP(CONTROLLER_4_B, CONTROLLER4, "B (SNES Y)"),
	EMU_ACTION_ID_MAP(CONTROLLER_4_SELECT, CONTROLLER4, "Select"),
	EMU_ACTION_ID_MAP(CONTROLLER_4_START, CONTROLLER4, "Start"),
	EMU_ACTION_ID_MAP(CONTROLLER_4_UP, CONTROLLER4, "Up"),
	EMU_ACTION_ID_MAP(CONTROLLER_4_DOWN, CONTROLLER4, "Down"),
	EMU_ACTION_ID_MAP(CONTROLLER_4_LEFT, CONTROLLER4, "Left"),
	EMU_ACTION_ID_MAP(CONTROLLER_4_RIGHT, CONTROLLER4, "Right"),
	EMU_ACTION_ID_MAP(CONTROLLER_4_SNES_A, CONTROLLER4, "SNES A"),
	EMU_ACTION_ID_MAP(CONTROLLER_4_SNES_X, CONTROLLER4, "SNES X"),
	EMU_ACTION_ID_MAP(CONTROLLER_4_SNES_L, CONTROLLER4, "SNES L"),
	EMU_ACTION_ID_MAP(CONTROLLER_4_SNES_R, CONTROLLER4, "SNES R"),
	EMU_ACTION_ID_MAP(CONTROLLER_4_A_TURBO, CONTROLLER4, "A (SNES B) Turbo"),
	EMU_ACTION_ID_MAP(CONTROLLER_4_A_TURBO_TOGGLE, CONTROLLER4, "A (SNES B) Turbo Toggle"),
	EMU_ACTION_ID_MAP(CONTROLLER_4_B_TURBO, CONTROLLER4, "B (SNES Y) Turbo"),
	EMU_ACTION_ID_MAP(CONTROLLER_4_B_TURBO_TOGGLE, CONTROLLER4, "B (SNES Y) Turbo Toggle"),
	EMU_ACTION_ID_MAP(CONTROLLER_4_SNES_A_TURBO, CONTROLLER1, "SNES A Turbo"),
	EMU_ACTION_ID_MAP(CONTROLLER_4_SNES_A_TURBO_TOGGLE, CONTROLLER1, "SNES A Turbo Toggle"),
	EMU_ACTION_ID_MAP(CONTROLLER_4_SNES_X_TURBO, CONTROLLER1, "SNES X Turbo"),
	EMU_ACTION_ID_MAP(CONTROLLER_4_SNES_X_TURBO_TOGGLE, CONTROLLER1, "SNES X Turbo Toggle"),
	EMU_ACTION_ID_MAP(CONTROLLER_4_SNES_L_TURBO, CONTROLLER1, "SNES L Turbo"),
	EMU_ACTION_ID_MAP(CONTROLLER_4_SNES_L_TURBO_TOGGLE, CONTROLLER1, "SNES L Turbo Toggle"),
	EMU_ACTION_ID_MAP(CONTROLLER_4_SNES_R_TURBO, CONTROLLER1, "SNES R Turbo"),
	EMU_ACTION_ID_MAP(CONTROLLER_4_SNES_R_TURBO_TOGGLE, CONTROLLER1, "SNES R Turbo Toggle"),

	EMU_ACTION_ID_MAP(MAT_1_1, POWERPAD1, "1"),
	EMU_ACTION_ID_MAP(MAT_1_2, POWERPAD1, "2"),
	EMU_ACTION_ID_MAP(MAT_1_3, POWERPAD1, "3"),
	EMU_ACTION_ID_MAP(MAT_1_4, POWERPAD1, "4"),
	EMU_ACTION_ID_MAP(MAT_1_5, POWERPAD1, "5"),
	EMU_ACTION_ID_MAP(MAT_1_6, POWERPAD1, "6"),
	EMU_ACTION_ID_MAP(MAT_1_7, POWERPAD1, "7"),
	EMU_ACTION_ID_MAP(MAT_1_8, POWERPAD1, "8"),
	EMU_ACTION_ID_MAP(MAT_1_9, POWERPAD1, "9"),
	EMU_ACTION_ID_MAP(MAT_1_10, POWERPAD1, "10"),
	EMU_ACTION_ID_MAP(MAT_1_11, POWERPAD1, "11"),
	EMU_ACTION_ID_MAP(MAT_1_12, POWERPAD1, "12"),
	EMU_ACTION_ID_MAP(MAT_2_1, POWERPAD2, "1"),
	EMU_ACTION_ID_MAP(MAT_2_2, POWERPAD2, "2"),
	EMU_ACTION_ID_MAP(MAT_2_3, POWERPAD2, "3"),
	EMU_ACTION_ID_MAP(MAT_2_4, POWERPAD2, "4"),
	EMU_ACTION_ID_MAP(MAT_2_5, POWERPAD2, "5"),
	EMU_ACTION_ID_MAP(MAT_2_6, POWERPAD2, "6"),
	EMU_ACTION_ID_MAP(MAT_2_7, POWERPAD2, "7"),
	EMU_ACTION_ID_MAP(MAT_2_8, POWERPAD2, "8"),
	EMU_ACTION_ID_MAP(MAT_2_9, POWERPAD2, "9"),
	EMU_ACTION_ID_MAP(MAT_2_10, POWERPAD2, "10"),
	EMU_ACTION_ID_MAP(MAT_2_11, POWERPAD2, "11"),
	EMU_ACTION_ID_MAP(MAT_2_12, POWERPAD2, "12"),
	EMU_ACTION_ID_MAP(ZAPPER_1_TRIGGER, ZAPPER1, "Trigger"),
	EMU_ACTION_ID_MAP(ZAPPER_1_TRIGGER_OFFSCREEN, ZAPPER1, "Trigger (Offscreen)"),
	EMU_ACTION_ID_MAP(ZAPPER_1_UPDATE_LOCATION, ZAPPER1, "Location"),
	EMU_ACTION_ID_MAP(ZAPPER_2_TRIGGER, ZAPPER2, "Trigger"),
	EMU_ACTION_ID_MAP(ZAPPER_2_TRIGGER_OFFSCREEN, ZAPPER2, "Trigger (Offscreen)"),
	EMU_ACTION_ID_MAP(ZAPPER_2_UPDATE_LOCATION, ZAPPER2, "Location"),
	EMU_ACTION_ID_MAP(KEYBOARD_ESCAPE, KEYBOARD, "Escape"),
	EMU_ACTION_ID_MAP(KEYBOARD_F1, KEYBOARD, "F1"),
	EMU_ACTION_ID_MAP(KEYBOARD_F2, KEYBOARD, "F2"),
	EMU_ACTION_ID_MAP(KEYBOARD_F3, KEYBOARD, "F3"),
	EMU_ACTION_ID_MAP(KEYBOARD_F4, KEYBOARD, "F4"),
	EMU_ACTION_ID_MAP(KEYBOARD_F5, KEYBOARD, "F5"),
	EMU_ACTION_ID_MAP(KEYBOARD_F6, KEYBOARD, "F6"),
	EMU_ACTION_ID_MAP(KEYBOARD_F7, KEYBOARD, "F7"),
	EMU_ACTION_ID_MAP(KEYBOARD_F8, KEYBOARD, "F8"),
	EMU_ACTION_ID_MAP(KEYBOARD_F9, KEYBOARD, "F9"),
	EMU_ACTION_ID_MAP(KEYBOARD_F10, KEYBOARD, "F10"),
	EMU_ACTION_ID_MAP(KEYBOARD_F11, KEYBOARD, "F11"),
	EMU_ACTION_ID_MAP(KEYBOARD_F12, KEYBOARD, "F12"),
	EMU_ACTION_ID_MAP(KEYBOARD_BACKQUOTE, KEYBOARD, "`"),
	EMU_ACTION_ID_MAP(KEYBOARD_1, KEYBOARD, "1"),
	EMU_ACTION_ID_MAP(KEYBOARD_2, KEYBOARD, "2"),
	EMU_ACTION_ID_MAP(KEYBOARD_3, KEYBOARD, "3"),
	EMU_ACTION_ID_MAP(KEYBOARD_4, KEYBOARD, "4"),
	EMU_ACTION_ID_MAP(KEYBOARD_5, KEYBOARD, "5"),
	EMU_ACTION_ID_MAP(KEYBOARD_6, KEYBOARD, "6"),
	EMU_ACTION_ID_MAP(KEYBOARD_7, KEYBOARD, "7"),
	EMU_ACTION_ID_MAP(KEYBOARD_8, KEYBOARD, "8"),
	EMU_ACTION_ID_MAP(KEYBOARD_9, KEYBOARD, "9"),
	EMU_ACTION_ID_MAP(KEYBOARD_0, KEYBOARD, "0"),
	EMU_ACTION_ID_MAP(KEYBOARD_MINUS, KEYBOARD, "-"),
	EMU_ACTION_ID_MAP(KEYBOARD_EQUALS, KEYBOARD, "="),
	EMU_ACTION_ID_MAP(KEYBOARD_BS, KEYBOARD, "Backspace"),
	EMU_ACTION_ID_MAP(KEYBOARD_TAB, KEYBOARD, "Tab"),
	EMU_ACTION_ID_MAP(KEYBOARD_q, KEYBOARD, "Q"),
	EMU_ACTION_ID_MAP(KEYBOARD_w, KEYBOARD, "W"),
	EMU_ACTION_ID_MAP(KEYBOARD_e, KEYBOARD, "E"),
	EMU_ACTION_ID_MAP(KEYBOARD_r, KEYBOARD, "R"),
	EMU_ACTION_ID_MAP(KEYBOARD_t, KEYBOARD, "T"),
	EMU_ACTION_ID_MAP(KEYBOARD_y, KEYBOARD, "Y"),
	EMU_ACTION_ID_MAP(KEYBOARD_u, KEYBOARD, "U"),
	EMU_ACTION_ID_MAP(KEYBOARD_i, KEYBOARD, "I"),
	EMU_ACTION_ID_MAP(KEYBOARD_o, KEYBOARD, "O"),
	EMU_ACTION_ID_MAP(KEYBOARD_p, KEYBOARD, "P"),
	EMU_ACTION_ID_MAP(KEYBOARD_LEFTBRACKET, KEYBOARD, "["),
	EMU_ACTION_ID_MAP(KEYBOARD_RIGHTBRACKET, KEYBOARD, "]"),
	EMU_ACTION_ID_MAP(KEYBOARD_BACKSLASH, KEYBOARD, "Backslash"),
	EMU_ACTION_ID_MAP(KEYBOARD_CAPS, KEYBOARD, "Caps Lock"),
	EMU_ACTION_ID_MAP(KEYBOARD_a, KEYBOARD, "A"),
	EMU_ACTION_ID_MAP(KEYBOARD_s, KEYBOARD, "S"),
	EMU_ACTION_ID_MAP(KEYBOARD_d, KEYBOARD, "D"),
	EMU_ACTION_ID_MAP(KEYBOARD_f, KEYBOARD, "F"),
	EMU_ACTION_ID_MAP(KEYBOARD_g, KEYBOARD, "G"),
	EMU_ACTION_ID_MAP(KEYBOARD_h, KEYBOARD, "H"),
	EMU_ACTION_ID_MAP(KEYBOARD_j, KEYBOARD, "J"),
	EMU_ACTION_ID_MAP(KEYBOARD_k, KEYBOARD, "K"),
	EMU_ACTION_ID_MAP(KEYBOARD_l, KEYBOARD, "L"),
	EMU_ACTION_ID_MAP(KEYBOARD_SEMICOLON, KEYBOARD, ";"),
	EMU_ACTION_ID_MAP(KEYBOARD_APOSTROPHE, KEYBOARD, "'"),
	EMU_ACTION_ID_MAP(KEYBOARD_ENTER, KEYBOARD, "Enter"),
	EMU_ACTION_ID_MAP(KEYBOARD_LSHIFT, KEYBOARD, "Left Shift"),
	EMU_ACTION_ID_MAP(KEYBOARD_z, KEYBOARD, "Z"),
	EMU_ACTION_ID_MAP(KEYBOARD_x, KEYBOARD, "X"),
	EMU_ACTION_ID_MAP(KEYBOARD_c, KEYBOARD, "C"),
	EMU_ACTION_ID_MAP(KEYBOARD_v, KEYBOARD, "V"),
	EMU_ACTION_ID_MAP(KEYBOARD_b, KEYBOARD, "B"),
	EMU_ACTION_ID_MAP(KEYBOARD_n, KEYBOARD, "N"),
	EMU_ACTION_ID_MAP(KEYBOARD_m, KEYBOARD, "M"),
	EMU_ACTION_ID_MAP(KEYBOARD_COMMA, KEYBOARD, ","),
	EMU_ACTION_ID_MAP(KEYBOARD_PERIOD, KEYBOARD, "."),
	EMU_ACTION_ID_MAP(KEYBOARD_SLASH, KEYBOARD, "/"),
	EMU_ACTION_ID_MAP(KEYBOARD_RSHIFT, KEYBOARD, "Right Shift"),
	EMU_ACTION_ID_MAP(KEYBOARD_LCTRL, KEYBOARD, "Left Ctrl"),
	EMU_ACTION_ID_MAP(KEYBOARD_LALT, KEYBOARD, "Left Alt"),
	EMU_ACTION_ID_MAP(KEYBOARD_SPACE, KEYBOARD, "Space"),
	EMU_ACTION_ID_MAP(KEYBOARD_RALT, KEYBOARD, "Right Alt"),
	EMU_ACTION_ID_MAP(KEYBOARD_RCTRL, KEYBOARD, "Right Ctrl"),

	EMU_ACTION_ID_MAP(KEYBOARD_INS, KEYBOARD, "Insert"),
	EMU_ACTION_ID_MAP(KEYBOARD_HOME, KEYBOARD, "Home"),
	EMU_ACTION_ID_MAP(KEYBOARD_PGUP, KEYBOARD, "Page Up"),
	EMU_ACTION_ID_MAP(KEYBOARD_DEL, KEYBOARD, "Delete"),
	EMU_ACTION_ID_MAP(KEYBOARD_END, KEYBOARD, "End"),
	EMU_ACTION_ID_MAP(KEYBOARD_PGDN, KEYBOARD, "Page Down"),
	EMU_ACTION_ID_MAP(KEYBOARD_UP, KEYBOARD, "Up"),
	EMU_ACTION_ID_MAP(KEYBOARD_LEFT, KEYBOARD, "Left"),
	EMU_ACTION_ID_MAP(KEYBOARD_DOWN, KEYBOARD, "Down"),
	EMU_ACTION_ID_MAP(KEYBOARD_RIGHT, KEYBOARD, "Right"),

	EMU_ACTION_ID_MAP(KEYBOARD_COLON, KEYBOARD, ":"),
	EMU_ACTION_ID_MAP(KEYBOARD_KANA, KEYBOARD, "Kana"),
	EMU_ACTION_ID_MAP(KEYBOARD_UNDERSCORE, KEYBOARD, "_"),
	EMU_ACTION_ID_MAP(KEYBOARD_GRAPH, KEYBOARD, "Graph"),
	EMU_ACTION_ID_MAP(KEYBOARD_PAUSE, KEYBOARD, "Pause"),
	EMU_ACTION_ID_MAP(KEYBOARD_BREAK, KEYBOARD, "Break"),
	EMU_ACTION_ID_MAP(KEYBOARD_RESET, KEYBOARD, "Reset"),
	EMU_ACTION_ID_MAP(KEYBOARD_NUMLOCK, KEYBOARD, "Numlock"),
	EMU_ACTION_ID_MAP(KEYBOARD_YEN, KEYBOARD, "Yen"),
	EMU_ACTION_ID_MAP(KEYBOARD_STOP, KEYBOARD, "Stop"),
	EMU_ACTION_ID_MAP(KEYBOARD_AT, KEYBOARD, "@"),
	EMU_ACTION_ID_MAP(KEYBOARD_CARET, KEYBOARD, "^"),

	EMU_ACTION_ID_MAP(MOUSE_1_LEFTBUTTON, MOUSE1, "Left Button"),
	EMU_ACTION_ID_MAP(MOUSE_1_RIGHTBUTTON, MOUSE1, "Right Button"),
	EMU_ACTION_ID_MAP(MOUSE_1_UPDATE_LOCATION, MOUSE1, "X/Y"),
	EMU_ACTION_ID_MAP(MOUSE_2_LEFTBUTTON, MOUSE2, "Left Button"),
	EMU_ACTION_ID_MAP(MOUSE_2_RIGHTBUTTON, MOUSE2, "Right Button"),
	EMU_ACTION_ID_MAP(MOUSE_2_UPDATE_LOCATION, MOUSE2, "X/Y"),
	EMU_ACTION_ID_MAP(MOUSE_3_LEFTBUTTON, MOUSE3, "Left Button"),
	EMU_ACTION_ID_MAP(MOUSE_3_RIGHTBUTTON, MOUSE3, "Right Button"),
	EMU_ACTION_ID_MAP(MOUSE_3_UPDATE_LOCATION, MOUSE3, "X/Y"),
	EMU_ACTION_ID_MAP(MOUSE_4_LEFTBUTTON, MOUSE4, "Left Button"),
	EMU_ACTION_ID_MAP(MOUSE_4_RIGHTBUTTON, MOUSE4, "Right Button"),
	EMU_ACTION_ID_MAP(MOUSE_4_UPDATE_LOCATION, MOUSE4, "X/Y"),
	EMU_ACTION_ID_MAP(VS_SERVICE_SWITCH, VS, "Service Switch"),
	EMU_ACTION_ID_MAP(VS_COIN_SWITCH_1, VS, "Coin Switch 1"),
	EMU_ACTION_ID_MAP(VS_COIN_SWITCH_2, VS, "Coin Switch 2"),
	EMU_ACTION_ID_MAP(DIP_SWITCH_1, MISC, "DIP Switch 1"),
	EMU_ACTION_ID_MAP(DIP_SWITCH_2, MISC, "DIP Switch 2"),
	EMU_ACTION_ID_MAP(DIP_SWITCH_3, MISC, "DIP Switch 3"),
	EMU_ACTION_ID_MAP(DIP_SWITCH_4, MISC, "DIP Switch 4"),
	EMU_ACTION_ID_MAP(DIP_SWITCH_5, MISC, "DIP Switch 5"),
	EMU_ACTION_ID_MAP(DIP_SWITCH_6, MISC, "DIP Switch 6"),
	EMU_ACTION_ID_MAP(DIP_SWITCH_7, MISC, "DIP Switch 7"),
	EMU_ACTION_ID_MAP(DIP_SWITCH_8, MISC, "DIP Switch 8"),
	EMU_ACTION_ID_MAP(MICROPHONE, MISC, "Famicom Microphone"),

	EMU_ACTION_ID_MAP(FDS_EJECT, EMULATOR, "FDS Disk Eject"),
	EMU_ACTION_ID_MAP(FDS_SELECT, EMULATOR, "FDS Disk Select"),

	EMU_ACTION_ID_MAP(SAVE_SCREENSHOT, EMULATOR, "Save Screenshot"),

	EMU_ACTION_ID_MAP(STATE_SAVE_OLDEST, SAVESTATE, "Save State (Oldest)"),
	EMU_ACTION_ID_MAP(STATE_SAVE_0, SAVESTATE, "Save State 0"),
	EMU_ACTION_ID_MAP(STATE_SAVE_1, SAVESTATE, "Save State 1"),
	EMU_ACTION_ID_MAP(STATE_SAVE_2, SAVESTATE, "Save State 2"),
	EMU_ACTION_ID_MAP(STATE_SAVE_3, SAVESTATE, "Save State 3"),
	EMU_ACTION_ID_MAP(STATE_SAVE_4, SAVESTATE, "Save State 4"),
	EMU_ACTION_ID_MAP(STATE_SAVE_5, SAVESTATE, "Save State 5"),
	EMU_ACTION_ID_MAP(STATE_SAVE_6, SAVESTATE, "Save State 6"),
	EMU_ACTION_ID_MAP(STATE_SAVE_7, SAVESTATE, "Save State 7"),
	EMU_ACTION_ID_MAP(STATE_SAVE_8, SAVESTATE, "Save State 8"),
	EMU_ACTION_ID_MAP(STATE_SAVE_9, SAVESTATE, "Save State 9"),
	EMU_ACTION_ID_MAP(STATE_LOAD_NEWEST, SAVESTATE, "Load State (Newest)"),
	EMU_ACTION_ID_MAP(STATE_LOAD_0, SAVESTATE, "Load State 0"),
	EMU_ACTION_ID_MAP(STATE_LOAD_1, SAVESTATE, "Load State 1"),
	EMU_ACTION_ID_MAP(STATE_LOAD_2, SAVESTATE, "Load State 2"),
	EMU_ACTION_ID_MAP(STATE_LOAD_3, SAVESTATE, "Load State 3"),
	EMU_ACTION_ID_MAP(STATE_LOAD_4, SAVESTATE, "Load State 4"),
	EMU_ACTION_ID_MAP(STATE_LOAD_5, SAVESTATE, "Load State 5"),
	EMU_ACTION_ID_MAP(STATE_LOAD_6, SAVESTATE, "Load State 6"),
	EMU_ACTION_ID_MAP(STATE_LOAD_7, SAVESTATE, "Load State 7"),
	EMU_ACTION_ID_MAP(STATE_LOAD_8, SAVESTATE, "Load State 8"),
	EMU_ACTION_ID_MAP(STATE_LOAD_9, SAVESTATE, "Load State 9"),
	EMU_ACTION_ID_MAP(STATE_SELECT_0, SAVESTATE, "Select State 0"),
	EMU_ACTION_ID_MAP(STATE_SELECT_1, SAVESTATE, "Select State 1"),
	EMU_ACTION_ID_MAP(STATE_SELECT_2, SAVESTATE, "Select State 2"),
	EMU_ACTION_ID_MAP(STATE_SELECT_3, SAVESTATE, "Select State 3"),
	EMU_ACTION_ID_MAP(STATE_SELECT_4, SAVESTATE, "Select State 4"),
	EMU_ACTION_ID_MAP(STATE_SELECT_5, SAVESTATE, "Select State 5"),
	EMU_ACTION_ID_MAP(STATE_SELECT_6, SAVESTATE, "Select State 6"),
	EMU_ACTION_ID_MAP(STATE_SELECT_7, SAVESTATE, "Select State 7"),
	EMU_ACTION_ID_MAP(STATE_SELECT_8, SAVESTATE, "Select State 8"),
	EMU_ACTION_ID_MAP(STATE_SELECT_9, SAVESTATE, "Select State 9"),
	EMU_ACTION_ID_MAP(STATE_SAVE_SELECTED, SAVESTATE, "Save Selected State"),
	EMU_ACTION_ID_MAP(STATE_LOAD_SELECTED, SAVESTATE, "Load Selected State"),
	EMU_ACTION_ID_MAP(STATE_SELECT_PREV, SAVESTATE, "Select Previous State"),
	EMU_ACTION_ID_MAP(STATE_SELECT_NEXT, SAVESTATE, "Select Next State"),

	EMU_ACTION_ID_MAP(SYSTEM_TYPE_SELECT, EMULATOR, "Select System Type"),

	EMU_ACTION_ID_MAP(NONE, MISC, NULL),
};

static char *controller_buttons[] = {
	"A", "B", "X", "Y", "Back", "Guide",
	"Start", "Left Stick", "Right Stick",
	"Left Shoulder", "Right Shoulder",
	"D-Pad Up", "D-Pad Down", "D-Pad Left",
	"D-Pad Right", NULL
};

static char *controller_axes[] = {
	"Left X", "Left Y", "Right X",
	"Right Y", "Left Trigger",
	"Right Trigger", NULL
};

const char *modifier_names[INPUT_MOD_COUNT] = {
	"MOD1",
	"MOD2",
	"MOD3",
	"KBD",
	"CTRL",
	"ALT",
	"SHIFT",
	"GUI",
};

static int ignore_events;
static int mod_bits;
struct input_event_node *event_hash[EVENT_HASH_SIZE];
static struct emu_action *emu_action_list;
static int modifier_count[INPUT_MOD_COUNT];
extern int running;
static struct input_mouse_motion_event motion_event;
extern int center_x, center_y;
extern int mouse_grabbed;

extern struct emu *emu;

static int get_effective_modifiers(struct input_event_node *event, int mod);
static struct input_event_node *input_lookup_event(union input_new_event *input_new_event);
int input_handle_event(union input_new_event *input_event, int force);

static int parse_mouse_binding(const char *buffer, union input_new_event *event)
{
	int button, end;

	end = 0;

	if (sscanf(buffer, " mouse button %d%n", &button, &end) >= 1) {
		event->type = INPUT_EVENT_TYPE_MOUSEBUTTON;
		/* FIXME multiple mice should be supported */
		event->mbutton.device = 0;
		event->mbutton.button = button;
		if (!end || buffer[end])
			return 0;
	} else if (strcasecmp(buffer, "mouse") == 0) {
		event->type = INPUT_EVENT_TYPE_MOUSEMOTION;
		/* FIXME multiple mice should be supported */
		event->motion.device = 0;
		event->motion.padding = 0;
	} else {
		return 0;
	}

	return 1;
}

static int parse_keyboard_binding(const char *buffer,
				  union input_new_event *event)
{
	uint32_t keycode;
	int offset;

	offset = 0;

	sscanf(buffer, " keyboard %n", &offset);
	if (offset == 0)
		return 0;

	keycode = input_lookup_keycode_from_name(buffer + offset);

	if (keycode == 0)
		return 0;

	event->type = INPUT_EVENT_TYPE_KEYBOARD;
	event->key.device = 0;
	event->key.key = keycode;

	return 1;
}

static int parse_joystick_binding(const char *buffer,
				  union input_new_event *event)
{
	int offset, end;
	int index;
	unsigned int joystick;
	unsigned int hat_direction_offset;
	char direction;
	char format_buffer[80];
	char *t;

	offset = 0;

	end = 0;
	joystick = -1;
	index = -1;
	hat_direction_offset = 0;
	direction = 0;

	if (sscanf(buffer, " joystick %u axis %u %[+-]%n", &joystick, &index,
		   &direction, &end) >= 3) {
		event->type = INPUT_EVENT_TYPE_JOYAXISBUTTON;
	} else if (sscanf(buffer, " joystick %u axis %u%n",
			  &joystick, &index, &end) >= 2) {
		event->type = INPUT_EVENT_TYPE_JOYAXIS;
	} else if (sscanf(buffer, " joystick %u button %u%n",
			  &joystick, &index, &end) >= 2) {
		event->type = INPUT_EVENT_TYPE_JOYBUTTON;
	} else if (sscanf(buffer, " joystick %u hat %u %n%*s %n", &joystick, &index,
			  &hat_direction_offset, &end) >= 2) {
		event->type = INPUT_EVENT_TYPE_JOYHAT;
	} else {
		int i;

		for (i = 0; controller_buttons[i]; i++) {
			snprintf(format_buffer, sizeof(format_buffer),
				 " joystick %%u %s %%n",
				 controller_buttons[i]);

			for (t = format_buffer; *t; t++)
				*t = tolower(*t);

			offset = 0;
			sscanf(buffer, format_buffer, &joystick, &offset);

			if (offset && !buffer[offset]) {
				end = offset;
				event->type = INPUT_EVENT_TYPE_JOYBUTTON;
				index = i + 0x100;
				break;
				
			}
		}

		for (i = 0; index < 0 && controller_axes[i]; i++) {
			snprintf(format_buffer, sizeof(format_buffer),
				 " joystick %%u %s %%n%%c",
				 controller_axes[i]);

			for (t = format_buffer; *t; t++)
				*t = tolower(*t);

			offset = 0;
			direction = 0;
			sscanf(buffer, format_buffer, &joystick, &offset, &direction);

			if ((direction == '+' || direction == '-') &&
			    !(buffer[offset + 1])) {
				end = offset + 1;
				event->type = INPUT_EVENT_TYPE_JOYAXISBUTTON;
				index = i + 0x100;
				break;
				
			} else if (!buffer[offset]) {
				end = offset;
				event->type = INPUT_EVENT_TYPE_JOYAXIS;
				index = i + 0x100;
				break;
			}
		}
	}

	if (!end || (end && buffer[end])) {
		return 0;
	}

	switch (event->type) {
	case INPUT_EVENT_TYPE_JOYAXIS:
		event->jaxis.device = joystick;
		event->jaxis.axis = index;
		break;
	case INPUT_EVENT_TYPE_JOYAXISBUTTON:
		event->jaxisbutton.device = joystick;
		event->jaxisbutton.axis = index;
		if (direction == '-')
			event->jaxisbutton.direction = -1;
		else
			event->jaxisbutton.direction = 1;
		break;
	case INPUT_EVENT_TYPE_JOYBUTTON:
		event->jbutton.device = joystick;
		event->jbutton.button = index;
		break;
	case INPUT_EVENT_TYPE_JOYHAT:
		event->jhat.device = joystick;
		event->jhat.hat = index;

		if (!hat_direction_offset)
			return 0;

		if (!strcasecmp(&buffer[hat_direction_offset], "up"))
			event->jhat.value = INPUT_JOYHAT_UP;
		else if (!strcasecmp(&buffer[hat_direction_offset], "down"))
			event->jhat.value = INPUT_JOYHAT_DOWN;
		else if (!strcasecmp(&buffer[hat_direction_offset], "left"))
			event->jhat.value = INPUT_JOYHAT_LEFT;
		else if (!strcasecmp(&buffer[hat_direction_offset], "right"))
			event->jhat.value = INPUT_JOYHAT_RIGHT;
		else
			return 0;

		break;
	}

	return 1;
}

void input_print_hash_bucket_sizes(void)
{
	int i;

	for (i = 0; i < EVENT_HASH_SIZE; i++) {
		struct input_event_node *p;
		int count;

		count = 0;
		p = event_hash[i];

		while (p) {
			if (i < 2) {
				printf("%d %d %x\n", p->event.common.type,
				       p->event.common.device,
				       p->event.common.index);
			}
			count++;
			p = p->next;
		}

		printf("slot %d: %d\n", i, count);
	}

}

struct emu_action *input_insert_emu_action(uint32_t emu_action)
{
	struct emu_action **e;
	
	e = &emu_action_list;

	while (*e) {
		if ((*e)->id == emu_action)
			return *e;
	
		e = &((*e)->next);
	}

	if (!*e) {
		*e = malloc(sizeof(**e));
		if (!*e)
			return NULL;

		(*e)->next = NULL;
	}

	(*e)->handler = NULL;
	(*e)->data = NULL;
	(*e)->id = emu_action;
	(*e)->count = 0;

//	input_print_hash_bucket_sizes();

	return *e;
}

static void sanitize_binding(char *dest, const char *src, int dest_len)
{
	int len, i, j;
	const char *end;

	/* Sanitize the string so that all chars are
	   lower case, leading whitespace is stripped
	   and whitespace sequences are reduced to a
	   single space character.
	*/
	while (*src && isspace(*src))
		src++;

	end = src + strlen(src) - 1;
	while ((end > src) && isspace(*end))
		end--;

	if (*end)
		end++;

	len = end - src;
	if (len > dest_len)
		len = dest_len;

	j = 0;
	for (i = 0; i < len; i++) {
		if (isspace(src[i])) {
			if (!isspace(src[i - 1])) {
				dest[j] = ' ';
				j++;
			}
		} else {
			dest[j] = tolower(src[i]);
			j++;
		}
	}

	dest[j] = '\0';
}

int input_parse_binding(const char *string, union input_new_event *event,
			int *modp)
{
	const char *ptr;
	int mod;
	int modifier_start, modifier_end, binding_start;
	char *modifier_tmp;
	char *saveptr, *token;
	char buf[80];
	int len;

	memset(event, 0, sizeof(*event));

	modifier_start = modifier_end = binding_start = 0;
	sscanf(string, " [ %n%*[^]]%n ] %n%*s", &modifier_start, &modifier_end,
	       &binding_start);

	modifier_tmp = NULL;
	mod = 0;
	len = modifier_end - modifier_start;
	modifier_tmp = malloc(len + 1);
	if (!modifier_tmp)
		return -1;

	memcpy(modifier_tmp, string + modifier_start, len);
	modifier_tmp[len] = '\0';

	token = strtok_r(modifier_tmp, "-", &saveptr);
	while (token) {
		int i;

		for (i = 0; i < INPUT_MOD_COUNT; i++) {
			if (strcasecmp(token, modifier_names[i]) == 0) {
				mod |= 1 << i;
				break;
			}
		}
		token = strtok_r(NULL, "-", &saveptr);
	}

	free(modifier_tmp);
	ptr = string + binding_start;

	sanitize_binding(buf, ptr, sizeof(buf));

	if (!parse_keyboard_binding(buf, event) &&
	    !parse_joystick_binding(buf, event) &&
	    !parse_mouse_binding(buf, event)) {
			
		return -1;
	}

	if (modp)
		*modp = mod;

	return 0;
}

int emu_action_lookup_by_name(const char *name, uint32_t *ptr)
{
	int i, count;

	count = sizeof(emu_action_id_map) /
		sizeof(emu_action_id_map[0]);

	for (i = 0; i < count; i++) {
		if (!strcasecmp(name, emu_action_id_map[i].name))
			break;
	}

	if (i == count)
		return -1;

	if (ptr)
		*ptr = emu_action_id_map[i].emu_action_id;

	return 0;
}

int input_bind(const char *binding, const char *emu_actions)
{
	union input_new_event event;
	struct emu_action *e;
	struct input_event_node *input_event;
	uint32_t emu_action;
	char *tmp;
	char *saveptr;
	int i;
	char *token;
	int mod;

	tmp = strdup(emu_actions);
	if (!tmp)
		return -1;

	memset(&event, 0, sizeof(event));

	if (input_parse_binding(binding, &event, &mod) != 0) {
		log_err("invalid binding %s\n", binding);
		return -1;
	}

	input_event = input_lookup_event(&event);
	if (input_event) {
		for (i = 0; i < input_event->mapping_count; i++) {
			if (input_event->mappings[i].mod_bits != mod)
				continue;
			
			memmove(&input_event->mappings[i],
				&input_event->mappings[i+1],
				sizeof(input_event->mappings[i]) *
				(input_event->mapping_count - i));
			input_event->mapping_count--;
			i--;
		}
	}

	saveptr = NULL;
	token = strtok_r(tmp, ",", &saveptr);
	while (token) {
		char *end;

		while (isspace(*token))
			token++;

		end = token + strlen(token) - 1;
		while (isspace(*end)) {
			*end = '\0';
			end--;
		}

		if (emu_action_lookup_by_name(token, &emu_action) < 0) {
			log_err("invalid emu_action '%s' for binding %s\n",
				token, binding);
		} else {
			e = input_lookup_emu_action(emu_action);
			input_insert_event(&event, mod, e);
		}
		
		token = strtok_r(NULL, ",", &saveptr);
	}

	free(tmp);

	return 0;
}

int input_connect_handlers(const struct input_event_handler *handlers, void *data)
{
	struct emu_action *e;
	int i;

	if (!handlers)
		return 0;

	for (i = 0; handlers[i].id != ACTION_NONE; i++) {
		uint32_t emu_action = handlers[i].id;

		if ((handlers[i].id & ACTION_PREFIX_MASK) ==
		    handlers[i].id) {
			for (e = emu_action_list; e; e = e->next) {
				if (((e->id & ACTION_PREFIX_MASK) !=
				     handlers[i].id) || e->handler) {
					continue;
				}

				e->handler = handlers[i].handler;
				e->data = data;
			}

			return 0;
		}

		e = input_lookup_emu_action(emu_action);
		if (!e)
			continue;

		e->handler = handlers[i].handler;
		e->data = data;
	}

	return 0;
}

struct emu_action *input_lookup_emu_action(uint32_t id)
{
	struct emu_action *e;
	
	e = emu_action_list;

	while (e) {
		if (e->id == id)
			break;

		e = e->next;
	}

	return e;
}

void input_ignore_events(int ignore)
{
	ignore_events = ignore;
}

void input_release_all(void)
{
	struct input_event_node *p;
	struct emu_action *event;
	int effective_mods;
	int i, j;

	for (i = 0; i < EVENT_HASH_SIZE; i++) {
		for (p = event_hash[i]; p; p = p->next) {
			if (!p->pressed)
				continue;

			effective_mods = get_effective_modifiers(p, mod_bits);
			for (j = 0; j < p->mapping_count; j++) {
				event = p->mappings[j].emu_action;
				if (p->mappings[j].mod_bits != effective_mods)
					continue;

				if (!event)
					continue;

				if (event->handler && event->count) {
					event->handler(event->data, 0,
						       event->id);
				}
				event->count = 0;
			}

			p->pressed = 0;
		}
	}

	for (i = 0; i < INPUT_MOD_COUNT; i++)
		modifier_count[i] = 0;

	/* Since MOD_KBD is a toggle, leave it set if it was set.
	   Clear all other modifiers.
	*/
	mod_bits &= INPUT_MOD_BITS_KBD;
}

static void input_update_mod_bits(int mod, int set)
{
	int new_mod_bits;
	struct input_event_node *p;
	struct emu_action *old, *new;
	int effective_mods;
	int i, j;

	if (mod == INPUT_MOD_KBD) {
		if (!set)
			return;

		set = !(mod_bits & (1 << mod));
	}

	if (set) {
		modifier_count[mod]++;
	} else if (modifier_count[mod] > 0) {
		modifier_count[mod]--;
	}

	if (modifier_count[mod])
		new_mod_bits = mod_bits | (1 << mod);
	else
		new_mod_bits = mod_bits & ~(1 << mod);

	if (new_mod_bits == mod_bits)
		return;

	/* Handle synthetic press events triggered by the new modifier
	 * set */
	for (i = 0; i < EVENT_HASH_SIZE; i++) {
		for (p = event_hash[i]; p; p = p->next) {
			if (!p->pressed || (p->modifier >= 0))
				continue;

			effective_mods =
				get_effective_modifiers(p, new_mod_bits);
			for (j = 0; j < p->mapping_count; j++) {
				new = p->mappings[j].emu_action;

				if (!EMU_ACTION_IS_BUTTON(new))
					continue;

				if (p->mappings[j].mod_bits !=
				    effective_mods) {
					continue;
				}

				if (!new)
					continue;

				new->count++;

				if ((new->count == 1) && new->handler)
					new->handler(new->data, 1, new->id);
			}
		}
	}

	/* Handle synthetic release events triggered by the new modifier
	 * set */
	for (i = 0; i < EVENT_HASH_SIZE; i++) {
		for (p = event_hash[i]; p; p = p->next) {
			int is_pressed = 0;
			if (!p->pressed || (p->modifier >= 0))
				continue;

			effective_mods =
				get_effective_modifiers(p, mod_bits);
			for (j = 0; j < p->mapping_count; j++) {
				old = p->mappings[j].emu_action;

				if (!EMU_ACTION_IS_BUTTON(old))
					continue;

				if (p->mappings[j].mod_bits !=
				    effective_mods) {
					continue;
				}

				if (!old || !old->count)
					continue;

				old->count--;

				if (old->count == 0) {
					if (old->handler) {
						old->handler(old->data, 0,
							     old->id);
					}
				} else {
					is_pressed++;
				}
			}

			/* if (!is_pressed) */
			/* 	p->pressed = 0; */
		}
	}
	mod_bits = new_mod_bits;
}

void input_get_mouse_state(int *x, int *y, int *xrel, int *yrel)
{
	int xdelta, ydelta;

	if (x)
		*x = motion_event.x;
	if (y)
		*y = motion_event.y;

	xdelta = ydelta = 0;
	if (mouse_grabbed) {
		xdelta = motion_event.xrel;
		ydelta = motion_event.yrel;
	}

	if (xrel)
		*xrel = xdelta;
	if (yrel)
		*yrel = ydelta;

	/* Reset the relative motion data
	   until the next mouse motion event
	   happens.
	*/
	motion_event.xrel = 0;
	motion_event.yrel = 0;
}

int input_handle_event(union input_new_event *input_event, int force)
{
	int value;
	int index;
	struct input_event_node *p;
	struct emu_action *e;
	int old_count;
	int mod, m;
	int call_handler;
	int is_button, is_axis;
	int effective_mods;
	int was_pressed;
	struct config *config;

	if (input_event->common.processed)
		return 0;

	input_event->common.processed = 1;

	if (ignore_events)
		return 0;

	mod = mod_bits;
	config = emu->config;

	/* Check if event exists and if so what modifier it is */
	p = input_lookup_event(input_event);
	if (!p)
		return 0;

	value = 0;
	is_button = 0;
	is_axis = 0;
	was_pressed = p->pressed;
	switch (input_event->type) {
	case INPUT_EVENT_TYPE_MOUSEMOTION:
		if (mouse_grabbed &&
		    (input_event->motion.x == center_x) &&
		    (input_event->motion.y == center_y)) {
			break;
		}
		memcpy(&motion_event, &input_event->motion,
		       sizeof(motion_event));
		value = (1 << 31);
		break;
	case INPUT_EVENT_TYPE_KEYBOARD:
		p->pressed = input_event->key.state;
		is_button = 1;
		break;
	case INPUT_EVENT_TYPE_MOUSEBUTTON:
		p->pressed = input_event->mbutton.state;
		is_button = 1;
		break;
	case INPUT_EVENT_TYPE_JOYBUTTON:
		p->pressed = input_event->jbutton.state;
		is_button = 1;
		break;
	case INPUT_EVENT_TYPE_JOYAXISBUTTON:
		p->pressed = input_event->jaxisbutton.state;
		is_button = 1;
		break;
	case INPUT_EVENT_TYPE_JOYAXIS:
		is_axis = 1;
		value = input_event->jaxis.value;
		if ((value < (-32768 / 2)) ||
		    (value > (32768 / 2))) {
			p->pressed = 1;
		} else {
			p->pressed = 0;
		}
		break;
	case INPUT_EVENT_TYPE_JOYHAT:
		p->pressed = input_event->jhat.state;
		is_button = 1;
		break;
	}

	if ((is_button || is_axis) && (was_pressed != p->pressed)) {
		/* Update modifier status, if necessary */
		m = p->modifier;
		/* Modifier keys can have bindings, but
		   without modifiers */
		if (m >= 0) {
			/* keyboard lock mode overrides all modifiers except
			   MOD_KBD */
			if (p->event.common.type == INPUT_EVENT_TYPE_KEYBOARD) {
				if ((mod_bits & INPUT_MOD_BITS_KBD) &&
				    (m != INPUT_MOD_KBD)) {
					mod = INPUT_MOD_BITS_KBD;
					m = -1;
				} else {
					mod = 0;
				}
			} else {
				mod = 0;
			}
		}

		if (m >= 0)
			input_update_mod_bits(m, p->pressed);
	} else if (is_button && (was_pressed == p->pressed)) {
		/* exit early for buttons, as there is nothing
		   else to do for them.  Axes can still be handled
		   as axis events.
		*/
		return 0;
	}

	effective_mods = get_effective_modifiers(p, mod);
	for (index = 0; index < p->mapping_count; index++) {
		if (p->mappings[index].mod_bits != effective_mods)
			continue;

		e = p->mappings[index].emu_action;
		if (!e)
			continue;

		if (emu->board && emu_system_is_vs(emu) &&
		    config->vs_coin_on_start) {
			uint32_t id = ACTION_VS_COIN_SWITCH_1;

			if ((config->swap_start_select ||
			     config->vs_swap_start_select)) {
				if ((e->id == ACTION_CONTROLLER_1_SELECT) ||
				    (e->id == ACTION_CONTROLLER_2_SELECT)) {
					e = input_lookup_emu_action(id);
				}
			} else if ((e->id == ACTION_CONTROLLER_1_START) ||
				   (e->id == ACTION_CONTROLLER_2_START)) {
				e = input_lookup_emu_action(id);
			}

			if (!e)
				continue;
		}

		
		if ((e->id & ACTION_DEFERRED_FLAG) && !force) {
			input_event->common.processed = 0;
			p->pressed = was_pressed;
			return 0;
		}

		call_handler = 1;
		if (EMU_ACTION_IS_BUTTON(e) && (was_pressed != p->pressed)) {
			old_count = e->count;
			value = p->pressed;

			if (value)
				e->count++;
			else if (e->count > 0)
				e->count--;

			if ((old_count && e->count) ||
			    (!old_count && !e->count)) {
				call_handler = 0;
			}

		}

		if (emu_paused(emu) &&
		    !(e->id & ACTION_ALLOWED_WHILE_PAUSED)) {
			call_handler = 0;
		} else if (!emu_loaded(emu) &&
			   !(e->id & ACTION_ALLOWED_WHILE_STOPPED)) {
			call_handler = 0;
		}


		if (call_handler && e->handler) {
			e->handler(e->data, value, e->id);
		}
	}


	return 0;
}

static int misc_buttons(void *data, uint32_t pressed, uint32_t button)
{
	struct emu *emu = data;
	int state_index;

	switch (button) {
	case ACTION_ALT_SPEED:
		if (pressed)
			emu_set_framerate(emu, emu->config->alternate_speed);
		else
			emu_set_framerate(emu, emu->nes_framerate);
		break;
	case ACTION_SYSTEM_TYPE_SELECT:
		if (pressed)
			emu_select_next_system_type(emu);
		break;
	case ACTION_SAVE_SCREENSHOT:
		if (pressed)
			save_screenshot();
		break;
	case ACTION_STATE_LOAD_0:
	case ACTION_STATE_LOAD_1:
	case ACTION_STATE_LOAD_2:
	case ACTION_STATE_LOAD_3:
	case ACTION_STATE_LOAD_4:
	case ACTION_STATE_LOAD_5:
	case ACTION_STATE_LOAD_6:
	case ACTION_STATE_LOAD_7:
	case ACTION_STATE_LOAD_8:
	case ACTION_STATE_LOAD_9:
	case ACTION_STATE_LOAD_NEWEST:
		if (pressed) {
			state_index = button - ACTION_STATE_LOAD_0;
			emu_quick_load_state(emu, state_index, 1);
		}
		break;
	case ACTION_STATE_SAVE_0:
	case ACTION_STATE_SAVE_1:
	case ACTION_STATE_SAVE_2:
	case ACTION_STATE_SAVE_3:
	case ACTION_STATE_SAVE_4:
	case ACTION_STATE_SAVE_5:
	case ACTION_STATE_SAVE_6:
	case ACTION_STATE_SAVE_7:
	case ACTION_STATE_SAVE_8:
	case ACTION_STATE_SAVE_9:
	case ACTION_STATE_SAVE_OLDEST:
		if (pressed) {
			state_index = button - ACTION_STATE_SAVE_0;
			emu_quick_save_state(emu, state_index, 1);
		}
		break;
	case ACTION_STATE_SELECT_0:
	case ACTION_STATE_SELECT_1:
	case ACTION_STATE_SELECT_2:
	case ACTION_STATE_SELECT_3:
	case ACTION_STATE_SELECT_4:
	case ACTION_STATE_SELECT_5:
	case ACTION_STATE_SELECT_6:
	case ACTION_STATE_SELECT_7:
	case ACTION_STATE_SELECT_8:
	case ACTION_STATE_SELECT_9:
		if (pressed) {
			state_index = button - ACTION_STATE_SELECT_0;
			emu_set_quick_save_slot(emu, state_index, 1);
		}
		break;
	case ACTION_STATE_SAVE_SELECTED:
		if (pressed)
			emu_quick_save_state(emu, -1, 1);
		break;
	case ACTION_STATE_LOAD_SELECTED:
		if (pressed)
			emu_quick_load_state(emu, -1, 1);
		break;
	case ACTION_STATE_SELECT_PREV:
		if (pressed) {
			state_index = emu_get_quick_save_slot(emu);
			state_index--;
			if (state_index < 0)
				state_index = 9;
			emu_set_quick_save_slot(emu, state_index, 1);
		}
		break;
	case ACTION_STATE_SELECT_NEXT:
		if (pressed) {
			state_index = emu_get_quick_save_slot(emu);
			state_index++;
			if (state_index > 9)
				state_index = 0;
			emu_set_quick_save_slot(emu, state_index, 1);
		}
		break;
	case ACTION_TOGGLE_CPU_TRACE:
		if (pressed)
			cpu_set_trace(emu->cpu, -1);
		break;
	case ACTION_SWITCH_FOUR_PLAYER_MODE:
		if (pressed)
			io_set_four_player_mode(emu->io, -1, 1);
		break;
	case ACTION_TOGGLE_MENUBAR:
#if GUI_ENABLED
		if (pressed)
			gui_toggle_menubar();
#endif
		break;
	case ACTION_TOGGLE_SPRITE_LIMIT:
		if (pressed)
			emu_toggle_sprite_limit(emu);
		break;
	case ACTION_TOGGLE_SPRITES:
		if (pressed)
			emu_toggle_sprites(emu);
		break;
	case ACTION_TOGGLE_BG:
		if (pressed)
			emu_toggle_bg(emu);
		break;
	case ACTION_TOGGLE_MOUSE_GRAB:
		if (pressed)
			video_set_mouse_grab(-1);
		break;
	case ACTION_TOGGLE_FULLSCREEN:
		if (pressed) {
			video_toggle_fullscreen(-1);
		}
		break;
	case ACTION_TOGGLE_FPS:
		if (pressed)
			video_toggle_fps(-1);
		break;
	case ACTION_TOGGLE_SCANLINE_RENDERER:
		if (pressed)
			emu_toggle_scanline_renderer(emu);
		break;
	case ACTION_HARD_RESET:
		if (pressed)
			emu_reset(emu, 1);
		break;
	case ACTION_SOFT_RESET:
		if (pressed)
			emu_reset(emu, 0);
		break;
	case ACTION_PAUSE:
		if (pressed)
			emu_pause(emu, !emu_paused(emu));
		break;
	case ACTION_QUIT:
		running = 0;
		break;
	case ACTION_DEVICE_SELECT_PORT1:
		if (pressed)
			io_ui_device_select(emu->io, PORT_1);
		break;
	case ACTION_DEVICE_SELECT_PORT2:
		if (pressed)
			io_ui_device_select(emu->io, PORT_2);
		break;
	case ACTION_DEVICE_SELECT_PORT3:
		if (pressed)
			io_ui_device_select(emu->io, PORT_3);
		break;
	case ACTION_DEVICE_SELECT_PORT4:
		if (pressed)
			io_ui_device_select(emu->io, PORT_4);
		break;
	case ACTION_DEVICE_SELECT_EXP:
		if (pressed)
			io_ui_device_select(emu->io, PORT_EXP);
		break;
	case ACTION_DEVICE_CONNECT_PORT1:
		if (pressed)
			io_ui_device_connect(emu->io, PORT_1);
		break;
	case ACTION_DEVICE_CONNECT_PORT2:
		if (pressed)
			io_ui_device_connect(emu->io, PORT_2);
		break;
	case ACTION_DEVICE_CONNECT_PORT3:
		if (pressed)
			io_ui_device_connect(emu->io, PORT_3);
		break;
	case ACTION_DEVICE_CONNECT_PORT4:
		if (pressed)
			io_ui_device_connect(emu->io, PORT_4);
		break;
	case ACTION_DEVICE_CONNECT_EXP:
		if (pressed)
			io_ui_device_connect(emu->io, PORT_EXP);
		break;
	case ACTION_TOGGLE_CHEATS:
		if (pressed)
			emu_toggle_cheats(emu);
		break;
	}

	return 0;
}

int input_disconnect_handlers(const struct input_event_handler *handlers)
{
	struct emu_action *e;
	int i;

	if (!handlers)
		return 0;

	for (i = 0; handlers[i].id != ACTION_NONE; i++) {
		uint32_t emu_action = handlers[i].id;
		e = input_lookup_emu_action(emu_action);
		if (!e)
			continue;

		e->handler = NULL;
		e->data = NULL;
	}
	return 0;
}

static struct input_event_handler misc_handlers[] = {
	{ ACTION_ALT_SPEED, misc_buttons },
	{ ACTION_STATE_LOAD_NEWEST, misc_buttons },
	{ ACTION_STATE_LOAD_0, misc_buttons },
	{ ACTION_STATE_LOAD_1, misc_buttons },
	{ ACTION_STATE_LOAD_2, misc_buttons },
	{ ACTION_STATE_LOAD_3, misc_buttons },
	{ ACTION_STATE_LOAD_4, misc_buttons },
	{ ACTION_STATE_LOAD_5, misc_buttons },
	{ ACTION_STATE_LOAD_6, misc_buttons },
	{ ACTION_STATE_LOAD_7, misc_buttons },
	{ ACTION_STATE_LOAD_8, misc_buttons },
	{ ACTION_STATE_LOAD_9, misc_buttons },
	{ ACTION_STATE_LOAD_SELECTED, misc_buttons },
	{ ACTION_STATE_SAVE_OLDEST, misc_buttons },
	{ ACTION_STATE_SAVE_0, misc_buttons },
	{ ACTION_STATE_SAVE_1, misc_buttons },
	{ ACTION_STATE_SAVE_2, misc_buttons },
	{ ACTION_STATE_SAVE_3, misc_buttons },
	{ ACTION_STATE_SAVE_4, misc_buttons },
	{ ACTION_STATE_SAVE_5, misc_buttons },
	{ ACTION_STATE_SAVE_6, misc_buttons },
	{ ACTION_STATE_SAVE_7, misc_buttons },
	{ ACTION_STATE_SAVE_8, misc_buttons },
	{ ACTION_STATE_SAVE_9, misc_buttons },
	{ ACTION_STATE_SAVE_SELECTED, misc_buttons },
	{ ACTION_STATE_SELECT_0, misc_buttons },
	{ ACTION_STATE_SELECT_1, misc_buttons },
	{ ACTION_STATE_SELECT_2, misc_buttons },
	{ ACTION_STATE_SELECT_3, misc_buttons },
	{ ACTION_STATE_SELECT_4, misc_buttons },
	{ ACTION_STATE_SELECT_5, misc_buttons },
	{ ACTION_STATE_SELECT_6, misc_buttons },
	{ ACTION_STATE_SELECT_7, misc_buttons },
	{ ACTION_STATE_SELECT_8, misc_buttons },
	{ ACTION_STATE_SELECT_9, misc_buttons },
	{ ACTION_STATE_SELECT_PREV, misc_buttons },
	{ ACTION_STATE_SELECT_NEXT, misc_buttons },
	{ ACTION_TOGGLE_CPU_TRACE, misc_buttons },
	{ ACTION_SWITCH_FOUR_PLAYER_MODE,  misc_buttons },
	{ ACTION_TOGGLE_SCANLINE_RENDERER,  misc_buttons },
	{ ACTION_SAVE_SCREENSHOT,  misc_buttons },
	{ ACTION_TOGGLE_FULLSCREEN,  misc_buttons },
	{ ACTION_TOGGLE_FPS,  misc_buttons },
	{ ACTION_TOGGLE_MENUBAR,  misc_buttons },
	{ ACTION_TOGGLE_SPRITE_LIMIT,  misc_buttons },
	{ ACTION_TOGGLE_SPRITES,  misc_buttons },
	{ ACTION_TOGGLE_BG,  misc_buttons },
	{ ACTION_TOGGLE_MOUSE_GRAB, misc_buttons },
	{ ACTION_HARD_RESET, misc_buttons },
	{ ACTION_SOFT_RESET, misc_buttons },
	{ ACTION_QUIT, misc_buttons },
	{ ACTION_TOGGLE_CHEATS, misc_buttons },
	{ ACTION_DEVICE_SELECT_PORT1, misc_buttons },
	{ ACTION_DEVICE_CONNECT_PORT1, misc_buttons },
	{ ACTION_DEVICE_SELECT_PORT2, misc_buttons },
	{ ACTION_DEVICE_CONNECT_PORT2, misc_buttons },
	{ ACTION_DEVICE_SELECT_PORT3, misc_buttons },
	{ ACTION_DEVICE_CONNECT_PORT3, misc_buttons },
	{ ACTION_DEVICE_SELECT_PORT4, misc_buttons },
	{ ACTION_DEVICE_CONNECT_PORT4, misc_buttons },
	{ ACTION_DEVICE_SELECT_EXP, misc_buttons },
	{ ACTION_DEVICE_CONNECT_EXP, misc_buttons },
	{ ACTION_PAUSE, misc_buttons },
	{ ACTION_SYSTEM_TYPE_SELECT, misc_buttons },
	{ ACTION_NONE },
};

struct input_event_node *input_insert_event(union input_new_event *event,
				       int mod,
				       struct emu_action *emu_action)
{
	struct input_event_node **e;
	struct input_event_node *native_event;
	uint32_t id;
	uint32_t type;
	uint32_t device;
	uint32_t index;
	uint32_t misc;
	int i;
	int is_button;

	type = event->common.type;
	device = event->common.device;
	index = event->common.index;
	misc = event->common.misc;

	id = type << 16;
	id |= device << 8;
	id |= index & 0xff;
	id = index & 7;

	switch (type) {
	case INPUT_EVENT_TYPE_KEYBOARD:
	case INPUT_EVENT_TYPE_MOUSEBUTTON:
	case INPUT_EVENT_TYPE_JOYBUTTON:
	case INPUT_EVENT_TYPE_JOYHAT:
	case INPUT_EVENT_TYPE_JOYAXISBUTTON:
		is_button = 1;
		if (emu_action && (emu_action->id & ACTION_TYPE_MASK) !=
		    ACTION_TYPE_DIGITAL) {
			return NULL;
		}
		break;
	case INPUT_EVENT_TYPE_JOYAXIS:
		is_button = 0;
		if (emu_action && (emu_action->id & ACTION_TYPE_MASK) !=
		    ACTION_TYPE_ANALOG) {
			return NULL;
		}
		break;
	case INPUT_EVENT_TYPE_MOUSEMOTION:
		is_button = 0;
		if (emu_action && (emu_action->id & ACTION_TYPE_MASK) !=
		    ACTION_TYPE_MOUSE) {
			return NULL;
		}
		break;
	default:
		is_button = 0;
	}
	
	e = &event_hash[id % EVENT_HASH_SIZE];

	while (*e) {
		if (((*e)->event.common.type == type) &&
		    ((*e)->event.common.device == device) &&
		    ((*e)->event.common.misc == misc) &&
		    ((*e)->event.common.index == index))
			break;

		e = &((*e)->next);
	}
	native_event = *e;

	if (!native_event) {
		native_event = malloc(sizeof(*native_event));
		if (!native_event)
			return NULL;

		native_event->event.common.type = type;
		native_event->event.common.device = device;
		native_event->event.common.index = index;
		native_event->event.common.misc = misc;
		native_event->mapping_count = 0;
		native_event->mapping_max = 0;
		native_event->modifier = -1;
		native_event->mappings = NULL;
		native_event->pressed = 0;
		native_event->next = NULL;

		*e = native_event;
	}

	if (!emu_action)
		return native_event;

	if (is_button && !EMU_ACTION_IS_BUTTON(emu_action)) {
		/* FIXME should have some sort of message here. */
		return native_event;
	}

	for (i = 0; i < native_event->mapping_count; i++) {
		if ((native_event->mappings[i].mod_bits == mod) &&
		    (native_event->mappings[i].emu_action == emu_action)) {
			return native_event;
		}
	}

	if (i == native_event->mapping_max) {
		struct input_event_mapping *tmp;
		size_t size;

		size = (i + 1) * sizeof(*tmp);
		tmp = realloc(native_event->mappings, size);

		if (!tmp)
			return NULL;

		native_event->mappings = tmp;
		native_event->mapping_max++;
	}

	native_event->mappings[i].mod_bits = mod;
	native_event->mappings[i].emu_action = emu_action;
	native_event->mapping_count++;

	return native_event;
}

int input_add_modifier(union input_new_event *event, int mod)
{
	struct input_event_node *native_event;

	if (mod >= INPUT_MOD_COUNT)
		return -1;

	native_event = input_lookup_event(event);
	if (!native_event)
		native_event = input_insert_event(event, 0, NULL);

	if (!native_event)
		return -1;

	native_event->modifier = mod;

	return 0;
}

static int get_effective_modifiers(struct input_event_node *event, int mod)
{
	int mod_tries[3] = { mod, mod & ~INPUT_MOD_BITS_KBD, 0 };
	int i;

	for (i = 0; i < 3; i++) {
		int j;

		for (j = 0; j < event->mapping_count; j++) {
			if (event->mappings[j].mod_bits == mod_tries[i]) {
				break;
			}
		}

		/* Stop processing on the first pass if this is a key
		   event and keyboard lock is enabled.
		*/
		if ((event->event.common.type == INPUT_EVENT_TYPE_KEYBOARD) &&
		    (mod & INPUT_MOD_BITS_KBD) && (i == 0)) {
			break;
		}

		if ((j < event->mapping_count) || (mod_tries[i] == 0))
			break;
	}

	return mod_tries[i];
}

static struct input_event_node *input_lookup_event(union input_new_event *event)
{
	struct input_event_node *native_event;
	uint32_t id;
	uint32_t type;
	uint32_t device;
	uint32_t index;
	uint32_t misc;
	
	type = event->common.type;

	device = event->common.device;
	index = event->common.index;
	misc = event->common.misc;

	id = (index & 7) % EVENT_HASH_SIZE;

	for (native_event = event_hash[id]; native_event;
	     native_event = native_event->next) {
		if ((native_event->event.common.type == type) &&
		    (native_event->event.common.device == device) &&
		    (native_event->event.common.index == index) &&
		    (native_event->event.common.misc == misc)) {
			break;
		}
	}

	if (!native_event) {
		return NULL;
	}

	return native_event;
}

int input_validate_modifier(const char *modifier)
{
	const char *ptr;
	union input_new_event event;
	char buf[80];

	memset(&event, 0, sizeof(event));
	sanitize_binding(buf, modifier, sizeof(buf));
	ptr = buf + 9;

	if (!parse_keyboard_binding(ptr, &event) &&
	    !parse_joystick_binding(ptr, &event) &&
	    !parse_mouse_binding(ptr, &event)) {
		return 0;
	}

	return 1;
}

static void configure_modifier(const char *key, const char *value)
{
	union input_new_event event;
	char buf[80];
	int mod;
	int i;

	mod = -1;

	memset(&event, 0, sizeof(event));
	sanitize_binding(buf, key, sizeof(buf));

	if (!parse_keyboard_binding(buf, &event) &&
	    !parse_joystick_binding(buf, &event) &&
	    !parse_mouse_binding(buf, &event)) {
		log_err("invalid event %s\n", key);
		return;
	}

	for (i = 0; i < INPUT_MOD_COUNT; i++) {
		if (strcasecmp(value, modifier_names[i]) == 0) {
			mod = i;
			break;
		}
	}

	if ((mod == INPUT_MOD_COUNT) && (strcasecmp(value, "none") != 0)) {
		log_err("invalid modifier %s\n", value);
		return;
	}

	if (input_add_modifier(&event, mod) < 0) {
		log_err("error adding modifier\n");
	} /* else if (mod >= 0) { */
	/* 	printf("assigning modifier %s (%x) to %s\n", value, mod, key); */
	/* } else { */
	/* 	printf("clearing modifier %d from %s\n", mod, key); */
	/* } */
}

void input_configure_modifier(const char *name, const char *value)
{
	int i;

	for (i = 0; i < KEYBOARD_MODIFIER_COUNT; i++) {
		if (!strcasecmp(name, keyboard_modifiers[i])) {
			log_err("Cannot change modifier for keyboard "
				"modifier key %s\n", name);

			return;
		}
	}

	configure_modifier(name, value);
}

void input_configure_keyboard_modifiers(void)
{
	configure_modifier("Keyboard Left Ctrl", "ctrl");
	configure_modifier("Keyboard Right Ctrl", "ctrl");
	configure_modifier("Keyboard Left Alt", "alt");
	configure_modifier("Keyboard Right Alt", "alt");
	configure_modifier("Keyboard Left Shift", "shift");
	configure_modifier("Keyboard Right Shift", "shift");
	configure_modifier("Keyboard Left GUI", "gui");
	configure_modifier("Keyboard Right GUI", "gui");
}

int input_init(struct emu *emu)
{
	int i;

	for (i = 0; i < EVENT_HASH_SIZE; i++)
		event_hash[i] = NULL;

	for (i = 0; emu_action_id_map[i].emu_action_id != ACTION_NONE; i++) {
		input_insert_emu_action(emu_action_id_map[i].emu_action_id);
	}

	input_connect_handlers(misc_handlers, emu);

	memset(modifier_count, 0, sizeof(modifier_count));
	mod_bits = 0;
	ignore_events = 0;
	event_queue = NULL;
	event_queue_size = 0;
	event_queue_count = 0;
	return input_native_init(emu);
}

static void input_clear_bindings(void)
{
	struct input_event_node *e, *tmp;
	int i;

	for (i = 0; i < EVENT_HASH_SIZE; i++) {
  		e = event_hash[i];
		while (e) {
			tmp = e->next;
			if (e->mappings)
				free(e->mappings);
			free(e);
			e = tmp;
		}
		event_hash[i] = NULL;
	}
}

void input_reset_bindings(void)
{
	input_clear_bindings();
}

int input_shutdown(void)
{
	struct emu_action *ee, *etmp;

	input_native_shutdown();

	input_clear_bindings();
	
	ee = emu_action_list;
	while (ee) {
		etmp = ee->next;
		free(ee);
		ee = etmp;
	}

	if (event_queue)
		free(event_queue);

	return 0;
}

int input_validate_binding_name(const char *name)
{
	union input_new_event event;

	return !input_parse_binding(name, &event, NULL);
}

int get_binding_name(char *buffer, int size, struct input_event_node *e)
{
	const char *keyname;
	int count;
	int index;
	int direction;
	const char *direction_string;

	switch (e->event.type) {
	case INPUT_EVENT_TYPE_KEYBOARD:
		keyname =
			input_lookup_keyname_from_code(e->event.key.key);
	        if (!keyname)
			return -1;

		snprintf(buffer, size, "Keyboard %s", keyname);
		break;
	case INPUT_EVENT_TYPE_MOUSEMOTION:
		snprintf(buffer, size, "Mouse");
		break;
	case INPUT_EVENT_TYPE_MOUSEBUTTON:
		snprintf(buffer, size, "Mouse Button %d",
			e->event.mbutton.button);
		break;
	case INPUT_EVENT_TYPE_JOYBUTTON:
		index = e->event.jbutton.button;
		if (index >= 0x100) {
			index -= 0x100;
			count = sizeof(controller_buttons) /
				sizeof(controller_buttons[0]);

			if (index >= count)
				return -1;

			snprintf(buffer, size, "Joystick %d %s",
				 e->event.jbutton.device,
				 controller_buttons[index]);
		} else {
			snprintf(buffer, size, "Joystick %d Button %d",
				  e->event.jbutton.device, index);
		}

		break;
	case INPUT_EVENT_TYPE_JOYAXIS:
		index = e->event.jaxis.axis;
		if (index >= 0x100) {
			index -= 0x100;
			count = sizeof(controller_axes) / sizeof(controller_axes[0]);

			if (index >= count)
				return -1;

			snprintf(buffer, size, "Joystick %d %s",
				 e->event.jaxis.device,
				 controller_axes[index]);
					
		} else {
			snprintf(buffer, size, "Joystick %d Axis %d",
				  e->event.jaxis.device, index);
		}

		break;
	case INPUT_EVENT_TYPE_JOYAXISBUTTON:
		index = e->event.jaxisbutton.axis;
		direction = e->event.jaxisbutton.direction;
		if (index >= 0x100) {
			index -= 0x100;
			count = sizeof(controller_axes) /
				sizeof(controller_axes[0]);

			if (index >= count)
				return -1;

			snprintf(buffer, size, "Joystick %d %s %c",
				 e->event.jaxis.device, controller_axes[index],
				 (direction < 0) ? '-' : '+');
					
		} else {
			snprintf(buffer, size, "Joystick %d Axis %d %c",
				 e->event.jaxis.device, index,
				 (direction < 0) ? '-' : '+');
		}

		break;
	case INPUT_EVENT_TYPE_JOYHAT:
		index = e->event.jhat.hat;
		direction = e->event.jhat.value;

		switch (direction) {
		case INPUT_JOYHAT_LEFT:
			direction_string = "Left";
			break;
		case INPUT_JOYHAT_RIGHT:
			direction_string = "Right";
			break;
		case INPUT_JOYHAT_UP:
			direction_string = "Up";
			break;
		case INPUT_JOYHAT_DOWN:
			direction_string = "Down";
			break;
		default:
			return -1;
		}

		snprintf(buffer, size, "Joystick %d Hat %d %s",
			 e->event.jaxis.device, index, direction_string);
		break;
	case INPUT_EVENT_TYPE_NONE:
		buffer[0] = '\0';
		break;
	}

	return 0;
}

void get_modifier_string(char *buffer, int size, int modbits)
{
	int m;
	int remaining;
	char *ptr;
	int len;

	buffer[0] = 0;

	if (!modbits)
		return;

	/* Need room for "] " */
	remaining = size - 2;
	ptr = buffer;
	for (m = 0; m < 8; m++) {
		if (!(modbits & (1 << m)))
			continue;

		snprintf(ptr, remaining, "-%s", modifier_names[m]);
		len = strlen(ptr);
		ptr += len;
		remaining -= len;
	}


	len = strlen(buffer);
	buffer[0] = '[';
	buffer[len] = ']';
	buffer[len + 1] = ' ';
	buffer[len + 2] = '\0';
}

static int append_data(char **buffer, int *used, int *size, const char *data, int len)
{
	int remaining;

	remaining = *size - *used;

	if (len > remaining) {
		char *tmp;
		int new_size = *used + len;

		tmp = realloc(*buffer, new_size);
		if (!tmp)
			return -1;

		*buffer = tmp;
		*size = new_size;
	}

	memcpy(*buffer + *used, data, len);
	*used += len;

	return 0;
}

int binding_compare(const void *a, const void *b)
{
	return strcmp( *(char * const *) a, * (char * const *) b);
}

//void input_describe_bindings(void (*callback)(const char *, const char *, int))
void input_get_binding_config(char **config_data, size_t *config_data_size)
{
	struct input_event_node *e;
	char buffer[80];
	char modbuffer[80];
	int i, j;
	int mod;

	char *data;
	int data_size;
	int used;
	int string_count;
	char **strings;
	int namecount;

	string_count = 0;

	for (i = 0; i < EVENT_HASH_SIZE; i++) {
		e = event_hash[i];
		while (e) {
			string_count += e->mapping_count;
			e = e->next;
		}
	}

	strings = malloc(sizeof(char *) * string_count);
	if (!strings)
		return;

	string_count = 0;

	data = NULL;
	data_size = 0;
	used = 0;

	for (i = 0; i < EVENT_HASH_SIZE; i++) {
		for (e = event_hash[i]; e; e = e->next) {
			if (get_binding_name(buffer, sizeof(buffer), e) < 0)
				continue;

			if ((e->modifier >= 0) && (e->modifier <= INPUT_MOD_KBD)) {
				intptr_t offset;

				offset = used;
				strings[string_count] = (char *)offset;
				string_count++;
				append_data(&data, &used, &data_size,
					    "modifier ", 9);
				append_data(&data, &used, &data_size,
					    buffer, strlen(buffer));
				append_data(&data, &used, &data_size,
					    " = ", 3);
				append_data(&data, &used, &data_size,
					    modifier_names[e->modifier],
					    strlen(modifier_names[e->modifier]));
#if _WIN32
				append_data(&data, &used, &data_size,
					    "\r\n", 3);
#else
				append_data(&data, &used, &data_size,
					    "\n", 2);
#endif				
			}

			for (mod = 0; mod < 256; mod++) {
				modbuffer[0] = '\0';

				if (!e->mapping_count)
					continue;

				get_modifier_string(modbuffer,
						    sizeof(modbuffer), mod);

				namecount = 0;
				for (j = 0; j < e->mapping_count; j++) {
					int event_count;
					int k;
					uint32_t id;
					const char *name;
					int len;

					if (e->mappings[j].mod_bits != mod)
						continue;

					id = e->mappings[j].emu_action->id;
					event_count = sizeof(emu_action_id_map) /
						sizeof(emu_action_id_map[0]);
					name = NULL;

					for (k = 0; k < event_count; k++) {
						if (emu_action_id_map[k].emu_action_id ==
						    id) {
							name = emu_action_id_map[k].name;
							break;
						}
					}

					if (!name)
						continue;

					len = strlen(name);
					if (namecount) {
						len++;
						append_data(&data, &used, &data_size, ", ", 2);
					} else {
						intptr_t offset;

						offset = used;
						append_data(&data, &used, &data_size, "binding ", 8); 
						append_data(&data, &used, &data_size, modbuffer,
							    strlen(modbuffer));
						append_data(&data, &used, &data_size, buffer,
						            strlen(buffer));
						append_data(&data, &used, &data_size, " = ", 3);

						strings[string_count] = (char *)offset;
						string_count++;
					}

					
					append_data(&data, &used, &data_size, name, strlen(name));
					namecount++;

				}

				if (namecount) {
#if _WIN32
					append_data(&data, &used, &data_size, "\r\n", 3);
#else
					append_data(&data, &used, &data_size, "\n", 2);
#endif
				}
			}
		}
	}

	if (string_count) {
		int i;
		size_t binding_config_size;
		char *config_buffer, *ptr;
		size_t config_buffer_size, remaining;

		binding_config_size = 0;
		for (i = 0; i < string_count; i++)  {
			strings[i] += (intptr_t)data - 0;
			binding_config_size += strlen(strings[i]);
		}

		config_buffer = *config_data;
		config_buffer_size = *config_data_size;

		config_buffer_size += binding_config_size;

		ptr = realloc(config_buffer, config_buffer_size);
		if (!ptr) {
			free(strings);
			free(data);
			return;
		}

		config_buffer = ptr;

		qsort(strings, string_count, sizeof(strings[0]), binding_compare);

		ptr = config_buffer + *config_data_size;
		remaining = config_buffer_size - *config_data_size;
		for (i = 0; i < string_count; i++) {
			size_t len;

			strncpy(ptr, strings[i], remaining);
			len = strlen(ptr);
			remaining -= len;
			ptr += len;
		}

		*config_data = config_buffer;
		*config_data_size = config_buffer_size;

		free(strings);
		free(data);
	}
}

const char *get_category_name(int action_category)
{
	if ((action_category >= ACTION_CATEGORY_COUNT) || (action_category < 0))
		return NULL;

	return category_names[action_category];
}

void get_event_name_and_category(uint32_t event_id, char **name, char **category)
{
	int i, event_count;

	event_count = sizeof(emu_action_id_map)/sizeof(emu_action_id_map[0]);

	for (i = 0; i < event_count; i++) {
		if (event_id == emu_action_id_map[i].emu_action_id)
			break;
			
	}

	if (i == event_count)
		return;

	if (category)
		*category = (char *)category_names[emu_action_id_map[i].category];

	if (name)
		*name = (char *)emu_action_id_map[i].description;
}

int input_bindings_loaded(void)
{
	int i;

	for (i = 0; i < EVENT_HASH_SIZE; i++) {
		if (event_hash[i])
			return 1;
	}

	return 0;
}

int input_queue_event(union input_new_event *event)
{
	int count;

	count = event_queue_count + 1;

	if (count > event_queue_size) {
		union input_new_event *tmp = realloc(event_queue, 
						     count * sizeof(*event));
		if (!tmp)
			return -1;

		event_queue = tmp;
		event_queue_size++;
	}

	memcpy(&event_queue[event_queue_count], event,
	       sizeof(*event));
	event_queue[event_queue_count].common.processed = 0;
	event_queue_count++;

	return 0;
}

int input_process_queue(int force)
{
	int i;
	union input_new_event *event;

	for (i = 0; i < event_queue_count; i++) {
		event = &event_queue[i];
		if (!event->common.processed) {
			input_handle_event(event, force);
		}
	}

	if (force) {
		event_queue_count = 0;
	}

	return 0;
}
