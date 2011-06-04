dnl ==================================================================
dnl Checks for CD-ROM
dnl ==================================================================

dnl XMMS Checks
AC_ARG_ENABLE(xmms,
[  --enable-xmms           enable checking for XMMS],
)
if test "x$mingw" != xyes -a "x$enable_xmms" == xyes; then
	AM_PATH_XMMS(0.9.5.1,
		HAVE_XMMS=yes,
		HAVE_XMMS=no)
	AC_SUBST(HAVE_XMMS)
fi	
AC_SUBST(CD_CFLAGS)


CDTYPE=""
CD_CFLAGS=""
CD_PLUGIN_TARGETS=""
if test "x$HAVE_VORBIS" = xyes; then
	CDTYPE=" OGG"
	CD_PLUGIN_TARGETS="cd_file.la"
fi

AC_MSG_CHECKING(for CD audio support)

dnl Linux
AC_EGREP_CPP([QF_maGiC_VALUE],
	[
#include <linux/cdrom.h>
#ifdef CDROMREADTOCENTRY
QF_maGiC_VALUE
#endif
	],
	CDTYPE="$CDTYPE Linux"
	CD_PLUGIN_TARGETS="$CD_PLUGIN_TARGETS cd_linux.la"
)

dnl XMMS audio
if test "x$HAVE_XMMS" = "xyes"; then
	CDTYPE="$CDTYPE XMMS"
	CD_PLUGIN_TARGETS="$CD_PLUGIN_TARGETS cd_xmms.la"
	AC_DEFINE(HAVE_XMMS, [1], [Define if you have XMMS])
fi

dnl SDL CD audio
if test "x$HAVE_SDL_CD" = "xyes"; then
	CDTYPE="$CDTYPE SDL"
	CD_PLUGIN_TARGETS="$CD_PLUGIN_TARGETS cd_sdl.la"
fi

dnl SGI
AC_EGREP_CPP([QF_maGiC_VALUE],
	[
#include <dmedia/cdaudio.h>
#ifdef CD_READY
QF_maGiC_VALUE
#endif
	],
	CDTYPE="$CDTYPE SGI"
	CD_PLUGIN_TARGETS="$CD_PLUGIN_TARGETS cd_sgi.la"
	SGI_CD_LIBS="-lcdaudio -lmediad -lds"
)
AC_SUBST(SGI_CD_LIBS)

AC_EGREP_CPP([QF_maGiC_VALUE],
	[
#include <windows.h>
#include <mmsystem.h>
#if defined(MCI_SET_DOOR_OPEN)
QF_maGiC_VALUE
#endif
	],
	CDTYPE="$CDTYPE WIN32"
	CD_PLUGIN_TARGETS="$CD_PLUGIN_TARGETS cd_win.la"
)
if test "x$CDTYPE" != "x"; then
	AC_MSG_RESULT([$CDTYPE])
else
	AC_MSG_RESULT([no])
fi

AC_SUBST(CD_LIBS)
