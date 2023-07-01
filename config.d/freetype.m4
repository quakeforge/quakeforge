AC_ARG_ENABLE(freetype,
	AS_HELP_STRING([--disable-freetype], [disable freetype support]))
if test "x$enable_freetype" != "xno"; then
  if test "x$PKG_CONFIG" != "x"; then
    PKG_CHECK_MODULES([FREETYPE], [freetype2], HAVE_FREETYPE=yes, HAVE_FREETYPE=no)
  fi
else
	HAVE_FREETYPE=no
	FREETYPE_LIBS=
fi
AC_SUBST(FREETYPE_LIBS)

if test "x$HAVE_FREETYPE" == "xyes"; then
	AC_DEFINE(HAVE_FREETYPE, 1, [Define if you have the freetype library])
fi


AC_ARG_ENABLE(harfbuzz,
	AS_HELP_STRING([--disable-harfbuzz], [disable HarfBuzz support]))
if test "x$enable_harfbuzz" != "xno"; then
  if test "x$PKG_CONFIG" != "x"; then
    PKG_CHECK_MODULES([HARFBUZZ], [harfbuzz], HAVE_HARFBUZZ=yes, HAVE_HARFBUZZ=no)
  fi
else
	HAVE_HARFBUZZ=no
	HARFBUZZ_LIBS=
fi
AC_SUBST(HARFBUZZ_LIBS)

if test "x$HAVE_HARFBUZZ" == "xyes"; then
	AC_DEFINE(HAVE_HARFBUZZ, 1, [Define if you have the HarfBuzz library])
fi

AC_ARG_ENABLE(fontconfig,
	AS_HELP_STRING([--disable-fontconfig], [disable fontconfig support]))
if test "x$enable_fontconfig" != "xno"; then
  if test "x$PKG_CONFIG" != "x"; then
    PKG_CHECK_MODULES([FONTCONFIG], [fontconfig], HAVE_FONTCONFIG=yes, HAVE_FONTCONFIG=no)
  fi
else
	HAVE_FONTCONFIG=no
	FONTCONFIG_LIBS=
fi
AC_SUBST(FONTCONFIG_LIBS)

if test "x$HAVE_FONTCONFIG" == "xyes"; then
	AC_DEFINE(HAVE_FONTCONFIG, 1, [Define if you have the fontconfig library])
fi
