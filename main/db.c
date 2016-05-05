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

#include <ctype.h>
#include "emu.h"
#include "file_io.h"
#include "db.h"
#include "crc32.h"
#include "sha1.h"

static struct rom_info *head;

struct db_parser_state {
	char *buffer;
	size_t buffer_size;
	size_t buffer_used;
	size_t buffer_avail;

	struct rom_info *current;
};

static struct db_parser_state state;

static void insert_rom(struct rom_info *info)
{
	info->flags |= ROM_FLAG_IN_DB;
	info->next = head;
	head = info;
}

static void extend_state_buffer(struct db_parser_state *state,
				size_t len)
{
	char *tmp;

	tmp = realloc(state->buffer, state->buffer_size + len);

	if (!tmp)
		return;

	state->buffer = tmp;
	state->buffer_size += len;
	state->buffer_avail += len;
}

static void resize_state_buffer(struct db_parser_state *state,
				size_t len)
{
	if (len <= state->buffer_size)
		return;

	extend_state_buffer(state, len - state->buffer_size);
}

static int parse_line(char *buffer, char **key, char **value)
{
	char *sep;
	char *v;

	sep = strchr(buffer, ':');
	if (!sep)
		return -1;

	*sep = '\0';

	v = sep + 1;
	while (*v && isspace(*v))
		v++;

	if (value)
		*value = v;

	sep--;
	while ((sep >= buffer) && isspace(*sep)) {
		*sep = '\0';
		sep--;
	}

	if (key)
		*key = buffer;

	return 0;
}

static char **tokenize_list(char *list) {
	int count;
	char *token;
	char **tokens;

	tokens = malloc(sizeof(char *));
	count = 1;

	token = strtok(list, ", \t");
	while (token) {
		char **tmp;

		tmp = realloc(tokens, (count + 1) * sizeof(char *));
		if (!tmp) {
			free(tokens);
			return NULL;
		}
		tokens = tmp;
		tokens[count - 1] = token;
		count++;
		token = strtok(NULL, ", \t");
	}

	tokens[count - 1] = NULL;

	return tokens;
}

static int parse_int_list(char *value, uint32_t *list, int limit, int base)
{
	char **tokens;
	int count;
	int i;

	tokens = tokenize_list(value);
	count = 0;

	for (i = 0; tokens[i] && i < limit; i++) {
		long v;
		char *end;
		v = strtol(tokens[i], &end, base);
		if (*end == 'k' || *end == 'K') {
			v *= 1024;
			end++;
		}

		if (!*end) {
			list[i] = (uint32_t)v;
			count++;
		} else {
			count = -1;
			break;
		}
	}

	free(tokens);

	return count;
}

static void parse_boolean_list(char *value, int *list, int limit)
{
	char **tokens;
	int i;

	tokens = tokenize_list(value);

	for (i = 0; tokens[i] && i < limit; i++) {
		if (!strcasecmp(tokens[i], "true"))
			list[i] = 1;
		else if (!strcasecmp(tokens[i], "false"))
			list[i] = 0;
		else
			list[i] = 0;
	}

	free(tokens);
}

static int parse_sha1_list(char *value, uint8_t (*list)[20], int limit)
{
	char **tokens;
	int i;
	int hash_count;

	tokens = tokenize_list(value);

	hash_count = 0;

	for (i = 0; tokens[i] && i < limit; i++) {
		int j;
		int count;
		char *value;

		value = tokens[i];
		count = 0;
		if (strlen(value) == 40) {
			for (j = 0; j < 20; j++) {
				count += sscanf(&value[j * 2], "%2hhx",
						&list[i][j]);
			}

			if (count == 20) {
				hash_count++;
			} else {
				hash_count = -1;
				break;
			}
		}

	}

	free(tokens);

	return hash_count;
}

struct system_type_mapping {
	const char *name;
	int value;
};

