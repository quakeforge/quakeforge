dnl ==================================================================
dnl Checks for library functions.
dnl ==================================================================

AC_FUNC_ALLOCA
AC_FUNC_MEMCMP
AC_FUNC_MMAP
AC_TYPE_SIGNAL
AC_FUNC_VPRINTF
AC_CHECK_FUNCS(
	access _access bsearch_r connect dlopen execvp fcntl ftime _ftime \
	getaddrinfo \
	gethostbyname gethostname getnameinfo getpagesize gettimeofday getuid \
	getwd ioctl mkdir _mkdir mprotect putenv qsort_r select sigaction \
	snprintf _snprintf socket stat strcasestr strerror strerror_r strndup \
	strnlen \
	strsep strstr vsnprintf _vsnprintf wait
)

AC_FUNC_VA_COPY
AC_FUNC__VA_COPY
AH_VERBATIM([DEFINE_VA_COPY],
[#ifndef HAVE_VA_COPY
# ifdef HAVE__VA_COPY
#  define va_copy(d,s) __va_copy ((d), (s))
# else
#  define va_copy(d,s) memcpy (&(d), &(s), sizeof (va_list))
# endif
#endif])

DL_LIBS=""
if test "x$ac_cv_func_dlopen" != "xyes"; then
	AC_CHECK_LIB(dl, dlopen,
		AC_DEFINE(HAVE_DLOPEN, 1, [Define if you have the dlopen function.])
		DL_LIBS="-ldl"
	)
fi
AC_SUBST(DL_LIBS)

if test "x$DL_LIBS" != "x"; then
AC_MSG_CHECKING([for RTLD_NOW])
AC_TRY_COMPILE(
	[#include <dlfcn.h>],
	[int foo = RTLD_NOW],
	AC_DEFINE(HAVE_RTLD_NOW, 1, [Define if you have RTLD_NOW.])
	AC_MSG_RESULT(yes),
	AC_MSG_RESULT(no)
)
AC_MSG_CHECKING([for RTLD_DEEPBIND])
AC_TRY_COMPILE(
	[#include <dlfcn.h>],
	[int foo = RTLD_DEEPBIND],
	AC_DEFINE(HAVE_RTLD_DEEPBIND, 1, [Define if you have RTLD_DEEPBIND.])
	AC_MSG_RESULT(yes),
	AC_MSG_RESULT(no)
)
fi

dnl Checks for stricmp/strcasecmp
#AC_CHECK_FUNC(strcasecmp,
#	,
#	AC_CHECK_FUNC(stricmp,
#		AC_DEFINE(strcasecmp,	stricmp)
#	)
#)
AC_CHECK_FUNC(strcasecmp, strcasecmp=yes, strcasecmp=no)
if test "x$strcasecmp" = xno; then
	AC_CHECK_FUNC(stricmp,
		AC_DEFINE(strcasecmp, stricmp, [Define strcasecmp as stricmp if you have one but not the other]),
		AC_MSG_ERROR([Neither stricmp nor strcasecmp found])
	)
fi

dnl Check for vsnprintf
if test "x$ac_cv_func_vsnprintf" = "xno" -a \
	"x$ac_cv_func__vsnprintf" = "xno"; then
	dnl libdb may have this
	AC_CHECK_LIB(db,vsnprintf)
fi

AC_CHECK_FUNCS(usleep)

AC_MSG_CHECKING(for fnmatch)
AC_TRY_LINK(
	[],
	[fnmatch();],
	BUILD_FNMATCH=no
	AC_MSG_RESULT(yes),
	BUILD_FNMATCH=yes
	AC_MSG_RESULT(no)
)
AM_CONDITIONAL(BUILD_FNMATCH, test "x$BUILD_FNMATCH" = "xyes")

AC_MSG_CHECKING(for opendir)
AC_TRY_LINK(
	[],
	[opendir();],
	BUILD_DIRENT=no
	AC_MSG_RESULT(yes),
	BUILD_DIRENT=yes
	AC_MSG_RESULT(no)
)
AM_CONDITIONAL(BUILD_DIRENT, test "x$BUILD_DIRENT" = "xyes")

AC_MSG_CHECKING(for getopt_long)
AC_TRY_LINK(
	[],
	[getopt_long();],
	BUILD_GETOPT=no
	AC_MSG_RESULT(yes),
	BUILD_GETOPT=yes
	AC_MSG_RESULT(no)
)
AM_CONDITIONAL(BUILD_GETOPT, test "x$BUILD_GETOPT" = "xyes")

AC_MSG_CHECKING(for log2f)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
	[[#include <math.h>]], [float (*foo)(float) = log2f;])],
	AC_DEFINE(HAVE_LOG2F, 1, [Define if you have log2f.])
	[HAVE_LOG2F=yes],
	[HAVE_LOG2F=no])
AC_MSG_RESULT($HAVE_LOG2F)
