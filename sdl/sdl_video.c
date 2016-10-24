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

#if _WIN32
#define _WIN32_WINNT 0x0601
#endif

#include <errno.h>
#include <unistd.h>
#include <glew/glew.h>
#if _WIN32
#include <glew/wglew.h>
#endif
#include <SDL.h>
#include <SDL_opengl.h>
#include <SDL_syswm.h>
#include <SDL_thread.h>
#include <SDL_image.h>
#include <SDL_ttf.h>
#include <time.h>
#include <sys/time.h>
#include <math.h>
#ifdef __unix__
#include <signal.h>
#endif

#if _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "emu.h"
#include "file_io.h"
#include "input.h"
#include "palette.h"
#include "nes_ntsc.h"
#include "scalebit.h"
#include "hqx.h"
#if GUI_ENABLED
#include "gui.h"
#endif

#define MAX_OSD_MSG_LEN 65

#define NTSC_WIDTH_STRETCH  8.0/7.0
#define NTSC_HEIGHT_STRETCH 1
#define PAL_WIDTH_STRETCH   1.3862
#define PAL_HEIGHT_STRETCH  1

#define NES_WIDTH  256
#define NES_HEIGHT 240

enum filter {
	FILTER_UNDEFINED,
	FILTER_NONE,
	FILTER_NTSC,
	FILTER_SCALE2X,
	FILTER_SCALE3X,
	FILTER_SCALE4X,
	FILTER_HQ2X,
	FILTER_HQ3X,
	FILTER_HQ4X,
};

struct texture {
	int w;
	int h;
	int target;
	GLfloat texw;
	GLfloat texh;
	GLuint id;
	GLuint fbo;
	SDL_Texture *sdl_texture;
};

struct SDL_Window
{
	const void *magic;
	Uint32 id;
	char *title;
	SDL_Surface *icon;
	int x, y;
	int w, h;
	int min_w, min_h;
	int max_w, max_h;
	Uint32 flags;
	Uint32 last_fullscreen_flags;
};

typedef struct SDL_Window SDL_Window;

struct palette_list_entry
{
	const char *name;
	const uint8_t *palette;
};

#if _WIN32
HANDLE disable_screensaver_request;
POWER_REQUEST_CONTEXT disable_screensaver_request_context;
#endif

static GLint position;
static GLint texture_coordinates;
static GLint texture_uniform;
static GLuint vertex_buffer, element_buffer;
static GLuint vertex_shader, fragment_shader, program;

/* FIXME use getters/setters for these? */
int display_fps = -1;
int fps_timer = 0;
int autohide_timer;
extern int total_frame_time;
extern int frame_times[60];
extern int time_base;
extern int frame_count;
extern int running;
extern int autohide_timer;
int fullscreen = -1;
int cursor_visible;
int window_minimized;
int center_x, center_y;
int mouse_grabbed;
int crosshairs_enabled;

static int using_sdl_renderer = 0;
#if GUI_ENABLED
static int device_scaling_factor = 1;
#endif
static uint32_t rgb_palette[512];
uint32_t *nes_screen;
static int nes_screen_width = 256;
static int nes_screen_height = 240;
uint32_t nes_pixel_screen[NES_WIDTH*NES_HEIGHT];
static int integer_scaling_factor;
static nes_ntsc_t ntsc;
static nes_ntsc_setup_t ntsc_setup;

static SDL_Rect scaled_rect;


/* OSD state */
static char osd_msg[MAX_OSD_MSG_LEN];
static char fps_display[32];
static TTF_Font *font;
static SDL_Surface *text_surface;
static struct texture *text_texture;
static SDL_Surface *fps_text_surface;
static struct texture *fps_text_texture;
static SDL_Rect osd_bg_dest_rect;
static SDL_Rect text_dest_rect;
static SDL_Rect text_clip_rect;
static SDL_Rect fps_bg_dest_rect;
static SDL_Rect fps_text_dest_rect;
static SDL_Rect fps_text_clip_rect;
static SDL_Color osd_fg_color;
static SDL_Color osd_bg_color;
static int osd_timer = 0;

#if GUI_ENABLED
static int mouse_lastx;
static int mouse_lasty;
#endif

static int paused_due_to_lost_focus;

/* OSD Options */
static char *osd_font_name;
static int osd_enabled;
static int osd_min_font_size;
static uint32_t osd_fg_rgba;
static uint32_t osd_bg_rgba;
static int osd_delay;

static SDL_SysWMinfo wm_info;

/* Config settings */
static int window_scaling_factor;
static int stretch_to_fit;
static char *scaling_mode;
static int left;
static int right;
static int bottom;
static int top;
static const char *aspect;
static double width_stretch_factor;
static double height_stretch_factor;
static enum filter current_filter = FILTER_UNDEFINED;
static int scanlines_enabled = -1;

/* Video state */
static SDL_Window *window;
static SDL_GLContext context = 0;
static SDL_Renderer *renderer;
static SDL_RendererInfo renderer_info;
static struct texture *nes_texture;
static struct texture *scaled_texture;
static struct texture *render_target;
static int has_target_texture;
static double current_scaling_factor;
static SDL_Rect clip_rect;
static SDL_Rect window_rect;
static SDL_Rect dest_rect;
static int view_w, view_h;

extern struct emu *emu;

#if GUI_ENABLED
extern int gui_enabled;
#endif

static const float inv255f = 1.0f / 255.0f;

/* static float const default_decoder [6] = { */
/* 	0.956f, 0.621f, -0.272f, -0.647f, -1.105f, 1.702f */
/* }; */

static float const nestopia_decoder [6] =
{
	/* 0.956084447457783, 0.620888499917131, -0.27429325219947, -0.646194407123613, -1.1056172410805, 1.70250125292921, */
	0.956f, 0.621f, -0.272f, -0.647f, -1.105f, 1.702f
};

static float const fcc_decoder [6] =
	{ 0.946882f, 0.623557f, -0.274788f, -0.635691f, -1.108545f, 1.709007f };

static float const consumer_decoder [6] =
{
	1.30832608599486, 0.849636894623442, -0.0918542466336432,
	-0.653576925369436, -1.48628965095479, 1.33826121271772,
	
};

static struct palette_list_entry palette_list[] = {
	{ "rc2c03b",                palette_rc2c03b },
	{ "rp2c03b",                palette_rp2c03b },
	{ "rp2c04",                 palette_rp2c04_master  },
	{ "fceux",                  palette_fceux  },
	{ "firebrandx_nostalgia",   palette_firebrandx_nostalgia },
	{ "firebrandx_nes_classic", palette_firebrandx_nes_classic },
	{ "yiq",                    NULL },
};

/* Sony CXA2025AS US */
/* static float const sony_decoder [6] = */
/* 	{ 1.630, 0.317, -0.378, -0.466, -1.089, 1.677 }; */

static GLfloat vertex_buffer_data[] = { 
    -1.0f, -1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f, -1.0f,
     1.0f,  1.0f, -1.0f, -1.0f,
};

static const GLushort element_buffer_data[] = { 0, 1, 2, 3 };

static char *vertex_shader_source[] = {
	"#version 110\n"
	"\n"
	"attribute vec2 position;\n"
	"attribute vec2 texture_coordinates;\n"
	"\n"
	"varying vec2 texcoord;\n"
	"\n"
	"void main()\n"
	"{\n"
	"    gl_Position = vec4(position, 0.0, 1.0);\n"
	"    texcoord = texture_coordinates;\n"
	"}\n"
};

static char *fragment_shader_source[] = {
	"#version 110\n"
	"\n"
	"uniform sampler2D texture;\n"
	"\n"
	"varying vec2 texcoord;\n"
	"\n"
	"void main()\n"
	"{\n"
	"    gl_FragColor = texture2D(texture, texcoord);\n"
	"}\n"
};

static void handle_resize_event(void);
void video_toggle_fullscreen(int fs);
void video_update_texture(void);
int video_draw_buffer(void);

static GLuint create_buffer(GLenum, const void *, GLsizei);
static GLuint make_shader(GLenum, char **);
static GLuint make_program(GLuint, GLuint);
static void render_copy(struct texture *, SDL_Rect *, SDL_Rect *);
static void set_alpha_blending(int);
static void fill_rect(SDL_Rect *);
static void set_color(SDL_Color *);
static void set_render_target(struct texture *);
static void destroy_texture(struct texture *);
static struct texture *create_texture_from_surface(SDL_Surface *);
static struct texture *create_texture(int, int, int, int);
static int power_of_2(int);

static void set_swap_interval(int interval)
{
#if _WIN32
	if (gui_enabled && WGLEW_EXT_swap_control)
		wglSwapIntervalEXT(interval);
#endif

	SDL_GL_SetSwapInterval(interval);
}

