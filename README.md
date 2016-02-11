cxNES Documentation

Version 0.2.0
Updated 2015-10-22

Website: http://perilsensitive.github.io/cxnes

Intro
=====
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
* Any of the above in a ZIP container
* Split ROMs inside a ZIP container (provided split ROM checksums exist in the database)

Input Devices/Accessories
-------------------------
* Standard Controllers
* Four-player adapters (NES and Famicom types)
* Arkanoid Controllers (NES and Famicom variants)
* Zapper
* Power Pad
* Family Trainer
* Family BASIC Keyboard
* SUBOR Keyboard
* SNES Mouse
* VS. Unisystem support

Supported Expansion Audio Chips
-------------------------------
* Nintendo MMC5
* Konami VRC6
* Konami VRC7
* Sunsoft 5B
* Namco 163
* Famicom Disk System

iNES Mapper Support
-------------------
*   0,   1,   2,   3,   4,   5,   7,   9,  10,  11,  13,  14
*  15,  16,  18,  19,  21,  22,  23,  24,  25,  26,  28,  29
*  30,  31,  32,  33,  34,  36,  37,  38,  39,  41,  44,  46
*  47,  48,  49,  58,  60,  61,  62,  64,  65,  66,  67,  68
*  69,  70,  71,  73,  74,  75,  76,  77,  78,  79,  80,  82
*  85,  86,  87,  88,  89,  90,  91,  93,  94,  95,  97,  99
* 105, 107, 112, 113, 115, 118, 119, 133, 137, 138, 139, 140
* 141, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153
* 154, 155, 158, 159, 166, 167, 178, 180, 182, 184, 185, 189,
* 192, 193, 200, 201, 202, 203, 205, 206, 207, 209, 210, 211,
* 218, 225, 226, 228, 230, 231, 232, 234, 240, 241, 245, 246

Patching
--------
* Supports IPS, UPS and BPS patch formats
* Autopatching support
* Soft-patching
* Patches may be applied via GUI or specified on
  the command line
* Patches may be included with ROM inside ZIP archive

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
* Joysticks are automatically configured when plugged in or unplugged;
  no need to restart the emulator.
* SDL GameController API
  + Allows supported joysticks (including XInput devices) to have
    a common set of mappings; remapping is not required when
    switching between multiple supported devices.
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

Portable Mode (Windows only)
----------------------------
The Windows build of cxNES stores all user data (save files, state files, cheats,
etc.) in %APPDATA%\cxnes by default.  To run cxNES as a portable app so
that user data is stored in the same folder as cxNES itself, you can enable portable
mode.  This can be done by passing '--portable' on the cxNES command line, but this
will only enable portable mode until you exit cxNES.  To make cxNES always use portable
mode, follow these steps:

