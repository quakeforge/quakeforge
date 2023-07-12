dnl Checks for SVGALib support
AC_ARG_WITH(svga,
	AS_HELP_STRING([--with-svga@<:@=DIR@:>@], [use SVGALib found in DIR]),
	HAVE_SVGA=$withval, HAVE_SVGA=auto)
if test "x$HAVE_SVGA" != xno -a "x$HAVE_SVGA" != xauto; then
	if test "x$HAVE_SVGA" != xauto; then
		SVGA_CFLAGS="$SVGA_CFLAGS -I$withval/include"
		SVGA_LIBS="$SVGA_LIBS -L$withval/lib"
		dnl The default system location is /usr/include or /usr/local/include
		dnl and we (obviously) do not need to set CFLAGS for that
	fi
	save_CPPFLAGS="$CPPFLAGS"
	CPPFLAGS="$CPPFLAGS $SVGA_CFLAGS"
	AC_CHECK_HEADER(vga.h, HAVE_SVGA=yes, HAVE_SVGA=no)
	CPPFLAGS="$save_CPPFLAGS"

	if test "x$ASM_ARCH" = xyes; then
		dnl Make sure -lvga works
		if test "x$HAVE_SVGA" = xyes; then
			AC_CHECK_LIB(vga, vga_getmousetype, SVGA_LIBS="$SVGA_LIBS -lvga"
				HAVE_SVGA=yes, HAVE_SVGA=no, [$SVGA_LIBS]
			)
		fi 
		if test "x$HAVE_SVGA" != xyes; then
			SVGA_CFLAGS="" SVGA_LIBS=""
		fi
	else
		HAVE_SVGA=no
		SVGA_CFLAGS=
		SVGA_LIBS=
	fi
fi
AC_SUBST(SVGA_CFLAGS)
AC_SUBST(SVGA_LIBS)
