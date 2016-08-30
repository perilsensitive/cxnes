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
#include <errno.h>

#include "emu.h"
#include "file_io.h"

#define PRO_ACTION_ROCKY_INIT_MAGIC 0xfcbdd275
#define PRO_ACTION_ROCKY_XOR_MAGIC  0xb8309722

static int cheats_enabled = 0;

static const char *game_genie_alphabet = "APZLGITYEOXUKSVN";
static const int rocky_shifts[31] = {
		3,  13, 14,  1,  6,  9,  5,  0,
		12,  7,  2,  8, 10, 11,  4, 19,
		21, 23, 22, 20, 17, 16, 18, 29,
		31, 24, 26, 25, 30, 27, 28
};

int add_cheat(struct cheat_state *cheats, char *code);
void remove_cheat(struct cheat_state *cheats, struct cheat *cheat);

struct cheat *find_cheat(struct cheat_state *cheats, uint16_t addr)
{
	struct cheat *cheat;

	cheat = NULL;

	for (cheat = cheats->cheat_list; cheat; cheat = cheat->next) {
		if ((cheat->address == addr) && cheat->enabled)
			break;
	}

	return cheat;
}

static CPU_READ_HANDLER(cheat_read_handler)
{
	struct cheat *cheat;
	uint8_t orig;

	cheat = find_cheat(emu->cheats, addr);

	if (cheat && cheat->orig_read_handler)
		orig = cheat->orig_read_handler(emu, addr, value, cycles);
	else
		orig = cpu_peek(emu->cpu, addr);

	if (!cheat || !emu->cheats->enabled ||
	    !(cheat->enabled) ||
	    ((cheat->compare >= 0) && orig != cheat->compare))
		return orig;
	else
		return cheat->value;
}

struct cheat_state *cheat_init(struct emu *emu)
{
	struct cheat_state *cheats;

	cheats = malloc(sizeof(*cheats));
	if (cheats) {
		memset(cheats, 0, sizeof(*cheats));
	}

	cheats->cheat_list = NULL;

	emu->cheats = cheats;
	cheats->emu = emu;

	return cheats;
}

static void free_cheat(struct cheat *cheat)
{
	if (cheat->description)
		free(cheat->description);

	free(cheat);
}

static void install_read_handler(struct cheat_state *cheats, struct cheat *cheat)
{
	if (!cheats_enabled)
		return;
	
	cpu_read_handler_t *old;

	old = cpu_get_read_handler(cheats->emu->cpu, cheat->address);

	if (old == cheat_read_handler)
		return;

	cheat->orig_read_handler = old;
	
	cpu_set_read_handler(cheats->emu->cpu, cheat->address, 1, 0,
			     cheat_read_handler);
}

static void remove_read_handler(struct cheat_state *cheats, struct cheat *cheat)
{
	cpu_set_read_handler(cheats->emu->cpu, cheat->address, 1, 0,
			     cheat->orig_read_handler);
}

void disable_cheat(struct cheat_state *cheats, struct cheat *cheat)
{
	cheat->enabled = 0;

	if (cheats_enabled)
		remove_read_handler(cheats, cheat);
}

int enable_cheat(struct cheat_state *cheats, struct cheat *cheat)
{
	struct cheat *tmp;
	int disabled_others = 0;

	for (tmp = cheats->cheat_list; tmp; tmp = tmp->next) {
		if ((tmp->address == cheat->address) &&
		    (tmp != cheat)) {
			disabled_others = 1;
			disable_cheat(cheats, tmp);
		}
	}

	cheat->enabled = 1;

	if (cheats_enabled)
		install_read_handler(cheats, cheat);

	return disabled_others;
}

void cheat_deinit(struct cheat_state *cheats)
{
	struct cheat *cheat, *tmp;

	cheat = cheats->cheat_list;
	while (cheat) {
		tmp = cheat->next;
		free_cheat(cheat);
		cheat = tmp;
	}
	cheats->cheat_list = NULL;
}

void cheat_cleanup(struct cheat_state *cheats)
{
	cheat_deinit(cheats);
	free(cheats);
}

void cheat_enable_all(struct cheat_state *cheats, int enable)
{
	cheats->enabled = enable;
}

