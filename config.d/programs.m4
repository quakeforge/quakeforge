dnl ==================================================================
dnl Checks for programs.
dnl ==================================================================

AC_PROG_INSTALL
AC_PROG_CC
AC_PROG_CPP
AC_PROG_LN_S
AC_PROG_RANLIB
AM_PROG_AS

PKG_PROG_PKG_CONFIG

AC_PROG_YACC
if echo $YACC | grep -v bison > /dev/null; then
	AC_MSG_ERROR(GNU bison is required but was not found)
else
	if echo $YACC | grep missing > /dev/null; then
		AC_MSG_ERROR(GNU bison is required but was not found)
	fi
fi

AM_PROG_LEX
if echo $LEX | grep -v flex > /dev/null; then
	AC_MSG_ERROR(GNU flex is required but was not found)
else
	if echo $LEX | grep missing > /dev/null; then
		AC_MSG_ERROR(GNU flex is required but was not found)
	fi
fi




AC_CHECK_LIB(l, main, LEXLIB="-ll", AC_CHECK_LIB(fl, main, LEXLIB="-lfl"))
AC_SUBST(LEXLIB)
