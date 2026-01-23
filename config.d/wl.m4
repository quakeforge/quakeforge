AC_ARG_ENABLE(wayland,
	AS_HELP_STRING([--disable-wayland], [disable Wayland support]))
if test "x$enable_wayland" != "xno"; then
  if test "x$PKG_CONFIG" != "x"; then
    PKG_CHECK_MODULES([WAYLAND], [wayland-client >= 1.6.0 xkbcommon], HAVE_WAYLAND=yes, HAVE_WAYLAND=no)
  fi
else
	HAVE_WAYLAND=no
	WAYLAND_LIBS=
fi
AC_SUBST(WAYLAND_LIBS)