void insert_cheat(struct cheat_state *cheats, struct cheat *cheat)
{
	struct cheat *p;

	if (!cheats->cheat_list) {
		cheats->cheat_list = cheat;
	} else {
		p = cheats->cheat_list;

		while (1) {
			if ((cheat->address == p->address) &&
			    (cheat->value == p->value) &&
			    (cheat->compare == p->compare) &&
			    (cheat->description && p->description) &&
			    !strcmp(p->description, cheat->description)) {
				free(cheat);
				return;
			}

			if (!p->next)
				break;

			p = p->next;
		}

		p->next = cheat;
	}

	cheat->next = NULL;
}

static int parse_virtuanes_code(const char *code, int *a, int *v, int *c, char **d, int *e)
{
	int desc_start;
	unsigned int a1, v1, c1;
	int has_compare;
	int status;

	desc_start = 0;
	has_compare = 0;

	sscanf(code, "#%d %x-%x-%x %n%*s \n", &status, &a1, &v1, &c1, &desc_start);
	if (desc_start) {
		has_compare = 1;
	} else {
		sscanf(code, "#%d %x-%x %n%*s \n", &status, &a1, &v1, &desc_start);
	}

	if (!desc_start)
		return 1;

	if ((a1 > 0xffff) || (v1 > 0xff) || (has_compare && (c1 > 0xff)))
		return 1;

	*a = a1;
	*v = v1;
	*d = (char *)code + desc_start;
	if (has_compare)
		*c = c1;

	/* Imported cheats always start as disabled */
	*e = 0;

	return 0;
}

static int parse_cxnes_code(const char *code, int *a, int *v, int *c, char **d, int *e)
{
	int desc_start;
	unsigned int a1, v1, c1;
	int has_compare;
	int status;

	desc_start = 0;
	has_compare = 0;

	sscanf(code, "%d %x:%x:%x %n%*s \n", &status, &a1, &v1, &c1, &desc_start);
	if (desc_start) {
		has_compare = 1;
	} else {
		sscanf(code, "%d %x:%x %n%*s \n", &status, &a1, &v1, &desc_start);
	}

	if (!desc_start)
		return 1;

	if ((a1 > 0xffff) || (v1 > 0xff) || (has_compare && (c1 > 0xff)))
		return 1;

	*a = a1;
	*v = v1;
	*d = (char *)code + desc_start;
	if (has_compare)
		*c = c1;

	switch (status) {
	case 0: *e = 0; break;
	case 1: *e = 1; break;
	default: *e = 0; break;
	}

	return 0;
}

static int parse_fceux_code(const char *code, int *a, int *v, int *c, char **d, int *e)
{
	char *tmpcode;
	int enabled;
	int has_compare;
	int desc_start;
	unsigned int a1, v1, c1;

	errno = 0;
	has_compare = 0;

	tmpcode = (char *)code;

	/* cxNES doesn't distinguish between "replacement" and "substitution"
	   cheats */
	if ((tmpcode[0] == 'S') || (tmpcode[0] == 's'))
		tmpcode++;

	if ((tmpcode[0] == 'C') || (tmpcode[0] == 'c')) {
		has_compare = 1;
		tmpcode++;
	}


	enabled = 0;
	/* If a ':' is present, the code is disabled */
	if (tmpcode[0] == ':') {
		tmpcode++;
	}

	desc_start = 0;
	if (has_compare)
		sscanf(tmpcode, "%x:%x:%x%n:%*s \n", &a1, &v1, &c1, &desc_start);
	else
		sscanf(tmpcode, "%x:%x%n:%*s \n", &a1, &v1, &desc_start);

	if (!desc_start)
		return 1;

	if (desc_start) {
		if (tmpcode[desc_start] == ':')
			desc_start++;
		else if (tmpcode[desc_start])
			return 1;
	}

	if ((a1 > 0xffff) || (v1 > 0xff) || (has_compare && (c1 > 0xff)))
		return 1;

	*a = a1;
	*v = v1;
	*e = enabled;
	*d = tmpcode + desc_start;
	if (has_compare)
		*c = c1;

	return 0;
}

int parse_raw_code(const char *code, int *a, int *v, int *c)
{
	int addr, value, compare;
	char *end;

	errno = 0;

	addr = strtoul(code, &end, 16);
	if (errno || !*end || ((*end != ':') && (*end != '-')) || (addr > 0xffff))
		return 1;
	*a = addr;

	value = strtoul(end + 1, &end, 16);
	if (errno || (*end && (*end != ':') && (*end != '-')) || (value > 0xff))
		return 1;
	*v = value;

	if (*end) {
		compare = strtoul(end + 1, &end, 16);
		if (errno || *end || (compare > 0xff))
			return 1;
		*c = compare;
	}

	/* printf("parsed raw code\"%s\" to addr=0x%04x value=0x%02x " */
	/*        "compare=0x%02x\n", code, addr, value, compare); */

	return 0;
}