static void calc_osd_rects(void)
{
	int text_line_skip;
	int text_w, text_h;
	int bg_margin;
	int text_margin;

	if (!font)
		return;

	text_line_skip = TTF_FontLineSkip(font);
	TTF_SizeUTF8(font, osd_msg, &text_w, &text_h);

	/* Allow some room at the bottom of the screen
	   in case the monitor bezel cuts off a few
	   scanlines.
	*/
	bg_margin = 8 * current_scaling_factor;
	text_margin = floor(current_scaling_factor) * 2;

	osd_bg_dest_rect.x = dest_rect.x + bg_margin;
	osd_bg_dest_rect.w = dest_rect.w - 2 * bg_margin;
	if (text_w + 2 * text_margin < osd_bg_dest_rect.w)
		osd_bg_dest_rect.w = text_w + 2 * text_margin;

	osd_bg_dest_rect.h = text_line_skip + 2 * text_margin;
	osd_bg_dest_rect.y = dest_rect.y + dest_rect.h - osd_bg_dest_rect.h - bg_margin;

	text_dest_rect.x = osd_bg_dest_rect.x + text_margin;
	text_dest_rect.w = osd_bg_dest_rect.w - 2 * text_margin;
	text_dest_rect.h = text_line_skip;
	text_dest_rect.y = osd_bg_dest_rect.y + text_margin;

	text_clip_rect.x = 0;
	text_clip_rect.y = 0;
	text_clip_rect.w = text_dest_rect.w;
	text_clip_rect.h = text_dest_rect.h;
}

static void calc_fps_display_rects(void)
{
	int text_line_skip;
	int text_w, text_h;
	int bg_margin;
	int text_margin;

	if (!font)
		return;

	text_line_skip = TTF_FontLineSkip(font);
	TTF_SizeUTF8(font, fps_display, &text_w, &text_h);

	/* Allow some room at the bottom of the screen
	   in case the monitor bezel cuts off a few
	   scanlines.
	*/
	bg_margin = 4 * current_scaling_factor;
	text_margin = current_scaling_factor * 2;

	fps_bg_dest_rect.x = dest_rect.x + dest_rect.w -
	                     (text_w + 2 * text_margin) - bg_margin;
	fps_bg_dest_rect.w = text_w + 2 * text_margin;

	fps_bg_dest_rect.h = text_line_skip + 2 * text_margin;
	fps_bg_dest_rect.y = dest_rect.y + bg_margin;

	fps_text_dest_rect.x = fps_bg_dest_rect.x + text_margin;
	fps_text_dest_rect.w = fps_bg_dest_rect.w - 2 * text_margin;
	fps_text_dest_rect.h = text_line_skip;
	fps_text_dest_rect.y = fps_bg_dest_rect.y + text_margin;

	fps_text_clip_rect.x = 0;
	fps_text_clip_rect.y = 0;
	fps_text_clip_rect.w = fps_text_dest_rect.w;
	fps_text_clip_rect.h = fps_text_dest_rect.h;
}

static void draw_fps_display(void)
{
	int text_w, text_h;
//	int text_line_skip;
	int framerate;
	double fps;

	if (!font)
		return;

	if (fps_text_surface) {
		SDL_FreeSurface(fps_text_surface);
		fps_text_surface = NULL;
	}

	if (fps_text_texture) {
		destroy_texture(fps_text_texture);
		fps_text_texture = NULL;
	}
	

	if (emu->display_framerate > emu->user_framerate)
		framerate = emu->user_framerate;
	else
		framerate = emu->display_framerate;

	fps = (double) time_base / ((double)total_frame_time / framerate);

	snprintf(fps_display, sizeof(fps_display), "%3.4f", fps);

	TTF_SizeUTF8(font, fps_display, &text_w, &text_h);
//	text_line_skip = TTF_FontLineSkip(font);
	fps_text_surface = TTF_RenderUTF8_Blended(font, fps_display, osd_fg_color);

	if (fps_text_surface) {
		fps_text_texture = create_texture_from_surface(fps_text_surface);

		calc_fps_display_rects();
	}

}

static void draw_osd(int count)
{
	int text_w, text_h;

	if (!font)
		return;

	if (text_surface) {
		SDL_FreeSurface(text_surface);
		text_surface = NULL;
	}

	if (text_texture) {
		destroy_texture(text_texture);
		text_texture = NULL;
	}

	TTF_SizeUTF8(font, osd_msg, &text_w, &text_h);
	text_surface = TTF_RenderUTF8_Blended(font, osd_msg, osd_fg_color);

	if (text_surface) {
		text_texture = create_texture_from_surface(text_surface);

		calc_osd_rects();
		if (count > 0)
			osd_timer = count;
	}

}

static void load_osd_font(void)
{
	int multiplier;
	
	if (font)
		TTF_CloseFont(font);
	font = NULL;

	multiplier = floor(current_scaling_factor);

	if (osd_font_name) {
		font = TTF_OpenFont(osd_font_name, osd_min_font_size *
				    multiplier);
		if (!font)
			log_err("Font failed to load: %s\n",
				TTF_GetError());
	}

	if (osd_timer) {
		calc_osd_rects();
		draw_osd(osd_timer);
	}


	if (display_fps > 0) {
		calc_fps_display_rects();
		draw_fps_display();
	}
}

static void create_scaled_texture(SDL_Rect *rect)
{
	int height;
	int width;

	if (scaled_texture) {
		destroy_texture(scaled_texture);
		scaled_texture = NULL;
	}

	if (!rect || (!rect->w && !rect->h) ||
	    (!renderer && using_sdl_renderer)) {
		return;
	}

	width = rect->w;
	height = rect->h;

	scaled_texture = create_texture(width, height, 1, 1);

	if (!scaled_texture) {
		log_err("failed to create scaled texture: %s\n",
			SDL_GetError());
	}
}

static void video_apply_palette_and_filter(struct emu *emu)
{
	int ppu_type;

	if (emu->loaded)
		ppu_type = ppu_get_type(emu->ppu);
	else ppu_type = -1;

	if (ppu_type != -1) {
		const uint8_t *pal;
		const char *do_merge_fields;
		uint8_t *tmp_pal;
		int is_rgb, is_custom;
		int palette_count;
		int i;

		ppu_type = ppu_get_type(emu->ppu);
		ntsc_setup = nes_ntsc_rgb;
		tmp_pal = NULL;
		do_merge_fields = emu->config->ntsc_filter_merge_fields;

		switch (ppu_type) {
		case PPU_TYPE_RP2C04_0001:
		case PPU_TYPE_RP2C04_0002:
		case PPU_TYPE_RP2C04_0003:
		case PPU_TYPE_RP2C04_0004:
			is_rgb = 1;
			pal = palette_rp2c04_master;
			break;
		case PPU_TYPE_RP2C02:
		case PPU_TYPE_RP2C07:
		case PPU_TYPE_DENDY:
			is_rgb = 0;
			pal = NULL;
			break;
		default:
			pal = palette_rp2c03b;
			is_rgb = 1;
		}

		is_custom = 0;


		palette_count = sizeof(palette_list) / sizeof(palette_list[0]);

		for (i = 0; i < palette_count; i++) {
			if (strcasecmp(emu->config->palette,
			               palette_list[i].name) == 0) {
				pal = palette_list[i].palette;
				break;
			}
		}

		if (strcasecmp(emu->config->palette, "custom") == 0) {
			if (emu->config->palette_path)
				is_custom = 1;
		}

		if (emu->config->ntsc_filter_auto_tune) {
			if (is_rgb) {
				ntsc_setup.sharpness = 0.2;
				ntsc_setup.fringing = -1;
				ntsc_setup.artifacts = -1;
				ntsc_setup.resolution = 0.7;
				ntsc_setup.bleed = -1;
				ntsc_setup.merge_fields = 1;
			} else {
				ntsc_setup.sharpness = 0;
				ntsc_setup.fringing = 0;
				ntsc_setup.artifacts = 0;
				ntsc_setup.resolution = 0;
				ntsc_setup.bleed = 0;
				ntsc_setup.merge_fields = 0;
			}
		} else {
			ntsc_setup.sharpness = emu->config->ntsc_filter_sharpness;
			ntsc_setup.fringing = emu->config->ntsc_filter_fringing;
			ntsc_setup.artifacts = emu->config->ntsc_filter_artifacts;
			ntsc_setup.resolution = emu->config->ntsc_filter_resolution;
			ntsc_setup.bleed = emu->config->ntsc_filter_bleed;
		}

		if (strcasecmp(do_merge_fields, "auto") == 0) {
			/* Disable merged fields if vsync is enabled and possible.
			   nes_ntsc_init() will force them on if necessary, like
			   if emulating an RGB PPU. */
			
			if (emu->config->vsync &&
			    (renderer_info.flags & SDL_RENDERER_PRESENTVSYNC)) {
				ntsc_setup.merge_fields = 0;
			} else {
				ntsc_setup.merge_fields = 1;
			}
		} else if (strcasecmp(do_merge_fields, "on") == 0) {
			ntsc_setup.merge_fields = 1;
		} else {
			ntsc_setup.merge_fields = 0;
		}

		tmp_pal = malloc(512 * 3);

		if (pal || is_custom) {
			int size = -1;

			if (is_custom) {
				size = load_external_palette(tmp_pal,
							     emu->config->palette_path);
				if (size < 0)
					pal = palette_rp2c03b;
				else
					pal = NULL;
			}

			if (pal)
				memcpy(tmp_pal, pal, 64 * 3);
			pal = tmp_pal;

			if (is_rgb && size == 64)
				palette_rgb_emphasis(tmp_pal);
		}

		if (is_rgb) {
			ntsc_setup.base_palette = NULL;
			ntsc_setup.palette = pal;
		} else {
			ntsc_setup.base_palette = pal;
			ntsc_setup.palette = NULL;
		}

		ntsc_setup.use_bisqwit_palette = 1;
		if ((!ntsc_setup.base_palette && !ntsc_setup.palette) ||
		    (!is_rgb && !is_custom)) {
			/* Only apply these to generated_palettes */
			ntsc_setup.saturation = emu->config->ntsc_palette_saturation - 1;
			ntsc_setup.hue = emu->config->ntsc_palette_hue / 180;
		} else {
			ntsc_setup.saturation = 0;
			ntsc_setup.hue = 0;
		}
		ntsc_setup.contrast = emu->config->ntsc_palette_contrast * 2 - 2;
		ntsc_setup.brightness = emu->config->ntsc_palette_brightness * 2 - 2;
		ntsc_setup.gamma = emu->config->ntsc_palette_gamma;
		
		ntsc_setup.palette_out = tmp_pal;

		if (!strcasecmp(emu->config->ntsc_rgb_decoder, "consumer")) {
			ntsc_setup.decoder_matrix = consumer_decoder;
		} else if (!strcasecmp(emu->config->ntsc_rgb_decoder, "fcc")) {
			ntsc_setup.decoder_matrix = fcc_decoder;
		} else if (!strcasecmp(emu->config->ntsc_rgb_decoder,
				     "alternative")) {
			ntsc_setup.decoder_matrix = nestopia_decoder;
		}

		nes_ntsc_init(&ntsc, &ntsc_setup);

		for (i = 0; i < 512; i++) {
			rgb_palette[i]  = tmp_pal[i * 3 + 0] << 16;
			rgb_palette[i] |= tmp_pal[i * 3 + 1] << 8;
			rgb_palette[i] |= tmp_pal[i * 3 + 2];
		}

		if (tmp_pal)
			free(tmp_pal);
	}

}