1. Create a folder named 'data' in the same folder where you extracted
   cxNES, then create a folder named 'cxnes' inside the new 'data'
   folder.  These folders are included in the cxNES zipfile, so they
   should already exist (assuming you didn't delete them).

2. Create a file in data\cxnes called 'portable.txt'.  You can create
   this file any way you'd like; it's probably easiest to just use
   Notepad.  The contents of this file don't matter; cxNES doesn't
   read the file, but only checks to see if it exists.  If this file
   exists, cxNES wil run in portable mode.  If not, and you didn't pass
   '--portable' on the command line, it will run normally.

3. If you have user data in %APPDATA%\cxnes already, you may want to
   copy it to data\cxnes inside the folder where cxNES is installed.

cxNES should now always look for user data in the data\cxnes folder
inside the folder where cxNES is located rather than in
%APPDATA%\cxnes.

To upgrade a portable installation, you can unzip the new version of
cxNES into the same folder as the old one.  User data (stuff in the
data folder) will not be touched, so you don't need to worry about
wiping out your save states or save files.  Alternatively, you can
just extract the new version of cxNES to a new folder and copy or move
the data folder from the old version to the new one.

Default Bindings
----------------

| Action                      | Binding(s)                                      |
| --------------------------- | ---------------------------------------------   |
| Controller 1 Up     	      | Up, Joystick 0 D-Pad Up / Left Y -        |
| Controller 1 Down   	      | Down, Joystick 0 D-Pad Down / Left Y +    |
| Controller 1 Left   	      | Left, Joystick 0 D-Pad Down / Left X -    |
| Controller 1 Right  	      | Right, Joystick 0 D-Pad Down / Left Y +   |
| Controller 1 A      	      | F, Joystick 0 A 	      	       	        |
| Controller 1 B      	      | D, Joystick 0 X 			        |
| Controller 1 Select 	      | S, Joystick 0 Back 	        |
| Controller 1 Start  	      | Enter, Joystick 0 Start 		        |
| Controller 1 A Turbo Toggle | V, Joystick 0 B 	        |
| Controller 1 B Turbo Toggle | C, Joystick 0 Y 	        |
| Controller 2 Up     	      | Joystick 1 D-Pad Up / Left Y - 	        |
| Controller 2 Down   	      | Joystick 1 D-Pad Down / Left Y + 	        |
| Controller 2 Left   	      | Joystick 1 D-Pad Down / Left X - 	        |
| Controller 2 Right  	      | Joystick 1 D-Pad Down / Left Y + 	        |
| Controller 2 A      	      | Joystick 1 A     	     	     	        |
| Controller 2 B      	      | Joystick 1 X			        |
| Controller 2 Select 	      | Joystick 1 Back 			        |
| Controller 2 Start  	      | Joystick 1 Start 			        |
| Controller 3 Up     	      | Joystick 2 D-Pad Up / Left Y - 	        |
| Controller 3 Down   	      | Joystick 2 D-Pad Down / Left Y + 	        |
| Controller 3 Left   	      | Joystick 2 D-Pad Down / Left X - 	        |
| Controller 3 Right  	      | Joystick 2 D-Pad Down / Left Y + 	        |
| Controller 3 A      	      | Joystick 2 A     	     	     	        |
| Controller 3 B      	      | Joystick 2 X			        |
| Controller 3 Select 	      | Joystick 2 Back 			        |
| Controller 3 Start  	      | Joystick 2 Start			        |
| Controller 4 Up     	      | Joystick 3 D-Pad Up / Left Y - 	        |
| Controller 4 Down   	      | Joystick 3 D-Pad Down / Left Y + 	        |
| Controller 4 Left   	      | Joystick 3 D-Pad Down / Left X - 	        |
| Controller 4 Right  	      | Joystick 3 D-Pad Down / Left Y + 	        |
| Controller 4 A      	      | Joystick 3 A     	     	     	        |
| Controller 4 B      	      | Joystick 3 X			        |
| Controller 4 Select 	      | Joystick 3 Back 			        |
| Controller 4 Start  	      | Joystick 3 Start			        |
| Arkanoid Dial               | Mouse, Joystick 0 Right X                 |
| Arkanoid Button             | Mouse Button 1, Joystick 0 Right Shoulder |
| Arkanoid Dial (Player 2)    | Joystick 1 Right X                        |
| Arkanoid Button (Player 2)  | Joystick 1 Right Shoulder                 |
| Power Pad (Port 2) Pad 1    | U 					        |
| Power Pad (Port 2) Pad 2    | I 					        |
| Power Pad (Port 2) Pad 3    | O 					        |
| Power Pad (Port 2) Pad 4    | P 					        |
| Power Pad (Port 2) Pad 5    | J 					        |
| Power Pad (Port 2) Pad 6    | K 					        |
| Power Pad (Port 2) Pad 7    | L 					        |
| Power Pad (Port 2) Pad 8    | ; 					        |
| Power Pad (Port 2) Pad 9    | M 					        |
| Power Pad (Port 2) Pad 10   | , 					        |
| Power Pad (Port 2) Pad 11   | . 					        |
| Power Pad (Port 2) Pad 12   | / 					        |
| Zapper (Port 2)    	      | Mouse 					        |
| Zapper Trigger (Port 2)     | Mouse Button 1 (Left Button) 		        |
| VS. Light Gun        	      | Mouse 	       	     			        |
| VS. Light Gun 	      | Mouse Button 1 (Left Button) 		        |
| Mouse (Port 2) X/Y          | Mouse 					        |
| Mouse (Port 2) Left Button  | Mouse Button 1 (Left Button) 		        |
| Mouse (Port 2) Right Button | Mouse Button 3 (Right Button) 		        |
| Famicom Keyboard Toggle     | Home					        |
| Famicom Keyboard Yen Key    | + 					        |
| Famicom Keyboard Stop Key   | End 					        |
| Famicom Keyboard Kana Key   | Left Alt 				        |
| Famicom Keyboard Grph Key   | Right Alt 				        |
| Load Newest State   	      | 0					        |
| Load State 1 	      	      | 1 					        |
| Load State 2 	      	      | 2 					        |
| Load State 3 	      	      | 3 					        |
| Load State 4 	      	      | 4 					        |
| Load State 5 	      	      | 5 					        |
| Load State 6 	      	      | 6 					        |
| Load State 7 	      	      | 7 					        |
| Load State 8 	      	      | 8 					        |
| Load State 9 	      	      | 9 					        |
| Save Oldest State   	      | Shift-0 				        |
| Save State 1 	      	      | Shift-1 				        |
| Save State 2 	      	      | Shift-2 				        |
| Save State 3 	      	      | Shift-3 				        |
| Save State 4 	      	      | Shift-4 				        |
| Save State 5 	      	      | Shift-5 				        |
| Save State 6 	      	      | Shift-6 				        |
| Save State 7 	      	      | Shift-7 				        |
| Save State 8 	      	      | Shift-8 				        |
| Save State 9 	      	      | Shift-9 				        |
| Soft Reset 	      	      | Shift-R 				        |
| Hard Reset 	      	      | Shift-T 				        |
| Toggle Fullscreen   	      | Alt-Enter 				        |
| Toggle Menubar 	      | Esc					        |
| Toggle FPS Display 	      | Ctrl-F					        |
| Pause/Resume Emulation      | Shift-P 				        |
| FDS Disk Eject/Insert       | Ctrl-F10 				        |
| FDS Disk Select 	      | F10 					        |
| Save Screenshot 	      | Alt-S 					        |
| Toggle Mouse Grab 	      | F11 					        |
| Port 1 Device Connect       | Ctrl-F6 				        |
| Port 1 Select 	      | F6 					        |
| Port 2 Device Connect	      | Ctrl-F7 				        |
| Port 2 Select 	      | F7 					        |
| Exp. Port Device Connect    | Ctrl-F8 				        |
| Exp. Port Device Select     | F8 					        |
| VS. Coin Switch  	      | F10 					        |
| VS. Service Switch 	      | Ctrl-F10 				        |
| Four-Player Mode Switch     | F9 					        |
| Alternate Speed             | Tab					        |
| Exit cxNES 	      	      | Alt-X 					        |

All other Famicom Keyboard keys are mapped to the key with the same
symbol on the keycap (e.g., 'U' is mapped to 'U').

Famicom Disk System
===================
Famicom Disk System support requires a copy of the Famicom Disk System BIOS. 
You can configure cxNES with the location of this file via the GUI
(Options -> Path Configuration -> FDS BIOS Path) or the config file (set fds_bios_path).
You can also put it in the directories listed below (which you use will depend on your
platform), but you must name the file 'disksys.rom' if you want cxNES to use it automatically.

Linux:  
* $HOME/.local/share/cxnes
* The global data directory for cxnes (/usr/local/share/cxnes by default, or
  /usr/share/cxnes if you installed from a package)

Windows:  
* %APPDATA%\cxnes
* The same folder as cxnes.exe


Credits
=======
Thanks to the nesdev community for their fantastic work at reverse-
engineering and documenting the NES.  Without the excellent information
found on the wiki, the various documents hosted on the nesdev.com website,
and the forum discussions, I would have been completely lost.

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