static int decode_game_genie(const char *string, int *a, int *v, int *c)
{
	uint8_t code[8];
	int addr;
	int value, compare;
	int length;
	int i, j;

	length = strlen(string);

	compare = -1;

	j = 0;
	for (i = 0; string[i]; i++) {
		switch (toupper(string[i])) {
		case 'A': code[j] = 0x0; break;
		case 'P': code[j] = 0x1; break;
		case 'Z': code[j] = 0x2; break;
		case 'L': code[j] = 0x3; break;
		case 'G': code[j] = 0x4; break;
		case 'I': code[j] = 0x5; break;
		case 'T': code[j] = 0x6; break;
		case 'Y': code[j] = 0x7; break;
		case 'E': code[j] = 0x8; break;
		case 'O': code[j] = 0x9; break;
		case 'X': code[j] = 0xa; break;
		case 'U': code[j] = 0xb; break;
		case 'K': code[j] = 0xc; break;
		case 'S': code[j] = 0xd; break;
		case 'V': code[j] = 0xe; break;
		case 'N': code[j] = 0xf; break;
		case '-': continue;
		}

		j++;
	}

	if ((j != 6) && (j != 8))
		return 1;

	addr = ((code[3] & 7) << 12) |
	    ((code[5] & 7) << 8) |
	    ((code[4] & 8) << 8) |
	    ((code[2] & 7) << 4) |
	    ((code[1] & 8) << 4) | (code[4] & 7) | (code[3] & 8);
	addr += 0x8000;

	value = ((code[1] & 7) << 4) |
		((code[0] & 8) << 4) | (code[0] & 7);

	if (length == 6) {
		value |= (code[5] & 8);
	} else {
		value |= (code[7] & 8);

		compare = ((code[7] & 7) << 4) |
		    ((code[6] & 8) << 4) | (code[6] & 7) | (code[5] & 8);
	}

	*a = addr;
	*v = value;
	*c = compare;

	return 0;
}

int parse_game_genie_code(const char *code, int *a, int *v, int *c)
{
	int i, j;

	if (!code || !*code)
		return 0;

	for (i = 0; code[i]; i++) {
		if (code[i] == '-')
			continue;

		for (j = 0; j < 16; j++) {
			if (toupper(code[i]) == game_genie_alphabet[j])
				break;
		}

		if (j == 16)
			break;
	}

	if (code[i])
		return 1;

	return decode_game_genie(code, a, v, c);
	/* printf("parsed raw code\"%s\" to addr=0x%04x value=0x%02x " */
	/*        "compare=0x%02x\n", code, *a, *v, *c); */


	return 0;
}

void decode_pro_action_rocky(uint32_t code, int *a, int *v, int *c)
{
	uint32_t result;
	uint32_t decode;
	int i;

	result = 0;

	decode = PRO_ACTION_ROCKY_INIT_MAGIC;
	for (i = 31; i > 0; i--) {

		if((code ^ decode) & 0x80000000) {
			result |= 1 << rocky_shifts[i - 1];
			decode ^= PRO_ACTION_ROCKY_XOR_MAGIC;
		}

		decode <<= 1;
		code <<= 1;
	}

	*a = (result & 0x7fff) | 0x8000;
	*c = (result >> 16) & 0xff;
	*v = (result >> 24) & 0xff;
}

int parse_pro_action_rocky_code(const char *code, int *a, int *v, int *c)
{
	unsigned long value;
	char *end;

	errno = 0;
	value = strtoul(code, &end, 16);

	if (errno || *end || (value > 0xffffffff))
		return 1;

	decode_pro_action_rocky(value, a, v, c);

	return 0;
}

