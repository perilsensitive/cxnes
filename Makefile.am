AM_CFLAGS = --param large-function-growth=600 -I ./include -Wall -Werror=format-security \
            -DPACKAGE_DATADIR='"$(pkgdatadir)"' -DDATADIR='"$(datadir)"' \
            -DDEFAULT_OSD_FONT='"PressStart2P.ttf"'
AM_LDFLAGS = -lm

.rc.o:
	windres $(WINDRESFLAGS) -o $@ $<


pkgdata_DATA = data/PressStart2P.ttf data/gamecontrollerdb.txt data/nsf.rom data/romdb.txt
if !WINDOWS

dist_doc_DATA = \
	COPYING \
	README.md \
	doc/example.cfg \
	doc/icon_license.txt \
	doc/icon_readme.txt \
	doc/PressStart2P_LICENSE.txt 

iconsdir = $(pkgdatadir)/icons
dist_icons_DATA = \
	data/icons/cxnes.svg \
	data/icons/cxnes.png \
	data/icons/cxnes32.png \
	data/icons/cxnes48.png \
	data/icons/cxnes64.png \
	data/icons/cxnes96.png \
	data/icons/cxnes128.png

pixmapsdir = $(datarootdir)/pixmaps
dist_pixmaps_DATA = data/icons/cxnes.svg

applicationsdir = $(datarootdir)/applications
dist_applications_DATA = data/cxnes.desktop
endif

bin_PROGRAMS = cxnes
cxnes_SOURCES = \
	sdl/main.c \
	sdl/file_io.c \
	sdl/log.c \
	sdl/sdl_audio.c \
	sdl/sdl_input.c \
	sdl/sdl_video.c \
	input/vs_unisystem.c \
	input/arkanoid.c \
	input/keyboard.c \
	input/controller.c \
	input/zapper.c \
	input/mat.c \
	input/snes_mouse.c \
	input/microphone.c \
	main/board.c \
	main/crc32.c \
	main/sha1.c \
	main/db.c \
	main/emu.c \
	main/nsf.c \
	main/patch.c \
	main/unif.c \
	main/ines.c \
	main/split_rom.c \
	main/input.c \
	main/palette.c \
	main/config.c \
	main/cheat.c \
	main/io.c \
	main/apu.c \
	main/rom.c \
	main/cpu.c \
	main/fds.c \
	main/ppu.c \
	main/blip_buf.c \
	main/state.c \
	main/nes_ntsc.c \
	main/text_buffer.c \
	boards/subor.c \
	boards/76in1.c \
	boards/cufrom.c \
	boards/150in1.c \
	boards/magicseries.c \
	boards/mortalkombat2.c \
	boards/super700in1.c \
	boards/gamestar.c \
	boards/cne_psb.c \
	boards/jycompany.c \
	boards/waixing_sgzlz.c \
	boards/ines225.c \
	boards/35in1.c \
	boards/22in1.c \
	boards/1200in1.c \
	boards/20in1.c \
	boards/sachen_tcu01.c \
	boards/sachen_72007.c \
	boards/sachen_72008.c \
	boards/sachen_nrom.c \
	boards/sachen_8259.c \
	boards/sachen_74x374b.c \
	boards/action52.c \
	boards/maxi15.c \
	boards/jaleco_jf14.c \
	boards/irem_g101.c \
	boards/vrc3.c \
	boards/74x161_161_32.c \
	boards/agci_50282.c \
	boards/bandai.c \
	boards/vrc7.c \
	boards/vs_unisystem.c \
	boards/74x139_74.c \
	boards/nsf.c \
	boards/caltron_6in1.c \
	boards/vrc6.c \
	boards/vrc4.c \
	boards/mmc5.c \
	boards/nrom.c \
	boards/jaleco_jf16.c \
	boards/mmc4.c \
	boards/sunsoft4.c \
	boards/singlechip.c \
	boards/taito_x1_017.c \
	boards/irem_tam_s1.c \
	boards/jaleco_ss88006.c \
	boards/irem_h3001.c \
	boards/timer/a12_timer.c \
	boards/timer/m2_timer.c \
	boards/ines201.c \
	boards/jaleco_jf13.c \
	boards/namco108.c \
	boards/cprom.c \
	boards/audio/emu2413.c \
	boards/audio/mmc5_audio.c \
	boards/audio/fds_audio.c \
	boards/audio/sunsoft5b_audio.c \
	boards/audio/vrc6_audio.c \
	boards/audio/vrc7_audio.c \
	boards/audio/namco163_audio.c \
	boards/bnrom.c \
	boards/sunsoft1.c \
	boards/sunsoft5b.c \
	boards/sunsoft3.c \
	boards/ntdec_193.c \
	boards/gxrom.c \
	boards/nina001.c \
	boards/taito_tc0190fmc.c \
	boards/streemerz_bundle.c \
	boards/mmc3.c \
	boards/mmc1.c \
	boards/namco163.c \
	boards/ines36.c \
	boards/uxrom.c \
	boards/unrom512.c \
	boards/pci556.c \
	boards/cnrom.c \
	boards/sunsoft2.c \
	boards/colordreams.c \
	boards/contra_100in1.c \
	boards/vrc1.c \
	boards/axrom.c \
	boards/fds.c \
	boards/camerica.c \
	boards/taito_x1_005.c \
	boards/nina003-006.c \
	boards/irem_74x161_161_21_138.c \
        boards/inlnsf.c