static int video_create_textures(struct emu *emu)
{
	int ppu_type;
	int flags;
	enum filter new_filter;
	char *new_scaling_mode;
	int nes_screen_size;
	int multiplier;

	new_scaling_mode = (char *)emu->config->scaling_mode;
	if (strcasecmp(emu->config->video_filter, "ntsc") == 0)
		new_filter = FILTER_NTSC;
	else if (strcasecmp(emu->config->video_filter, "scale2x") == 0)
		new_filter = FILTER_SCALE2X;
	else if (strcasecmp(emu->config->video_filter, "scale3x") == 0)
		new_filter = FILTER_SCALE3X;
	else if (strcasecmp(emu->config->video_filter, "scale4x") == 0)
		new_filter = FILTER_SCALE4X;
	else if (strcasecmp(emu->config->video_filter, "hq2x") == 0)
		new_filter = FILTER_HQ2X;
	else if (strcasecmp(emu->config->video_filter, "hq3x") == 0)
		new_filter = FILTER_HQ3X;
	else if (strcasecmp(emu->config->video_filter, "hq4x") == 0)
		new_filter = FILTER_HQ4X;
	else
		new_filter = FILTER_NONE;

	ppu_type = -1;
	if (emu_loaded(emu))
		ppu_type = ppu_get_type(emu->ppu);

	if ((ppu_type == PPU_TYPE_RP2C07) || (ppu_type == PPU_TYPE_DENDY)) {
		aspect = emu->config->pal_pixel_aspect_ratio;
		left = emu->config->pal_first_pixel;
		right = emu->config->pal_last_pixel;
		top = emu->config->pal_first_scanline;
		bottom = emu->config->pal_last_scanline;
	} else {
		aspect = emu->config->ntsc_pixel_aspect_ratio;
		left = emu->config->ntsc_first_pixel;
		right = emu->config->ntsc_last_pixel;
		top = emu->config->ntsc_first_scanline;
		bottom = emu->config->ntsc_last_scanline;
	}

	clip_rect.x = left;
	clip_rect.y = top;
	clip_rect.w = right - left + 1;
	clip_rect.h = bottom - top + 1;

	view_w = clip_rect.w;
	view_h = clip_rect.h;

	width_stretch_factor = 1;
	height_stretch_factor = 1;

	nes_screen_width = 256;
	nes_screen_height = 240;
	multiplier = 1;

	scanlines_enabled = emu->config->scanlines_enabled;
	switch (new_filter) {
	case FILTER_NONE:
		break;
	case FILTER_NTSC:
		memset(nes_pixel_screen, 0, 256 * 240 * sizeof(*nes_pixel_screen));
		nes_screen_width = NES_NTSC_OUT_WIDTH(256);
		clip_rect.w = NES_NTSC_OUT_WIDTH(clip_rect.w);
		if (clip_rect.x)
			clip_rect.x = NES_NTSC_OUT_WIDTH(clip_rect.x);

		view_w *= 2;
		view_h *= 2;
		break;
	case FILTER_SCALE2X:
		multiplier = 2;
		break;
	case FILTER_SCALE3X:
		multiplier = 3;
		break;
	case FILTER_SCALE4X:
		multiplier = 4;
		break;
	case FILTER_HQ2X:
		multiplier = 2;
		break;
	case FILTER_HQ3X:
		multiplier = 3;
		break;
	case FILTER_HQ4X:
		multiplier = 4;
		break;
	default:
		break;
	}

	if (multiplier > 1) {
		nes_screen_width *= multiplier;
		nes_screen_height *= multiplier;
		clip_rect.h *= multiplier;
		clip_rect.y *= multiplier;
		clip_rect.w *= multiplier;
		clip_rect.x *= multiplier;
		view_w *= multiplier;
		view_h *= multiplier;
	}

	if (scanlines_enabled) {
		nes_screen_height *= 2;
		clip_rect.h *= 2;
		clip_rect.y *= 2;
		if (new_filter != FILTER_NTSC) {
			view_h *= 2;
			view_w *= 2;
		}
	}



	if (strcasecmp(aspect, "ntsc") == 0) {
		if (new_filter == FILTER_NTSC)
			view_w = NES_NTSC_OUT_WIDTH(right - left + 1);
		else
			width_stretch_factor = NTSC_WIDTH_STRETCH;
		height_stretch_factor = NTSC_HEIGHT_STRETCH;
	} else if (strcasecmp(aspect, "pal") == 0) {
		width_stretch_factor = PAL_WIDTH_STRETCH;
		height_stretch_factor = PAL_HEIGHT_STRETCH;
	} else {
		width_stretch_factor = 1;
		height_stretch_factor = 1;
		if (strcasecmp(aspect, "square") != 0) {
			log_err("Invalid aspect ratio \"%s\"\n",
				aspect);
		}
	}

	current_filter = new_filter;

	if (nes_screen) {
		free(nes_screen);
		nes_screen = NULL;
	}

	if (nes_texture) {
		destroy_texture(nes_texture);
		nes_texture = NULL;
	}
	nes_screen_size = nes_screen_width * nes_screen_height;

	nes_screen = malloc(nes_screen_size * sizeof(*nes_screen));
	if (!nes_screen)
		return 1;

	if (window && using_sdl_renderer && (!renderer ||
	               (!!emu->config->vsync !=
	                !!(renderer_info.flags & SDL_RENDERER_PRESENTVSYNC)))) {
		if (renderer)
			SDL_DestroyRenderer(renderer);

		flags = SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE;
		if (emu->config->vsync) {
			flags |= SDL_RENDERER_PRESENTVSYNC;
		}

		renderer = SDL_CreateRenderer(window, 0, flags);
		if (!renderer) {
			printf("video_init: SDL_CreateRenderer() failed: %s\n",
				SDL_GetError());
			return 1;
		}

		if (SDL_GetRendererInfo(renderer, &renderer_info) < 0) {
			printf("video_init: SDL_GetRendererInfo() failed: %s\n",
				SDL_GetError());
			return 1;
		}
	}

	if (renderer || !using_sdl_renderer) {
		int linear;

		if (nes_texture)
			destroy_texture(nes_texture);

		if (strcasecmp(scaling_mode, "linear") == 0)
			linear = 1;
		else
			linear = 0;

		nes_texture = create_texture(nes_screen_width, nes_screen_height,
		                             linear, 0);
		if (!nes_texture) {
			printf("video_init: SDL_CreateTexture() failed: %s\n",
				SDL_GetError());
			return 1;
		}
	}


	if (using_sdl_renderer) {
		has_target_texture = 0;
		if (renderer_info.flags & SDL_RENDERER_TARGETTEXTURE)
			has_target_texture = 1;
	}

	window_scaling_factor = emu->config->window_scaling_factor;

	if (display_fps == -1)
		display_fps = emu->config->fps_display_enabled;

	if (fullscreen < 0)
		fullscreen = emu->config->fullscreen;

	stretch_to_fit = emu->config->stretch_to_fit;
	if (scaling_mode)
		free(scaling_mode);
	scaling_mode = strdup(new_scaling_mode);

	if (strcasecmp(scaling_mode, "nearest_then_linear") != 0) {
		destroy_texture(scaled_texture);
		scaled_texture = NULL;
	} else {
		create_scaled_texture(&scaled_rect);
	}

	window_rect.w = view_w * width_stretch_factor * window_scaling_factor;
	window_rect.h = view_h * window_scaling_factor;

	return 0;
}

