Configuration
=============
cxNES uses a simple "key=value" configuration format.  Blank lines are allowed, and '#' may be used at the start of a line to denote a comment.  Key names are not case-sensitive.  Values are also not usually case-sensitive, but this depends on what the configuration option is used for.  Path names, for example, are case-sensitive on *nix systems. A full list of configuration options and what they do is provided at the end of this document.

cxNES will look for a configuration file in ~/.cxnes/config.  You can force it to use a different file by passing the '--maincfgfile=FILENAME' option on the command line.  You can also supress loading of the main config file entirely by passing the '--no-maincfg' option.

Additionally, cxNES can load a ROM-specific config file.  This makes it possible to tweak the configuration for a particular game, such as selecting the attached input device, enabling four-player mode, changing the screen crop settings, etc.  These settings are applied *after* those specified in the main config file so you only need to specify your common settings once.

The config file must have the same name as the ROM file, but with a .cfg extension.  cxNES will look for the file in the same directory as the ROM file first, then in the config path (~/.cxnes/cfg by default).  For example, if your ROM file is named 'roms/myrom.nes', the following paths will be tried when looking for a ROM-specific configuration:

* roms/myrom.cfg
* ~/.cxnes/cfg/myrom.cfg

You can specify an alternative path to try for the ROM-specific config by passing '--romcfgfile=FILENAME' on the commandline.  You can also suppress loading of the configuration entirely by passing '--no-romcfg, or re-enable it with '--romcfg'.

The configuration variable 'skip_romcfg' can be set to 'true' to disable ROM-specific configuration, or 'false' to enable it.  The default is 'false'.

You can override the default directory where cxNES looks for ROM configuration files by setting 'path.config' in the main config file.  The value must be an absolute path.  The default is ~/.cxnes/cfg.

You can set a configuration option on the commandline by passing '-o key=value' or '--option key=value'.  Any number of options may be set this way. This will override any previous value these options were set to in the main or ROM-specific configurations.

There are a number of configuration variables related to keybindings that are described in input_events.txt.  The remaining variables are described below:

Video Configuration
-------------------
###vsync

Type: boolean  
Default: false

Specifies whether or not the emulator should wait until the vertical refresh to draw the emulated screen.  Setting this to true will reduce or eliminate tearing, but may cause frames to be skipped in order to prevent audio buffer underruns (and will result in audio pops/clicks if the frame skip isn't enough to compensate).  To reduce the possibility of skipped frames or audio dropouts with VSync enabled, you may want to set [dynamic\_rate\_control](#dynamic_rate_control) to true.

VSync should work in fullscreen mode, but may or may not work in windowed mode depending on your video hardware and drivers.

###stretch\_to\_fit

Type: boolean  
Default: true

If set to true, in full-screen mode the emulator will attempt to stretch the image after scaling to fill as much of the screen as possible while still preserving the configured pixel aspect ratio.  If set to false, the image will only be scaled by the integer value specified by the [fullscreen\_scaling\_factor](#fullscreen_scaling_factor) variable.


###fullscreen\_scaling\_factor

Type: integer  
Default: -1

Specifies the multiplier used to scale the image when in full-screen mode.  The default value of -1 (or any value &lt;= 0) means to scale the image by the largest factor possible that doesn't exceed the height or width of the screen.  It is possible to manually set this to a value that would cause the image to exceed the screen size, although this is probably not desirable.

###window\_scaling\_factor

Type: integer  
Default: 1

Specifies the multiplier used to scale the image when in windowed mode.  The special value -1 (or any value &lt;= 0) will scale the image by the largest factor possible that doesn't exceed the height or width of the screen.

###ntsc\_pixel\_aspect\_ratio

Type: string  
Default: "none"

Specifies the pixel aspect ratio (PAR) to use for NTSC games.  Valid values are: "none" (no correction), "ntsc" (8:7 pixel aspect ratio) or "pal" (1.3862:1 pixel aspect ratio).

See also [pal\_pixel\_aspect\_ratio](#pal_pixel_aspect_ratio).

###pal\_pixel\_aspect\_ratio

Type: string  
Default: "none"

Specifies the pixel aspect ratio (PAR) to use for PAL games.  Valid values are: "none" (no correction), "ntsc" (8:7 pixel aspect ratio) or "pal" (1.3862:1 pixel aspect ratio).

See also [ntsc\_pixel\_aspect\_ratio](#ntsc_pixel_aspect_ratio).

###fullscreen

Type: boolean  
Default: false

Specifies whether or not the emulator should start in full-screen mode by default.

###ntsc\_top\_crop

Type: integer  
Default: 8

Specifies the first scanline drawn for NTSC games.  The first 8 scanlines (give or take a few) were often not visible on a TV due to the bezel covering the edges of the screen.  You may wish to change this in a ROM-specific config file to hide scrolling artifacts or attribute glitches.

###ntsc\_bottom\_crop

Type: integer  
Default: 231

Specifies the last scanline drawn for NTSC games.  The last 8 scanlines (give or take a few) were often not visible on a TV due to the bezel covering the edges of the screen.  You may wish to change this in a ROM-specific config file to hide scrolling artifacts or attribute glitches.

###ntsc\_left\_crop

Type: integer  
Default: 0

Specifies the leftmost column of pixels drawn for NTSC games.  You may wish to change this in a ROM-specific config file to hide scrolling artifacts or attribute glitches.

###ntsc\_right\_crop

Type: integer  
Default: 255

Specifies the rightmost column of pixels drawn for NTSC games.  You may wish to change this in a ROM-specific config file to hide scrolling artifacts or attribute glitches.

###pal\_top\_crop

Type: integer  
Default: 1

Specifies the first scanline drawn for PAL games.  The first 1 scanlines was usually not visible on TVs, so it is hidden by default.  You may wish to change this in a ROM-specific config file to hide scrolling artifacts or attribute glitches.

###pal\_bottom\_crop

Type: integer  
Default: 239

Specifies the last scanline drawn for PAL games.  You may wish to change this in a ROM-specific config file to hide scrolling artifacts or attribute glitches.

###pal\_left\_crop

Type: integer  
Default: 0

Specifies the leftmost column of pixels drawn for PAL games.  You may wish to change this in a ROM-specific config file to hide scrolling artifacts or attribute glitches.

###pal\_right\_crop

Type: integer  
Default: 255

Specifies the rightmost column of pixels drawn for PAL games.  You may wish to change this in a ROM-specific config file to hide scrolling artifacts or attribute glitches.

Audio Configuration
-------------------
###sampling\_rate

Type: integer  
Default: 48000

Specifies the audio sampling rate to use.  If your hardware does not support this rate SDL will pick a rate your hardware can use.

###audio\_buffer\_size

Type: integer  
Default: 2048

The size of your audio buffer in samples.  The default should work for most configurations.  Note that higher values will add audio latency, although this may or may not be perceptible.

###dynamic\_rate\_control

Type: boolean
Default: false

Controls whether or not the emulator will adjust the resampling rate dynamicaly to keep the audio buffer filled.  This is especially useful when [vsync](#vsync) is enabled as it is near-impossible to stay synced to the vertical refresh and keep from skipping frames or suffering audio underruns without this.  The adjustments should be small enough to avoid audible changes in pitch.  This feature is experimental, but seems to work well enough for the author's purposes.

Note that frames may still occasionally be skipped due to large, sudden changes in CPU load or OS scheduling priority.

Input Configuration
-------------------

CPU Configuration
-----------------

PPU Configuration
-----------------

APU Configuration
-----------------