void raw_to_game_genie(int addr, int value, int compare, char *buffer)
{
	uint8_t code[8];
	int length;
	int i;

	if (!buffer)
		return;

	if (addr < 0x8000) {
		buffer[0] = '\0';
		return;
	}

	addr -= 0x8000;

	if (compare >= 0)
		length = 8;
	else
		length = 6;

	buffer[8] = 0;
	for (i = 0; i < 8; i++) {
		code[i] = 0;
		buffer[i] = 0;
	}

	code[3] |= (addr & 0x7000) >> 12;
	code[3] |= (addr & 0x0008);

	code[5] |= (addr & 0x0700) >> 8;

	code[4] |= (addr & 0x0800) >> 8;
	code[4] |= (addr & 0x0007);

	code[2] |= (addr & 0x0070) >> 4;
	if (length == 8)
		code[2] |= 0x08;

	code[1] |= (addr & 0x0080) >> 4;

	code[1] |= (value & 0x70) >> 4;

	code[0] |= (value & 0x80) >> 4;
	code[0] |= (value & 0x07);

	if (length == 8) {
		code[7] |= (value & 0x08);
		code[7] |= (compare & 0x70) >> 4;
		code[6] |= (compare & 0x80) >> 4;
		code[6] |= (compare & 0x07);
		code[5] |= (compare & 0x08);
	} else {
		code[5] |= (value & 0x08);
	}

	for (i = 0; i < length; i++) {
		buffer[i] = game_genie_alphabet[code[i]];
	}
}

void raw_to_pro_action_rocky(int addr, int value, int compare, char *buffer)
{
	int i;
	uint32_t decode;
	uint32_t result;
	uint32_t code;

	if (addr < 0x8000 || compare < 0) {
		buffer[0] = '\0';
		return;
	}
	addr &= 0x7fff;

	decode = 0xfcbdd274;
	result = 0;
	code = addr | ((compare & 0xff) << 16) | ((value & 0xff) << 24);
	for (i = 31; i > 0; i--) {
		int bit = code >> rocky_shifts[i - 1] & 1;
		result |= (decode >> 31 ^ bit) << i;

		if (bit)
			decode ^= 0xB8309722;

		decode <<= 1;
	}

	snprintf(buffer, 9, "%08X", result);
}

static void cheat_callback(char *line, int num, void *data)
{
	struct cheat_state *cheats;
	struct cheat *cheat;
	char *description;
	char *code;
	int start, end;
	int enabled;
	int description_start;
	int addr, value, compare;
	int rc;

	cheats = data;

	enabled = 0;

	compare = -1;
	rc = parse_cxnes_code(line, &addr, &value, &compare,
	       	              &description, &enabled);

	if (rc != 0) {
		rc = parse_fceux_code(line, &addr, &value, &compare,
				      &description, &enabled);
	}

	if (rc != 0) {
		rc = parse_virtuanes_code(line, &addr, &value, &compare,
					  &description, &enabled);
	}

	/* Fallback to handle simple "code+description" formats */
	if (rc != 0) {
		end = 0;
		description_start = 0;
		sscanf(line, " %n%*s%n %n%*s \n",
		       &start, &end, &description_start);

		if ((!end || !description_start)) {
			log_err("error parsing line %d\n", num);
			return;
		}

		line[end] = '\0';

		code = line + start;
		description = line + description_start;

		if (rc != 0)
			rc = parse_raw_code(code, &addr, &value, &compare);

		if (rc != 0)
			rc = parse_game_genie_code(code, &addr, &value, &compare);

		if (rc != 0)
			rc = parse_pro_action_rocky_code(code, &addr, &value, &compare);
	}

	if (rc != 0)
		return;

	cheat = malloc(sizeof(*cheat));
	if (!cheat)
		return;

	memset(cheat, 0, sizeof(*cheat));

	cheat->address = addr;
	cheat->value = value;
	cheat->compare = compare;
	cheat->description = strdup(description);

	insert_cheat(cheats, cheat);

	/* If the file specifies that multiple cheats for the same
	   address are to be enabled, make sure that only one of
	   them actually is. */
	if (enabled)
		enable_cheat(cheats, cheat);

#if 0
	if (compare >= 0) {
		printf("Added %s cheat \"%s\" "
		       "(address=0x%04x value=0x%02x compare=0x%02x) (%s)\n",
		       enabled ? "enabled" : "disabled",
		       description, addr, value, compare, code);
	} else {
		printf("Added %s cheat \"%s\" (address=0x%04x "
		       "value=0x%02x) (%s)\n",
		       enabled ? "enabled" : "disabled",
		       description, addr, value, code);
	}
#endif
}

