dnl ==================================================================
dnl Checks for evdev
dnl ==================================================================

AC_CHECK_HEADER(linux/input.h,
	[QF_NEED(input,[evdev])
	 AC_DEFINE([HAVE_EVDEV], [1], [Define if you have evdev])])