int video_apply_config(struct emu *emu)
{

	if (emu->config->vsync)
		set_swap_interval(1);
	else
		set_swap_interval(0);

	video_create_textures(emu);

	osd_enabled = emu->config->osd_enabled;

	osd_fg_rgba = emu->config->osd_fg_rgba;
	osd_bg_rgba = emu->config->osd_bg_rgba;

	osd_fg_color.r = (osd_fg_rgba >> 24) & 0xff;
	osd_fg_color.g = (osd_fg_rgba >> 16) & 0xff;
	osd_fg_color.b = (osd_fg_rgba >>  8) & 0xff;
	osd_fg_color.a = osd_fg_rgba & 0xff;

	osd_bg_color.r = (osd_bg_rgba >> 24) & 0xff;
	osd_bg_color.g = (osd_bg_rgba >> 16) & 0xff;
	osd_bg_color.b = (osd_bg_rgba >>  8) & 0xff;
	osd_bg_color.a = osd_bg_rgba & 0xff;

	osd_delay = emu->config->osd_delay;

	if (osd_delay < 0)
		osd_delay = 0;

	osd_min_font_size = emu->config->osd_min_font_size;
	load_osd_font();

	video_apply_palette_and_filter(emu);

#if GUI_ENABLED
	if (gui_enabled)
	{
		if (!fullscreen)
			gui_set_size(window_rect.w, window_rect.h);
		handle_resize_event();
	}
	else
#endif
	{
		SDL_SetWindowSize(window, window_rect.w, window_rect.h);
		handle_resize_event();
	}

	if (emu_paused(emu)) {
		video_update_texture();
		video_draw_buffer();
	}

	return 0;
}

void video_update_fps_display(void)
{	
	draw_fps_display();
}

static void reset_autohide_timer(void)
{
	int fps;
	
	if (!emu_loaded(emu) || emu_paused(emu)) {
		fps = 10;
	} else if ((emu->system_type == EMU_SYSTEM_TYPE_PAL_NES) ||
		   (emu->system_type == EMU_SYSTEM_TYPE_DENDY)) {
		fps = 50;
	} else {
		fps = 60;
	}
	
	autohide_timer = 3 * fps;
}

void video_show_cursor(int show)
{
	if ((crosshairs_enabled && !show))
		return;

	if (mouse_grabbed)
		show = 0;

	if (show)
		reset_autohide_timer();
	else
		autohide_timer = 0;

	if (cursor_visible == show)
		return;

	cursor_visible = show;

#if GUI_ENABLED
	if (gui_enabled) {
		gui_show_cursor(show);
		return;
	}
#endif
	SDL_ShowCursor(show ? SDL_ENABLE : SDL_DISABLE);
}

#if _WIN32
static int windows_init_for_opengl(HWND hwnd)
{
	HDC hdc = NULL;
	HGLRC hglrc, old_hglrc;
	PIXELFORMATDESCRIPTOR pfd;
	int iAttribs[64];
	int *iAttr;
	float fAttribs[1] = { 0 };
	int pixel_format;
	UINT matching;
	int rc;


	rc = -1;

	printf("window is %p\n", window);

	hdc = GetDC(hwnd);
	if (hdc == NULL)
		return -1;


	/*
	pixel_format = GetPixelFormat(hdc);
	if (!pixel_format) {
		printf("failed to get pixel format\n");
		goto error;
	}

	DescribePixelFormat(hdc, pixel_format, sizeof(pfd), &pfd);
	*/

	iAttr = &iAttribs[0];
	*iAttr++ = WGL_DRAW_TO_WINDOW_ARB;
	*iAttr++ = GL_TRUE;
	*iAttr++ = WGL_RED_BITS_ARB;
	*iAttr++ = 8;
	*iAttr++ = WGL_GREEN_BITS_ARB;
	*iAttr++ = 8;
	*iAttr++ = WGL_BLUE_BITS_ARB;
	*iAttr++ = 8;
	*iAttr++ = WGL_ALPHA_BITS_ARB;
	*iAttr++ = 8;
	*iAttr++ = WGL_DOUBLE_BUFFER_ARB;
	*iAttr++ = 1;
	*iAttr++ = WGL_ACCELERATION_ARB;
	*iAttr++ = WGL_FULL_ACCELERATION_ARB;
	*iAttr = 0;

	memset(&pfd, 0, sizeof(pfd));
	pfd.nVersion = 1;
	pfd.nSize = sizeof(pfd);
	pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL |
	              PFD_DOUBLEBUFFER;
	pfd.iLayerType = PFD_MAIN_PLANE;
	pfd.iPixelType = PFD_TYPE_RGBA;
	pfd.cRedBits = 8;
	pfd.cGreenBits = 8;
	pfd.cBlueBits = 8;
	pfd.cAlphaBits = 8;
	pfd.cColorBits = 24;

	pixel_format = ChoosePixelFormat(hdc, &pfd);

	if (!pixel_format) {
		printf("failed to choose pixel format\n");
		goto error;
	}

	if (!SetPixelFormat(hdc, pixel_format, &pfd)) {
		printf("failed to set pixel format\n");
		goto error;
	}

	hglrc = wglCreateContext(hdc);
	if (!hglrc) {
		printf("failed to create WGL context\n");
		goto error;
	}

	old_hglrc = wglGetCurrentContext();
	wglMakeCurrent(hdc, hglrc);

	if (WGLEW_ARB_pixel_format) {
		wglChoosePixelFormatARB(hdc, iAttribs, fAttribs, 1,
		                        &pixel_format, &matching);

	}

	wglMakeCurrent(hdc, old_hglrc);
	wglDeleteContext(hglrc);

	rc = 0;

error:
	ReleaseDC(hwnd, hdc);

	return rc; 
}
#endif


static int video_create_window(void)
{
	int display_index;
	uint32_t flags;
	SDL_DisplayMode display_mode;
	int rc;
#if GUI_ENABLED
	void *native_window;
#endif

	flags = 0;
#if GUI_ENABLED
	if (gui_enabled) {
		native_window = gui_init();
		window = SDL_CreateWindowFrom((void *)native_window);
#if _WIN32
		if (!using_sdl_renderer)
			windows_init_for_opengl(native_window);
#endif
		window->flags |= SDL_WINDOW_OPENGL;
		/* FIXME may need additional magic to get gl working in all cases (set visual?) */
		SDL_GL_LoadLibrary(NULL); /* FIXME check result */
	}
	else
#endif
	{
		flags = SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_MOUSE_FOCUS;

		if (!using_sdl_renderer)
			flags |= SDL_WINDOW_OPENGL;

		window = SDL_CreateWindow(PACKAGE_NAME,
					  SDL_WINDOWPOS_UNDEFINED,
					  SDL_WINDOWPOS_UNDEFINED,
					  window_rect.w, window_rect.h, flags);
	}


	if (!window) {
		log_err("video_init: SDL_CreateWindow() failed: %s\n",
			SDL_GetError());
		return 1;
	}

	if (!using_sdl_renderer) {
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, 0);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

		context = SDL_GL_CreateContext(window);
		if (!context) {
			printf("failed to create context: %s\n", SDL_GetError());
			return -1;
		}

		if (SDL_GL_MakeCurrent(window, context) < 0)
			return -1;

		rc = glewInit();
		if (rc != GLEW_OK) {
			printf("failed to initialize glew: %d\n", rc);
			return 1;
		}

#if (GUI_ENABLED && _WIN32)
		if (gui_enabled)
			windows_init_for_opengl(native_window);
#endif

		if (GLEW_EXT_framebuffer_object)
			has_target_texture = 1;
	}

	SDL_VERSION(&wm_info.version);
	SDL_GetWindowWMInfo(window, &wm_info);

	display_index = SDL_GetWindowDisplayIndex(window);

	if (SDL_GetCurrentDisplayMode(display_index, &display_mode)) {
		log_err("video_init: "
			"SDL_GetCurrentDisplayMode() failed: %s\n",
			SDL_GetError());
		return 1;
	}

	display_mode.refresh_rate = 60;
	if (SDL_SetWindowDisplayMode(window, &display_mode) < 0) {
		log_err("failed to set mode\n");
	}

	emu->display_framerate = display_mode.refresh_rate;

	video_apply_config(emu);

	return 0;
}

