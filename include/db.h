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

#ifndef __DB_H__
#define __DB_H__

#include "emu.h"

struct rom_info *db_lookup(struct rom *rom, struct rom_info *start);
struct rom_info *db_lookup_split_rom(struct archive_file_list *list, int *chip_list,
				     struct rom_info *start);

void print_rom(struct rom *rom);
int db_load_file(struct emu *emu, const char *filename);
void db_cleanup(void);
void free_rom(struct rom *rom);
int db_rom_load(struct emu *emu, struct rom *rom);
int validate_checksums(struct rom *rom, struct rom_info *info);

#endif /* __DB_H__ */
