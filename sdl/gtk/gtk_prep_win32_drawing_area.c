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

#include <gtk/gtk.h>
#define WIN32_LEAN_AND_MEAN
#include <gdk/gdkwin32.h>

int gui_prep_drawing_area(GtkWidget *drawingarea)
{
	if (drawingarea)
		return 0;
	else
		return -1;
}

void *gui_get_window_handle(GdkWindow *gdkwindow)
{
	return (void *)gdk_win32_window_get_handle(gdkwindow);
}