int video_init_testing(struct emu *emu)
{
	nes_screen_width = 256;
	nes_screen_height = 240;
	nes_screen = malloc(nes_screen_width * nes_screen_height *
			    sizeof(*nes_screen));
	current_filter = FILTER_NONE;
	video_apply_palette_and_filter(emu);
	emu->display_framerate = 60;

	return 0;
}

int video_init(struct emu *emu)
{
	cursor_visible = 1;
	display_fps = -1;
	fps_timer = 0;
	reset_autohide_timer();

   	hqxInit();

	osd_font_name = config_get_path(emu->config,
					    CONFIG_DATA_FILE_OSD_FONT,
					    NULL, -1);

	scaled_texture = NULL;

	window_minimized = 0;

	paused_due_to_lost_focus = 0;

	if (TTF_Init() < 0)
		log_err("failed to init TTF\n");

	/* Apply config to calculate rect sizes */
	video_apply_config(emu);

	video_create_window();

	vertex_buffer = create_buffer(GL_ARRAY_BUFFER, vertex_buffer_data,
	                              sizeof(vertex_buffer_data));

	element_buffer = create_buffer(GL_ARRAY_BUFFER, element_buffer_data,
	                               sizeof(element_buffer_data));

	vertex_shader = make_shader(GL_VERTEX_SHADER, vertex_shader_source);
	fragment_shader = make_shader(GL_FRAGMENT_SHADER,
	                              fragment_shader_source);

	program = make_program(vertex_shader, fragment_shader);
	if (program == 0)
		return -1;

	texture_uniform = glGetUniformLocation(program, "texture");
	position = glGetAttribLocation(program, "position");
	texture_coordinates = glGetAttribLocation(program, "texture_coordinates");

	if (fullscreen
#if GUI_ENABLED
	    && !gui_enabled
#endif
	   ) {
		video_toggle_fullscreen(1);
	}

#if _WIN32
	disable_screensaver_request_context.Version =
	    POWER_REQUEST_CONTEXT_VERSION;
	disable_screensaver_request_context.Flags =
	    POWER_REQUEST_CONTEXT_SIMPLE_STRING;
	disable_screensaver_request_context.Reason.SimpleReasonString =
	    L"emulation active";
	disable_screensaver_request =
	    PowerCreateRequest(&disable_screensaver_request_context);
#endif

	//video_show_cursor(0);

	return 0;
}

void video_enable_crosshairs(int enable)
{
	SDL_Cursor *cursor, *current;

	if (crosshairs_enabled && !enable)
		reset_autohide_timer();

	crosshairs_enabled = enable;

#if GUI_ENABLED
	if (gui_enabled) {
		gui_show_crosshairs(enable);
		return;
	}
#endif

	if (enable)
		cursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_CROSSHAIR);
	else
		cursor = SDL_GetDefaultCursor();

	if (cursor) {
		current = SDL_GetCursor();
		if (current)
			SDL_FreeCursor(current);

		SDL_ShowCursor(cursor_visible);
		SDL_SetCursor(cursor);

	}
}

static int output_pitch = sizeof(*nes_screen) * NES_NTSC_OUT_WIDTH(256);

void double_output_height( void )
{
	uint32_t const *in;
	uint32_t *out;
	int n, y;

	int intensity = emu->config->scanline_intensity;
	
	y = nes_screen_height / 2 - 1;
	in = nes_screen + y * nes_screen_width;
	out = nes_screen + y * nes_screen_width * 2;

	for (n = nes_screen_width; n > 0; n--) {
		uint32_t prev = *in;
		/* mix rgb without losing low bits */
		uint32_t mixed = (prev - (prev & 0x00010101)) >> 1;
		uint32_t scanline_out;

		*out = prev;
		scanline_out = ((mixed & 0xff0000) * intensity / 100) &
			0xff0000;
		scanline_out |=	((mixed & 0xff00) * intensity / 100) &
			0x00ff00;
		scanline_out |=
			((mixed & 0xff) * intensity / 100) & 0x0000ff;

		*(out + nes_screen_width) = scanline_out;

		in++;
		out++;
	}

	for (y = nes_screen_height / 2 - 2; y >= 0; y--) {
		uint32_t const *in = nes_screen + y * nes_screen_width;
		uint32_t *out = nes_screen + y * nes_screen_width * 2;

		for (n = nes_screen_width; n > 0; n--) {
			uint32_t prev = *in;
			uint32_t next = *(in + nes_screen_width);
			/* mix rgb without losing low bits */
			uint32_t mixed = (prev + next - ((prev ^ next) & 0x00010101)) >> 1;
			uint32_t scanline_out;

			*out = prev;
			scanline_out = ((mixed & 0xff0000) * intensity / 100) &
				0xff0000;
			scanline_out |=	((mixed & 0xff00) * intensity / 100) &
				0x00ff00;
			scanline_out |=
				((mixed & 0xff) * intensity / 100) & 0x0000ff;

			*(out + nes_screen_width) = scanline_out;
			in++;
			out++;
		}
	}
}

int video_draw_buffer(void)
{
	int i;

	SDL_RenderClear(renderer);

	if (current_filter == FILTER_NTSC) {
		int burst_phase = ppu_get_burst_phase(emu->ppu);

		if (ntsc_setup.merge_fields)
			burst_phase = 0;

		nes_ntsc_blit(&ntsc, nes_pixel_screen,
			      256, burst_phase, 256, 240,
			      nes_screen, output_pitch);
	} else if (current_filter == FILTER_NONE) {
		for (i = 0; i < 256 * 240; i++) {
			nes_screen[i] = rgb_palette[nes_pixel_screen[i]];
		}
	} else {
		for (i = 0; i < 256 * 240; i++) {
			nes_pixel_screen[i] = rgb_palette[nes_pixel_screen[i]];
		}

		if (current_filter == FILTER_SCALE2X) {
			scale(2, nes_screen, nes_screen_width * sizeof(uint32_t),
			      nes_pixel_screen, 256 * sizeof(uint32_t), 4, 256, 240);
		} else if (current_filter == FILTER_SCALE3X) {
			scale(3, nes_screen, nes_screen_width * sizeof(uint32_t),
			      nes_pixel_screen, 256 * sizeof(uint32_t), 4, 256, 240);
		} else if (current_filter == FILTER_SCALE4X) {
			scale(4, nes_screen, nes_screen_width * sizeof(uint32_t),
			      nes_pixel_screen, 256 * sizeof(uint32_t), 4, 256, 240);
		} else if (current_filter == FILTER_HQ2X) {
			hq2x_32(nes_pixel_screen, nes_screen, 256, 240);
		} else if (current_filter == FILTER_HQ3X) {
			hq3x_32(nes_pixel_screen, nes_screen, 256, 240);
		} else if (current_filter == FILTER_HQ4X) {
			hq4x_32(nes_pixel_screen, nes_screen, 256, 240);
		}
	}

	if (scanlines_enabled) {
		double_output_height();
	}

	if (has_target_texture && scaled_texture) {
		set_render_target(scaled_texture);
		render_copy(nes_texture, &clip_rect, NULL);
		set_render_target(NULL);
		render_copy(scaled_texture, NULL, &dest_rect);
	} else {
		set_render_target(NULL);
		render_copy(nes_texture, &clip_rect, &dest_rect);
	}

	if (osd_timer > 0) {
		set_alpha_blending(1);
		set_color(&osd_bg_color);
		fill_rect(&osd_bg_dest_rect);
		set_color(&osd_fg_color);
		render_copy(text_texture, &text_clip_rect,
			    &text_dest_rect);
		set_alpha_blending(0);
		osd_timer--;
	}

	if (display_fps > 0) {
		set_alpha_blending(1);
		set_color(&osd_bg_color);
		fill_rect(&fps_bg_dest_rect);
		set_color(&osd_fg_color);
		render_copy(fps_text_texture, &fps_text_clip_rect,
			    &fps_text_dest_rect);
		set_alpha_blending(0);
	}

	if (using_sdl_renderer)
		SDL_RenderPresent(renderer);
	else
		SDL_GL_SwapWindow(window);

	return 0;
}

void video_window_minimized(int min)
{
	window_minimized = min;
}

int video_shutdown_testing(void)
{
	if (nes_screen)
		free(nes_screen);

	return 0;
}

int video_shutdown(void)
{
	if (nes_texture)
		destroy_texture(nes_texture);

	if (scaled_texture)
		destroy_texture(scaled_texture);

	if (renderer)
		SDL_DestroyRenderer(renderer);

	if (TTF_WasInit()) {
		if (text_surface)
			SDL_FreeSurface(text_surface);

		if (text_texture)
			destroy_texture(text_texture);

		if (font)
			TTF_CloseFont(font);
		TTF_Quit();
	}

	if (osd_font_name)
		free(osd_font_name);

#if GUI_ENABLED
	if (gui_enabled)
		gui_cleanup();
	else
#endif	
		SDL_DestroyWindow(window);

#if _WIN32
	CloseHandle(disable_screensaver_request);
#endif

	return 0;
}

