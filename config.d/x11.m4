dnl Checks for X11 and XShm
if test "x$mingw" != xyes; then
	AC_PATH_XTRA
	if test "x$no_x" = x; then
		HAVE_X=yes
		AC_CHECK_LIB(Xext, XShmQueryExtension,
			X_SHM_LIB=-lXext,
			HAVE_X=no,
			[ $X_LIBS -lX11 $X_EXTRA_LIBS ]
		)
	fi
	AC_SUBST(X_SHM_LIB)
fi

dnl Check for XFree86-VidMode support
AC_ARG_ENABLE(vidmode,
	AS_HELP_STRING([--disable-vidmode],
		[do not use XFree86 VidMode extension]),
	HAVE_VIDMODE=$enable_vidmode, HAVE_VIDMODE=auto)
if test "x$HAVE_VIDMODE" != xno; then
	save_CPPFLAGS="$CPPFLAGS"
	CPPFLAGS="$X_CFLAGS $CPPFLAGS"
	AC_CHECK_HEADER(X11/extensions/xf86vmode.h,
		dnl Make sure the library works
		[AC_CHECK_LIB(Xxf86vm, XF86VidModeSwitchToMode,
			AC_DEFINE(HAVE_VIDMODE, 1, [Define if you have the XFree86 VIDMODE extension])
			VIDMODE_LIBS="-lXxf86vm",,
			[$X_LIBS -lXext -lX11 $X_EXTRA_LIBS]
		)],
		[],
		[#include <X11/Xlib.h>]
	)
	CPPFLAGS="$save_CPPFLAGS"
fi
AC_SUBST(VIDMODE_LIBS)

dnl Check for XInput2 support
AC_ARG_ENABLE(xi2,
	AS_HELP_STRING([--disable-xi2], [do not use Xorg XInput2 extension]),
	HAVE_XI2=$enable_xi2, HAVE_XI2=auto)
if test "x$HAVE_XI2" != xno; then
	save_CPPFLAGS="$CPPFLAGS"
	CPPFLAGS="$X_CFLAGS $CPPFLAGS"
	AC_CHECK_HEADER(X11/extensions/XInput2.h,
		dnl Make sure the library works
		[AC_CHECK_LIB(Xi, XIQueryVersion,
			AC_DEFINE(HAVE_XI2, 1, [Define if you have the Xorg XInput2 extension])
			XI2_LIBS="-lXi",,
			[$X_LIBS -lXext -lX11 $X_EXTRA_LIBS]
		)],,
		[#include <X11/Xlib.h>]
	)
	CPPFLAGS="$save_CPPFLAGS"
fi
AC_SUBST(XI2_LIBS)

dnl Check for XInput2 support
AC_ARG_ENABLE(xfixes,
	AS_HELP_STRING([--disable-xfixes], [do not use Xorg Xfixes extension]),
	HAVE_XFIXES=$enable_xfixes, HAVE_XFIXES=auto)
if test "x$HAVE_XFIXES" != xno; then
	save_CPPFLAGS="$CPPFLAGS"
	CPPFLAGS="$X_CFLAGS $CPPFLAGS"
	AC_CHECK_HEADER(X11/extensions/Xfixes.h,
		dnl Make sure the library works
		[AC_CHECK_LIB(Xi, XIQueryVersion,
			AC_DEFINE(HAVE_XFIXES, 1, [Define if you have the Xorg Xfixes extension])
			XFIXES_LIBS="-lXfixes",,
			[$X_LIBS -lXext -lX11 $X_EXTRA_LIBS]
		)],,
		[#include <X11/Xlib.h>]
	)
	CPPFLAGS="$save_CPPFLAGS"
fi
AC_SUBST(XFIXES_LIBS)

dnl Check for DGA support
AC_ARG_ENABLE(dga,
	AS_HELP_STRING([--disable-dga], [do not use XFree86 DGA extension]),
	HAVE_DGA=$enable_dga, HAVE_DGA=auto)
if test "x$HAVE_DGA" != xno; then
	save_CPPFLAGS="$CPPFLAGS"
	CPPFLAGS="$X_CFLAGS $CPPFLAGS"
	AC_CHECK_HEADER(X11/extensions/Xxf86dga.h,
		dnl Make sure the library works
		[AC_CHECK_LIB(Xxf86dga, XF86DGAQueryVersion,
			AC_DEFINE(HAVE_DGA, 1, [Define if you have the XFree86 DGA extension])
			DGA_LIBS="-lXxf86dga",,
			[$X_LIBS -lXext -lX11 $X_EXTRA_LIBS]
		)],
		[AC_CHECK_HEADER(X11/extensions/xf86dga.h,
			dnl Make sure the library works
			[AC_CHECK_LIB(Xxf86dga, XF86DGAQueryVersion,
				AC_DEFINE(HAVE_DGA, 1, [Define if you have the XFree86 DGA extension])
				AC_DEFINE(DGA_OLD_HEADERS, 1, [Define if DGA uses old headers])
				DGA_LIBS="-lXxf86dga",,
				[$X_LIBS -lXext -lX11 $X_EXTRA_LIBS]
			)],
			[],
			[#include <X11/Xlib.h>]
		)],
		[#include <X11/Xlib.h>]
	)
	CPPFLAGS="$save_CPPFLAGS"
fi
AC_SUBST(DGA_LIBS)
