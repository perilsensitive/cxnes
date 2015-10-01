---
title: cxNES
layout: default
---

cxNES is an open source NES/Famicom emulator written in C.  It is
primarily written as a part-time hobby project to suit the needs of
its author, but with accuracy, performance and portability as primary
goals.  It has been in development, in various forms, since 2011 and
was initially released for the Linux and Windows platforms in 2015.

Features
========

iNES Mapper Support
-------------------
*   0,   1,   2,   3,   4,   5,   7,   9,  10,  11,  13,  15
*  16,  18,  19,  21,  22,  23,  24,  25,  26,  28,  30,  31
*  32,  33,  34,  36,  37,  38,  41,  46,  47,  48,  61,  64
*  65,  66,  67,  68,  69,  70,  71,  73,  74,  75,  76,  77
*  78,  79,  80,  82,  85,  86,  87,  88,  89,  90,  93,  94
*  95,  97,  99, 105, 112, 113, 115, 118, 119, 133, 137, 138
* 139, 140, 141, 143, 144, 145, 146, 147, 148, 149, 150, 151,
* 152, 154, 155, 158, 159, 178, 180, 184, 185, 189, 192, 193,
* 200, 201, 203, 205, 206, 207, 209, 210, 211, 218, 225, 228,
* 230, 231, 232, 234, 240, 241, 245, 246

Supported Image/ROM Formats
---------------------------
* iNES/NES 2.0
* UNIF
* FDS
  + With or without fwNES header
  + With or without gaps and checksums

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

Supported Expansion Audio Chips
-------------------------------
* Nintendo MMC5
* Konami VRC6
* Sunsoft 5B
* Namco 163
* Famicom Disk System

General
-------
* Savestates
  + Can auto-load/save states on ROM load/unload
  + Can auto-save state at a user-specified interval
* PNG screenshots
* NSF player
* Dynamic audio resampling
  + Uses Blargg's blip_buf resampler
* Cheat support (Game Genie, Pro Action Rocky, and raw formats)
  + Cheats can be auto-loaded/saved on ROM load/unload
* Autopatching support (IPS, UPS and BPS formats)
  + Supports patch stacking
* Soft-patching
* NTSC palette generator and video filter (optional)
  + Bisquit's NTSC palette generator
  + Blargg's nes_ntsc filter
    + Can be used with generated palette or external 64 or 512
      color palette
* ROM Database (optional) for handling ROMs with incorrect or
  missing headers to run properly.
* High-level optimizations of FDS disk I/O routines (optional)
* Automatic disk selection and change for most FDS games (optional)
* FDS images saved as IPS patches
* Enhanced sprite limit workaround: limits sprites for scanlines where
  the limit appears to be triggered intentionally, otherwise does not
  limit sprites.
* Flexible input binding support
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
* VS. Unisystem support
* Can be compiled or run without the GUI
  + Can specify configuration options, patches and cheats on command line

License
=======
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
