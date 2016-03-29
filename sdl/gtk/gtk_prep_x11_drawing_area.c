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
#include <gdk/gdkx.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <X11/Xlib.h>

static GLint glxattributes[] = {
	GLX_RGBA,
	GLX_RED_SIZE, 3,
	GLX_GREEN_SIZE, 3,
	GLX_BLUE_SIZE, 2,
	GLX_DOUBLEBUFFER,
	GLX_DEPTH_SIZE, 16,
	None
};

int gui_prep_drawing_area(GtkWidget *drawingarea)
{
	GdkScreen *screen;
	int xscreen;
	XVisualInfo *vinfo = NULL;
	GdkVisual *gdkvisual;
	Display *xdisplay;

	screen = gdk_screen_get_default();
	xdisplay = gdk_x11_get_default_xdisplay();
	xscreen = DefaultScreen(xdisplay);
	vinfo = glXChooseVisual(xdisplay, xscreen, glxattributes);

	if (!vinfo)
		return -1;

	gdkvisual = gdk_x11_screen_lookup_visual(screen, vinfo->visualid);
	XFree(vinfo);

	if (!gdkvisual)
		return -1;

	gtk_widget_set_visual(drawingarea, gdkvisual);

	return 0;
}

void *gui_get_window_handle(GdkWindow *gdkwindow)
{
	return (void *)gdk_x11_window_get_xid(gdkwindow);
}
