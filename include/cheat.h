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

#ifndef __CHEAT_H__
#define __CHEAT_H__

#include "emu.h"

#define CHEAT_FLAG_ENABLED 1

struct cheat {
	int address;
	int value;
	int compare;
	char *description;
	uint32_t crc;
	int flags;
	int enabled;
	cpu_read_handler_t *orig_read_handler;
	struct cheat *prev;
	struct cheat *next;
};

struct cheat_state {
	struct emu *emu;
	struct cheat *cheat_list;
	int enabled;
};

struct cheat_state *cheat_init(struct emu *emu);
void insert_cheat(struct cheat_state *cheats, struct cheat *cheat);
void cheat_deinit(struct cheat_state *cheats);
void cheat_cleanup(struct cheat_state *cheats);
void cheat_reset(struct emu *emu, int hard);
int cheat_apply_config(struct cheat_state *cheats);
int add_cheat(struct cheat_state *cheats, char *code);
void remove_cheat(struct cheat_state *cheats, struct cheat *cheat);
int cheat_enable_cheats(struct cheat_state *cheats, int enabled);
void raw_to_game_genie(int addr, int value, int compare, char *buffer);
int enable_cheat(struct cheat_state *cheats, struct cheat *cheat);
void disable_cheat(struct cheat_state *cheats, struct cheat *cheat);
int cheat_load_file(struct emu *emu, const char *filename);
int cheat_save_file(struct emu *emu, const char *filename);
extern int parse_raw_code(const char *code, int *a, int *v, int *c);
extern int parse_game_genie_code(const char *code, int *a, int *v, int *c);
extern int parse_pro_action_rocky_code(const char *code, int *a, int *v, int *c);
extern void raw_to_pro_action_rocky(int addr, int value, int compare, char *buffer);

#endif				/* __CHEAT_H__ */