static struct system_type_mapping system_type_mappings[] = {
	{ "NES-NTSC", EMU_SYSTEM_TYPE_NES },
	{ "NES-PAL", EMU_SYSTEM_TYPE_PAL_NES },
	{ "NES-PAL-A", EMU_SYSTEM_TYPE_PAL_NES },
	{ "NES-PAL-B", EMU_SYSTEM_TYPE_PAL_NES },
	{ "Famicom", EMU_SYSTEM_TYPE_FAMICOM },
	{ "Dendy", EMU_SYSTEM_TYPE_DENDY },
	{ "VS-RP2C03B", EMU_SYSTEM_TYPE_VS_RP2C03B },
	{ "VS-RP2C03G", EMU_SYSTEM_TYPE_VS_RP2C03G },
	{ "VS-RP2C04-0001", EMU_SYSTEM_TYPE_VS_RP2C04_0001 },
	{ "VS-RP2C04-0002", EMU_SYSTEM_TYPE_VS_RP2C04_0002 },
	{ "VS-RP2C04-0003", EMU_SYSTEM_TYPE_VS_RP2C04_0003 },
	{ "VS-RP2C04-0004", EMU_SYSTEM_TYPE_VS_RP2C04_0004 },
	{ "VS-RC2C03B", EMU_SYSTEM_TYPE_VS_RC2C03B },
	{ "VS-RC2C03B", EMU_SYSTEM_TYPE_VS_RC2C03B },
	{ "VS-RC2C05-01", EMU_SYSTEM_TYPE_VS_RC2C05_01 },
	{ "VS-RC2C05-02", EMU_SYSTEM_TYPE_VS_RC2C05_02 },
	{ "VS-RC2C05-03", EMU_SYSTEM_TYPE_VS_RC2C05_03 },
	{ "VS-RC2C05-04", EMU_SYSTEM_TYPE_VS_RC2C05_04 },
	{ "VS-RC2C05-05", EMU_SYSTEM_TYPE_VS_RC2C05_05 },
	{ "PlayChoice", EMU_SYSTEM_TYPE_PLAYCHOICE },
	{ NULL },
};

static int parse_system_type(char *value)
{
	int system_type;
	int i;

	system_type = EMU_SYSTEM_TYPE_UNDEFINED;

	for (i = 0; system_type_mappings[i].name; i++) {
		if (!strcasecmp(value, system_type_mappings[i].name)) {
			system_type = system_type_mappings[i].value;
			break;
		}
	}

	if (system_type == EMU_SYSTEM_TYPE_UNDEFINED) {
		system_type = EMU_SYSTEM_TYPE_NES;
		/* err_message("Unknown system type %s\n", value); */
	}

	return system_type;
}

static int parse_mirroring(char *value)
{
	int mirroring;

	if (!strcasecmp(value, "Horizontal"))
		mirroring = MIRROR_H;
	else if (!strcasecmp(value, "Vertical"))
		mirroring = MIRROR_V;
	else if (!strcasecmp(value, "Four-Screen"))
		mirroring = MIRROR_4;
	else if (!strcasecmp(value, "Single-Screen A"))
		mirroring = MIRROR_1A;
	else if (!strcasecmp(value, "Single-Screen B"))
		mirroring = MIRROR_1B;
	else if (!strcasecmp(value, "Mapper-Controlled"))
		mirroring = MIRROR_M;
	else
		mirroring = MIRROR_M; /* HACK */

	return mirroring;
}

static void set_auto_device(struct rom_info *node, int port, const char *dev_string)
{
	int device;

	if (port < 0 || port > 4)
		return;

	device = IO_DEVICE_NONE;

	if (!strcasecmp(dev_string, "zapper")) {
		if (port == 0) {
			device = IO_DEVICE_ZAPPER_1;
		} else if (port == 1) {
			device = IO_DEVICE_ZAPPER_2;
		}
	} else if (!strcasecmp(dev_string, "powerpad_a")) {
		if (port == 0) {
			device = IO_DEVICE_POWERPAD_A1;
		} else if (port == 1) {
			device = IO_DEVICE_POWERPAD_A2;
		}
	} else if (!strcasecmp(dev_string, "powerpad_b")) {
		if (port == 0) {
			device = IO_DEVICE_POWERPAD_B1;
		} else if (port == 1) {
			device = IO_DEVICE_POWERPAD_B2;
		}
	} else if (!strcasecmp(dev_string, "familytrainer_a")) {
		device = IO_DEVICE_FTRAINER_A;
	} else if (!strcasecmp(dev_string, "familytrainer_b")) {
		device = IO_DEVICE_FTRAINER_B;
	} else if (!strcasecmp(dev_string, "nes_arkanoid")) {
		if (port == 0) {
			device = IO_DEVICE_ARKANOID_NES_1;
		} else if (port == 1) {
			device = IO_DEVICE_ARKANOID_NES_2;
		}
	} else if (!strcasecmp(dev_string, "fc_arkanoid")) {
		device = IO_DEVICE_ARKANOID_FC;
	} else if (!strcasecmp(dev_string, "arkanoid2")) {
		device = IO_DEVICE_ARKANOID_II;
	} else if (!strcasecmp(dev_string, "familykeyboard")) {
		device = IO_DEVICE_KEYBOARD;
	} else if (!strcasecmp(dev_string, "vs_zapper")) {
		device = IO_DEVICE_VS_ZAPPER;
	}

	node->auto_device_id[port] = device;
}

