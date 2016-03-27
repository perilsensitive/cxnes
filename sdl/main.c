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

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <getopt.h>
#if GUI_ENABLED
#include <gtk/gtk.h>
#endif
#include <SDL.h>
#include <SDL_syswm.h>
#include <SDL_thread.h>
#include <SDL_ttf.h>
#include <time.h>
#include <sys/time.h>
#include <math.h>
#ifdef __unix__
#include <signal.h>
#endif

#include "log.h"
#include "file_io.h"
#include "input.h"

#include "config.h"
#include "cheat.h"
#include "cpu.h"
#include "ppu.h"
#include "apu.h"
#include "io.h"
#include "board.h"
#include "rom.h"
#include "emu.h"
#include "video.h"
#include "audio.h"

#define NS_PER_SEC 1000000000L

/* FIXME hack */
int running = 1;

static int testing = 0;
static int passed_duration = 0;
static int test_duration = -1;
static const char *frame_dumpfile;
static const char *rom_dumpfile;

static int portable;
static int nomaincfg;
static int noromcfg;
static int show_help;
static int show_version;
static int noautopatch;
static int print_config;
extern int fullscreen;

extern int autohide_timer;
extern int fps_timer;
extern int display_fps;
extern int center_x, center_y;
extern int mouse_grabbed;
extern int window_minimized;
extern uint32_t *nes_screen;
extern uint16_t nes_pixel_screen[];

#if __unix__
static struct timespec prev_clock;
#else
static uint32_t prev_clock;
#endif

#if __unix__
static int screensaver_deactivate_delay;
static int screensaver_counter;
#endif

static int savestate_counter;

int total_frame_time;
#if __unix__
const int time_base = 1000000000;
#else
const int time_base = 1000;
#endif
static int frame_times[60];
static int frame_index = 0;

#if GUI_ENABLED
int gui_enabled;
#endif

struct emu *emu;

extern void gui_enable_event_timer(void);

int open_rom(struct emu *emu, char *filename, int patch_count, char **patchfiles);
int close_rom(struct emu *emu);
void video_clear(void);
int video_apply_config(struct emu *emu);
void video_toggle_fullscreen(int);
void video_show_cursor(int);
void video_resize_window(void);

/* FIXME put this in header */
extern void input_load_queue(void);

static struct option long_options[] = {
	{ "version", no_argument, &show_version, 1 },
	{ "track", required_argument, 0, 't' },
	{ "help", no_argument, &show_help, 1 },
	{ "no-maincfg", no_argument, &nomaincfg, 1 },
	{ "maincfg", no_argument, &nomaincfg, 0 },
	{ "no-romcfg", no_argument, &noromcfg, 1 },
	{ "romcfg", no_argument, &noromcfg, 0 },
	{ "maincfgfile", required_argument, 0, 'c' },
	{ "pal", no_argument, 0, 'p' },
	{ "ntsc", no_argument, 0, 'N' },
	{ "romcfgfile", required_argument, 0, 'R' },
	{ "option", required_argument, 0, 'o' },
	{ "trace", no_argument, 0, 'T' },
	{ "fullscreen", no_argument, 0, 'f' },
	{ "window", no_argument, 0, 'w' },
	{ "cheat", required_argument, 0, 'g' },
	{ "printconfig", no_argument, 0, 'C' },
	{ "no-autopatch", no_argument, &noautopatch, 1 },
	{ "autopatch", no_argument, &noautopatch, 0 },
	{ "regression-test", no_argument, &testing, 1},
	{ "frame-dumpfile", required_argument, 0, 'D'},
	{ "rom-dumpfile", required_argument, 0, 'F'},
	{ "test-duration", required_argument, &passed_duration, 1 },
	{ "portable", no_argument, &portable, 1 },
	{ 0, 0, 0, 0 },
};

void update_clock(void)
{
#if __unix__
	long ticks = prev_clock.tv_sec * 1000000000 + prev_clock.tv_nsec;
	if (clock_gettime(CLOCK_MONOTONIC, &prev_clock) < 0) {
		log_err("clock_gettime() failed\n");
	}
	ticks = (prev_clock.tv_sec * 1000000000 + prev_clock.tv_nsec) - ticks;

	total_frame_time -= frame_times[frame_index];
	frame_times[frame_index] = ticks;
#else
	int ticks = SDL_GetTicks();

	total_frame_time -= frame_times[frame_index];
	frame_times[frame_index] = ticks - prev_clock;

	prev_clock = ticks;
#endif
	total_frame_time += frame_times[frame_index];
	frame_index = (frame_index + 1) % 60;
}

