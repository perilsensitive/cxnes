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

#ifndef __LOG_H__
#define __LOG_H__

enum {
	LOG_PRIORITY_DEBUG,
	LOG_PRIORITY_INFO,
	LOG_PRIORITY_WARN,
	LOG_PRIORITY_ERROR,
	LOG_PRIORITY_CRITICAL
};

#define log_err(fmt, ...) log_message(LOG_PRIORITY_ERROR, \
                                      fmt, ##__VA_ARGS__)

#define log_dbg(fmt, ...) log_message(LOG_PRIORITY_DEBUG, \
                                      fmt, ##__VA_ARGS__)

#define log_warn(fmt, ...) log_message(LOG_PRIORITY_WARN, \
				       fmt, ##__VA_ARGS__)

#define log_info(fmt, ...) log_message(LOG_PRIORITY_INFO, \
				       fmt, ##__VA_ARGS__)

extern void log_message(int priority, const char *fmt, ...);
extern void log_set_loglevel(int priority);
extern void err_message(const char *fmt, ...);

#endif
