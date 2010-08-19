#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.59])
AC_INIT([QuakeForge], m4_esyscmd([git log -1 --pretty=%h HEAD | tr -d '\n']))
AC_CONFIG_SRCDIR([template.c])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([foreign])

# Checks for programs.
AC_PROG_INSTALL
# PKG_PROG_PKG_CONFIG

# Checks for libraries.

# Checks for header files.

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.

TOPDIR=`readlink -f ${srcdir}/..`
AC_SUBST(TOPDIR)

AC_CONFIG_FILES([Makefile man/Makefile quakeforge.dox])
AC_OUTPUT
