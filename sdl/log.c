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

#include "SDL.h"
#include <stdio.h>

#if GUI_ENABLED
extern void gui_error(const char *);
extern int gui_enabled;
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
