if test "x$PKG_CONFIG" != "x"; then
	PKG_CHECK_MODULES([BACKTRACE], [libbacktrace],
					  HAVE_BACKTRACE=yes,
					  HAVE_BACKTRACE=no)
fi
if test "x$HAVE_BACKTRACE" != "xyes"; then
	# backtrace may be available even with no .pc
	AC_CHECK_LIB([backtrace], [backtrace_create_state],
		[HAVE_BACKTRACE=yes
		 BACKTRACE_CFLAGS=
		 BACKTRACE_LIBS=-lbacktrace],
		[HAVE_BACKTRACE=no])
fi
AC_SUBST(BACKTRACE_LIBS)
AC_SUBST(BACKTRACE_CFLAGS)