#if __unix__
static void deactivate_screensaver(void)
{
	int child = fork();

	if (child < 0) {
		log_err("failed to fork screensaver command: %s\n",
			strerror(errno));
		return;
	} else if (child > 0) {
		return;
	}

	close(STDOUT_FILENO);
	close(STDERR_FILENO);

	exit(system(emu->config->screensaver_deactivate_command) & 0xff);
}
#endif

static void throttle(int draw_frame)
{
#if __unix__
	int rc;
	long ns;
	struct timespec sleep_time;
#else
	uint32_t clock;
#endif

	update_clock();

	if ((!window_minimized && emu->config->vsync) ||
	    !draw_frame) {
		return;
	}
	
#if __unix__
	sleep_time.tv_sec = prev_clock.tv_sec;
	sleep_time.tv_nsec = prev_clock.tv_nsec;
	ns = sleep_time.tv_nsec + emu->current_delay_ns;
	sleep_time.tv_sec  += ns / NS_PER_SEC;
	sleep_time.tv_nsec = ns % NS_PER_SEC;

	do {
		rc = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME,
				     &sleep_time, NULL);
	} while (rc == EINTR);

	if (rc && (rc != EINTR)) {
		log_err("clock_nanosleep() failed: %s\n",
			strerror(errno));
	}
#else
	clock = SDL_GetTicks();
	//frame_times[frame_index] = clock - prev_clock;

	int fdelay = clock - prev_clock;
	if (fdelay > 16)
		fdelay = 0;

	//printf("delay: %d\n", delay_ns / 1000000 - fdelay);
	SDL_Delay((emu->current_delay_ns / 1000000) - fdelay);
#endif
}

extern int input_process_event(SDL_Event *event);
extern int video_process_event(SDL_Event *event);
int misc_process_event(SDL_Event *event)
{
	int rc;

	return 0;

	rc = 1;
	switch (event->type) {
	case SDL_QUIT:
		running = 0;
		break;
	default:
		rc = 0;
	}

	return rc;
}

void process_events(void)
{
	SDL_Event event;

	SDL_PumpEvents();

	while (1) {
		int rc;

		rc = SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_FIRSTEVENT,
				    SDL_KEYDOWN - 1);

		if (!rc) {
			rc = SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_MULTIGESTURE + 1,
					    SDL_LASTEVENT);
		}

		if (rc <= 0)
			break;

		if (video_process_event(&event))
			continue;

		if (misc_process_event(&event))
			continue;
	}
}

int save_screenshot(void)
{
	char *screenshot_path, *buffer, *base, *tmp;
	int len;
	int i;
	int rc;

	rc = -1;
	screenshot_path = config_get_path(emu->config,
					      CONFIG_DATA_DIR_SCREENSHOT,
					      NULL, 1);

	if (!screenshot_path)
		return -1;

	base = strdup(emu->rom_file);

	if (!base) {
		free(screenshot_path);
		return -1;
	}

	tmp = strrchr(base, '.');
	if (tmp)
		*tmp = '\0';

	len = strlen(screenshot_path) + 1 + strlen(base) +
		strlen("_000.png") + 1;

	buffer = malloc(len);
	if (!buffer) {
		free(screenshot_path);
		free(base);
		return -1;
	}

	i = 1;
	while (i < 1000) {
		snprintf(buffer, len, "%s%c%s_%03d.png",
			 screenshot_path, PATHSEP[0],
			 base, i);

		if (!check_file_exists(buffer))
			break;

		i++;
	}

	if (i < 1000)
		rc = video_save_screenshot(buffer);

	free(buffer);
	free(screenshot_path);
	free(base);

	return rc;
}

