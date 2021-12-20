dnl ==================================================================
dnl Checks for CD-ROM
dnl ==================================================================

dnl XMMS Checks
AC_ARG_ENABLE(xmms,
	AS_HELP_STRING([--enable-xmms], [enable checking for XMMS])
)
if test "x$mingw" != xyes -a "x$enable_xmms" == xyes; then
	AM_PATH_XMMS(0.9.5.1, HAVE_XMMS=yes, HAVE_XMMS=no)
fi

CDTYPE=""
if test "x$HAVE_VORBIS" = xyes; then
	CDTYPE=" File"
	QF_NEED(cd,[file])
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
	QF_NEED(cd,[linux])
)

dnl XMMS audio
if test "x$HAVE_XMMS" = "xyes"; then
	CDTYPE="$CDTYPE XMMS"
	QF_NEED(cd,[xmms])
	AC_DEFINE(HAVE_XMMS, [1], [Define if you have XMMS])
fi

dnl SDL CD audio
if test "x$HAVE_SDL_CD" = "xyes"; then
	CDTYPE="$CDTYPE SDL"
	QF_NEED(cd,[sdl])
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
	QF_NEED(cd,[sgi])
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
	QF_NEED(cd,[win])
)
if test "x$CDTYPE" != "x"; then
	AC_MSG_RESULT([$CDTYPE])
else
	AC_MSG_RESULT([no])
fi

AC_SUBST(CD_LIBS)

AC_ARG_WITH([cd-default],
	AS_HELP_STRING([--with-cd-default=...],
				   [plugin to use for the default cd driver.]
				   [Defaults to file.]
				   [[file linux xmms sdl sgi win32]]),
	[cd_default="$withval"]
)
