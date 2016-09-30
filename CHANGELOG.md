[0.3.3] - 2016-05-10
- added: support for the FDS BIOS being placed the same directory as the ROM
- added: support for SNES Controllers
- fixed: iNES header generation
- fixed: MMC5 IRQ timing
- fixed: set background to black before blitting each frame
- fixed: segfault when opening a new ROM
- fixed: bugs in mappers 36 and 185
- fixed: various bugs in input handling
- fixed: VRC2 mirroring register
- fixed: portable mode on Windows
- fixed: bugs in handling of joystick removal

[0.3.2] - 2016-03-03
- fixed: mirroring for Holy Diver

[0.3.1] - 2016-03-02
- updated gamecontroller db
- cleaned up default keybindings
- fixed: compiler warnings
- fixed: various save state bugs
- fixed: sprite zero flag timing bug
- fixed: redraw window on expose events

[0.3.0] - 2016-02-11
- added: enable vs_zapper by default for games that use it
- added: support for PlayChoice ROMs in the database
- added: support GameControllers with extra axes, buttons or hats
- added: support for split ROMs
- added: support for iNES mappers 29, 39, 44, 49, 58, 62, 91, 107, 166, 167 and 202
- added: support for Fighting Heroes III
- added: support for VRC7 audio
- added: save state support for input state
- updated gamecontroller db
- fallback to iNES when NES 2.0 header has no NV ram but has battery
- reset memory on hard reset
- fixed: various ROM loading bugs
- fixed: various FDS bugs
- fixed: FDS auto eject support for certain games
- fixed: Make preferred console type work again
- fixed: various input handling bugs
- fixed: NSF file handling bugs
- fixed: various Vs. System bugs
- fixed: handling of system type and input devices
- fixed: palette bugs
- fixed: cursor hiding when mouse grabbed
- fixed: bugs in iNES mappers 15, 112 and 226
- fixed: arkanoid controller support
- fixed: various save state bugs

[0.2.0] - 2015-10-22
- added: support for zipped ROMs
- fixed: bug when clearing all bindings for a given action
- fixed: case-sensitivity bug with file globs
- fixed: error when destroying grab event dialog

[0.1.3] - 2015-10-19
- fixed: A12 timer bugs
- fixed: path handling bugs
- fixed: ROM loading bugs
- fixed: bugs in famicom four-player mode
- cleaned up input binding configuration dialog

[0.1.2] - 2015-10-15
- cleaned up input category names
- fixed: GTK menu bugs
- fixed: A12 timer bug

[0.1.1] - 2015-10-12
- added: support for alternate emulation speed
- update game controller db
- fixed bug in create_directory

0.1.0 - 2015-09-30
- public source code release

[0.3.3]: https://github.com/perilsensitive/cxnes/compare/v0.3.2...v0.3.3
[0.3.2]: https://github.com/perilsensitive/cxnes/compare/v0.3.1...v0.3.2
[0.3.1]: https://github.com/perilsensitive/cxnes/compare/v0.3.0...v0.3.1
[0.3.0]: https://github.com/perilsensitive/cxnes/compare/v0.2.0...v0.3.0
[0.2.0]: https://github.com/perilsensitive/cxnes/compare/v0.1.3...v0.2.0
[0.1.3]: https://github.com/perilsensitive/cxnes/compare/v0.1.2...v0.1.3
[0.1.2]: https://github.com/perilsensitive/cxnes/compare/v0.1.1...v0.1.2
[0.1.1]: https://github.com/perilsensitive/cxnes/compare/v0.1.0...v0.1.1

