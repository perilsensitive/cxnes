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
		while (p->next)
			p = p->next;

		p->next = cheat;
	}

	cheat->next = NULL;
}

int parse_raw_code(const char *code, int *a, int *v, int *c)
{
	int addr, value, compare;
	char *end;

	errno = 0;

	addr = strtoul(code, &end, 16);
	if (errno || !*end || (*end != ':') || (addr > 0xffff))
		return 1;
	*a = addr;

	value = strtoul(end + 1, &end, 16);
	if (errno || (*end && *end != ':') || (value > 0xff))
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

void decode_game_genie(const char *string, int *a, int *v, int *c)
{
	uint8_t code[8];
	int addr;
	int value, compare;
	int length;
	int i;

	length = strlen(string);

	compare = -1;

	for (i = 0; i < length; i++) {
		switch (toupper(string[i])) {
		case 'A': code[i] = 0x0; break;
		case 'P': code[i] = 0x1; break;
		case 'Z': code[i] = 0x2; break;
		case 'L': code[i] = 0x3; break;
		case 'G': code[i] = 0x4; break;
		case 'I': code[i] = 0x5; break;
		case 'T': code[i] = 0x6; break;
		case 'Y': code[i] = 0x7; break;
		case 'E': code[i] = 0x8; break;
		case 'O': code[i] = 0x9; break;
		case 'X': code[i] = 0xa; break;
		case 'U': code[i] = 0xb; break;
		case 'K': code[i] = 0xc; break;
		case 'S': code[i] = 0xd; break;
		case 'V': code[i] = 0xe; break;
		case 'N': code[i] = 0xf; break;
		}
	}

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
}

int parse_game_genie_code(const char *code, int *a, int *v, int *c)
{
	int length;
	int i, j;

	if (!code || !*code)
		return 0;

	length = strlen(code);

	if ((length != 6) && (length != 8))
		return 1;

	for (i = 0; i < length; i++) {
		for (j = 0; j < 16; j++) {
			if (toupper(code[i]) == game_genie_alphabet[j])
				break;
		}

		if (j == 16)
			break;
	}

	if (i < length)
		return 1;

	decode_game_genie(code, a, v, c);
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
	char *raw, *gg, *rocky, *description;
	/* char *code; */
	int raw_start, raw_end;
	int gg_start, gg_end;
	int rocky_start, rocky_end;
	int enabled;
	int description_start;
	int addr, value, compare;

	cheats = data;

	rocky_end = 0;
	raw_end = 0;
	gg_end = 0;
	description_start = 0;
	enabled = -1;
	sscanf(line, " %n%*s%n %n%*s%n %n%*s%n %d %n%*s \n",
	       &raw_start, &raw_end, &gg_start, &gg_end,
	       &rocky_start, &rocky_end,
	       &enabled, &description_start);

	if ((!gg_end || !rocky_end || !raw_end || !description_start || (enabled < 0))) {
		log_err("error parsing line %d\n", num);
		return;
	}

	line[raw_end] = '\0';
	line[gg_end] = '\0';
	line[rocky_end] = '\0';

	raw = line + raw_start;
	gg = line + gg_start;
	rocky = line + rocky_start;
	description = line + description_start;

	/* If a raw code is present and valid, it is used and
	   the appropriate Game Genie or Pro Action Rocky code
	   is generated from it.

	   If a valid raw code is not present but a Game Genie
	   one is, it will be used and the raw and Pro Action
	   Rocky codes generated from it instead.

	   Finally, if only a Pro Action Rocky code  is available,
	   it will be decoded and used to generate the other two.

	   All three codes are present in the file mostly for
	   informational purposes, and possibly as a sanity check.
	   The emulator uses only the raw format internally, but
	   retains the others for the user's convenience.

	   If a code is not available in a particular format, or
	   the code cannot be expressed in that format, a dash
	   ('-') character must be used in place of a code for
	   that field.

	   The enabled field should be '0' if the code is not
	   enabled, '1' if it is.

	   The description can be any sequence of characters
	   except newline.  Leading and trailing whitespace is
	   removed, but all whitespace inside the string is
	   left alone.  The description may be omitted.
	*/
	compare = -1;
	/* code = raw; */
	if (parse_raw_code(raw, &addr, &value, &compare) != 0) {
		/* code = gg; */
		if (parse_game_genie_code(gg, &addr, &value, &compare) != 0) {
			/* code = rocky; */
			if (parse_pro_action_rocky_code(rocky, &addr, &value,
							&compare) != 0) {

				return;
			}
		}

	}

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

/*
 * Cheat file format:
 *
 * Blank lines and lines starting with '#' ignored
 * Witespace-separated fields:
 * raw gamegenie rocky enabled description
 * 
 * Description may contain spaces, no quoting necessary because
 * it's the last "field". It may also be omitted entirely.
 *
 * Raw codes must be in format addr:value or addr:value:compare.  If
 * 'compare' present, value stored at addr must be equal to compare
 * for 'value' to be returned.  Otherwise, the unmodified data is
 * returned.  All three of the above must be in hexadecimal format, and
 * may be preceeded by the prefix '0x'.
 *
 * Game Genie codes must be either 6 or 8 characters long and consist only
 * of the letters APZLGITYEOXUKSVN (either upper- or lower-case is fine).
 *
 * Pro Action Rocky codes are 32-bit integers in hexidecimal format. They
 * may use upper or lower case digits and may be prefixed with '0x'.
 *
 * The 'enabled' flag may be set to 0 to indicate that the code is not enabled
 * by default, or any non-zero value to indicate that it is enabled by default.
 */
int cheat_load_file(struct emu *emu, const char *filename)
{
	printf("filename: %s\n", filename);
	return process_file(filename, emu->cheats,
			    cheat_callback, 1, 1);
}

int cheat_save_file(struct emu *emu, const char *filename)
{
	struct cheat *cheat;
	char gg[9];
	char rocky[9];
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
		/* raw code: address:value (4:2), 1 for : and 1 for ' ' */
		size += 4 + 2 + 1 + 1;
		if (cheat->compare >= 0)
			size += 2 + 1; /* compare value (2) and ':' */

		/* gg code: 6 + 1 */
		if (cheat->address >= 0x8000) {
			size += 6 + 1;
			if (cheat->compare >= 0)
				size += 2; /* two extra letters for compare */
		} else {
			size += 1 + 1; /* '-' plus space */
		}

		if (cheat->address >= 0x8000 && cheat->compare >= 0) {
			/* rocky code: 8 + 1 */
			size += 8 + 1;
		} else {
			size += 1 + 1; /* '-' plus space */
		}

		/* enabled: 1*/
		size += 1;

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
		raw_to_game_genie(cheat->address, cheat->value,
				 cheat->compare, gg);
		raw_to_pro_action_rocky(cheat->address, cheat->value,
					cheat->compare, rocky);

		if (!gg[0]) {
			gg[0] = '-';
			gg[1] = '\0';
		}

		if (!rocky[0]) {
			rocky[0] = '-';
			rocky[1] = '\0';
		}

		rc = snprintf(p, remaining, "%04X:%02X",
			      cheat->address, cheat->value);
		p += rc;
		remaining -= rc;

		if (cheat->compare >= 0) {
			rc = snprintf(p, remaining, ":%02X", cheat->compare);
			p += rc;
			remaining -= rc;
		}

		rc = snprintf(p, remaining, " %s %s %d", gg, rocky,
			      cheat->enabled ? 1 : 0);

		p += rc;
		remaining -= rc;

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
				printf("returning\n");
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