static void fixup_entry(struct rom_info *info)
{
	int i;

	info->total_prg_size = 0;
	info->total_chr_size = 0;

	for (i = 0; i < MAX_PRG_ROMS; i++)
		info->total_prg_size +=	info->prg_size[i];

	for (i = 0; i < MAX_CHR_ROMS; i++)
		info->total_chr_size +=	info->chr_size[i];

	/* Handle roms that only have prg data on a single
	   chip with no chr data and therefore may have no prg
	   crc list.
	*/
	if ((info->prg_size_count == 1) &&
	    !info->chr_size_count) {
		if (!info->prg_crc_count &&
		    (info->flags & ROM_FLAG_HAS_CRC)) {
			info->prg_crc_count = 1;
			info->prg_crc[0] = info->combined_crc;
		}

		if (!info->prg_sha1_count &&
		    (info->flags & ROM_FLAG_HAS_SHA1)) {
			info->prg_sha1_count = 1;
			memcpy(&info->prg_sha1[0], info->combined_sha1, 20);
		}
	}

	/* If the checksum counts don't match the size
	   counts, set the checksum counts to 0.  Only
	   the combined checksums (if present) may be
	   used.
	*/

	if (info->prg_crc_count != info->prg_size_count)
		info->prg_crc_count = 0;

	if (info->prg_sha1_count != info->prg_size_count)
		info->prg_sha1_count = 0;

	if (info->chr_crc_count != info->chr_size_count)
		info->chr_crc_count = 0;

	if (info->chr_sha1_count != info->chr_size_count)
		info->chr_sha1_count = 0;

	/* Split rom checksums must be present for both
	   prg and chr (if chr is present), otherwise
	   split rom checksums aren't useful.
	*/
	if (info->chr_size_count &&
	    (info->prg_sha1_count || info->prg_crc_count) &&
	    !(info->chr_sha1_count || info->chr_crc_count)) {
		info->prg_sha1_count = 0;
		info->prg_crc_count = 0;
	}

	if ((info->chr_sha1_count || info->chr_crc_count) &&
	    !(info->prg_sha1_count || info->prg_crc_count)) {
		info->chr_sha1_count = 0;
		info->chr_crc_count = 0;
	}

	if (!info->prg_crc_count && !info->prg_sha1_count &&
	    !(info->flags & (ROM_FLAG_HAS_CRC|ROM_FLAG_HAS_SHA1))) {
		free(info);
	} else {
		insert_rom(info);
	}
}

