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

#ifndef __VIDEO_H__
#define __VIDEO_H__

#include "emu.h"

int video_init(struct emu *emu);
int video_init_testing(struct emu *emu);
int video_shutdown(void);
int video_shutdown_testing(void);
uint32_t *video_get_back_buffer(void);
int video_draw_buffer(void);
void video_toggle_fullscreen(int);
void video_toggle_fps(int);
int video_wait_for_frame(void);
void video_frame_done(void);
void video_enable_crosshairs(int enable);
void video_show_cursor(int show);
void video_calc_nes_coord(int *x, int *y);
uint32_t video_get_pixel_color(int, int);
void video_window_minimized(int);
void video_set_mouse_grab(int grab);
void video_warp_mouse(int x, int y);
int video_grab_screenshot(const char *filename);
void video_set_window_title(const char *title);
void video_update_fps_display(void);
int video_save_screenshot(const char *filename);
void video_update_texture(void);
int video_apply_config(struct emu *emu);
void video_set_window_title(const char *title);
void video_clear(void);

#if GUI_ENABLED
void video_resize(int w, int h);
void video_redraw(void);
void video_focus(int focused);
void video_mouse_button(int button, int state, int x, int y);
void video_mouse_motion(int x, int y, int state);
void video_hidden(void);
void video_minimized(void);
void video_maximized(void);
void video_restored(void);
void video_get_windowed_size(int *w, int *h);
void video_set_scaling_factor(int factor);
#endif

#endif				/* __VIDEO_H__ */
