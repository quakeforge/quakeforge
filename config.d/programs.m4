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
BISON_REQ=2.6
AC_MSG_CHECKING([is bison version >= ${BISON_REQ}])
BISON_VER=`$YACC --version | sed -n '1s/^.*) //p'`
AS_VERSION_COMPARE([$BISON_VER], [2.6],
	[AC_MSG_RESULT([no])
	LOC=`which ${YACC}`
	AC_MSG_ERROR(
		[GNU bison >= 2.6 is required. $BISON_VER found in $LOC]
	)],
	AC_MSG_RESULT([yes]),
	AC_MSG_RESULT([yes])
)

AC_PROG_LEX(noyywrap)
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
