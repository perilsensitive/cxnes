AM_CFLAGS = -I ./include -Wall -Werror=format-security \
            -DPACKAGE_DATADIR='"$(pkgdatadir)"' -DDATADIR='"$(datadir)"' \
            -DDEFAULT_OSD_FONT='"PressStart2P.ttf"' -std=gnu99
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
	input/konami_hyper_shot.c \
	main/archive.c \
	main/archive_null.c \
	main/archive_7zip.c \
	main/archive_zip.c \
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
	main/scalebit.c \
	main/scale2x.c \
	main/scale3x.c \
	main/hq2x.c \
	main/hq3x.c \
	main/hq4x.c \
	main/hqx_init.c \
	main/blip_buf.c \
	main/state.c \
	main/nes_ntsc.c \
	main/text_buffer.c \
	main/7z/7zAlloc.c \
	main/7z/7zBuf2.c \
	main/7z/7zCrcOpt.c \
	main/7z/7zFile.c \
	main/7z/7zStream.c \
	main/7z/Bra.c \
	main/7z/CpuArch.c \
	main/7z/LzmaDec.c \
	main/7z/Ppmd7Dec.c \
	main/7z/7zBuf.c \
	main/7z/7zCrc.c \
	main/7z/7zDec.c \
	main/7z/7zIn.c \
	main/7z/Bcj2.c \
	main/7z/Bra86.c \
	main/7z/Lzma2Dec.c \
	main/7z/Ppmd7.c \
	boards/unl_ac08.c \
	boards/unl_cc_21.c \
	boards/unl_smb2j.c \
	boards/btl_smb2a.c \
	boards/btl_smb2b.c \
	boards/btl_smb2c.c \
	boards/whirlwind.c \
	boards/unl_lh32.c \
	boards/unl_bb.c \
	boards/subor.c \
	boards/76in1.c \
	boards/cufrom.c \
	boards/bmc_150in1.c \
	boards/magicseries.c \
	boards/mortalkombat2.c \
	boards/bmc_super700in1.c \
	boards/gamestar.c \
	boards/cne_psb.c \
	boards/jycompany.c \
	boards/waixing_sgzlz.c \
	boards/bmc_110in1.c \
	boards/rcm.c \
	boards/bmc_35in1.c \
	boards/22in1.c \
	boards/bmc_1200in1.c \
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
	boards/ntdec_112.c \
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
	boards/waixing_ps2.c \
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
if WINDOWS
cxnes_SOURCES += sdl/gtk/gtk_prep_win32_drawing_area.c
else
cxnes_SOURCES += sdl/gtk/gtk_prep_x11_drawing_area.c
endif
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
	mkdir -p $(DESTDIR)$(pkgdatadir)/data
	mkdir -p $(DESTDIR)$(pkgdatadir)/doc
	sed 's/$$/\r/' COPYING > $(DESTDIR)$(pkgdatadir)/COPYING.txt
	for f in doc/*.txt; do \
		sed 's/$$/\r/' $$f > $(DESTDIR)$(pkgdatadir)/$$f; \
	done
	sed 's/$$/\r/' README.md > $(DESTDIR)$(pkgdatadir)/README.txt
	mv $(DESTDIR)$(pkgdatadir)/romdb.txt $(DESTDIR)$(pkgdatadir)/data
	mv $(DESTDIR)$(pkgdatadir)/gamecontrollerdb.txt $(DESTDIR)$(pkgdatadir)/data
	mv $(DESTDIR)$(pkgdatadir)/nsf.rom $(DESTDIR)$(pkgdatadir)/data
	mv $(DESTDIR)$(pkgdatadir)/*.ttf $(DESTDIR)$(pkgdatadir)/data
	sed -i.bak 's/$$/\r/' $(DESTDIR)$(pkgdatadir)/data/romdb.txt && \
	  rm -f $(DESTDIR)$(pkgdatadir)/data/romdb.txt.bak
	sed -i.bak 's/$$/\r/' $(DESTDIR)$(pkgdatadir)/data/gamecontrollerdb.txt && \
	  rm -f $(DESTDIR)$(pkgdatadir)/data/gamecontrollerdb.txt.bak
	mv $(DESTDIR)$(bindir)/cxnes.exe $(DESTDIR)$(pkgdatadir)/cxnes.exe
	cp *.dll $(DESTDIR)$(pkgdatadir)
	cp -r share $(DESTDIR)$(pkgdatadir)
	cp -r data/icons $(DESTDIR)$(pkgdatadir)/data
	(cd $(DESTDIR)$(pkgdatadir) && zip -r ../../$(PACKAGE)-$(VERSION)-$(ZIP_ARCH).zip .)
endif
