if test "x$ac_cv_header_pthread_h" = "xyes"; then
	save_LIBS="$LIBS"
	case "$host_os" in
		*qnx*)  dnl qnx have all pthread* functions in the libc.
			;;
		*openbsd*)
			LIBS="$LIBS -pthread"
			AC_TRY_LINK(
				[#include <pthread.h>],
				[pthread_attr_t type;
				 pthread_attr_setstacksize(&type, 0x100000);],
				[PTHREAD_LDFLAGS=-pthread],
				[PTHREAD_LDFLAGS=-lpthread]
			)
			;;
		*)  LIBS="$LIBS -lpthread"
			AC_TRY_LINK(
				[#include <pthread.h>],
				[pthread_attr_t type;
				 pthread_attr_setstacksize(&type, 0x100000);],
				[PTHREAD_LDFLAGS=-lpthread],
				[PTHREAD_LDFLAGS=-pthread]
			)
			;;
	esac
	LIBS="$save_LIBS"
	PTHREAD_CFLAGS=-D_REENTRANT
fi
AC_SUBST(PTHREAD_LDFLAGS)
AC_SUBST(PTHREAD_CFLAGS)
