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
