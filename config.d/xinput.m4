dnl ==================================================================
dnl Checks for evdev
dnl ==================================================================

AC_CHECK_HEADER(xinput.h,
	[QF_NEED(input,[xinput])
	 AC_DEFINE([HAVE_XINPUT], [1], [Define if you have xinput])])
