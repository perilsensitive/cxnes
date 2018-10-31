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

#include <stddef.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#if defined _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <direct.h>
#endif

#include "emu.h"
#include "file_io.h"
#include "actions.h"
#include "input.h"

#include "config_private.h"

void config_load_default_bindings(void)
{
	int i;

	if (!input_bindings_loaded()) {
		for (i = 0; default_bindings[i].name; i++) {
			input_bind(default_bindings[i].name,
				   default_bindings[i].value);
		}
	}
}

void config_callback(char *line, int num, void *data)
{
	struct config *config;
	struct config_parameter *parameters;
	char *sep, *value;
	void **ptr_list;

	ptr_list = data;
	config = ptr_list[0];
	parameters = ptr_list[1];

	sep = strchr(line, '=');

	if (!sep || sep == line) {
		log_warn("config_load_file: line %d: "
			 "invalid key/value pair\n", num);
		return;
	}

	value = sep + 1;

	while (*value && isspace(*value))
		value++;

	/* Remove trailing whitespace from the key name */
	sep--;
	while (isspace(*sep))
		sep--;
	*(sep + 1) = '\0';

	config_set(config, parameters, line, value);
}

int config_load_file(struct config *config, struct config_parameter *parameters,
			    const char *filename)
{
	int rc;
	void *ptr_list[2] = { config, parameters };

	rc = process_file(filename, ptr_list, config_callback, 1, 1, 1);

	return rc;
}

int config_save_file(struct config *config,
			    struct config_parameter *parameters,
			    int rom_specific,
			    const char *filename)
{
	int i;
	struct config_parameter *param;
	char *ptr;
	char value_buffer[32];
	char *value;
	char *ending;
	size_t size;
	char *buffer, *tmp;
	int rc;
	int len;

	size = 0;
	buffer = NULL;

	for (i = 0; parameters[i].name; i++) {
		param = &parameters[i];
		ptr = ((char *)config) + param->offset;

		value = value_buffer;
		switch (param->type) {
		case CONFIG_TYPE_INTEGER:
			snprintf(value_buffer, sizeof(value_buffer),
				 "%i", *((int *)ptr));
			break;
		case CONFIG_TYPE_UNSIGNED_INTEGER:
			snprintf(value_buffer, sizeof(value_buffer),
				 "0x%x", *((unsigned int *)ptr));
			break;
		case CONFIG_TYPE_BOOLEAN:
			snprintf(value_buffer, sizeof(value_buffer),
				 "%s", *((int *)ptr) ? "true" : "false");
			break;
		case CONFIG_TYPE_FLOAT:
			snprintf(value_buffer, sizeof(value_buffer),
				 "%f", *((double *)ptr));
			break;
		case CONFIG_TYPE_STRING:
			value = *((char **)ptr);
			break;
		case CONFIG_TYPE_BOOLEAN_NOSAVE:
			continue;
			break;
		}

		len = strlen(param->name);
		len += 1; /* For '=' */

		if (value)
			len += strlen(value);
		/* Add space for line ending chars */
#if _WIN32
		len += 2;
		ending = "\r\n";
#else
		len += 1;
		ending = "\n";
#endif

		tmp = realloc(buffer, size + len + 1);
		if (!tmp) {
			free(buffer);
			return -1;
		}

		buffer = tmp;
		tmp = buffer + size;

		snprintf(tmp, len + 1, "%s=%s%s", param->name,
			 (value ? value : ""), ending);
		size += len;
	}

	if (!rom_specific)
		input_get_binding_config(&buffer, &size);

	rc = writefile(filename, (uint8_t *)buffer, size);
	free(buffer);

	return rc;
}

int config_save_main_config(struct config *config)
{
	char *buffer;
	int rc;

	buffer = config_get_path(config, CONFIG_DATA_FILE_MAIN_CFG,
				     NULL, 1);

	if (!buffer)
		return 1;

	rc = config_save_file(config, config_parameters, 0, buffer);
	free(buffer);

	return rc;
}

int config_save_rom_config(struct config *config)
{
	int rc;

	char *path;

	path = emu_generate_rom_config_path(config->emu, 1);
	if (!path)
		return 0;

	if (!config_check_changed(config, rom_config_parameters) &&
	    !check_file_exists(path)) {
		return 0;
	}

	rc = config_save_file(config, rom_config_parameters, 1, path);

	free(path);

	return rc;
}

int config_load_main_config(struct config *config)
{
	char *buffer;
	int skip;
	int rc;

	buffer = config_get_path(config, CONFIG_DATA_FILE_MAIN_CFG,
				     NULL, 1);

	rc = 0;

	if (buffer) {
		skip = config->skip_maincfg;

		log_dbg("%s main config file %s\n", skip ? "Skipping" : "Loading",
			buffer);

		if (!skip && check_file_exists(buffer))
			rc = config_load_file(config, config_parameters, buffer);
	}

	/* Load default bindings if none were loaded from the config
	   file. */
	config_load_default_bindings();

	free(buffer);

	return rc;
}

int config_load_rom_config(struct config *config)
{
	char *filename;
	int skip;
	int rc;

	rc = 0;
	skip = config->skip_romcfg;

	filename = emu_generate_rom_config_path(config->emu, 1);
	if (!filename)
		return 0;

	printf("%s ROM config file %s\n",
	       skip ? "Skipping" : "Loading", filename);
	if (!skip)
		rc = config_load_file(config, rom_config_parameters,
				      filename);

	free(filename);

	return rc;
}

