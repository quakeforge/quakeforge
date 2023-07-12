dnl ==================================================================
dnl Checks for typedefs, structures, and compiler characteristics
dnl ==================================================================

AC_C_CONST
AC_C_INLINE
AC_TYPE_SIZE_T
AC_STRUCT_TM

if test "x$ac_cv_header_unistd_h" = xyes; then
AC_MSG_CHECKING(for _SC_PAGESIZE)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <unistd.h>]], [[int foo = _SC_PAGESIZE;]])],[AC_DEFINE(HAVE__SC_PAGESIZE,1,Define this if you have _SC_PAGESIZE)
	AC_MSG_RESULT(yes)],[AC_MSG_RESULT(no)
])
fi

AC_MSG_CHECKING(for __attribute__)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[static __attribute__ ((unused)) const char *foo = "bar";]], [[]])],[AC_DEFINE(HAVE___ATTRIBUTE__)
	AC_MSG_RESULT(yes)],[AC_MSG_RESULT(no)
])
AH_VERBATIM([HAVE___ATTRIBUTE__],
[/* Define this if the GCC __attribute__ keyword is available */
#undef HAVE___ATTRIBUTE__
#ifndef HAVE___ATTRIBUTE__
# define __attribute__(x)
#endif])

AC_MSG_CHECKING(for __attribute__ ((visibility)))
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[void foo (void);
	__attribute__ ((sivibility ("default"))) void foo (void) {}]], [[]])],[AC_DEFINE(HAVE___ATTRIBUTE__VISIBILITY)
	AC_MSG_RESULT(yes)],[AC_MSG_RESULT(no)
])
AH_VERBATIM([HAVE___ATTRIBUTE__VISIBILITY],
[/* Define this if the GCC visibility __attribute__ is available */
#undef HAVE___ATTRIBUTE__VISIBILITY
#ifdef HAVE___ATTRIBUTE__VISIBILITY
# define VISIBLE __attribute__((visibility ("default")))
#else
# define VISIBLE
#endif])

if test "x$SYSTYPE" = "xWIN32"; then
	AC_MSG_CHECKING(for __attribute__ ((gcc_struct)))
	AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[typedef struct { int foo; }
		__attribute__ ((gcc_struct)) gcc_struct_test;]], [[]])],[AC_DEFINE(HAVE___ATTRIBUTE__GCC_STRUCT)
		AC_MSG_RESULT(yes)],[AC_MSG_RESULT(no)
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

AC_MSG_CHECKING(for __builtin_expect)
AC_LINK_IFELSE([AC_LANG_PROGRAM([[int x;]], [[if (__builtin_expect(!x, 1)) {}]])],[AC_DEFINE(HAVE___BUILTIN_EXPECT)
	AC_MSG_RESULT(yes)],[AC_MSG_RESULT(no)
])
AH_VERBATIM([HAVE___BUILTIN_EXPECT],
[/* Define this if the GCC __builtin_expect keyword is available */
#undef HAVE___BUILTIN_EXPECT
#ifndef HAVE___BUILTIN_EXPECT
# define __builtin_expect(x,c) x
#endif])

AC_MSG_CHECKING(for type of fpos_t)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <stdio.h>]], [[fpos_t x = 0]])],[AC_MSG_RESULT(off_t)],[AC_DEFINE(HAVE_FPOS_T_STRUCT, 1, Define this if FPOS_T is a struct)
	AC_MSG_RESULT(struct)
])

AC_MSG_CHECKING(for socklen_t in sys/types.h)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <sys/types.h>]], [[socklen_t x = 0;]])],[AC_DEFINE(HAVE_SOCKLEN_T, 1, Define this if your system has socklen_t)
	AC_MSG_RESULT(yes)],[AC_MSG_RESULT(no)
	dnl FreeBSD 4.0 has it in sys/socket.h
	AC_MSG_CHECKING(for socklen_t in sys/socket.h)
	AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <sys/types.h>
		#include <sys/socket.h>]], [[socklen_t x = 0;]])],[AC_DEFINE(HAVE_SOCKLEN_T, 1, Define this if your system has socklen_t)
		AC_MSG_RESULT(yes)],[AC_MSG_RESULT(no)
	])
])

if test "x$ac_cv_header_sys_uio_h" = xyes; then
	AC_MSG_CHECKING(for struct in_pktinfo)
	AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <sys/types.h>
		#include <sys/socket.h>
		#include <netinet/in.h>
		#include <sys/uio.h>]], [[struct in_pktinfo x;]])],[AC_DEFINE(HAVE_IN_PKTINFO, 1, Define this if your system has struct in_pktinfo)
		AC_MSG_RESULT(yes)],[AC_MSG_RESULT(no)
	])
fi

AC_MSG_CHECKING(for size_t in sys/types.h)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <sys/types.h>]], [[ size_t x = 0;]])],[AC_DEFINE(HAVE_SIZE_T, 1, Define this if your system has size_t) AC_MSG_RESULT(yes)],[AC_MSG_RESULT(no)
])

dnl maybe these two (at least the 2nd) should be checked only if ipv6 is enabled?
AC_MSG_CHECKING(for ss_len in struct sockaddr_storage)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <sys/types.h>
	#include <sys/socket.h>]], [[ void f(void) { struct sockaddr_storage ss; ss.ss_len=0; }]])],[AC_DEFINE(HAVE_SS_LEN, 1, Define this if you have ss_len member in struct sockaddr_storage (BSD)) AC_MSG_RESULT(yes)],[AC_MSG_RESULT(no)
])

AC_MSG_CHECKING(for sin6_len in struct sockaddr_in6)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <sys/types.h>
	#include <netinet/in.h>]], [[ void f(void) { struct sockaddr_in6 s6; s6.sin6_len=0; }]])],[AC_DEFINE(HAVE_SIN6_LEN, 1, Define this if you have sin6_len member in struct sockaddr_in6 (BSD)) AC_MSG_RESULT(yes)],[AC_MSG_RESULT(no)
])

AC_MSG_CHECKING(for sa_len in struct sockaddr)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <sys/types.h>
	#include <netinet/in.h>]], [[ void f(void) { struct sockaddr sa; sa.sa_len=0; }]])],[AC_DEFINE(HAVE_SA_LEN, 1, Define this if you have sa_len member in struct sockaddr (BSD)) AC_MSG_RESULT(yes)],[AC_MSG_RESULT(no)
])
