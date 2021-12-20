AC_ARG_ENABLE(curses,
	AS_HELP_STRING([--disable-curses], [disable curses support]))
if test "x$enable_curses" != "xno"; then
  if test "x$PKG_CONFIG" != "x"; then
    PKG_CHECK_MODULES([NCURSES], [ncurses], HAVE_NCURSES=yes, HAVE_NCURSES=no)
  else
	AC_CHECK_HEADER([curses.h], [],
					[AC_CHECK_HEADER([ncurses/curses.h],
						[NCURSES_CFLAGS=-I${prefix}/include/ncurses])])
	AC_CHECK_LIB(ncurses, initscr,
		NCURSES_LIBS=-lncurses,
		AC_CHECK_LIB(pdcurses, initscr,
			NCURSES_LIBS=-lpdcurses,
			AC_CHECK_LIB(curses, initscr,
				NCURSES_LIBS=-lcurses,
				NCURSES_LIBS=
			)
		)
	)
	if test "x$NCURSES_LIBS" != "x"; then
		HAVE_NCURSES=yes
	else
		HAVE_NCURSES=no
	fi
  fi
else
	HAVE_NCURSES=no
	NCURSES_LIBS=
fi
AC_SUBST(NCURSES_LIBS)

if test "x$HAVE_NCURSES" == "xyes"; then
	AC_DEFINE(HAVE_NCURSES, 1, [Define if you have the ncurses library])
	if test "x$PKG_CONFIG" != "x"; then
		PKG_CHECK_MODULES([PANEL], [panel], HAVE_PANEL=yes, HAVE_PANEL=no)
	else
		AC_CHECK_HEADER(panel.h,
			[AC_CHECK_LIB(panel, new_panel,
				[AC_DEFINE(HAVE_PANEL, 1,
						[Define if you have the ncurses panel library])
					PANEL_LIBS=-lpanel
					HAVE_PANEL=yes],
				[HAVE_PANEL=no],
				$NCURSES_LIBS
			)],
			[HAVE_PANEL=no],
			[$NCURSES_CFLAGS]
		)
	fi
else
	PANEL_LIBS=
fi
AC_SUBST(PANEL_LIBS)
