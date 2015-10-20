---
title: cxNES
layout: default
---

cxNES is an open source, cross-platform NES/Famicom emulator.  It is
mainly written as a part-time hobby project to suit the needs of its
author, but with accuracy, performance and portability as primary
goals.

cxNES is written in C and uses SDL 2.0 for graphics, sound and input,
and GTK+ for the GUI toolkit.  It should have the same functionality
and roughly the same look and feel on all supported platforms.
Currently only Windows and Linux are supported; OS X support will be
coming as soon as the author can get a Mac for development and
testing.  Other unix variants may work as well provided they have
working SDL 2.0 and GTK+ libraries, but none have been tested.

You can contact the author at [perilsensitive@gmail.com](mailto:perilsensitive@gmail.com).

Features
========

Supported Image/ROM Formats
---------------------------
* iNES/NES 2.0
* UNIF
* FDS
  + fwNES format, with or without header
  + "Raw" format (includes gaps, start marks and checksums)
* NSF

Input Devices/Accessories
-------------------------
* Standard Controllers
* Four-player adapters (NES and Famicom types)
* Arkanoid Controllers (NES and Famicom variants)
* Zapper
* Power Pad
* Family Trainer
* Family BASIC Keyboard
* SNES Mouse
* VS. Unisystem support

Supported Expansion Audio Chips
-------------------------------
* Nintendo MMC5
* Konami VRC6
* Sunsoft 5B
* Namco 163
* Famicom Disk System

iNES Mapper Support
-------------------
*   0,   1,   2,   3,   4,   5,   7,   9,  10,  11,  14,  15,
*  16,  18,  19,  21,  22,  23,  24,  25,  26,  28,  30,  31,
*  32,  33,  34,  36,  37,  38,  41,  46,  47,  48,  60,  61,
*  64,  65,  66,  67,  68,  69,  70,  71,  73,  74,  75,  76,
*  77,  78,  79,  80,  82,  85,  86,  87,  88,  89,  90,  93,
*  94,  95,  97,  99, 105, 112, 113, 115, 118, 119, 133, 137,
* 138, 139, 140, 141, 143, 144, 145, 146, 147, 148, 149, 150,
* 151, 152, 153, 154, 155, 158, 159, 178, 180, 182, 184, 185,
* 189, 192, 193, 200, 201, 203, 205, 206, 207, 209, 210, 211,
* 218, 225, 228, 230, 231, 232, 234, 240, 241, 245, 246

Patching
--------
* Supports IPS, UPS and BPS patch formats
* Autopatching support
* Soft-patching
* Patches may be applied via GUI or specified on
  the command line

Video
-----
* Bisquit's NTSC palette generator
* Blargg's nes_ntsc filter
* User can specify a 64 or 512 color external palette
  file to be used instead of the generated palette.
* Enhanced sprite limit workaround: limits sprites for scanlines where
  the limit appears to be exploited intentionally, otherwise does not
  limit sprites.
* Supports correct TV aspect ratios for NTSC and PAL consoles
* PNG screenshots

Input
-----
* Flexible input binding support
  + Nearly all emulator actions can have custom keyboard and/or
    joystick mappings
  + User-defined 'modifiers' useful for button combos on gamepads
    with few buttons (such as standard NES controllers)
* SDL GameController API
  + Allows supported joysticks (including XInput devices) to have
    a common set of sane default mappings.
  + Additional gamepads may be added by editing a plain-text
    mapping database
  + SDL Joystick API also supported for devices not recognized by
    the GameController API.
* Turbo support for controllers
  + Supports dedicated turbo buttons
  + Supports toggle buttons to enable/disable turbo on the standard
    A and B buttons.

Misc
----
* Savestates
  + Can auto-load/save states on ROM load/unload
  + Can auto-save state at a user-specified interval
* Dynamic audio resampling
  + Uses Blargg's blip_buf resampler
* Cheat support (Game Genie, Pro Action Rocky, and raw formats)
  + Cheats can be auto-loaded/saved on ROM load/unload
* ROM Database (optional) for handling ROMs with incorrect or
  missing headers.
* Can be compiled or run without the GUI
* Can specify configuration options, patches and cheats on command line
* FDS enhancements
  + High-level optimizations of FDS disk I/O routines (optional)
  + Automatic disk selection and change for most FDS games (optional)
  + Writes to FDS images saved as IPS patches
* Windows builds can be configured to run in portable mode, storing all
  user data (save files, states, etc.) in the application directory.
