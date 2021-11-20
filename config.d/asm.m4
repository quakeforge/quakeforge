dnl Check for ia32
AC_MSG_CHECKING(for an ia32 machine)
case "${host}" in
	i?86-*-*)
		AC_MSG_RESULT(yes)
		AC_MSG_CHECKING(to see if we should disable asm optimizations)
		AC_ARG_ENABLE(asmopt,
			[  --disable-asmopt        disable assembler optimization],
			AC_MSG_RESULT(yes),
			AC_DEFINE(USE_INTEL_ASM, 1, [Define this if you want to use Intel assembly optimizations])
			ASM_ARCH=yes
			AC_MSG_RESULT(no)
		)
		;;
	*) AC_MSG_RESULT(no)
esac
AM_CONDITIONAL(ASM_ARCH, test "x$ASM_ARCH" = "xyes")

AC_MSG_CHECKING(for underscore prefix in names)
AC_LINK_IFELSE(
	[AC_LANG_PROGRAM([[asm(".long _bar"); int bar;]], [[]])],
	[AC_DEFINE(HAVE_SYM_PREFIX_UNDERSCORE, 1, Define this if C symbols are prefixed with an underscore) AC_MSG_RESULT(yes)],
	[AC_MSG_RESULT(no)
])
