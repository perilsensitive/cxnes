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

#include "SDL.h"
#include "emu.h"
#include <stdio.h>

#if GUI_ENABLED
extern void gui_error(const char *);
extern int gui_enabled;
#endif

/* Taken from SDL2's SDL_log.c */
static const char *priority_prefixes[SDL_NUM_LOG_PRIORITIES] = {
    NULL,
    "VERBOSE",
    "DEBUG",
    "INFO",
    "WARN",
    "ERROR",
    "CRITICAL"
};

#if _WIN32
static void console_output_log(void *userdata, int category, SDL_LogPriority priority,
                               const char *message)
{
	const char *priority_string;

	priority_string = priority_prefixes[priority];

	fprintf(stderr, "%s: %s\n", priority_string, message);
}
#endif

void log_set_loglevel(int priority)
{
	/* Our lowest priority is debug, but otherwise we match up to SDL */
	priority += SDL_LOG_PRIORITY_DEBUG;

	SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, priority);
}


void log_message(int priority, const char *fmt, ...)
{
	va_list args;

	/* Our lowest priority is debug, but otherwise we match up to SDL */
	priority += SDL_LOG_PRIORITY_DEBUG;

	va_start(args, fmt);

	SDL_LogMessageV(SDL_LOG_CATEGORY_APPLICATION, priority, fmt, args);

	va_end(args);
}

void err_message(const char*fmt, ...)
{
	char buffer[256];
	va_list args;

	va_start(args, fmt);
	vsnprintf(buffer, sizeof(buffer), fmt, args);

	va_end(args);
#if GUI_ENABLED
	if (gui_enabled) {
		gui_error(buffer);
	}
#endif

	log_message(SDL_LOG_PRIORITY_ERROR, buffer);
}

void log_init(void)
{
#if _WIN32
	SDL_LogSetOutputFunction(console_output_log, NULL);
#endif
}

void log_apply_config(struct emu *emu)
{
	const char *priority;
	int log_priority;
	int i;

	priority = emu->config->log_priority;
	log_priority = LOG_PRIORITY_INFO;

	for (i = 2; priority && (i < LOG_PRIORITY_NUM_PRIORITIES + 2); i++) {
		if (strcasecmp(priority, priority_prefixes[i]) == 0) {
			log_priority = i - 2;
			break;
		}
	}

	log_set_loglevel(log_priority);
}