static void process_field(struct db_parser_state *state)
{
	char *key, *value;
	int boolean_list[2];

	if (!state->buffer)
		return;

	if (!strcmp(state->buffer, "%%")) {
		/* If no valid split sizes and checksums present,
		   and no valid combined checksums present, drop
		   this entry and move on.
		*/
		fixup_entry(state->current);
		
		state->current = NULL;

		goto done;
	}

	if (parse_line(state->buffer, &key, &value) < 0)
		goto done;

	if (!state->current) {
		boolean_list[0] = 0;
		boolean_list[1] = 0;
		state->current = malloc(sizeof(*state->current));
		memset(state->current, 0, sizeof(*state->current));
		state->current->mirroring = MIRROR_UNDEF;
		state->current->four_player_mode = FOUR_PLAYER_MODE_NONE;
		state->current->vs_controller_mode = VS_CONTROLLER_MODE_STANDARD;
		state->current->auto_device_id[0] = IO_DEVICE_CONTROLLER_1;
		state->current->auto_device_id[1] = IO_DEVICE_CONTROLLER_2;
		state->current->auto_device_id[2] = IO_DEVICE_CONTROLLER_3;
		state->current->auto_device_id[3] = IO_DEVICE_CONTROLLER_4;
		state->current->auto_device_id[4] = IO_DEVICE_NONE;
	}

	if (!strcasecmp(key, "prg crc")) {
		int count = parse_int_list(value,
					   state->current->prg_crc, MAX_PRG_ROMS, 16);
		if (count > 0)
			state->current->prg_crc_count = count;
		else
			state->current->prg_crc_count = 0;
	} else if (!strcasecmp(key, "prg")) {
		int count = parse_int_list(value,
					   state->current->prg_size, MAX_PRG_ROMS, 10);
		if (count > 0)
			state->current->prg_size_count = count;
		else
			state->current->prg_size_count = 0;
	} else if (!strcasecmp(key, "chr crc")) {
		int count = parse_int_list(value,
					   state->current->chr_crc, MAX_CHR_ROMS, 16);
		if (count > 0)
			state->current->chr_crc_count = count;
		else
			state->current->chr_crc_count = 0;
	} else if (!strcasecmp(key, "prg sha1")) {
		int count = parse_sha1_list(value, &state->current->prg_sha1[0], MAX_PRG_ROMS);
		if (count > 0)
			state->current->prg_sha1_count = count;
		else
			state->current->prg_sha1_count = 0;
	} else if (!strcasecmp(key, "chr sha1")) {
		int count = parse_sha1_list(value, &state->current->chr_sha1[0], MAX_CHR_ROMS);
		if (count > 0)
			state->current->chr_sha1_count = count;
		else
			state->current->chr_sha1_count = 0;
	} else if (!strcasecmp(key, "chr")) {
		int count = parse_int_list(value,
					   state->current->chr_size, MAX_CHR_ROMS, 10);
		if (count > 0)
			state->current->chr_size_count = count;
		else
			state->current->chr_size_count = 0;
	} else if (!strcasecmp(key, "wram0")) {
		parse_int_list(value, &state->current->wram_size[0], 1, 10);
	} else if (!strcasecmp(key, "wram1")) {
		parse_int_list(value, &state->current->wram_size[1], 1, 10);
	} else if (!strcasecmp(key, "vram0")) {
		parse_int_list(value, &state->current->vram_size[0], 1, 10);
	} else if (!strcasecmp(key, "vram1")) {
		parse_int_list(value, &state->current->vram_size[1], 1, 10);
	} else if (!strcasecmp(key, "wram0-battery")) {
		parse_boolean_list(value, boolean_list, 1);
		if (boolean_list[0])
			state->current->flags |= ROM_FLAG_WRAM0_NV;
	} else if (!strcasecmp(key, "wram1-battery")) {
		parse_boolean_list(value, boolean_list, 1);
		if (boolean_list[0])
			state->current->flags |= ROM_FLAG_WRAM1_NV;
	} else if (!strcasecmp(key, "vram0-battery")) {
		parse_boolean_list(value, boolean_list, 1);
		if (boolean_list[0])
			state->current->flags |= ROM_FLAG_VRAM0_NV;
	} else if (!strcasecmp(key, "vram1-battery")) {
		parse_boolean_list(value, boolean_list, 1);
		if (boolean_list[0])
			state->current->flags |= ROM_FLAG_VRAM1_NV;
	} else if (!strcasecmp(key, "mapper-battery")) {
		parse_boolean_list(value, boolean_list, 1);
		if (boolean_list[0])
			state->current->flags |= ROM_FLAG_MAPPER_NV;
	} else if (!strcasecmp(key, "playchoice")) {
		parse_boolean_list(value, boolean_list, 1);
		if (boolean_list[0])
			state->current->flags |= ROM_FLAG_PLAYCHOICE;
	} else if (!strcasecmp(key, "system")) {
		state->current->system_type = parse_system_type(value);
	} else if (!strcasecmp(key, "board")) {
		state->current->board_type = board_find_type_by_name(value);
	} else if (!strcasecmp(key, "mirroring")) {
		state->current->mirroring = parse_mirroring(value);
	} else if (!strcasecmp(key, "crc")) {
		int count = parse_int_list(value, &state->current->combined_crc, 1, 16);
		if (count == 1)
			state->current->flags |= ROM_FLAG_HAS_CRC;
	} else if (!strcasecmp(key, "sha1")) {
		int count = parse_sha1_list(value, &state->current->combined_sha1, 1);
		if (count == 1)
			state->current->flags |= ROM_FLAG_HAS_SHA1;
	} else if (!strcasecmp(key, "name")) {
		/* printf("title: %s\n", value); */
	} else if (!strcasecmp(key, "port-1")) {
		set_auto_device(state->current, 0, value);
	} else if (!strcasecmp(key, "port-2")) {
		set_auto_device(state->current, 1, value);
	} else if (!strcasecmp(key, "port-3")) {
		set_auto_device(state->current, 2, value);
	} else if (!strcasecmp(key, "port-4")) {
		set_auto_device(state->current, 3, value);
	} else if (!strcasecmp(key, "port-exp")) {
		set_auto_device(state->current, 4, value);
	} else if (!strcasecmp(key, "vs-controller-mode")) {
		int mode;

		if (!strcasecmp(value, "standard")) {
			mode = VS_CONTROLLER_MODE_STANDARD;
		} else if (!strcasecmp(value, "swapped")) {
			mode = VS_CONTROLLER_MODE_SWAPPED;
		} else if (!strcasecmp(value, "vssuperskykid")) {
			mode = VS_CONTROLLER_MODE_VSSUPERSKYKID;
		} else if (!strcasecmp(value, "vspinballj")) {
			mode = VS_CONTROLLER_MODE_VSPINBALLJ;
		} else if (!strcasecmp(value, "vsbungelingbay")) {
			mode = VS_CONTROLLER_MODE_BUNGELINGBAY;
		} else {
			mode = VS_CONTROLLER_MODE_STANDARD;
		}

		state->current->vs_controller_mode = mode;
	} else if (!strcasecmp(key, "four-player")) {
		int mode;

		if (!strcasecmp(value, "famicom")) {
			mode = FOUR_PLAYER_MODE_FC;
		} else if (!strcasecmp(value, "fourscore")) {
			mode = FOUR_PLAYER_MODE_NES;
		} else if (!strcasecmp(value, "nes")) {
			mode = FOUR_PLAYER_MODE_NES;
		} else if (!strcasecmp(value, "none")) {
			mode = FOUR_PLAYER_MODE_NONE;
		} else {
			mode = FOUR_PLAYER_MODE_NONE;
		}

		state->current->four_player_mode = mode;
	}

done:
	state->buffer_used = 0;
	state->buffer_avail = state->buffer_size;
}