static int main_loop(struct emu *emu)
{
#if __unix__
	if (screensaver_deactivate_delay) {
		screensaver_counter = emu->current_framerate *
			screensaver_deactivate_delay;
	} else {
		screensaver_counter = -1;
	}

	if (emu->config->periodic_savestate_enabled &&
	    (emu->config->periodic_savestate_delay > 0)) {
		savestate_counter = emu->current_framerate *
			emu->config->periodic_savestate_delay;
	} else {
		savestate_counter = -1;
	}

	if (clock_gettime(CLOCK_MONOTONIC, &prev_clock) < 0) {
		log_err("clock_gettime() failed\n");
		return -1;
	}
#else
	prev_clock = SDL_GetTicks();
#endif

	fps_timer = 0;

	emu->draw_frame = 1;
	while (running) {
		int cycles;

#if GUI_ENABLED
		while (gui_enabled && gtk_events_pending()) {
			gtk_main_iteration_do(0);
		}
#endif

#if GUI_ENABLED
		if (!gui_enabled || (emu->loaded && !emu->paused))
#endif
		{
			input_poll_events();
			process_events();
		}

#if GUI_ENABLED
		if (!gui_enabled)
#endif
		{
			if (!emu->loaded || emu->paused)
				input_process_queue(1);
		}

		if (mouse_grabbed) {
			autohide_timer = 0;
			video_show_cursor(0);
		} else if (autohide_timer && !mouse_grabbed &&
		    emu->config->autohide_cursor) {
			autohide_timer--;
			if (!autohide_timer)
				video_show_cursor(0);
		}

		if (!emu->loaded || emu->paused) {
			SDL_Delay(100);
			continue;
		}

		cycles = emu_run_frame(emu, nes_screen, nes_pixel_screen);
		if (testing && test_duration > 0) {
			test_duration--;
			if (!test_duration) {
				video_save_screenshot(frame_dumpfile);
				running = 0;
				continue;
			}
		}

		if (!testing)
			audio_fill_buffer(cycles);

		if (display_fps) {
			fps_timer--;
			if (fps_timer < 0) {
				video_update_fps_display();
				fps_timer = 60;
			}
		}


		if (emu->blargg_reset_timer > 0)
			emu->blargg_reset_timer--;
		else if (!emu->blargg_reset_timer)
			emu_reset(emu, 0);

		if (testing)
			continue;

		if (emu->draw_frame) {
			video_update_texture();
			video_draw_buffer();
		/* } else { */
		/* 	printf("skipping frame\n"); */
		}

		if (mouse_grabbed) {
			video_warp_mouse(center_x, center_y);
		}

		throttle(emu->draw_frame);

		emu->draw_frame = audio_buffer_check();

#if __unix__
		if (screensaver_counter > 0) {
			screensaver_counter--;

			if (!screensaver_counter ) {
				screensaver_counter =
					emu->current_framerate *
					emu->config->screensaver_deactivate_delay;
				deactivate_screensaver();
			}
		}
#endif

		if (savestate_counter > 0) {
			savestate_counter--;

			if (!savestate_counter) {
				const char *path;

				path = emu->config->periodic_savestate_path;
				savestate_counter =
					emu->current_framerate *
					emu->config->periodic_savestate_delay;
				if (!path || !path[0])
					emu_quick_save_state(emu, 1, 0);
				else
					emu_save_state(emu, path);
			}
		}

		process_events();
		input_poll_events();
		input_process_queue(1);
	}

	return 0;
}

void version(void)
{
	printf(PACKAGE_STRING "\n");
}

void help(const char *name, int brief)
{
	printf("Usage: %s [OPTION] FILE [PATCH [PATCH2 ...]]\n", name);

	if (brief)
		return;

	printf("\n");
	printf("      --autopatch\tenable auto-patching\n");
	printf("      --no-autopatch\tdisable auto-patching\n");
	printf("  -C, --printconfig\tPrint configuration to stdout and exit\n");
	printf("  -c, --cfgfile=filename\tUse alternative main config file\n");
	printf("  -f, --fullscreen\tstart in full-screen mode\n");
	printf("  -g, --cheat\tadd Game Genie, Pro Action Rocky or raw code\n");
	printf("  -N, --ntsc\t\tforce ntsc mode\n");
	printf("      --maincfg\tenable loading of main config file\n");
	printf("      --no-maincfg\tdisable loading of main config file\n");
	printf("  -o, --option=KEY=VAL\tset configuration option KEY to VAL\n");
	printf("  -p, --pal\t\tforce PAL mode\n");
	printf("  -R, --romcfgfile=filename\tUse alternative ROM config file\n");
	printf("      --romcfg\tenable loading of ROM-specific config file\n");
	printf("      --no-romcfg\tdisable loading of ROM-specific config file\n");
	printf("  -t, --track\t\tspecify first track to play (NSF)\n");
	printf("  -T, --trace\t\tdisplay CPU trace on STDOUT\n");
	printf("  -w, --window\t\tstart in windowed mode\n");
	printf("      --help\t\tdisplay this help and exit\n");
	printf("      --version\t\tdisplay version information and exit\n");
	printf("      --portable\t\trun " PACKAGE_NAME " in portable mode\n");
}

