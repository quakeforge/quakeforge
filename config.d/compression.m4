AC_ARG_ENABLE(flac, AS_HELP_STRING([--disable-flac], [disable flac support]))
HAVE_FLAC=no
if test "x$enable_flac" != "xno"; then
  if test "x$PKG_CONFIG" != "x"; then
    PKG_CHECK_MODULES([FLAC], [flac], HAVE_FLAC=yes, HAVE_FLAC=no)
  else
    AM_PATH_LIBFLAC(HAVE_FLAC=yes, HAVE_FLAC=no)
  fi
  if test "x$HAVE_FLAC" = xyes; then
    AC_DEFINE(HAVE_FLAC, 1, [define this if you have flac libs])
  fi
fi
AM_CONDITIONAL(HAVE_FLAC, test "x$HAVE_FLAC" = "xyes")

AC_ARG_ENABLE(wildmidi,
	AS_HELP_STRING([--disable-wildmidi], disable libWildMidi support]))
HAVE_WILDMIDI=no
WM_LIBS=
if test "x$enable_wildmidi" != "xno"; then
  AC_CHECK_LIB(WildMidi, WildMidi_GetString, HAVE_WILDMIDI=yes, HAVE_WILDMIDI=no)
  if test "x$HAVE_WILDMIDI" = "xyes"; then
    AC_CHECK_HEADER(wildmidi_lib.h, HAVE_WILDMIDI=yes, HAVE_WILDMIDI=no)
    if test "x$HAVE_WILDMIDI" = "xyes"; then
      WM_LIBS="-lWildMidi"
      AC_DEFINE(HAVE_WILDMIDI, 1, [Define if you have WildMidi])
    fi
  fi
fi							
AC_SUBST(WM_LIBS)
AM_CONDITIONAL(HAVE_MIDI, test "x$HAVE_WILDMIDI" = "xyes")

AC_ARG_ENABLE(vorbis,
	AS_HELP_STRING([--disable-vorbis], [disable ogg vorbis support]))
HAVE_VORBIS=no
if test "x$enable_vorbis" != "xno"; then
  if test "x$PKG_CONFIG" != "x"; then
    PKG_CHECK_MODULES([OGG], [ogg], HAVE_OGG=yes, HAVE_OGG=no)
	if test "x$HAVE_OGG" = xyes; then
	  PKG_CHECK_MODULES([VORBIS], [vorbis], HAVE_VORBIS=yes, HAVE_VORBIS=no)
	  if test "x$HAVE_VORBIS" = xyes; then
	    PKG_CHECK_MODULES([VORBISFILE], [vorbisfile], HAVE_VORBISFILE=yes, HAVE_VORBISFILE=no)
	    AC_DEFINE(HAVE_VORBIS, 1, [define this if you have ogg/vorbis libs])
	  fi
	fi
  else
    XIPH_PATH_OGG(HAVE_OGG=yes, HAVE_OGG=no)
	if test "x$HAVE_OGG" = xyes; then
	  XIPH_PATH_VORBIS(HAVE_VORBIS=yes, HAVE_VORBIS=no)
	  if test "x$HAVE_VORBIS" = xyes; then
	    AC_DEFINE(HAVE_VORBIS, 1, [define this if you have ogg/vorbis libs])
	  fi
	fi
  fi
fi
AM_CONDITIONAL(HAVE_VORBIS, test "x$HAVE_VORBIS" = "xyes")


AC_ARG_ENABLE(zlib, AS_HELP_STRING([--disable-zlib], [disable zlib support]))
HAVE_ZLIB=no
Z_LIBS=""
if test "x$enable_zlib" != "xno"; then
	if test "x$PKG_CONFIG" != "x"; then
		PKG_CHECK_MODULES([Z], [zlib], HAVE_ZLIB=yes, HAVE_ZLIB=no)
	else
		dnl Check for working -lz
		dnl Note - must have gztell *and* gzgets in -lz *and* zlib.h
		AC_CHECK_LIB(z, gztell, HAVE_ZLIB=yes, HAVE_ZLIB=no, [$LIBS])
		if test "x$HAVE_ZLIB" = "xyes"; then
			 AC_CHECK_LIB(z, gzgets, HAVE_ZLIB=yes, HAVE_ZLIB=no, [$LIBS])
			 if test "x$HAVE_ZLIB" = "xyes"; then
				  AC_CHECK_HEADER(zlib.h, HAVE_ZLIB=yes Z_LIBS=-lz,
								  HAVE_ZLIB=no)
			 fi
		fi
	fi
fi
AC_SUBST(Z_LIBS)
if test "x$HAVE_ZLIB" = "xyes"; then
	 AC_DEFINE(HAVE_ZLIB, 1, [Define if you have zlib])
fi

AC_ARG_ENABLE(png, AS_HELP_STRING([--disable-png], [disable png support]))
HAVE_PNG=no
PNG_LIBS=""
if test "x$enable_png" != "xno"; then
	if test "x$PKG_CONFIG" != "x"; then
		PKG_CHECK_MODULES([PNG], [libpng], HAVE_PNG=yes, HAVE_PNG=no)
	else
		AC_CHECK_LIB(png, png_set_read_fn, HAVE_PNG=yes, HAVE_PNG=no, [$LIBS])
		if test "x$HAVE_PNG" = "xyes"; then
			AC_CHECK_HEADER(png.h, HAVE_PNG=yes PNG_LIBS="-lpng", HAVE_PNG=no)
		fi
	fi
fi
AC_SUBST(PNG_LIBS)
if test "x$HAVE_PNG" = "xyes"; then
	AC_DEFINE(HAVE_PNG, 1, [Define if you have libpng])
fi