static void new_db_callback(char *line, int num, void *data)
{
	struct db_parser_state *state;
	int len;

	state = data;

	if (isspace(*line)) {
		char *ptr;

		if (!state->buffer_used)
			return;

		ptr = line;
		while (*ptr && isspace(*ptr))
			ptr++;

		len = strlen(ptr);
		extend_state_buffer(state, len + 1);

		state->buffer[state->buffer_used] = ' ';
		strncpy(state->buffer + state->buffer_used + 1, ptr, len + 1);
		state->buffer_used += len + 1;
		state->buffer_avail -= len + 1;
	} else {
		len = strlen(line);
		process_field(state);
		resize_state_buffer(state, len + 1);
		strncpy(state->buffer, line, len + 1);
		state->buffer_used = len + 1;
		state->buffer_avail = state->buffer_size - len + 1;
	}

}

int db_load_file(struct emu *emu, const char *filename)
{
	int rc;

	memset(&state, 0, sizeof(state));

	if (!check_file_exists(filename))
	    return 0;

	rc = process_file(filename, &state,
			  new_db_callback, 0, 1);

	if (state.buffer_used)
		process_field(&state);

	if (state.current)
		insert_rom(state.current);

	state.current = NULL;

	return rc;
}

void db_cleanup(void)
{
	struct rom_info *tmp;

	while (head) {
		tmp = head->next;
		free(head);
		head = tmp;
	}

	if (state.buffer) {
		free(state.buffer);
		state.buffer_used = 0;
		state.buffer_size = 0;
		state.buffer_avail = 0;
	}

}

struct rom_info *db_lookup_split_rom(struct archive_file_list *list, int *chip_list,
				     struct rom_info *start)
{
	if (start)
		start = start->next;
	else
		start = head;

