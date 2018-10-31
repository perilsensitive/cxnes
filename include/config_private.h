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

#ifndef __CONFIG_PRIVATE_H__
#define __CONFIG_PRIVATE_H__

enum {
	CONFIG_TYPE_INTEGER,
	CONFIG_TYPE_UNSIGNED_INTEGER,
	CONFIG_TYPE_BOOLEAN,
	CONFIG_TYPE_BOOLEAN_NOSAVE,
	CONFIG_TYPE_FLOAT,
	CONFIG_TYPE_STRING,
	CONFIG_NONE,
};

struct config_binding {
	char name[32];
	int mod;
	uint32_t binding;
};

union config_default {
	const int integer;
	const unsigned int unsigned_int;
	const int bool;
	const double fpoint;
	char *string;
};

union config_limit {
	const int svalue;
	const unsigned int uvalue;
	const double fvalue;
};

union config_valid_list {
	const int *integer_list;
	const unsigned int *unsigned_int_list;
	const double *float_list;
	const char **string_list;
};

struct config_parameter {
	const char *name;
	size_t offset;
	int type; /* FIXME enum (string, int, double, bool) */
	union config_limit min;
	union config_limit max;
	union config_valid_list valid_values;
	union config_default default_value;
	const char **valid_value_names;
	int valid_value_count;
	int (*validator)(struct config_parameter *parameter, void *value);
};

extern struct config_parameter config_parameters[];
extern struct config_parameter rom_config_parameters[];

int config_check_changed(struct config *config,
			 struct config_parameter *parameters);

void config_set(struct config *config,
	        struct config_parameter *parameters,
	        const char *name, const char *value);

#endif				/* __CONFIG_PRIVATE_H__ */
