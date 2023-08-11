dnl ==================================================================
dnl Checks for compiler attributes
dnl ==================================================================

AC_MSG_CHECKING(for __attribute__)
AC_COMPILE_IFELSE(
	[AC_LANG_PROGRAM(
		[[static __attribute__ ((unused)) const char *foo = "bar";]]
		[[int bar(void);]],
		[[return bar();]])],
	[AC_DEFINE(HAVE___ATTRIBUTE__)
	 AC_MSG_RESULT(yes)],
	[AC_MSG_RESULT(no)]
)
AH_VERBATIM([HAVE___ATTRIBUTE__],
[/* Define this if the GCC __attribute__ keyword is available */
#undef HAVE___ATTRIBUTE__
#ifndef HAVE___ATTRIBUTE__
# define __attribute__(x)
#endif])

AC_MSG_CHECKING(for __attribute__ ((visibility)))
AC_COMPILE_IFELSE(
	[AC_LANG_PROGRAM(
		[[void foo (void);]]
		[[__attribute__ ((visibility ("default"))) void foo (void) {}]]
		[[int bar(void);]],
		[[return bar();]]
	)],
	[AC_DEFINE(HAVE___ATTRIBUTE__VISIBILITY)
	 AC_MSG_RESULT(yes)],
	[AC_MSG_RESULT(no)]
)
AH_VERBATIM([HAVE___ATTRIBUTE__VISIBILITY],
[/* Define this if the GCC visibility __attribute__ is available */
#undef HAVE___ATTRIBUTE__VISIBILITY
#ifdef HAVE___ATTRIBUTE__VISIBILITY
# define VISIBLE __attribute__((visibility ("default")))
#else
# define VISIBLE
#endif])

AC_MSG_CHECKING(for __attribute__ ((designated_init)))
AC_COMPILE_IFELSE(
	[AC_LANG_PROGRAM(
		[[struct x { char y; } __attribute__ ((designated_init));]]
		[[int bar(void);]],
		[[return bar();]]
	)],
	[AC_DEFINE(HAVE___ATTRIBUTE__DESIGNATED_INIT)
	 AC_MSG_RESULT(yes)],
	[AC_MSG_RESULT(no)]
)
AH_VERBATIM([HAVE___ATTRIBUTE__DESIGNATED_INIT],
[/* Define this if the GCC designated_init __attribute__ is available */
#undef HAVE___ATTRIBUTE__DESIGNATED_INIT
#ifdef HAVE___ATTRIBUTE__DESIGNATED_INIT
# define DESIGNATED_INIT __attribute__((designated_init))
#else
# define DESIGNATED_INIT
#endif])

if test "x$SYSTYPE" = "xWIN32"; then
	AC_MSG_CHECKING(for __attribute__ ((gcc_struct)))
	AC_COMPILE_IFELSE(
		[AC_LANG_PROGRAM(
			[[typedef struct { int foo; }]]
			[[__attribute__ ((gcc_struct)) gcc_struct_test;]],
			[[return gcc_struct_test.foo]])],
		[AC_DEFINE(HAVE___ATTRIBUTE__GCC_STRUCT)
		 AC_MSG_RESULT(yes)],
		[AC_MSG_RESULT(no)
	])
fi
AH_VERBATIM([HAVE___ATTRIBUTE__GCC_STRUCT],
[/* Define this if the GCC gcc_struct __attribute__ is available */
#undef HAVE___ATTRIBUTE__GCC_STRUCT
#ifdef HAVE___ATTRIBUTE__GCC_STRUCT
# define GCC_STRUCT __attribute__((gcc_struct))
#else
# define GCC_STRUCT
#endif])
