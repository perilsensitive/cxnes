AC_INIT([cxnes], [0.1.0])
AM_INIT_AUTOMAKE([subdir-objects])
AM_SILENT_RULES([yes])
AC_CANONICAL_HOST

AM_CONDITIONAL(WINDOWS, [ test "$host_os" = mingw || test $host_os = cygwin ])

CC_LIST="gcc cc"
EXTRA_GUI_LIBS=""
case $host_os in
	mingw32|cygwin)
		# This is an ugly hack, but the windows build is not meant
		# to be used like a *nix program so the standard rules don't
		# apply.  We want everything to be copied to one directory
		# so that the program is portable.
		AC_SUBST([bindir], ['portable'])
		AC_SUBST([datarootdir], ['portable'])
		AC_SUBST([docdir], ['portable'])
		pkgdatadir="$(datarootdir)"
		
		CFLAGS="$CFLAGS -mwindows -D__USE_MINGW_ANSI_STDIO=1"
		LIBS="$LIBS -lshlwapi"
		if test $host_cpu = x86_64
		then
				CC_LIST="x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc"
		elif test $host_cpu = i686
		then
				CC_LIST="i686-pc-mingw32-gcc"
		fi
		;;
	*)
		EXTRA_GUI_LIBS="-lX11 -lGL"
		;;
esac

AC_PROG_CC([$CC_LIST])

case $CC in
	*i686-w64-mingw32-gcc*)
		AC_SUBST(WINDRESFLAGS, "-F pe-i386")
		if test x$PKG_CONFIG_PATH = 'x'
		then
			PKG_CONFIG_PATH=/usr/i686-w64-mingw32/sys-root/mingw/lib/pkgconfig
			export PKG_CONFIG_PATH
		fi
		;;
	*i686-pc-mingw32-gcc*)
		AC_SUBST(WINDRESFLAGS, "-F pe-i386")
		if test x$PKG_CONFIG_PATH = 'x'
		then
			PKG_CONFIG_PATH=/usr/i686-pc-mingw32/sys-root/mingw/lib/pkgconfig
			export PKG_CONFIG_PATH
		fi
		;;
	*x86_64-w64-mingw32-gcc*)
		AC_SUBST(WINDRESFLAGS, "-F pe-x86-64")
		if test x$PKG_CONFIG_PATH = 'x'
		then
			PKG_CONFIG_PATH=/usr/x86_64-w64-mingw32/sys-root/mingw/lib/pkgconfig
			export PKG_CONFIG_PATH
		fi
		;;
esac

SDL_VERSION=2.0.0
GTK_VERSION=3.0.0

dnl Check for GTK+ for gui
AC_ARG_ENABLE(gui,
  [AS_HELP_STRING([--enable-gui],
    [enable the GTK+ user interface (default enabled)])])
AM_CONDITIONAL([GUI_ENABLED], [test "${enable_gui}" != "no"])

AM_CONDITIONAL(HAVE_WINDRES, which windres > /dev/null)

AM_PATH_SDL2($SDL_VERSION,:,
	AC_MSG_ERROR([*** SDL version $SDL_VERSION not found!])
)

if test "${enable_gui}" != "no"
then
  AM_PATH_GTK_3_0($GTK_VERSION,:,
	AC_MSG_ERROR([*** GTK version $GTK_VERSION not found!])
)
fi

PKG_CHECK_MODULES([SDL2_ttf], [SDL2_ttf])
PKG_CHECK_MODULES([SDL2_image], [SDL2_image])

CFLAGS="$CFLAGS $SDL2_ttf_CFLAGS $SDL2_image_CFLAGS"
LIBS="$LIBS $SDL2_ttf_LIBS $SDL2_image_LIBS"

AM_COND_IF([GUI_ENABLED], [CFLAGS="$CFLAGS $GTK_CFLAGS -DGUI_ENABLED"])
AM_COND_IF([GUI_ENABLED], [LIBS="$LIBS $GTK_LIBS $EXTRA_GUI_LIBS"])

AC_OUTPUT([Makefile])
