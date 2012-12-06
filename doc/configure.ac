#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.59])
AC_INIT([QuakeForge], [git-master])
AC_CONFIG_SRCDIR([template.c])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([foreign])

AC_PROG_INSTALL
# PKG_PROG_PKG_CONFIG

AC_CHECK_PROG(DOXYGEN, doxygen, doxygen, false)
AC_CHECK_PROG(DOT, dot, dot, false)
AC_CHECK_PROG(MSCGEN, mscgen, mscgen, false)
AC_CHECK_PROG(TRANSFIG, transfig, transfig, false)

if test "x$DOXYGEN" = "xfalse" \
		-o "x$DOT" = "xfalse" \
		-o "x$MSCGEN" = "xfalse" \
		-o "x$TRANSFIG" = "xfalse"; then
	AC_MSG_ERROR([required tools missing, unable to build documentation])
fi

AC_ARG_ENABLE(static-doc,
	[  --enable-static-doc     Enable generation of doxygen docs for static]
	[                          functions.])
STATIC_DOC=NO
if test "x$enable_static_doc" = xyes; then
	STATIC_DOC=YES
fi
AC_SUBST(STATIC_DOC)

TOPSRC=`readlink -f ${srcdir}/..`
AC_SUBST(TOPSRC)

AC_CONFIG_FILES([Makefile man/Makefile quakeforge.dox.conf])
AC_OUTPUT