	while (start) {
		int i;
		int remaining;
	
		remaining = start->prg_size_count + start->chr_size_count;

		if (!remaining) {
			start = start->next;
			continue;
		}

		for (i = 0; i < MAX_ROMS; i++)
			chip_list[i] = -1;

		if ((start->prg_size_count &&
		     !(start->prg_crc_count || start->prg_sha1_count)) ||
		    (start->chr_size_count &&
		     !(start->chr_crc_count || start->chr_sha1_count))) {
			start = start->next;
			continue;
		}

		for (i = 0; i < list->count; i++) {
			int j;

			if (!remaining)
				break;

			for (j = 0; j < start->prg_size_count; j++) {
				if (start->prg_crc_count &&
				    (list->entries[i].crc != start->prg_crc[j])) {
					continue;
				}

				if (start->prg_sha1_count &&
				    memcmp(list->entries[i].sha1, start->prg_sha1[j],
					   20)) {
					continue;
				}

				if (chip_list[j] < 0) {
					chip_list[j] = i;
					remaining--;
					break;
				}
			}

			/* Found a match; don't test chr */
			if (j < start->prg_size_count) {
				continue;
			}

			for (j = 0; j < start->chr_size_count; j++) {
				if (start->chr_crc_count &&
				    (list->entries[i].crc != start->chr_crc[j])) {
					continue;
				}

				if (start->chr_sha1_count &&
				    memcmp(list->entries[i].sha1, start->chr_sha1[j],
					   20)) {
					continue;
				}

				if (chip_list[j + start->prg_size_count] < 0) {
					chip_list[j + start->prg_size_count] = i;
					remaining--;
					break;
				}
			}

		}

		if (!remaining)
			break;

		start = start->next;
	}

	return start;
}

static struct rom_info *db_find_entry(uint32_t crc32, uint8_t *sha1, size_t size, int combined)
{

	struct rom_info *start;

	start = head;

	while (start) {
		size_t total_size;
		int match = 0;

		total_size = start->total_prg_size;
		total_size += start->total_chr_size;

		if (combined) {
			if (start->flags & (ROM_FLAG_HAS_CRC|ROM_FLAG_HAS_SHA1)) {
				if (size == total_size)
					match = 1;

				if (match && (start->flags & ROM_FLAG_HAS_CRC) &&
				    (start->combined_crc != crc32)) {
					match = 0;
				}

				if (match && (start->flags & ROM_FLAG_HAS_SHA1) &&
				    (memcmp(start->combined_sha1, sha1, 20))) {
					match = 0;
				}
			}
		} else {
			if (start->prg_crc_count || start->prg_sha1_count) {
				if (start->prg_size[0] == size) {
					match = 1;
				}

				if (match && start->prg_crc_count &&
				    (start->prg_crc[0] != crc32)) {
					match = 0;
				}

				if (match && start->prg_sha1_count &&
				    memcmp(start->prg_sha1[0], sha1, 20)) {
					match = 0;
				}
			}
		}

		if (match) {
			if (start->board_type != BOARD_TYPE_UNKNOWN)
				break;
		}

		start = start->next;
	}

	return start;
}

static uint32_t calculate_checksum_new(uint8_t *data, size_t size,
				       uint8_t *sha1)
{
	sha1nfo sha1nfo;
	uint8_t *result;
	uint32_t crc;

	sha1_init(&sha1nfo);
	crc = crc32buf(data, size, NULL);
	sha1_write(&sha1nfo, (char *)data, size);
	result = sha1_result(&sha1nfo);
	memcpy(sha1, result, 20);

	return crc;
}

int validate_checksums(struct rom *rom, struct rom_info *info)
{
	uint32_t crc32;
	uint8_t sha1[20];
	int i;
	off_t offset;

	offset = rom->offset;

	/* Validate PRG chunks */
	for (i = 0; i < info->prg_size_count; i++) {
		crc32 = calculate_checksum_new(rom->buffer + offset,
					       info->prg_size[i], sha1);

		if (info->prg_crc_count && (crc32 != info->prg_crc[i]))
			break;

		if (info->prg_sha1_count &&
		    memcmp(sha1, info->prg_sha1[i], 20)) {
			break;
		}

		offset += info->prg_size[i];
	}

	if (i < info->prg_crc_count)
		return 0;

	/* Validate CHR chunks */
	for (i = 0; i < info->chr_crc_count; i++) {
		crc32 = calculate_checksum_new(rom->buffer + offset,
					       info->chr_size[i], sha1);

		if (info->chr_crc_count && (crc32 != info->chr_crc[i]))
			break;

		if (info->chr_sha1_count &&
		    memcmp(sha1, info->chr_sha1[i], 20)) {
			break;
		}

		offset += info->chr_size[i];
	}

	if (i < info->chr_crc_count)
		return 0;

	return 1;
}

