AC_CHECK_LIB([backtrace], [backtrace_create_state],
	[HAVE_BACKTRACE=yes
	 BACKTRACE_LIBS=-lbacktrace],
	[HAVE_BACKTRACE=no])
AC_SUBST(BACKTRACE_LIBS)