int parse_command_line(struct cheat_state *cheats,
		       struct config *config,
		       int argc, char **argv)
{
	char *p;
	int c;
	int option_index = 0;
	//int print_config = 0;

	/* Reset optind so that we can reparse the command line again
	   if desired.
	*/
	optind = 1;
	while (1) {
		c = getopt_long(argc, argv, "fwTo:g:c:R:F:pNCt:",
					  long_options, &option_index);

		if (c == -1)
			break;

		switch (c) {
		case 'F':
			rom_dumpfile = optarg;
			break;
		case 'D':
			frame_dumpfile = optarg;
			break;
		case 'C':
			print_config = 1;
			break;
		case 'p':
			rom_config_set(config, "timing", "pal");
			break;
		case 'N':
			rom_config_set(config, "timing", "ntsc");
			break;
		case 'f':
			fullscreen = 1;
			break;
		case 't':
			main_config_set(config, "nsf_first_track", optarg);
			break;
		case 'w':
			fullscreen = 0;
			break;
		case 'T':
			main_config_set(config, "cpu_trace_enabled", "1");
			break;
		case 'o':
			p = strchr(optarg, '=');
			if (!p) {
				log_err("Invalid option: format should"
					" be 'key=value'\n");
				return 1;
			}

			*p = '\0';

			main_config_set(config, optarg, p + 1);
			*p = '=';
			
			break;
		case 'c':
			main_config_set(config, "maincfg_path", optarg);
			break;
		case 'R':
			main_config_set(config, "romcfg_path", optarg);
			break;
		case 'g':
			add_cheat(cheats, optarg);
			break;
		case '?':
			log_err("Try '%s --help' for more "
				"information.\n", argv[0]);
			/* Error message already printed */
			return 1;
			break;
		case 0:
			if (passed_duration) {
				passed_duration = 0;
				test_duration = atoi(optarg);
			}
			break;
		}
	}

	return 0;
}

int main(int argc, char **argv)
{
	struct config *config;
	struct cheat_state *cheats;
	int rc;

	setbuf(stdout, NULL);
	setbuf(stderr, NULL);

	log_set_loglevel(LOG_PRIORITY_INFO);

	emu = emu_new();
	if (!emu) {
		log_err("failed to allocate memory for emu\n");
		return -1;
	}

	config_init(emu);

	config = emu->config;
	cheats = emu->cheats;

	noautopatch = 0;
	nomaincfg = 0;
	noromcfg = 0;
	show_help = 0;
	show_version = 0;
	portable = 0;

	rc = parse_command_line(cheats, emu->config, argc, argv);
	if (rc)
		return rc;

	if (!testing && SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_JOYSTICK|
		     SDL_INIT_GAMECONTROLLER)) {
		log_err("SDL_Init() failed: %s\n",
			SDL_GetError());
		return 1;
	}

	if (!testing)
		input_init(emu);

	if (show_version) {
		version();
		return 0;
	}

	if (show_help) {
		help(argv[0], 0);
		return 0;
	}

	if (nomaincfg)
		config->skip_maincfg = 1;
	
	if (noromcfg)
		config->skip_romcfg = 1;

#if _WIN32
	if (config_set_portable_mode(portable))
		return 1;
#endif

	rc = config_load_main_config(emu->config);
	rc = parse_command_line(cheats, emu->config, argc, argv);

#if GUI_ENABLED
	gui_enabled = config->gui_enabled;

	if (testing)
		gui_enabled = 0;
#endif

	if (!testing)
		audio_init(emu);

#if GUI_ENABLED
	if ((optind == argc) && !gui_enabled)
#else
	if (optind == argc)
