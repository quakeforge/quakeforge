AC_ARG_ENABLE(ibus,
	AS_HELP_STRING([--disable-ibus], [disable ibus support]))
if test "x$enable_ibus" != "xno"; then
  if test "x$PKG_CONFIG" != "x"; then
    PKG_CHECK_MODULES([IBUS], [ibus-1.0], HAVE_IBUS=yes, HAVE_IBUS=no)
  else
	AC_MSG_WARN([cannot check for ibus])
	HAVE_IBUS=no
	IBUS_LIBS=
	IBUS_CFLAGS=
  fi
else
	HAVE_IBUS=no
	IBUS_LIBS=
	IBUS_CFLAGS=
fi
AC_SUBST(IBUS_LIBS)
AC_SUBST(IBUS_CFLAGS)
AC_DEFINE(HAVE_IBUS, 1, [Define if you have ibus])