struct rom_info *db_lookup(struct rom *rom, struct rom_info *junk)
{
	struct rom_info *info;
	uint32_t crc32;
	uint8_t sha1[20];
	size_t size;
	int mode;
	int old_system_type;

	mode = 1;
	size = (rom->buffer_size - rom->offset) / SIZE_8K * SIZE_8K;
	info = NULL;
	while (size >= SIZE_8K) {
		crc32 = calculate_checksum_new(rom->buffer + rom->offset,
					       size, sha1);

		info = db_find_entry(crc32, sha1, size, mode);

		if (info) {
			if (mode || validate_checksums(rom, info))
				break;
			else
				info = NULL;
		}

		if (mode) {
			int count = 0;

			while (size) {
				size >>= 1;
				count++;
			}

			if (count)
				size = 1 << (count - 1);

			mode = 0;
		} else {
			size /= 2;
		}
	}

	old_system_type = rom->info.system_type;

	if (info) {
		memcpy(&rom->info, info, sizeof(*info));
		if ((old_system_type == EMU_SYSTEM_TYPE_PLAYCHOICE) &&
		    (rom->info.flags & ROM_FLAG_PLAYCHOICE)) {
			rom->info.system_type = EMU_SYSTEM_TYPE_PLAYCHOICE;
		}
	}

	rom_calculate_checksum(rom);

	return info;
}

/* DB-based ROM loader for simple formats like iNES: header of zero or
   more bytes, followed by prg rom, followed by chr rom.

   This will also correctly handle iNES roms which have the header
   removed and/or extra data appended to the ROM.
*/
int db_rom_load(struct emu *emu, struct rom *rom)
{
	struct rom_info *db_entry;
	size_t slack;

	if (!rom)
		return -1;

	rom->offset = 0;
	rom->info.prg_size[0] = rom->buffer_size & ~(SIZE_8K - 1);
	rom->info.chr_size[0] = 0;
	slack = rom->buffer_size & (SIZE_8K - 1);

	rom_calculate_checksum(rom);
	db_entry = db_lookup(rom, NULL);

	if (!db_entry && slack >= INES_HEADER_SIZE) {
		rom->offset = INES_HEADER_SIZE;
		rom_calculate_checksum(rom);
		db_entry = db_lookup(rom, NULL);
	}

	if (!db_entry && slack != INES_HEADER_SIZE) {
		rom->offset = slack;
		rom_calculate_checksum(rom);
		db_entry = db_lookup(rom, NULL);
	}

	/* Handle a few corner cases here:
	   - PC10 roms without iNES header
	   - PC10 roms with instruction screen rom in CHR area
	   - Roms with PRG size 8K, 24K or 40K
	 */
	if (!db_entry && (rom->info.prg_size[0] > SIZE_8K)) {
		rom->info.prg_size[0] -= SIZE_8K;
		rom->offset = 0;
		rom_calculate_checksum(rom);
		db_entry = db_lookup(rom, NULL);

		if (!db_entry && slack >= INES_HEADER_SIZE) {
			rom->offset = INES_HEADER_SIZE;
			rom_calculate_checksum(rom);
			db_entry = db_lookup(rom, NULL);
		}

		if (!db_entry && slack != INES_HEADER_SIZE) {
			rom->offset = slack;
			rom_calculate_checksum(rom);
			db_entry = db_lookup(rom, NULL);
		}

		if (!db_entry) {
			rom->offset = SIZE_8K;
			rom_calculate_checksum(rom);
			db_entry = db_lookup(rom, NULL);
		}

		if (!db_entry && slack >= INES_HEADER_SIZE) {
			rom->offset = SIZE_8K + INES_HEADER_SIZE;
			rom_calculate_checksum(rom);
			db_entry = db_lookup(rom, NULL);
		}

		if (!db_entry && slack != INES_HEADER_SIZE) {
			rom->offset = SIZE_8K + slack;
			rom_calculate_checksum(rom);
			db_entry = db_lookup(rom, NULL);
		}
	}

	if (!db_entry)
		return -1;

	return 0;
}