void video_clear(void)
{
	/* Reset video device */
	/* Need to do this twice if we're
	   double-buffered */
	SDL_RenderClear(renderer);
	SDL_RenderPresent(renderer);
	SDL_RenderClear(renderer);
	SDL_RenderPresent(renderer);
}

/* Update rects based on new window size */
static void handle_resize_event(void)
{
	double scaling_factor;
	int old_width, old_height;
	int winw, winh;

	if (!window)
		return;

	SDL_GetWindowSize(window, &winw, &winh);

	old_width = dest_rect.w;
	old_height = dest_rect.h;

	if (winh < winw) {
		scaling_factor = (double)winh / view_h;
		integer_scaling_factor = floor(scaling_factor);
	} else {
		scaling_factor = (double)winw / (view_w * width_stretch_factor);
		integer_scaling_factor = floor(scaling_factor);
	}

	if (stretch_to_fit || !integer_scaling_factor) {
		dest_rect.w = view_w * width_stretch_factor * scaling_factor;
		dest_rect.h = view_h * scaling_factor;
	} else {
		dest_rect.w = view_w * width_stretch_factor *
		              integer_scaling_factor;
		dest_rect.h = view_h * integer_scaling_factor;
	}


	dest_rect.x = (winw - dest_rect.w) / 2;
	dest_rect.y = (winh - dest_rect.h) / 2;

	scaled_rect.x = 0;
	scaled_rect.y = 0;

	scaled_rect.w = clip_rect.w * integer_scaling_factor;
	scaled_rect.h = view_h * integer_scaling_factor;

	current_scaling_factor = scaling_factor;

	center_x = dest_rect.x + dest_rect.w / 2;
	center_y = dest_rect.y + dest_rect.h / 2;

	if (scaled_texture && (dest_rect.w == old_width) &&
	    (dest_rect.h == old_height)) {
		return;
	}

	/* Reset video device */
	/* Need to do this twice if we're
	   double-buffered */
	if (scaled_texture) {
		/* Make sure to clear the memory used by the
		   target texture as well.
		*/
		set_render_target(scaled_texture);
		video_clear();
		set_render_target(NULL);
	}

	/* Yes, calling this twice is intentional. For some
	   reason sometimes once isn't enough with some
	   hardware and in some configurations.  Not really
	   sure why. :-/
	*/
	video_clear();
	video_clear();

	if (!has_target_texture ||
	    strcasecmp(scaling_mode, "nearest_then_linear")) {
		return;
	}

	/* FIXME re-load OSD font */
	load_osd_font();

	if (scaled_texture) {
		destroy_texture(scaled_texture);
		scaled_texture = NULL;
	}

	create_scaled_texture(&scaled_rect);
}

void video_toggle_fps(int enabled)
{
	if (enabled < 0)
		enabled = !(display_fps > 0);

	display_fps = enabled;
	if (enabled)
		fps_timer = 0;
}

void video_toggle_fullscreen(int fs)
{
	int flags;

#if GUI_ENABLED
	if (gui_enabled) {
		gui_resize(fs);
		return;
	}
#endif

	flags = SDL_GetWindowFlags(window);

	if (fs < 0)
		flags ^= SDL_WINDOW_FULLSCREEN_DESKTOP;
	else if (fs)
		flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
	else
		flags &= ~SDL_WINDOW_FULLSCREEN_DESKTOP;

	fullscreen = flags & SDL_WINDOW_FULLSCREEN_DESKTOP;

	SDL_SetWindowFullscreen(window, flags);
}

uint32_t video_get_pixel_color(int x, int y)
{
	uint32_t result;
	if ((x < 0 || x > 255) || (y < 0 || y > 239))
		return 0;

	result = nes_pixel_screen[y * 256 + x];

	return result;
}

void video_calc_nes_coord(int *x, int *y)
{
	double h_scale, v_scale;
	int new_x, new_y;

	new_x = *x - dest_rect.x;
	new_y = *y - dest_rect.y;
	h_scale = (double)dest_rect.w / clip_rect.w;
	v_scale = (double)dest_rect.h / clip_rect.h;
	new_x /= h_scale;
	new_y /= v_scale;
	new_x += clip_rect.x;
	new_y += clip_rect.y;

	h_scale = nes_screen_width / 256.0;
	v_scale = nes_screen_height / 240.0;

	new_x /= h_scale;
	new_y /= v_scale;

	*x = new_x;
	*y = new_y;
}

void video_update_texture(void)
{
	int width;

	if (current_filter == FILTER_NTSC)
		width = NES_NTSC_OUT_WIDTH(NES_WIDTH);
	else if (current_filter == FILTER_SCALE2X)
		width = NES_WIDTH * 2;
	else if (current_filter == FILTER_SCALE3X)
		width = NES_WIDTH * 3;
	else if (current_filter == FILTER_SCALE4X)
		width = NES_WIDTH * 4;
	else if (current_filter == FILTER_HQ2X)
		width = NES_WIDTH * 2;
	else if (current_filter == FILTER_HQ3X)
		width = NES_WIDTH * 3;
	else if (current_filter == FILTER_HQ4X)
		width = NES_WIDTH * 4;
	else
		width = NES_WIDTH;
	
	if (using_sdl_renderer) {
		SDL_UpdateTexture(nes_texture->sdl_texture, NULL,
		                  nes_screen, width * sizeof(*nes_screen));

		return;
	}

	/* FIXME what do I do with width here? */

	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, nes_texture->id);
	/* glPixelStorei(GL_UNPACK_ALIGNMENT, 1); */
	/* glPixelStorei(GL_UNPACK_ROW_LENGTH, nes_texture->w); */
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
			nes_texture->w,	nes_texture->h,
			GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV,
			nes_screen);
	glDisable(GL_TEXTURE_2D);
}


void video_set_mouse_grab(int grab)
{
	if (grab < 0)
		grab = !mouse_grabbed;

	mouse_grabbed = grab;

#if GUI_ENABLED
	if (gui_enabled) {
		gui_grab(grab);
	}
#endif

	osdprintf("Mouse %s", grab ? "grabbed" : "released");
}

int video_process_event(SDL_Event *event)
{
	int do_pause;
	if (event->type != SDL_WINDOWEVENT)
		return 0;

	switch (event->window.event) {
	case SDL_WINDOWEVENT_EXPOSED:
		if (emu_paused(emu)) {
			video_update_texture();
			video_draw_buffer();
		}
		break;
	case SDL_WINDOWEVENT_MINIMIZED:
		window_minimized = 1;
		break;
	case SDL_WINDOWEVENT_MAXIMIZED:
	case SDL_WINDOWEVENT_RESTORED:
		window_minimized = 0;
		break;
	case SDL_WINDOWEVENT_FOCUS_GAINED:
		input_ignore_events(0);
		if (emu_loaded(emu)) {
			do_pause = 0;
			if (board_get_type(emu->board) == BOARD_TYPE_NSF) {
				if (!emu->config->nsf_run_in_background)
					do_pause = 1;
			} else if (!emu->config->run_in_background) {
				do_pause = 1;
			}
			if (do_pause && paused_due_to_lost_focus) {
				paused_due_to_lost_focus = 0;
				emu_pause(emu, 0);
			}
		}
		break;
	case SDL_WINDOWEVENT_FOCUS_LOST:
		input_release_all();
		input_ignore_events(1);
		if (emu_loaded(emu)) {
			do_pause = 0;
			if (board_get_type(emu->board) == BOARD_TYPE_NSF) {
				if (!emu->config->nsf_run_in_background)
					do_pause = 1;
			} else if (!emu->config->run_in_background) {
				do_pause = 1;
			}

			if (do_pause && !emu_paused(emu)) {
				paused_due_to_lost_focus = 1;
				emu_pause(emu, 1);
			}
		}
		break;
	case SDL_WINDOWEVENT_SHOWN:
	case SDL_WINDOWEVENT_SIZE_CHANGED:
	case SDL_WINDOWEVENT_RESIZED:
		handle_resize_event();
		video_clear();
		if (emu_paused(emu)) {
			video_update_texture();
			video_draw_buffer();
		}
		break;
	case SDL_WINDOWEVENT_CLOSE:
		running = 0;
		break;
	}

	return 1;
}

int osdprintf(const char *format, ...)
{
	va_list args;
	char *t;
	int rc;
	
	va_start(args, format);
	rc = vsnprintf(osd_msg, sizeof(osd_msg), format, args);
	t = strrchr(osd_msg, '\n');
	if (t) {
		*t = 0;
	}
	t = strrchr(osd_msg, '\r');
	if (t) {
		*t = 0;
	}
	va_end(args);

	draw_osd(osd_delay * emu->display_framerate);
	return rc;
}

void video_warp_mouse(int x, int y)
{
	SDL_WarpMouseInWindow(window, x, y);
}

