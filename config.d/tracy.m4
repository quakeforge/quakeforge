AC_ARG_ENABLE(tracy,
	AS_HELP_STRING(
		[--disable-tracy],
		[disable use of tracy profiler]
	)
)
HAVE_TRACY=no
if test "x$enable_tracy" = "xyes"; then
	tracy_dir=${srcdir}/tracy/public
	if test -d ${tracy_dir}; then
		TRACY_CFLAGS="-I ${tracy_dir} -DHAVE_TRACY"
		HAVE_TRACY=yes
	fi
fi
AC_SUBST(TRACY_CFLAGS)
AC_SUBST(TRACY_SRC)
AM_CONDITIONAL(HAVE_TRACY, test "x$HAVE_TRACY" = "xyes")

if test "x$HAVE_TRACY" = "xyes"; then
AC_MSG_CHECKING([for SymFromAddr in -ldbghelp])
SAVELIBS="$LIBS"
LIBS="$LIBS -ldbghelp"
AC_LINK_IFELSE([AC_LANG_PROGRAM([[
#include <windows.h>
#include <dbghelp.h>
]], [[
SymFromAddr(NULL, 0, NULL, NULL);
]])],[NET_LIBS="$NET_LIBS -ldbghelp"
	ac_cv_func_connect=yes
	ac_cv_func_gethostbyname=yes
	HAVE_WS2=yes
	AC_MSG_RESULT(yes)],[AC_MSG_RESULT(no)
])
LIBS="$SAVELIBS"
fi
