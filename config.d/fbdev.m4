dnl Checks for Linux FBDev support
AC_ARG_WITH(fbdev,
	AS_HELP_STRING([--with-fbdev], [use Linux framebuffer device]),
	HAVE_FBDEV=$withval,
	HAVE_FBDEV=no)
if test "x$HAVE_FBDEV" != xno; then
	dnl We should still be able to compile it even if
	dnl there is no fbdev support in the running kernel
	AC_CHECK_HEADERS(linux/fb.h, HAVE_FBDEV=yes, HAVE_FBDEV=no)
fi

if test "x$HAVE_FBDEV" = xyes; then
	AC_MSG_CHECKING(for FB_AUX_VGA_PLANES_VGA4)
	AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include "linux/fb.h"]], [[int foo = FB_AUX_VGA_PLANES_VGA4;]])],[AC_DEFINE(HAVE_FB_AUX_VGA_PLANES_VGA4, 1, Define this if you have FB_AUX_VGA_PLANES_VGA4)
		AC_MSG_RESULT(yes)],[AC_MSG_RESULT(no)
	])
	AC_MSG_CHECKING(for FB_AUX_VGA_PLANES_CFB4)
	AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include "linux/fb.h"]], [[int foo = FB_AUX_VGA_PLANES_CFB4;]])],[AC_DEFINE(HAVE_FB_AUX_VGA_PLANES_CFB4, 1, Define this if you have FB_AUX_VGA_PLANES_CFB4)
		AC_MSG_RESULT(yes)],[AC_MSG_RESULT(no)
	])
	AC_MSG_CHECKING(for FB_AUX_VGA_PLANES_CFB8)
	AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include "linux/fb.h"]], [[int foo = FB_AUX_VGA_PLANES_CFB8;]])],[AC_DEFINE(HAVE_FB_AUX_VGA_PLANES_CFB8, 1, Define this if you have FB_AUX_VGA_PLANES_CFB4)
		AC_MSG_RESULT(yes)],[AC_MSG_RESULT(no)
	])
fi