int cheat_load_file(struct emu *emu, const char *filename)
{
	return process_file(filename, emu->cheats,
			    cheat_callback, 0, 1, 1);
}

int cheat_save_file(struct emu *emu, const char *filename)
{
	struct cheat *cheat;
	char *buffer, *p;
	size_t size, remaining;
	int rc;

	/* Prevent accidental clearing of the list by not
	   allowing an empty list to be saved.
	*/
	if (emu->cheats->cheat_list == NULL)
		return 0;

	size = 0;

	for (cheat = emu->cheats->cheat_list; cheat; cheat = cheat->next) {
		/* raw code: address:value (4:2), 1 for :, 1 for ' ' and
	           1 for enabled/disabled and 1 for another ' ' */
		size += 1 + 1; /* status + separator */
		size += 4 + 2 + 1 + 1; /* addr:value + separator */
		if (cheat->compare >= 0)
			size += 2 + 1; /* compare value (2), ':' */

		/* description: strlen(desc) + 2 (for line ending) */
		if (cheat->description) {
			size += 1; /* for space */
			size += strlen(cheat->description);
		}

		/* Line ending sequence */
		size += 2;
	}

	/* Add one byte for trailing null */
	buffer = malloc(size + 1);
	if (!buffer)
		return 1;

	remaining = size;
	p = buffer;
	for (cheat = emu->cheats->cheat_list; cheat; cheat = cheat->next) {
		rc = snprintf(p, remaining, "%d %04X:%02X",
		              (cheat->enabled ? 1 : 0),
			      cheat->address, cheat->value);
		p += rc;
		remaining -= rc;

		if (cheat->compare >= 0) {
			rc = snprintf(p, remaining, ":%02X", cheat->compare);
			p += rc;
			remaining -= rc;
		}

		if (cheat->description) {
			rc = snprintf(p, remaining, " %s", cheat->description);
			p += rc;
			remaining -= rc;
		}

#if defined _WIN32
		rc = snprintf(p, remaining, "\r\n");
#else
		rc = snprintf(p, remaining, "\n");		
#endif
		p += rc;
		remaining -= rc;
	}

	create_directory(filename, 1, 1);

	rc = writefile(filename, (uint8_t *)buffer, size - remaining);
	free(buffer);

	return rc;
}

int cheat_apply_config(struct cheat_state *cheats)
{
	cheats->enabled = cheats->emu->config->cheats_enabled;

	return 0;
}

int cheat_enable_cheats(struct cheat_state *cheats, int enabled)
{
	struct cheat *cheat;

	if (enabled < 0)
		enabled = !cheats->enabled;

	cheats->enabled = enabled;
	for (cheat = cheats->cheat_list; cheat; cheat = cheat->next) {
		if (!cheat->enabled)
			continue;

		if (enabled)
			install_read_handler(cheats, cheat);
		else
			remove_read_handler(cheats, cheat);
	}

	return cheats->enabled;
}

void cheat_reset(struct emu *emu, int hard)
{

	cheats_enabled = emu->config->cheats_enabled;

	if (cheats_enabled)
		cheat_enable_cheats(emu->cheats, emu->config->cheats_enabled);
}

int add_cheat(struct cheat_state *cheats, char *code)
{
	struct cheat *cheat;
	int addr;
	int compare;
	int value;


	compare = -1;

	if (parse_raw_code(code, &addr, &value, &compare) != 0) {
		if (parse_game_genie_code(code, &addr, &value,
					  &compare) != 0) {
			if (parse_pro_action_rocky_code(code, &addr, &value,
							&compare) != 0) {
				return 1;
			}
		}
	}

	cheat = malloc(sizeof(*cheat));

	if (!cheat)
		return 1;

	memset(cheat, 0, sizeof(*cheat));

	cheat->address = addr;
	cheat->compare = compare;
	cheat->value = value;
	cheat->enabled = 1;

	insert_cheat(cheats, cheat);

	return 0;
}

void remove_cheat(struct cheat_state *cheats, struct cheat *cheat)
{
	if (!cheats || !cheat)
		return;

	if (cheat->prev == NULL)
		cheats->cheat_list = cheat->next;
	else
		cheat->prev->next = cheat->next;

	if (cheat->next)
		cheat->next->prev = cheat->prev;

	cpu_set_read_handler(cheats->emu->cpu, cheat->address, 1, 0,
			     cheat->orig_read_handler);

	free_cheat(cheat);
}