#endif
	{
		help(argv[0], 1);
		return 1;
	}

	if (noautopatch)
		config->autopatch_enabled = 0; /* FIXME false */

	if (print_config) {
		config_print_current_config(emu->config);
		return 0;
	}

#if __unix__
	screensaver_deactivate_delay = emu->config->screensaver_deactivate_delay;
#endif


#if __unix__
	/* Tell the kernel to automatically reap our children so we
	   don't need to call waitpid().  This isn't guaranteed to
	   work on all platforms, but Linux and FreeBSD are fine.
	   Anything POSIX.1-2001 compliant should work. */
	signal(SIGCHLD, SIG_IGN);
#endif

#if GUI_ENABLED
	if (gui_enabled && !testing)
		video_init(emu);
	if (optind < argc) {
		if (open_rom(emu, argv[optind], argc - optind - 1,
		             &argv[optind + 1])) {
			if (!gui_enabled) {
				return 1;
			}
		}

		if (rom_dumpfile) {
			if (emu->rom->info.flags & ROM_FLAG_IN_DB)
				writefile(rom_dumpfile, emu->rom->buffer,
					  emu->rom->offset +
					  emu->rom->info.total_prg_size +
					  emu->rom->info.total_chr_size);
			goto done;
			
		}
	}
	if (!gui_enabled && !testing)
		video_init(emu);
#else
	if (optind < argc) {
		if (open_rom(emu, argv[optind], argc - optind - 1,
		             &argv[optind + 1])) {
			return 1;
		}

		if (rom_dumpfile) {

			if (emu->rom->info.flags & ROM_FLAG_IN_DB)
				writefile(rom_dumpfile, emu->rom->buffer,
					  emu->rom->offset +
					  emu->rom->info.total_prg_size +
					  emu->rom->info.total_chr_size);
			goto done;
			
		}
	}
	if (!testing)
		video_init(emu);
#endif
	if (testing)
		video_init_testing(emu);

#if GUI_ENABLED
	if (!emu_loaded(emu) && gui_enabled)
		gui_enable_event_timer();
#endif

	main_loop(emu);

	close_rom(emu);
	emu_cleanup(emu);
	if (!testing) {
		video_shutdown();
		audio_shutdown();
		input_shutdown();
	} else {
		video_shutdown_testing();
	}

	config_shutdown();
done:

	if (!testing) {
#if GUI_ENABLED
	if (!gui_enabled)
#endif
		SDL_Quit();
	}

	return 0;
}

int open_rom(struct emu *emu, char *filename, int patch_count, char **patchfiles)
{
	size_t size;

	if (emu_loaded(emu))
		close_rom(emu);
	
	if (emu_init(emu) < 0) {
		err_message("Failed to initialize emulator core");
		return -1;
	}

	size = get_file_size(filename);
	if ((int)size < 0) {
		return 1;
	}

	/* config_load_rom_config(emu->config, filename); */

	if (emu_load_rom(emu, filename, patch_count, patchfiles)) {
		return 1;
	}

	if (!testing) {
		video_apply_config(emu);
		audio_apply_config(emu);
	}
	/* FIXME input_apply_config(emu); */
	emu_apply_config(emu);

	emu_reset(emu, 1);

	if (emu->config->autoload_state) {
		char *path;
		path = config_get_path(emu->config, CONFIG_DATA_DIR_STATE,
					   emu->state_file, 1);
		printf("path is %s\n", path);
		if (check_file_exists(path)) {
			if (emu_load_state(emu, path) == 0)
				osdprintf("State autoloaded\n");
			else
				osdprintf("State autoload failed\n");
		}

		free(path);
	}

	return 0;
}

int close_rom(struct emu *emu)
{
	if (!emu->loaded)
		return 0;

	if (emu->config->autosave_state) {
		char *path;
		path = config_get_path(emu->config, CONFIG_DATA_DIR_STATE,
					   emu->state_file, 1);

		if (emu_save_state(emu, path) == 0)
			osdprintf("State autosaved\n");
		else
			osdprintf("State autosave failed\n");

		free(path);
	}

	if (emu)
		emu_deinit(emu);

#if GUI_ENABLED
	if (gui_enabled)
		gui_enable_event_timer();
#endif

	if (!testing)
		audio_shutdown();

	return 0;
}
