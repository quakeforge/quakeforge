AC_INIT(texturepaint.c)
PACKAGE=texturepaint
VERSION=1.1

AM_INIT_AUTOMAKE($PACKAGE, $VERSION)
AM_CONFIG_HEADER(config.h)

AC_SUBST(VERSION)

dnl Check for programs
AC_PROG_CC
AC_PATH_XTRA

dnl Check for libs.

AC_CHECK_LIB(GL, glXChooseVisual, LIBS="$LIBS -lGL",
	[AC_CHECK_LIB(MesaGL, glXChooseVisual, LIBS="$LIBS -lMesaGL",
		AC_MSG_ERROR(OpenGL not found), 
		$X_PRE_LIBS $X_LIBS -lX11 -lXext -lm $X_EXTRA_LIBS)], 
	$X_PRE_LIBS $X_LIBS -lX11 -lXext -lm $X_EXTRA_LIBS)

AC_CHECK_LIB(GLU, gluPerspective, LIBS="$LIBS -lGLU",
	[AC_CHECK_LIB(MesaGLU, gluPerspective, LIBS="$LIBS -lMesaGLU",
		AC_MSG_ERROR(libGLU not found), 
		$X_PRE_LIBS $X_LIBS -lX11 -lXext -lm $X_EXTRA_LIBS)],
	$X_PRE_LIBS $X_LIBS -lX11 -lXext -lm $X_EXTRA_LIBS)

AM_PATH_GTK(1.0.0, CFLAGS="$CFLAGS $GTK_CFLAGS"; LIBS="$LIBS $GTK_LIBS",
	AC_MSG_ERROR(GTK+ not found))

AC_CHECK_LIB(gtkgl, gtk_gl_area_begingl, LIBS="$LIBS -lgtkgl", 
	AC_MSG_ERROR(gtkglarea not found))

AM_PATH_GIMP(1.0.0, CFLAGS="$CFLAGS $GIMP_CFLAGS"; LIBS="$LIBS $GIMP_LIBS",
	AC_MSG_ERROR(GIMP not found))

dnl Output stuff.
AC_OUTPUT(Makefile)