void video_set_window_title(const char *title)
{
	int length;
	char *tmp;
	const char *format;

	length = strlen(PACKAGE_NAME);

	if (title) {
		length += 3;
		length += strlen(title);
		format = "%s - %s";
	} else {
		format = "%s";
	}
	length ++;

	tmp = malloc(length);
	if (!tmp)
		return;

	snprintf(tmp, length, format, PACKAGE_NAME, title);

#if GUI_ENABLED
	if (gui_enabled) {
		gui_set_window_title(tmp);
		free(tmp);
		return;
	}
#endif
	SDL_SetWindowTitle(window, tmp);
	free(tmp);
}

#if GUI_ENABLED
void video_resize(int w, int h)
{
	if (window)
		SDL_SetWindowSize(window, w * device_scaling_factor,
				  h * device_scaling_factor);
}

/* Force a redraw of the current buffer.  Useful if
   the emu is paused and something partially covering
   the emulator window (like a menu) is removed.
*/
void video_redraw(void)
{
	if (!renderer && !context)
		return;

	if (using_sdl_renderer)
		SDL_RenderPresent(renderer);
	else
		SDL_GL_SwapWindow(window);

}

void video_focus(int focused)
{
	SDL_Event focus_event;

	if (!window)
		return;

	focus_event.type = SDL_WINDOWEVENT;
	focus_event.window.windowID = SDL_GetWindowID(window);
	
	if (focused)
		focus_event.window.event = SDL_WINDOWEVENT_FOCUS_GAINED;
	else
		focus_event.window.event = SDL_WINDOWEVENT_FOCUS_LOST;


	SDL_PushEvent(&focus_event);
}

void video_mouse_button(int button, int state, int x, int y)
{
	SDL_Event sdlevent;

	if (!window)
		return;

	if (state) {
		sdlevent.type = SDL_MOUSEBUTTONDOWN;
		sdlevent.button.state = SDL_PRESSED;
	} else {
		sdlevent.type = SDL_MOUSEBUTTONUP;
		sdlevent.button.state = SDL_RELEASED;
	}

	sdlevent.button.which = 0;
	sdlevent.button.windowID = SDL_GetWindowID(window);
	sdlevent.button.x = x * device_scaling_factor;
	sdlevent.button.y = y * device_scaling_factor;
	sdlevent.button.button = button;
	sdlevent.button.clicks = 1;

	video_show_cursor(1);

	SDL_PushEvent(&sdlevent);
}

void video_mouse_motion(int x, int y, int button_state)
{
	SDL_Event sdlevent;

	if (!window)
		return;

	sdlevent.type = SDL_MOUSEMOTION;
	sdlevent.motion.which = 0;
	sdlevent.button.windowID = SDL_GetWindowID(window);
	sdlevent.motion.x = x * device_scaling_factor;
	sdlevent.motion.y = y * device_scaling_factor;
	sdlevent.motion.state = button_state;
	sdlevent.motion.xrel = (x - mouse_lastx) * device_scaling_factor;
	sdlevent.motion.yrel = (y - mouse_lasty) * device_scaling_factor;
	mouse_lastx = x;
	mouse_lasty = y;

	SDL_PushEvent(&sdlevent);
}

static void video_state_event(SDL_WindowEventID event)
{
	SDL_Event sdl_event;

	if (!window)
		return;

	sdl_event.type = SDL_WINDOWEVENT;
	sdl_event.window.windowID = SDL_GetWindowID(window);
	sdl_event.window.event = event;

	SDL_PushEvent(&sdl_event);
}

void video_hidden(void)
{
	video_state_event(SDL_WINDOWEVENT_HIDDEN);
}

void video_minimized(void)
{
	video_state_event(SDL_WINDOWEVENT_MINIMIZED);
}

void video_maximized(void)
{
	video_state_event(SDL_WINDOWEVENT_MAXIMIZED);
}

void video_restored(void)
{
	video_state_event(SDL_WINDOWEVENT_RESTORED);
}

void video_set_scaling_factor(int factor)
{
	device_scaling_factor = factor;
}

void video_get_windowed_size(int *w, int *h)
{
	*w = window_rect.w;
	*h = window_rect.h;
}
#endif

int video_save_screenshot(const char *filename)
{
	SDL_Surface *screen;
	int rc;

	rc = 0;
	screen = SDL_CreateRGBSurface(0, nes_screen_width,
				      240, 32,
				      0x00ff0000,
				      0x0000ff00,
				      0x000000ff,
				      0x00000000);

	if (!screen) {
		printf("failed to create surface: %s\n", SDL_GetError());
		return -1;
	}

	if (current_filter == FILTER_NTSC) {
		int burst_phase = ppu_get_burst_phase(emu->ppu);
		int pitch = 4 * NES_NTSC_OUT_WIDTH(256);

		if (ntsc_setup.merge_fields)
			burst_phase = 0;

		nes_ntsc_blit(&ntsc, nes_pixel_screen,
			      256, burst_phase, 256, 240,
			      (uint32_t *)screen->pixels, pitch);
	} else if (current_filter == FILTER_SCALE2X) {
		scale(2, screen->pixels, 256 * 4 * 2, nes_pixel_screen, 256 * 4,
		      4, 256, 240);
	} else if (current_filter == FILTER_SCALE3X) {
		scale(3, screen->pixels, 256 * 4 * 2, nes_pixel_screen, 256 * 4,
		      4, 256, 240);
	} else if (current_filter == FILTER_SCALE4X) {
		scale(4, screen->pixels, 256 * 4 * 2, nes_pixel_screen, 256 * 4,
		      4, 256, 240);
	} else {
		int i;

		for (i = 0; i < 256 * 240; i++) {
			((uint32_t *)screen->pixels)[i] = rgb_palette[nes_pixel_screen[i]];
		}
	}

	

	if (IMG_SavePNG(screen, filename) < 0) {
		err_message("%s", SDL_GetError());
		rc = -1;
	}

	SDL_FreeSurface(screen);

	return rc;
}

void video_set_screensaver_enabled(int enabled)
{
	if (enabled) {
		SDL_EnableScreenSaver();
#if _WIN32
		PowerClearRequest(disable_screensaver_request,
		                  PowerRequestDisplayRequired);
#endif
	} else {
		SDL_DisableScreenSaver();
#if _WIN32
		PowerSetRequest(disable_screensaver_request,
		                PowerRequestDisplayRequired);
#endif
	}
}

static struct texture *create_texture(int width, int height,
				      int linear, int target)
{
	struct texture *texture;
	GLenum scale_mode;
	int texture_h;
	int texture_w;

	texture = malloc(sizeof(*texture));
	if (!texture)
		return NULL;

	memset(texture, 0, sizeof(*texture));

	if (using_sdl_renderer) {
		int target_type;

		if (target)
			target_type = SDL_TEXTUREACCESS_TARGET;
		else
			target_type = SDL_TEXTUREACCESS_STREAMING;

		if (linear)
			SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
		else
			SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");

		texture->sdl_texture =
			SDL_CreateTexture(renderer,
					  SDL_PIXELFORMAT_ARGB8888,
					  target_type, width, height);
		return texture;
	}

	if (target && has_target_texture) {
		texture->target = 1;
		glGenFramebuffers(1, &texture->fbo);
	} else {
		texture->target = 0;
	}

	glGenTextures(1, &texture->id);
	/* FIXME error checking */

	texture->w = width;
	texture->h = height;
	texture_w = power_of_2(texture->w);
	texture_h = power_of_2(texture->h);
	texture->texw = (GLfloat) (texture->w) / texture_w;
	texture->texh = (GLfloat) (texture->h) / texture_h;

	if (linear) {
		scale_mode = GL_LINEAR;
	} else {
		scale_mode = GL_NEAREST;
	}

	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, texture->id);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, scale_mode);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, scale_mode);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	/* FIXME bunch of mac os specific stuff here in sdl */

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, texture_w, texture_h, 0,
		     GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);

	glDisable(GL_TEXTURE_2D);

	return texture;
}

static GLuint create_buffer(GLenum target, const void *data, GLsizei size)
{
	GLuint buffer;
	glGenBuffers(1, &buffer);
	glBindBuffer(target, buffer);
	glBufferData(target, size, data, GL_DYNAMIC_DRAW);

	return buffer;
}

static void show_info_log(GLuint object,
    PFNGLGETSHADERIVPROC glGet__iv,
    PFNGLGETSHADERINFOLOGPROC glGet__InfoLog)
{
	GLint log_length;
	char *log;

	glGet__iv(object, GL_INFO_LOG_LENGTH, &log_length);
	log = malloc(log_length);
	glGet__InfoLog(object, log_length, NULL, log);
	fprintf(stderr, "%s", log);
	free(log);
}

static GLuint make_shader(GLenum type, char **source)
{
	GLint length;
	GLuint shader;
	GLint shader_ok;

	if (!source)
		return 0;

	length = strlen(source[0]);
	shader = glCreateShader(type);
	glShaderSource(shader, 1, (const GLchar**)source, &length);
	glCompileShader(shader);

	glGetShaderiv(shader, GL_COMPILE_STATUS, &shader_ok);
	if (!shader_ok) {
		fprintf(stderr, "Failed to compile %s:\n", source[0]);
		show_info_log(shader, glGetShaderiv, glGetShaderInfoLog);
		glDeleteShader(shader);

		return 0;
	}

	return shader;
}

