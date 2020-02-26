AC_ARG_ENABLE(curses,
[  --disable-curses        disable curses support]
)
if test "x$enable_curses" != "xno"; then
	AC_CHECK_HEADER(curses.h)
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
	if test "x$CURSES_LIBS" != "x"; then
		AC_DEFINE(HAVE_CURSES, 1, [Define if you have the ncurses library])
		HAVE_CURSES=yes
	else
		HAVE_CURSES=no
	fi
else
	HAVE_CURSES=no
	CURSES_LIBS=
fi
AC_SUBST(CURSES_LIBS)

if test "x$HAVE_CURSES" == "xyes"; then
	AC_CHECK_HEADER(panel.h,
		[AC_CHECK_LIB(panel, new_panel,
			[AC_DEFINE(HAVE_PANEL, 1,
					[Define if you have the ncurses panel library])
				PANEL_LIBS=-lpanel
				HAVE_PANEL=yes],
			[HAVE_PANEL=no],
			$CURSES_LIBS
		)],
		HAVE_PANEL=no,
		[]
	)
else
	PANEL_LIBS=
fi
AC_SUBST(PANEL_LIBS)
