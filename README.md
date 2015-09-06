cxNES Documentation

Version 0.10
Updated 2015-04-12

Website: http://github.com/perilsensitive/cxnes

Intro
=====
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
  + Can auto-save state at a particular interval
* PNG screenshots
* NSF player
* Dynamic audio resampling
* Cheat support (Game Genie, Pro Action Rocky, and raw formats)
  + Cheats can be auto-loaded/saved on ROM load/unload
* Autopatching support (IPS, UPS and BPS formats)
  + Supports patch stacking
* Soft-patching
* NTSC video filter and palette generator
* ROM Database (optional)
  + Allows ROMS with bad or missing headers to run if their
    checksums match the database.
  + Provides sane defaults for input devices or VS. Unisystem
    settings.
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
  + Can specify configuration options, patches and cheats on command-line


Default Bindings

| Action                      | Binding(s)                                      |
| --------------------------- | ---------------------------------------------   |
| Controller 1 Up     	      | Up, Joystick 0 D-Pad Up / Left Y -        |
| Controller 1 Down   	      | Down, Joystick 0 D-Pad Down / Left Y +    |
| Controller 1 Left   	      | Left, Joystick 0 D-Pad Down / Left X -    |
| Controller 1 Right  	      | Right, Joystick 0 D-Pad Down / Left Y +   |
| Controller 1 A      	      | C, Joystick 0 A 	      	       	        |
| Controller 1 B      	      | X, Joystick 0 X 			        |
| Controller 1 Select 	      | Z, Joystick 0 Back 	        |
| Controller 1 Start  	      | Enter, Joystick 0 Start 		        |
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
| Pause/Resume Emulation      | P 					        |
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
| Exit cxNES 	      	      | Alt-X 					        |

All other Famicom Keyboard keys are mapped to the key with the same
symbol on the keycap (e.g., 'U' is mapped to 'U').


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