if GUI_ENABLED
cxnes_SOURCES += sdl/gtk/gtkmain.c sdl/gtk/gtkconfig.c sdl/gtk/gtkvideoconfig.c sdl/gtk/gtkcheats.c sdl/gtk/gtkbindingconfig.c sdl/gtk/gtkmenubar.c sdl/gtk/gtkjoystickinfo.c
endif

if ZIP_ENABLED
cxnes_SOURCES += main/unzip.c main/ioapi.c
endif

if HAVE_WINDRES
cxnes_SOURCES += sdl/gtk/windows_resources.rc

clean-local:
	rm -rf $(DESTDIR)$(pkgdatadir)
	rm -rf $(PACKAGE)-$(VERSION)-$(ZIP_ARCH).zip

install-data-hook:
	rm -rf $(PACKAGE)-$(VERSION)-$(ZIP_ARCH).zip
	sed 's/$$/\r/' COPYING > $(DESTDIR)$(pkgdatadir)/COPYING.txt
	sed 's/$$/\r/' doc/icon_license.txt > $(DESTDIR)$(pkgdatadir)/icon_license.txt
	sed 's/$$/\r/' doc/icon_readme.txt > $(DESTDIR)$(pkgdatadir)/icon_readme.txt
	sed 's/$$/\r/' README.md > $(DESTDIR)$(pkgdatadir)/README.txt
	sed -i.bak 's/$$/\r/' $(DESTDIR)$(pkgdatadir)/romdb.txt && \
	  rm -f $(DESTDIR)$(pkgdatadir)/romdb.txt.bak
	sed -i.bak 's/$$/\r/' $(DESTDIR)$(pkgdatadir)/gamecontrollerdb.txt && \
	  rm -f $(DESTDIR)$(pkgdatadir)/gamecontrollerdb.txt.bak
	cd $(DESTDIR)$(pkgdatadir) && mkdir -p data/$(PACKAGE_NAME)
	mv $(DESTDIR)$(bindir)/cxnes.exe $(DESTDIR)$(pkgdatadir)/cxnes.exe
	cp *.dll $(DESTDIR)$(pkgdatadir)
	cp -r share $(DESTDIR)$(pkgdatadir)
	(cd $(DESTDIR)$(pkgdatadir) && zip -r ../../$(PACKAGE)-$(VERSION)-$(ZIP_ARCH).zip .)
endif
