AC_ARG_ENABLE(backtrace,
	AS_HELP_STRING(
		[--disable-backtrace],
		[disable use of libbacktrace]
	)
)
HAVE_BACKTRACE=no
if test "x$enable_backtrace" = "xyes"; then
AC_CHECK_LIB([backtrace], [backtrace_create_state],
	[HAVE_BACKTRACE=yes
	 BACKTRACE_LIBS=-lbacktrace],
	[HAVE_BACKTRACE=no])
AC_SUBST(BACKTRACE_LIBS)
fi
AM_CONDITIONAL(HAVE_BACKTRACE, test "x$HAVE_BACKTRACE" = "xyes")

