if test "x$ac_cv_header_pthread_h" = "xyes"; then
	save_LIBS="$LIBS"
	HAVE_PTHREAD=yes
	case "$host_vendor-$host_os" in
		*android*)  dnl android has all pthread* functions in the libc.
			;;
		*ps3*)  dnl ps3toolchain doesn't have a working pthread yet
			HAVE_PTHREAD=no
			;;
		*qnx*)  dnl qnx has all pthread* functions in the libc.
			;;
		*openbsd*)
			LIBS="$LIBS -pthread"
			AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <pthread.h>]], [[pthread_attr_t type;
				 pthread_attr_setstacksize(&type, 0x100000);]])],[PTHREAD_LDFLAGS=-pthread],[PTHREAD_LDFLAGS=-lpthread
			])
			;;
		*)  LIBS="$LIBS -lpthread"
			AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <pthread.h>]], [[pthread_attr_t type;
				 pthread_attr_setstacksize(&type, 0x100000);]])],[PTHREAD_LDFLAGS=-lpthread],[PTHREAD_LDFLAGS=-pthread
			])
			;;
	esac
	LIBS="$save_LIBS"
	PTHREAD_CFLAGS=-D_REENTRANT
fi
if test "x$HAVE_PTHREAD" = "xyes"; then
	 AC_DEFINE(HAVE_PTHREAD, 1, [Define if you have working pthread])
fi
AC_SUBST(PTHREAD_LDFLAGS)
AC_SUBST(PTHREAD_CFLAGS)
