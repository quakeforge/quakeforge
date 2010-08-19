dnl Tests for joystick support
AC_MSG_CHECKING(for joystick support)
if test -z "$JOYTYPE" -a "x$ac_cv_header_linux_joystick_h" = "xyes"; then
	AC_EGREP_CPP([QF_maGiC_VALUE],[
#include <linux/joystick.h>
#ifdef JS_VERSION
QF_maGiC_VALUE
#endif
	], JOYTYPE="Linux")
fi

if test -z "$JOYTYPE" -a "x$ac_cv_header_dinput_h" = "xyes"; then
	JOYTYPE="Win32"
fi

if test "$JOYTYPE"; then
	AC_MSG_RESULT([$JOYTYPE])
else
	AC_MSG_RESULT([no, using null joystick driver])
fi
AC_SUBST(JOY_LIBS)
AC_SUBST(JOY_CFLAGS)
AM_CONDITIONAL(JOYTYPE_LINUX, test "$JOYTYPE" = "Linux")
AM_CONDITIONAL(JOYTYPE_WIN32, test "$JOYTYPE" = "Win32")
AM_CONDITIONAL(JOYTYPE_NULL, test "$JOYTYPE" != "Linux" -a "$JOYTYPE" != "Win32")
