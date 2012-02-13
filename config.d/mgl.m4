dnl Checks for MGL support
AC_ARG_WITH(mgl,
[  --with-mgl=DIR          use MGL found in DIR],
HAVE_MGL=$withval, HAVE_MGL=auto)
if test "x$HAVE_MGL" != xno; then
	if test "x$ac_cv_header_windows_h" != "xyes"; then
		HAVE_MGL=no
	else
		if test "x$HAVE_MGL" != xauto; then
			MGL_CFLAGS="$MGL_CFLAGS -I$withval/include"
			MGL_LIBS="$MGL_LIBS -L$withval/lib"
		fi
		save_CPPFLAGS="$CPPFLAGS"
		CPPFLAGS="$CPPFLAGS $MGL_CFLAGS"
		AC_CHECK_HEADER(mgraph.h, HAVE_MGL=yes, HAVE_MGL=no)
		CPPFLAGS="$save_CPPFLAGS"

		dnl Make sure -lmgllt or -lmglfx works
		if test "x$HAVE_MGL" = xyes; then
			for lib in mglfx mgllt; do 
				MGL_LIBS="$MGL_LIBS -lgdi32 -lwinmm -ldinput -lddraw"
				AC_CHECK_LIB($lib, MGL_registerDriver,
					MGL_LIBS="-l$lib $MGL_LIBS"
					HAVE_MGL=yes
					break,
					HAVE_MGL=no,
					[$MGL_LIBS]
				)
			done
		fi
	fi
	if test "x$HAVE_MGL" != xyes; then
		MGL_CFLAGS="" MGL_LIBS=""
	fi
fi
AC_SUBST(MGL_CFLAGS)
AC_SUBST(MGL_LIBS)
