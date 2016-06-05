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

struct priority_info {
	enum log_priority priority;
	SDL_LogPriority sdl_priority;
	const char *name;
};

static struct priority_info priority_info[] = {
	{ LOG_PRIORITY_DEBUG, SDL_LOG_PRIORITY_DEBUG, "DEBUG" },
	{ LOG_PRIORITY_INFO, SDL_LOG_PRIORITY_INFO, "INFO" },
	{ LOG_PRIORITY_WARN, SDL_LOG_PRIORITY_WARN, "WARN" },
	{ LOG_PRIORITY_ERROR, SDL_LOG_PRIORITY_ERROR, "ERROR" },
	{ LOG_PRIORITY_CRITICAL, SDL_LOG_PRIORITY_CRITICAL, "CRITICAL" },
};

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
static void console_output(void *userdata, int category, SDL_LogPriority priority,
                           const char *message)
{
	const char *priority_string;

	priority_string = priority_prefixes[priority];

	fprintf(stderr, "%s: %s\n", priority_string, message);
}
#endif

void log_set_loglevel(enum log_priority priority)
{
	SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION,
	                   priority_info[priority].sdl_priority);
}


void log_message(enum log_priority priority, const char *fmt, ...)
{
	SDL_LogPriority sdl_priority;
	va_list args;

	sdl_priority = priority_info[priority].sdl_priority;

	va_start(args, fmt);

	SDL_LogMessageV(SDL_LOG_CATEGORY_APPLICATION, sdl_priority,
	                fmt, args);

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
	SDL_LogSetOutputFunction(console_output, NULL);
#endif
}

void log_apply_config(struct emu *emu)
{
	const char *priority;
	enum log_priority log_priority;
	int i;

	priority = emu->config->log_priority;
	if (!priority) {
		log_set_loglevel(LOG_PRIORITY_INFO);
		return;
	}

	log_priority = LOG_PRIORITY_INFO;

	for (i = LOG_PRIORITY_DEBUG; i < LOG_PRIORITY_NUM_PRIORITIES; i++) {
		if (strcasecmp(priority, priority_info[i].name) == 0) {
			log_priority = i;
			break;
		}
	}

	log_set_loglevel(log_priority);
}