static GLuint make_program(GLuint vertex_shader, GLuint fragment_shader)
{
	GLint program_ok;

	GLuint program = glCreateProgram();

	glAttachShader(program, vertex_shader);
	glAttachShader(program, fragment_shader);
	glLinkProgram(program);

	glGetProgramiv(program, GL_LINK_STATUS, &program_ok);
	if (!program_ok) {
		fprintf(stderr, "Failed to link shader program:\n");
		show_info_log(program, glGetProgramiv, glGetProgramInfoLog);
		glDeleteProgram(program);

		return 0;
	}

	return program;
}

static struct texture *create_texture_from_surface(SDL_Surface *surface)
{
	struct texture *texture;

	if (using_sdl_renderer) {
		texture = malloc(sizeof(*texture));
		memset(texture, 0, sizeof(*texture));

		texture->sdl_texture = SDL_CreateTextureFromSurface(renderer,
		                                                    surface);

		return texture;
	}

	texture = create_texture(surface->w, surface->h, 0, 0);
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, texture->id);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
			texture->w, texture->h,
			GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV,
			surface->pixels);
	glDisable(GL_TEXTURE_2D);

	return texture;
}

static void destroy_texture(struct texture *texture)
{
	if (!texture)
		return;

	if (using_sdl_renderer) {
		SDL_DestroyTexture(texture->sdl_texture);
	} else {
		if (texture->fbo) {
			glDeleteFramebuffers(1, &texture->fbo);
		}

		glDeleteTextures(1, &texture->id);
	}

	free(texture);
}

static void set_render_target(struct texture *texture)
{
	int window_w, window_h;

	if (using_sdl_renderer) {
		SDL_SetRenderTarget(renderer,
		                    texture ? texture->sdl_texture : NULL);

		return;
	}

	render_target = texture;

	if (texture == NULL) {
		SDL_GetWindowSize(window, &window_w, &window_h);
		glBindFramebuffer(GL_FRAMEBUFFER_EXT, 0);
		glViewport(0, 0, window_w, window_h);
		/*
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(0, window_w, window_h, 0, 0.0, 1.0);
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		*/
		
		return;
	}

	glViewport(0, 0, texture->w, texture->h);
	glBindFramebuffer(GL_FRAMEBUFFER_EXT, texture->fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
				  GL_TEXTURE_2D, texture->id, 0);
	/*
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, texture->w, 0, texture->h, 0.0, 1.0);
	*/
}

static void set_color(SDL_Color *color)
{
	if (using_sdl_renderer) {
		SDL_SetRenderDrawColor(renderer, color->r, color->g,
		                       color->b, color->a);

		return;
	}

	glColor4f(color->r * inv255f, color->g * inv255f,
		  color->b * inv255f, color->a * inv255f);
}

static void fill_rect(SDL_Rect *rect)
{
	if (using_sdl_renderer) {
		SDL_RenderFillRect(renderer, rect);

		return;
	}

	glRectf(rect->x, rect->y, rect->x + rect->w, rect->y + rect->h);
}

static void set_alpha_blending(int enabled)
{
	if (!enabled) {
		if (using_sdl_renderer) {
			SDL_SetRenderDrawBlendMode(renderer,
			                           SDL_BLENDMODE_NONE);
		} else {
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE,
			          GL_REPLACE);
			glDisable(GL_BLEND);
		}

		return;
	}
	
	if (using_sdl_renderer) {
		SDL_SetRenderDrawBlendMode(renderer,
		                           SDL_BLENDMODE_BLEND);
	} else {
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
}

static void render_copy(struct texture *texture, SDL_Rect *srcrect,
			SDL_Rect *dstrect)
{
	GLfloat minx, miny, maxx, maxy;
	GLfloat minu, maxu, minv, maxv;
	SDL_Rect real_srcrect, real_dstrect;
	GLubyte *ptr;
	int dest_w, dest_h;
	int i;

	if (using_sdl_renderer) {
		SDL_RenderCopy(renderer, texture->sdl_texture,
		               srcrect, dstrect);

		return;
	}

	glEnable(GL_TEXTURE_2D);

	real_srcrect.x = 0;
	real_srcrect.y = 0;
	real_srcrect.w = texture->w;
	real_srcrect.h = texture->h;

	real_dstrect.x = 0;
	real_dstrect.y = 0;
	if (render_target) {
		real_dstrect.w = render_target->w;
		real_dstrect.h = render_target->h;
	} else {
		int window_w, window_h;

		SDL_GetWindowSize(window, &window_w, &window_h);
		real_dstrect.w = window_w;
		real_dstrect.h = window_h;
	}

	dest_w = real_dstrect.w;
	dest_h = real_dstrect.h;

	if (srcrect && !SDL_IntersectRect(srcrect, &real_srcrect,
					  &real_srcrect)) {
		return;
	}

	if (dstrect) {
		if (!SDL_HasIntersection(dstrect, &real_dstrect))
			return;
		real_dstrect.x = dstrect->x;
		real_dstrect.y = dstrect->y;
		real_dstrect.w = dstrect->w;
		real_dstrect.h = dstrect->h;
	}

	minx = real_dstrect.x;
	miny = real_dstrect.y;
	maxx = real_dstrect.x + real_dstrect.w;
	maxy = real_dstrect.y + real_dstrect.h;

	minx = (minx / dest_w) * 2.0 - 1.0;
	maxx = (maxx / dest_w) * 2.0 - 1.0;
	miny = 1.0 - (miny / dest_h) * 2.0;
	maxy = 1.0 - (maxy / dest_h) * 2.0;

	minu = (GLfloat) real_srcrect.x / texture->w;
	minu *= texture->texw;
	maxu = (GLfloat) (real_srcrect.x + real_srcrect.w) / texture->w;
	maxu *= texture->texw;
	minv = (GLfloat) real_srcrect.y / texture->h;
	minv *= texture->texh;
	maxv = (GLfloat) (real_srcrect.y + real_srcrect.h) / texture->h;
	maxv *= texture->texh;

	if (!render_target) {
		vertex_buffer_data[0] = minx;
		vertex_buffer_data[1] = miny;
		vertex_buffer_data[2] = minu;
		vertex_buffer_data[3] = minv;

		vertex_buffer_data[4] = maxx;
		vertex_buffer_data[5] = miny;
		vertex_buffer_data[6] = maxu;
		vertex_buffer_data[7] = minv;

		vertex_buffer_data[8] = minx;
		vertex_buffer_data[9] = maxy;
		vertex_buffer_data[10] = minu;
		vertex_buffer_data[11] = maxv;

		vertex_buffer_data[12] = maxx;
		vertex_buffer_data[13] = maxy;
		vertex_buffer_data[14] = maxu;
		vertex_buffer_data[15] = maxv;
	} else {
		vertex_buffer_data[0] = minx;
		vertex_buffer_data[1] = miny;
		vertex_buffer_data[2] = minu;
		vertex_buffer_data[3] = maxv;

		vertex_buffer_data[4] = maxx;
		vertex_buffer_data[5] = miny;
		vertex_buffer_data[6] = maxu;
		vertex_buffer_data[7] = maxv;

		vertex_buffer_data[8] = minx;
		vertex_buffer_data[9] = maxy;
		vertex_buffer_data[10] = minu;
		vertex_buffer_data[11] = minv;

		vertex_buffer_data[12] = maxx;
		vertex_buffer_data[13] = maxy;
		vertex_buffer_data[14] = maxu;
		vertex_buffer_data[15] = minv;
	}

	glUseProgram(program);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture->id);
	glUniform1i(texture_uniform, 0);

	/* FIXME update vertex buffer with new coords */
	glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
	ptr = glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
	for (i = 0; i < 16; i++) {
		*(GLfloat *)ptr = vertex_buffer_data[i];
		ptr += sizeof(GLfloat);
	}
	glUnmapBuffer(GL_ARRAY_BUFFER);
	glVertexAttribPointer(position, 2, GL_FLOAT, GL_FALSE,
	                      sizeof(GLfloat) * 4, (void *)0);

	glEnableVertexAttribArray(position);
	glVertexAttribPointer(texture_coordinates, 2, GL_FLOAT, GL_FALSE,
	                      sizeof(GLfloat) * 4, (void *)(sizeof(GLfloat)*2));

	glEnableVertexAttribArray(texture_coordinates);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, element_buffer);
	glDrawElements(GL_TRIANGLE_STRIP, 4, GL_UNSIGNED_SHORT, (void *)0);
	glDisableVertexAttribArray(position);
	glDisableVertexAttribArray(texture_coordinates);

	glDisable(GL_TEXTURE_2D);
}

static int power_of_2(int input)
{
	int value = 1;

	while (value < input) {
		value <<= 1;
	}

	return value;
}

