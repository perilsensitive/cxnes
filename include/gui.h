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

#ifndef __GUI_H__
#define __GUI_H__

void gui_resize(int fs);
void gui_grab(int grab);
void gui_toggle_menubar(void);
extern void *gui_init(void);
void gui_cleanup(void);
extern void gui_show_cursor(int show);
extern void gui_show_crosshairs(int show);
extern void gui_set_window_title(const char *title);
extern void gui_set_size(int w, int h);

#endif /* __GUI_H__ */
