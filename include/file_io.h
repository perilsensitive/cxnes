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

#ifndef __FILE_IO_H__
#define __FILE_IO_H__

#include <stdint.h>
#if __unix__
#include <sys/types.h>
#endif

int check_file_exists(const char *name);
int readfile(const char *name, uint8_t * buf, uint32_t size);
int writefile(const char *name, uint8_t * buf, uint32_t size);
ssize_t get_file_size(const char *name);
int process_file(const char *filename, void *data,
                 void (*callback)(char *, int, void *), int, int);
int get_file_mtime(const char *path, int64_t *secptr, int32_t *nsecptr);

char *get_user_data_path(void);
char *get_base_path(void);

int create_directory(const char *path, int recursive, int is_file);

#endif				/* __FILE_IO_H__ */
