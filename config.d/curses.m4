AC_ARG_ENABLE(curses,
[  --disable-curses        disable curses support]
)
if test "x$enable_curses" != "xno"; then
	AC_CHECK_HEADERS(curses.h)
	AC_CHECK_LIB(ncurses, initscr,
		CURSES_LIBS=-lncurses,
		AC_CHECK_LIB(pdcurses, initscr,
			CURSES_LIBS=-lpdcurses,
			AC_CHECK_LIB(curses, initscr,
				CURSES_LIBS=-lcurses,
				CURSES_LIBS=
			)
		)
	)
else
	CURSES_LIBS=
fi
AC_SUBST(CURSES_LIBS)
