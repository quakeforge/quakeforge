dnl Process this file with autoconf to produce a configure script.
AC_PREREQ(2.52)
AC_INIT(qw/source/sv_main.c)
AC_REVISION($Revision$) dnl
AM_CONFIG_HEADER(include/config.h)
AC_CANONICAL_SYSTEM
AC_VALIDATE_CACHED_SYSTEM_TUPLE(
	rm $cache_file
	echo restart configure
	exit 1
)


dnl This is the only place where the package version appears
AM_INIT_AUTOMAKE(quakeforge, 0.5.5-CVS)

dnl Define the proper name and extra version numbers for package
PROGRAM=QuakeForge
NQ_VERSION=1.09
NQ_QSG_VERSION=1.0
QW_VERSION=2.40
QW_QSG_VERSION=2.0
RPMVERSION=`echo $VERSION | tr - _`
AC_SUBST(RPMVERSION)

AC_DEFINE_UNQUOTED(PROGRAM, 		"$PROGRAM", ["Proper" package name])
AC_DEFINE_UNQUOTED(NQ_VERSION,		"$NQ_VERSION", [Define this to the NetQuake standard version you support])
AC_DEFINE_UNQUOTED(NQ_QSG_VERSION,	"$NQ_QSG_VERSION", [Define this to the QSG standard version you support in NetQuake])
AC_DEFINE_UNQUOTED(QW_VERSION,		"$QW_VERSION", [Define this to the QuakeWorld standard version you support])
AC_DEFINE_UNQUOTED(QW_QSG_VERSION,	"$QW_QSG_VERSION", [Define this to the QSG standard version you support in QuakeWorld])

AC_SUBST(PROGRAM)
AC_SUBST(NQ_VERSION)
AC_SUBST(NQ_QSG_VERSION)
AC_SUBST(QW_VERSION)
AC_SUBST(QW_QSG_VERSION)

ISODATE=`date +%Y-%m-%d`
AC_SUBST(ISODATE)

AC_LANG_C

if test "x${CFLAGS-unset}" = xunset; then
	CFLAGS=""
fi
if test "$x{AR-unset}" = xunset; then
	AR="ar"
fi
export AR

endian=""
case "$host_os" in
	mingw32*)
		mingw=yes
		CFLAGS="$CFLAGS -I\$(top_srcdir)/include/win32"
		if test "x$host" != "x$build"; then
			case "$build_os" in
				cygwin*)
					CFLAGS="$CFLAGS -mno-cygwin -mconsole"
					CPPFLAGS="$CPPFLAGS $CFLAGS"
					;;
				*)
					CC="$host_cpu-$host_os-gcc"
					AS="$CC"
					AR="$host_cpu-$host_os-ar"
					RANLIB="$host_cpu-$host_os-ranlib"
					;;
			esac
		fi
		endian="little"
		;;
	cygwin*)
		cygwin=yes
		if test "x$host" != "x$build"; then
			CC="$host_cpu-$host_os-gcc"
			AS="$CC"
			AR="$host_cpu_$host_os-ar"
			RANLIB="$host_cpu-$host_os-ranlib"
			endian="little"
		fi
	;;
esac

dnl ==================================================================
dnl Checks for programs.
dnl ==================================================================

AC_PROG_INSTALL
AC_PROG_CC
AC_PROG_CPP
AC_PROG_LN_S
AC_PROG_RANLIB
AC_LIBTOOL_WIN32_DLL
AM_PROG_LIBTOOL

AC_PROG_YACC
if echo $YACC | grep -v bison > /dev/null; then
	AC_MSG_ERROR(GNU bison is required but wasn't found)
fi

AM_PROG_LEX
if echo $LEX | grep -v flex > /dev/null; then
	AC_MSG_ERROR(GNU flex is required but wasn't found)
fi



AC_ARG_WITH(cpp,
[  --with-cpp=CPP          how qfcc should invoke cpp],
	cpp_name="$withval", cpp_name=auto
)
if test "x$cpp_name" != xauto; then
	CPP_NAME="$cpp_name"
else
	CPP_NAME="cpp %d -o %o %i"
	case "$target_os" in
		*freebsd*)
			CPP_NAME="cpp %d %i %o"
			;;
		*qnx*)
			CPP_NAME="gcc -E -x c++ %d -o %o %i"
			;;
		*bsd*)
			touch conftest.c
			CPP_NAME="`(f=\`$CC -v -E -Dfoo conftest.c -o conftest.i 2>&1 | grep -e -Dfoo\`; set $f; echo "$1")` %d %i %o"
			rm -f conftest.[ci]
			;;
	esac
fi

AC_CHECK_LIB(l, main, LEXLIB="-ll", AC_CHECK_LIB(fl, main, LEXLIB="-lfl"))
AC_SUBST(LEXLIB)

dnl ==================================================================
dnl Checks for system services
dnl ==================================================================

AC_SYS_LONG_FILE_NAMES
AC_EXEEXT
AC_OBJEXT

dnl ==================================================================
dnl Checks for header files.
dnl ==================================================================

AC_HEADER_DIRENT
AC_HEADER_STDC
AC_HEADER_MAJOR
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS(
	alloca.h alsa/asoundlib.h arpa/inet.h asm/io.h assert.h conio.h \
	ctype.h ddraw.h dinput.h direct.h dirent.h dlfcn.h dmedia/audio.h \
	dmedia/cdaudio.h dpmi.h dsound.h errno.h execinfo.h fcntl.h io.h \
	libc.h limits.h linux/cdrom.h linux/joystick.h linux/soundcard.h \
	machine/soundcard.h malloc.h math.h mgraph.h _mingw.h netdb.h \
	netinet/in.h process.h pthread.h pwd.h rpc/types.h setjmp.h signal.h \
	stdarg.h stdio.h stdlib.h string.h strings.h sys/asoundlib.h \
	sys/audioio.h sys/filio.h sys/ioctl.h sys/io.h sys/ipc.h sys/mman.h \
	sys/param.h sys/poll.h sys/shm.h sys/signal.h sys/socket.h \
	sys/soundcard.h sys/stat.h sys/time.h sys/types.h sys/uio.h termios.h \
	time.h unistd.h vgakeyboard.h vgamouse.h windows.h winsock.h
)
if test "x$mingw" = xyes; then
	AC_MSG_CHECKING(for fnmatch.h)
	AC_MSG_RESULT(yes)
	AC_DEFINE(HAVE_FNMATCH_H, 1, [Define this if you have fnmatch.h])
	AC_MSG_CHECKING(for tchar.h)
	AC_MSG_RESULT(yes)
	AC_DEFINE(HAVE_TCHAR_H, 1, [Define this if you have tchar.h])
else
	AC_CHECK_HEADERS(fnmatch.h)
fi

if test "x$ac_cv_header_pthread_h" = "xyes"; then
	save_ldflags="$LDFLAGS"
	case "$host_os" in
		*qnx*)  dnl qnx have all pthread* functions in the libc.
			;;
		*)  LDFLAGS="$LDFLAGS -lpthread"
			AC_TRY_LINK(
				[#include <pthread.h>],
				[pthread_attr_t type;
				 pthread_attr_setstacksize(&type, 0x100000);],
				[PTHREAD_LDFLAGS=-lpthread],
				[PTHREAD_LDFLAGS=-pthread]
			)
			;;
	esac
	LDFLAGS="$save_ldflags"
	PTHREAD_CFLAGS=-D_REENTRANT
fi
AC_SUBST(PTHREAD_LDFLAGS)
AC_SUBST(PTHREAD_CFLAGS)

dnl ==================================================================
dnl Checks for typedefs, structures, and compiler characteristics
dnl ==================================================================

AC_C_CONST
AC_C_INLINE
AC_TYPE_SIZE_T
AC_STRUCT_ST_BLKSIZE
AC_HEADER_TIME
AC_STRUCT_TM

if test "x$ac_cv_header_unistd_h" = xyes; then
AC_MSG_CHECKING(for _SC_PAGESIZE)
AC_TRY_COMPILE(
	[#include <unistd.h>],
	[int foo = _SC_PAGESIZE;],
	AC_DEFINE(HAVE__SC_PAGESIZE,1,[Define this if you have _SC_PAGESIZE])
	AC_MSG_RESULT(yes),
	AC_MSG_RESULT(no)
)
fi

AC_MSG_CHECKING(for __attribute__)
AC_TRY_COMPILE(
	[static __attribute__ ((unused)) const char *foo = "bar";],
	[],
	AC_DEFINE(HAVE___ATTRIBUTE__)
	AC_MSG_RESULT(yes),
	AC_MSG_RESULT(no)
)
AH_VERBATIM([HAVE___ATTRIBUTE__],
[/* Define this if the GCC __attribute__ keyword is available */
#undef HAVE___ATTRIBUTE__
#ifndef HAVE___ATTRIBUTE__
# define __attribute__(x)
#endif])

AC_MSG_CHECKING(for __builtin_expect)
AC_TRY_COMPILE(
	[long (*foo) (long, long) = __builtin_expect;],
	[],
	AC_DEFINE(HAVE___BUILTIN_EXPECT)
	AC_MSG_RESULT(yes),
	AC_MSG_RESULT(no)
)
AH_VERBATIM([HAVE___BUILTIN_EXPECT],
[/* Define this if the GCC __builtin_expect keyword is available */
#undef HAVE___BUILTIN_EXPECT
#ifndef HAVE___BUILTIN_EXPECT
# define __builtin_expect(x,c) x
#endif])

AC_TYPE_VA_LIST

if test "x$cross_compiling" = xyes; then
	AC_MSG_CHECKING(whether byte ordering is bigendian)
	AC_ARG_WITH(endian,
[  --with-endian=TYPE      set endian of target system for
                          cross-compiling. TYPE = little or big.],
		endian="$withval",
	)
	case "x$endian" in
		xbig)
			AC_DEFINE(WORDS_BIGENDIAN)
			AC_MSG_RESULT(yes)
			;;
		xlittle)
			AC_MSG_RESULT(no)
			;;
		x)
			AC_MSG_RESULT(unspecified, use --with-endian={big,little})
			exit 1
			;;
		x*)
			AC_MSG_RESULT(unrecognized endianness)
			exit 1
			;;
	esac
else
	AC_C_BIGENDIAN
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

AC_MSG_CHECKING(for alloca in stdlib.h)
AC_TRY_COMPILE(
	[#include <stdlib.h>],
	[void *(*foo)(size_t) = alloca;],
	have_alloca_proto=yes
	AC_MSG_RESULT(yes),
	have_alloca_proto=no
	AC_MSG_RESULT(no)
)

if test "x$have_alloca_proto" = no -a "x$ac_cv_header_alloca_h" = yes; then
	AC_MSG_CHECKING(for alloca in alloca.h)
	AC_TRY_COMPILE(
		[#include <stdlib.h>]
		[#include <alloca.h>],
		[void *(*foo)(size_t) = alloca;],
		have_alloca_proto=yes
		AC_MSG_RESULT(yes),
		have_alloca_proto=no
		AC_MSG_RESULT(no)
	)
fi

if test "x$have_alloca_proto" = xyes; then
	AC_DEFINE(HAVE_ALLOCA_PROTO)
fi
AH_VERBATIM([HAVE_ALLOCA_PROTO],
[/* Define this if alloca is prototyped */
#undef HAVE_ALLOCA_PROTO
#ifndef HAVE_ALLOCA_PROTO
#ifndef QFASM
void *alloca (int size);
#endif
#endif])

AC_MSG_CHECKING(for fnmatch in fnmatch.h)
AC_TRY_COMPILE(
	[#include "fnmatch.h"],
	[int (*foo)() = fnmatch;],
	AC_DEFINE(HAVE_FNMATCH_PROTO, 1, [Define this if fnmatch is prototyped in fnmatch.h])
	AC_MSG_RESULT(yes),
	AC_MSG_RESULT(no)
)

AC_MSG_CHECKING(for strnlen in string.h)
AC_TRY_COMPILE(
	[#include "string.h"],
	[int (*foo)() = strnlen;],
	AC_DEFINE(HAVE_STRNLEN_PROTO, 1, [Define this if strnlen is prototyped in string.h])
	AC_MSG_RESULT(yes),
	AC_MSG_RESULT(no)
)

AC_MSG_CHECKING(for strcasestr in string.h)
AC_TRY_COMPILE(
	[#include "string.h"],
	[int (*foo)() = strcasestr;],
	AC_DEFINE(HAVE_STRCASESTR_PROTO, 1, [Define this if strcasestr is prototyped in string.h])
	AC_MSG_RESULT(yes),
	AC_MSG_RESULT(no)
)

AC_MSG_CHECKING(for type of fpos_t)
AC_TRY_COMPILE(
	[#include <stdio.h>],
	[fpos_t x = 0],
	AC_MSG_RESULT(off_t),
	AC_DEFINE(HAVE_FPOS_T_STRUCT, 1, [Define this if FPOS_T is a struct])
	AC_MSG_RESULT(struct)
)

AC_MSG_CHECKING(for socklen_t in sys/types.h)
AC_TRY_COMPILE(
	[#include <sys/types.h>],
	[ socklen_t x = 0;],
	AC_DEFINE(HAVE_SOCKLEN_T, 1, [Define this if your system has socklen_t])
	AC_MSG_RESULT(yes),
	AC_MSG_RESULT(no)
	dnl FreeBSD 4.0 has it in sys/socket.h
	AC_MSG_CHECKING(for socklen_t in sys/socket.h)
	AC_TRY_COMPILE(
		[#include <sys/types.h>
		#include <sys/socket.h>],
		[ socklen_t x = 0;],
		AC_DEFINE(HAVE_SOCKLEN_T, 1, [Define this if your system has socklen_t]) AC_MSG_RESULT(yes),
		AC_MSG_RESULT(no)
	)
)

if test "x$ac_cv_header_sys_uio_h" = xyes; then
	AC_MSG_CHECKING(for struct in_pktinfo)
	AC_TRY_COMPILE(
		[#include <sys/types.h>
		#include <sys/socket.h>
		#include <netinet/in.h>
		#include <sys/uio.h>],
		[struct in_pktinfo x;],
		AC_DEFINE(HAVE_IN_PKTINFO, 1, [Define this if your system has struct in_pktinfo])
		AC_MSG_RESULT(yes),
		AC_MSG_RESULT(no)
	)
fi

AC_MSG_CHECKING(for underscore prefix in names)
AC_TRY_LINK(
	[asm(".long _bar");
	int bar;],
	[],
	AC_DEFINE(HAVE_SYM_PREFIX_UNDERSCORE, 1, [Define this if C symbols are prefixed with an underscore]) AC_MSG_RESULT(yes),
	AC_MSG_RESULT(no)
)

AC_MSG_CHECKING(for size_t in sys/types.h)
AC_TRY_COMPILE(
	[#include <sys/types.h>],
	[ size_t x = 0;],
	AC_DEFINE(HAVE_SIZE_T, 1, [Define this if your system has size_t]) AC_MSG_RESULT(yes),
	AC_MSG_RESULT(no)
)

dnl maybe these two (at least the 2nd) should only be checked if ipv6 is enabled?
AC_MSG_CHECKING(for ss_len in struct sockaddr_storage)
AC_TRY_COMPILE(
	[#include <sys/types.h>
	#include <sys/socket.h>],
	[ void f(void) { struct sockaddr_storage ss; ss.ss_len=0; }],
	AC_DEFINE(HAVE_SS_LEN, 1, [Define this if you have ss_len member in struct sockaddr_storage (BSD)]) AC_MSG_RESULT(yes),
	AC_MSG_RESULT(no)
)

AC_MSG_CHECKING(for sin6_len in struct sockaddr_in6)
AC_TRY_COMPILE(
	[#include <sys/types.h>
	#include <netinet/in.h>],
	[ void f(void) { struct sockaddr_in6 s6; s6.sin6_len=0; }],
	AC_DEFINE(HAVE_SIN6_LEN, 1, [Define this if you have sin6_len member in struct sockaddr_in6 (BSD)]) AC_MSG_RESULT(yes),
	AC_MSG_RESULT(no)
)

AC_MSG_CHECKING(for sa_len in struct sockaddr)
AC_TRY_COMPILE(
	[#include <sys/types.h>
	#include <netinet/in.h>],
	[ void f(void) { struct sockaddr sa; sa.sa_len=0; }],
	AC_DEFINE(HAVE_SA_LEN, 1, [Define this if you have sa_len member in struct sockaddr (BSD)]) AC_MSG_RESULT(yes),
	AC_MSG_RESULT(no)
)

dnl ==================================================================
dnl Checks for library functions.
dnl ==================================================================

AC_FUNC_ALLOCA
AC_FUNC_MEMCMP
AC_FUNC_MMAP
AC_TYPE_SIGNAL
AC_FUNC_VPRINTF
AC_FUNC_VA_COPY
AC_FUNC__VA_COPY
AC_CHECK_FUNCS(
	access _access gethostname gethostbyname connect gettimeofday getwd \
	mkdir _mkdir ftime _ftime fcntl stat putenv select socket strerror \
	strcasestr strnlen strstr snprintf _snprintf vsnprintf _vsnprintf \
	strsep dlopen getaddrinfo getnameinfo mprotect getpagesize
)

DL_LIBS=""
if test "x$ac_cv_func_dlopen" != "xyes"; then
	AC_CHECK_LIB(dl, dlopen,
		AC_DEFINE(HAVE_DLOPEN, 1, [Define if you have the dlopen function.]) DL_LIBS="-ldl"
	)
fi
AC_SUBST(DL_LIBS)

AC_ARG_ENABLE(curses,
[  --disable-curses        disable curses support]
)

if test "x$enable_curses" != "xno"; then
	AC_CHECK_HEADERS(curses.h)
	AC_CHECK_LIB(ncurses, initscr,
		CURSES_LIBS=-lncurses,
		AC_CHECK_LIB(pdcurses, initscr,
			CURSES_LIBS=-lpdcurses,
			AC_CHECK_LIB(curses, initscr,
				CURSES_LIBS=-lcurses,
				CURSES_LIBS=
			)
		)
	)
else
	CURSES_LIBS=
fi
AC_SUBST(CURSES_LIBS)

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

WINMM=""
case "$target_os" in
	cygwin*)
		WINMM="-lwinmm"
	;;
	mingw32*)
		WINMM="-lwinmm"
	;;
	*)
		dnl Checks for working -lm
		AC_CHECK_LIB(m, pow,, AC_MSG_ERROR([math library (-lm) appears broken]))
	;;
esac	

AC_ARG_ENABLE(wildmidi,
[  --disable-wildmidi         disable libWildMidi support],
)
HAVE_WILDMIDI=no
WM_LIBS=
if test "x$enable_wildmidi" != "xno"; then
  AC_CHECK_LIB(WildMidi, WildMidi_GetString, HAVE_WILDMIDI=yes, HAVE_WILDMIDI=no)
  if test "x$HAVE_WILDMIDI" = "xyes"; then
    AC_CHECK_HEADER(wildmidi_lib.h, HAVE_WILDMIDI=yes, HAVE_WILDMIDI=no)
    if test "x$HAVE_WILDMIDI" = "xyes"; then
      WM_LIBS="-lWildMidi"
      AC_DEFINE(HAVE_WILDMIDI, 1, [Define if you have WildMidi])
    fi
  fi
fi							

AC_SUBST(WM_LIBS)

AC_ARG_ENABLE(vorbis,
[  --disable-vorbis         disable ogg vorbis support],
)

HAVE_VORBIS=no
if test "x$enable_vorbis" != "xno"; then
  AM_PATH_OGG(HAVE_OGG=yes, HAVE_OGG=no)
  if test "x$HAVE_OGG" = xyes; then
    AM_PATH_VORBIS(HAVE_VORBIS=yes, HAVE_VORBIS=no)
    if test "x$HAVE_VORBIS" = xyes; then
      AC_DEFINE(HAVE_VORBIS, 1, [define this if you have ogg/vorbis libs])
    fi
  fi
fi

AC_ARG_ENABLE(zlib,
[  --disable-zlib          disable zlib support],
)

HAVE_ZLIB=no
Z_LIBS=""
if test "x$enable_zlib" != "xno"; then
  dnl Check for working -lz
  dnl Note - must have gztell *and* gzgets in -lz *and* zlib.h
  AC_CHECK_LIB(z, gztell, HAVE_ZLIB=yes, HAVE_ZLIB=no, [$LIBS])
  if test "x$HAVE_ZLIB" = "xyes"; then
     AC_CHECK_LIB(z, gzgets, HAVE_ZLIB=yes, HAVE_ZLIB=no, [$LIBS])
     if test "x$HAVE_ZLIB" = "xyes"; then
        AC_CHECK_HEADER(zlib.h, HAVE_ZLIB=yes, HAVE_ZLIB=no)
        if test "x$HAVE_ZLIB" = "xyes"; then
           Z_LIBS="-lz"
        fi
     fi
  fi
fi

AC_ARG_ENABLE(png,
[  --disable-png           disable png support],
)
HAVE_PNG=no
PNG_LIBS=""
if test "x$enable_zlib" != "xno"; then
	AC_CHECK_LIB(png, png_set_read_fn, HAVE_PNG=yes, HAVE_PNG=no, [$LIBS])
	if test "x$HAVE_PNG" = "xyes"; then
		AC_CHECK_HEADER(png.h, HAVE_PNG=yes, HAVE_PNG=no)
		if test "x$HAVE_PNG" = "xyes"; then
			PNG_LIBS="-lpng"
			AC_DEFINE(HAVE_PNG, 1, [Define if you have libpng])
		fi
	fi
fi
AC_SUBST(PNG_LIBS)

AC_ARG_WITH(ipv6,
[  --with-ipv6=DIR         enable IPv6 support. Optional argument specifies
                          location of inet6 libraries.],
	if test "x$withval" = xno ; then
		NETTYPE_IPV6=no
	else
		AC_DEFINE(HAVE_IPV6, 1, [Define this if you want IPv6 support])
		NETTYPE_IPV6=yes
		if test "x$withval" != xyes ; then
			LIBS="$LIBS -L${withval}"
		fi
	fi
	,
	NETTYPE_IPV6=no
)
AM_CONDITIONAL(NETTYPE_IPV6, test "x$NETTYPE_IPV6" = "xyes")

dnl Checks for MGL support
AC_ARG_WITH(mgl,
[  --with-mgl=DIR          use MGL found in DIR],
HAVE_MGL=$withval, HAVE_MGL=auto)
if test "x$HAVE_MGL" != xno; then
	if test "x$ac_cv_header_windows_h" != "xyes"; then
		HAVE_MGL=no
	else
		if test "x$HAVE_MGL" != xauto; then
			MGL_CFLAGS="$MGL_CFLAGS -I$withval/include"
			MGL_LIBS="$MGL_LIBS -L$withval/lib"
		fi
		save_CPPFLAGS="$CPPFLAGS"
		CPPFLAGS="$CPPFLAGS $MGL_CFLAGS"
		AC_CHECK_HEADER(mgraph.h, HAVE_MGL=yes, HAVE_MGL=no)
		CPPFLAGS="$save_CPPFLAGS"

		dnl Make sure -lmgllt or -lmglfx works
		if test "x$HAVE_MGL" = xyes; then
			for lib in mglfx mgllt; do 
				MGL_LIBS="$MGL_LIBS -lgdi32 -lwinmm -ldinput -lddraw"
				AC_CHECK_LIB($lib, MGL_registerDriver,
					MGL_LIBS="-l$lib $MGL_LIBS"
					HAVE_MGL=yes
					break,
					HAVE_MGL=no,
					[$MGL_LIBS]
				)
			done
		fi
	fi
	if test "x$HAVE_MGL" != xyes; then
		MGL_CFLAGS="" MGL_LIBS=""
	fi
fi
AC_SUBST(HAVE_MGL)
AC_SUBST(MGL_CFLAGS)
AC_SUBST(MGL_LIBS)

dnl Checks for Linux FBDev support
AC_ARG_WITH(fbdev,
[  --with-fbdev            use Linux framebuffer device],
HAVE_FBDEV=$withval, HAVE_FBDEV=auto)
if test "x$HAVE_FBDEV" != xno; then
	dnl We should still be able to compile it even if
	dnl there is no fbdev support in the running kernel
	AC_CHECK_HEADERS(linux/fb.h, HAVE_FBDEV=yes, HAVE_FBDEV=no)
fi

AC_SUBST(HAVE_FBDEV)
if test "x$HAVE_FBDEV" = xyes; then
	AC_MSG_CHECKING(for FB_AUX_VGA_PLANES_VGA4)
	AC_TRY_COMPILE(
		[#include "linux/fb.h"],
		[int foo = FB_AUX_VGA_PLANES_VGA4;],
		AC_DEFINE(HAVE_FB_AUX_VGA_PLANES_VGA4, 1, [Define this if you have FB_AUX_VGA_PLANES_VGA4])
		AC_MSG_RESULT(yes),
		AC_MSG_RESULT(no)
	)
	AC_MSG_CHECKING(for FB_AUX_VGA_PLANES_CFB4)
	AC_TRY_COMPILE(
		[#include "linux/fb.h"],
		[int foo = FB_AUX_VGA_PLANES_CFB4;],
		AC_DEFINE(HAVE_FB_AUX_VGA_PLANES_CFB4, 1, [Define this if you have FB_AUX_VGA_PLANES_CFB4])
		AC_MSG_RESULT(yes),
		AC_MSG_RESULT(no)
	)
	AC_MSG_CHECKING(for FB_AUX_VGA_PLANES_CFB8)
	AC_TRY_COMPILE(
		[#include "linux/fb.h"],
		[int foo = FB_AUX_VGA_PLANES_CFB8;],
		AC_DEFINE(HAVE_FB_AUX_VGA_PLANES_CFB8, 1, [Define this if you have FB_AUX_VGA_PLANES_CFB4])
		AC_MSG_RESULT(yes),
		AC_MSG_RESULT(no)
	)
fi

dnl Checks for SVGALib support
AC_ARG_WITH(svga,
[  --with-svga=DIR         use SVGALib found in DIR],
HAVE_SVGA=$withval, HAVE_SVGA=auto)
if test "x$HAVE_SVGA" != xno; then
	if test "x$HAVE_SVGA" != xauto; then
		SVGA_CFLAGS="$SVGA_CFLAGS -I$withval/include"
		SVGA_LIBS="$SVGA_LIBS -L$withval/lib"
		dnl The default system location is /usr/include or /usr/local/include
		dnl and we (obviously) do not need to set CFLAGS for that
	fi
	save_CPPFLAGS="$CPPFLAGS"
	CPPFLAGS="$CPPFLAGS $SVGA_CFLAGS"
	AC_CHECK_HEADER(vga.h, HAVE_SVGA=yes, HAVE_SVGA=no)
	CPPFLAGS="$save_CPPFLAGS"

	dnl Make sure -lvga works
	if test "x$HAVE_SVGA" = xyes; then
		AC_CHECK_LIB(vga, vga_getmousetype, SVGA_LIBS="$SVGA_LIBS -lvga"
			HAVE_SVGA=yes, HAVE_SVGA=no, [$SVGA_LIBS]
		)
	fi 
	if test "x$HAVE_SVGA" != xyes; then
		SVGA_CFLAGS="" SVGA_LIBS=""
	fi
fi
AC_SUBST(HAVE_SVGA)
AC_SUBST(SVGA_CFLAGS)
AC_SUBST(SVGA_LIBS)

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
[  --disable-vidmode       don't use XFree86 VidMode extension],
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

dnl Check for DGA support
AC_ARG_ENABLE(dga,
[  --disable-dga           don't use XFree86 DGA extension],
HAVE_DGA=$enable_dga, HAVE_DGA=auto)
if test "x$HAVE_DGA" != xno; then
	save_CPPFLAGS="$CPPFLAGS"
	CPPFLAGS="$X_CFLAGS $CPPFLAGS"
	AC_CHECK_HEADER(X11/extensions/xf86dga.h,
		dnl Make sure the library works
		[AC_CHECK_LIB(Xxf86dga, XF86DGAQueryVersion,
			AC_DEFINE(HAVE_DGA, 1, [Define if you have the XFree86 DGA extension])
			DGA_LIBS="-lXxf86dga",,
			[$X_LIBS -lXext -lX11 $X_EXTRA_LIBS]
		)],
		[],
		[#include <X11/Xlib.h>]
	)
	CPPFLAGS="$save_CPPFLAGS"
fi
AC_SUBST(DGA_LIBS)

dnl SDL/SDL-GL checks
AC_ARG_ENABLE(sdl,
[  --disable-sdl           disable checking for SDL],
)

if test "x$enable_sdl" != xno; then
	AM_PATH_SDL(1.2.0,
		HAVE_SDL=yes,
		HAVE_SDL=no)
	AC_SUBST(HAVE_SDL)
	if test "x$HAVE_SDL" = "xyes"; then
		case "$host_os" in
			mingw*)
				case "$build_os" in
					cygwin*)
						SDL_LIBS=`echo $SDL_LIBS | sed -e 's/-mwindows//'`
						;;
				esac
			;;
		esac
	fi
fi

dnl XMMS Checks
AC_ARG_ENABLE(xmms,
[  --disable-xmms          disable checking for XMMS],
)

if test "x$mingw" != xyes -a "x$enable_xmms" != xno; then
	AM_PATH_XMMS(0.9.5.1,
		HAVE_XMMS=yes,
		HAVE_XMMS=no)
	AC_SUBST(HAVE_XMMS)	
fi	


dnl ==================================================================
dnl Checks for system type
dnl ==================================================================

dnl Checks for which system driver to use
AC_MSG_CHECKING(for system driver)
case "${host}" in
	i?86-*-mingw32*)
		SYSTYPE=WIN32
		AC_MSG_RESULT([Win32 driver])
		WIN32_LIBS=' $(NET_LIBS)'
		if test "x$host" != "x$build"; then
			case "$build_os" in
				cygwin*)
					WIN32_LIBS=" -L/usr/lib/w32api $WIN32_LIBS"
					;;
			esac
		fi

		;;
	i?86-*-cygwin*)
		SYSTYPE=WIN32
		AC_MSG_RESULT([Win32 driver])
		WIN32_LIBS=' $(NET_LIBS)'
		;;
	*)
		SYSTYPE=POSIX
		AC_MSG_RESULT([default POSIX driver])
		WIN32_LIBS=''
		;;
esac
AM_CONDITIONAL(SYSTYPE_WIN32, test "x$SYSTYPE" = "xWIN32")
AC_SUBST(WIN32_LIBS)

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

dnl ==================================================================
dnl Checks for sound
dnl ==================================================================

AC_CHECK_LIB(mme, waveOutOpen, HAVE_LIBMME=yes)

AC_ARG_ENABLE(alsa,
	[  --disable-alsa          disable checks for ALSA support])

AC_ARG_ENABLE(oss,
	[  --disable-oss           disable checks for OSS support])

AC_ARG_ENABLE(sound,
	[  --disable-sound         disable sound outright])

unset SOUND_TYPES
AC_MSG_CHECKING(for sound support)

HAVE_ALSA=no
if test "x$enable_alsa" != "xno"; then
	if test "x$ac_cv_header_sys_asoundlib_h" = "xyes" -o \
		"x$ac_cv_header_alsa_asoundlib_h" = "xyes"; then
			AC_EGREP_CPP([QF_maGiC_VALUE],
			[
#include <alsa/asoundlib.h>
#if defined(SND_LIB_MAJOR) && defined(SND_LIB_MINOR) && \
    defined(SND_LIB_SUBMINOR)
#if SND_LIB_MAJOR > 0 || (SND_LIB_MAJOR == 0 && SND_LIB_MINOR >= 9 && \
			  SND_LIB_SUBMINOR >= 8)
QF_maGiC_VALUE
#endif
#endif
			],
			SOUND_TYPES="$SOUND_TYPES ALSA"
			ALSA_LIBS="-lasound"
			HAVE_ALSA=yes,
			AC_EGREP_CPP([QF_maGiC_VALUE],
				[
#include <sys/asoundlib.h>
#if defined(SND_LIB_MAJOR) && defined(SND_LIB_MINOR) && \
    defined(SND_LIB_SUBMINOR)
#if SND_LIB_MAJOR > 0 || (SND_LIB_MAJOR == 0 && SND_LIB_MINOR >= 9 && \
			  SND_LIB_SUBMINOR >= 8)
QF_maGiC_VALUE
#endif
#endif
				],
				SOUND_TYPES="$SOUND_TYPES ALSA"
				ALSA_LIBS="-lasound"
				HAVE_ALSA=yes
			)
	    )
	fi
fi
if  test "x$HAVE_ALSA" = "xno"; then
    if test "x$enable_alsa" != "xno"; then
	if test "x$ac_cv_header_sys_asoundlib_h" = "xyes" -o \
		"x$ac_cv_header_alsa_asoundlib_h" = "xyes"; then
			AC_EGREP_CPP([QF_maGiC_VALUE],
			[
#include <alsa/asoundlib.h>
#if defined(SND_LIB_MAJOR) && defined(SND_LIB_MINOR) && \
    defined(SND_LIB_SUBMINOR)
#if SND_LIB_MAJOR == 0 && SND_LIB_MINOR >= 9 && SND_LIB_SUBMINOR < 8
QF_maGiC_VALUE
#endif
#endif
			],
			SOUND_TYPES="$SOUND_TYPES ALSA0_9"
			ALSA_LIBS="-lasound"
			HAVE_ALSA=yes,
			AC_EGREP_CPP([QF_maGiC_VALUE],
				[
#include <sys/asoundlib.h>
#if (defined(SND_LIB_MAJOR) && defined(SND_LIB_MINOR) && \
     defined(SND_LIB_SUBMINOR)
#if SND_LIB_MAJOR == 0 && SND_LIB_MINOR >= 9 && SND_LIB_SUBMINOR < 8
QF_maGiC_VALUE
#endif
#endif
				],
				SOUND_TYPES="$SOUND_TYPES ALSA0_9"
				ALSA_LIBS="-lasound"
				HAVE_ALSA=yes
			)
		)
	fi
    fi
fi
AC_SUBST(ALSA_LIBS)
AC_SUBST(HAVE_ALSA)

SOUND_TYPES="$SOUND_TYPES NULL DISK"

dnl MME
if test "x$ac_cv_header_mme_mmsystem_h" = "xyes" -a \
		"x$HAVE_LIBMME" = "xyes"; then
	AC_EGREP_CPP([QF_maGiC_VALUE],
		[
#include <mme/mmsystem.h>
#ifdef WAVE_OPEN_SHAREABLE
QF_maGiC_VALUE
#endif
		],
		SOUND_TYPES="$SOUND_TYPES MME"
		MME_LIBS="-lmme"
	)
fi
AC_SUBST(MME_LIBS)

dnl OSS
HAVE_OSS=no
if test "x$enable_oss" != "xno"; then
	if test "x$ac_cv_header_sys_soundcard_h" = "xyes" -o \
		"x$ac_cv_header_machine_soundcard_h" = "xyes" -o \
		"x$ac_cv_header_linux_soundcard_h" = "xyes"; then
	AC_EGREP_CPP([QF_maGiC_VALUE],
		[
#include <sys/soundcard.h>
#ifdef SNDCTL_DSP_SETTRIGGER
QF_maGiC_VALUE
#endif
		],
		SOUND_TYPES="$SOUND_TYPES OSS"
		HAVE_OSS=yes
		OSS_LIBS=
		,
		AC_EGREP_CPP([QF_maGiC_VALUE],
			[
#include <linux/soundcard.h>
#ifdef SNDCTL_DSP_SETTRIGGER
QF_maGiC_VALUE
#endif
			],
			SOUND_TYPES="$SOUND_TYPES OSS"
			HAVE_OSS=yes
			OSS_LIBS=
			,
			AC_EGREP_CPP([QF_maGiC_VALUE],
				[
#include <machine/soundcard.h>
#ifdef SNDCTL_DSP_SETTRIGGER
QF_maGiC_VALUE
#endif
				],
				SOUND_TYPES="$SOUND_TYPES OSS"
				HAVE_OSS=yes
				OSS_LIBS=
			)
		)
	)
    fi
fi
AC_SUBST(HAVE_OSS)
AC_SUBST(OSS_LIBS)

dnl SDL digital audio
if test "x$HAVE_SDL" = "xyes"; then
	SOUND_TYPES="$SOUND_TYPES SDL"
fi

dnl SGI
if test "x$ac_cv_header_dmedia_audio_h" = "xyes"; then
   AC_EGREP_CPP([QF_maGiC_VALUE],
   		[
#include <dmedia/audio.h>
#ifdef AL_SAMPLE_16
# ifdef AL_RATE
QF_maGiC_VALUE
# endif
#endif
		],
		SOUND_TYPES="$SOUND_TYPES SGI"
		SGISND_LIBS="-laudio")
fi
AC_SUBST(SGISND_LIBS)

dnl Sun
if test "x$ac_cv_header_sys_audioio_h" = "xyes"; then
	AC_EGREP_CPP([QF_maGiC_VALUE],
		[
#include <sys/audioio.h>
#ifdef AUDIO_SETINFO
QF_maGiC_VALUE
#endif
		],
		SOUND_TYPES="$SOUND_TYPES SUN"
	)
fi

dnl Win32
if test "x$ac_cv_header_windows_h" = "xyes"; then
	SOUND_TYPES="$SOUND_TYPES Win32"
	WINSND_LIBS="-lwinmm"
	if test "x$ac_cv_header_dsound_h" = "xyes"; then
	AC_EGREP_CPP([QF_maGiC_VALUE],
		[
#include <windows.h>
#include <dsound.h>
#ifdef GMEM_MOVEABLE
# ifdef DirectSoundEnumerate
QF_maGiC_VALUE
# endif
#endif
		],
		SOUND_TYPES="$SOUND_TYPES DirectX"
	)
	fi
fi
AC_SUBST(WINSND_LIBS)

if test "x$enable_sound" = "xno"; then
	unset SOUND_TYPES
fi

if test "$SOUND_TYPES"; then
	AC_MSG_RESULT([$SOUND_TYPES])
else
	AC_MSG_RESULT([no])
fi

dnl Tests for joystick support
AC_MSG_CHECKING(for joystick support)
if test -z "$JOYTYPE" -a "x$ac_cv_header_linux_joystick_h" = "xyes"; then
	AC_EGREP_CPP([QF_maGiC_VALUE],[
#include <linux/joystick.h>
#ifdef JS_VERSION
QF_maGiC_VALUE
#endif
	], JOYTYPE="Linux")
fi

if test -z "$JOYTYPE" -a "x$ac_cv_header_dinput_h" = "xyes"; then
	JOYTYPE="Win32"
fi

if test "$JOYTYPE"; then
	AC_MSG_RESULT([$JOYTYPE])
else
	AC_MSG_RESULT([no, using null joystick driver])
fi
AC_SUBST(JOY_LIBS)
AC_SUBST(JOY_CFLAGS)
AM_CONDITIONAL(JOYTYPE_LINUX, test "$JOYTYPE" = "Linux")
AM_CONDITIONAL(JOYTYPE_WIN32, test "$JOYTYPE" = "Win32")
AM_CONDITIONAL(JOYTYPE_NULL, test "$JOYTYPE" != "Linux" -a "$JOYTYPE" != "Win32")

dnl ==================================================================
dnl Checks for CD-ROM
dnl ==================================================================

CD_CFLAGS=""
CD_PLUGIN_TARGETS="cd_file.la"
unset CDTYPE

AC_MSG_CHECKING(for CD audio support)

dnl Linux
AC_EGREP_CPP([QF_maGiC_VALUE],
	[
#include <linux/cdrom.h>
#ifdef CDROMREADTOCENTRY
QF_maGiC_VALUE
#endif
	],
	CDTYPE="$CDTYPE Linux"
	CD_PLUGIN_TARGETS="$CD_PLUGIN_TARGETS cd_linux.la"
)

dnl XMMS audio
if test "x$HAVE_XMMS" = "xyes"; then
	CDTYPE="$CDTYPE XMMS"
	CD_PLUGIN_TARGETS="$CD_PLUGIN_TARGETS cd_xmms.la"
	AC_DEFINE(HAVE_XMMS, [1], [Define if you have XMMS])
fi

dnl SDL CD audio
if test "x$HAVE_SDL" = "xyes"; then
	CDTYPE="$CDTYPE SDL"
	CD_PLUGIN_TARGETS="$CD_PLUGIN_TARGETS cd_sdl.la"
fi

dnl SGI
AC_EGREP_CPP([QF_maGiC_VALUE],
	[
#include <dmedia/cdaudio.h>
#ifdef CD_READY
QF_maGiC_VALUE
#endif
	],
	CDTYPE="$CDTYPE SGI"
	CD_PLUGIN_TARGETS="$CD_PLUGIN_TARGETS cd_sgi.la"
	SGI_CD_LIBS="-lcdaudio -lmediad -lds"
)
AC_SUBST(SGI_CD_LIBS)

AC_EGREP_CPP([QF_maGiC_VALUE],
	[
#include <windows.h>
#include <mmsystem.h>
#if defined(MCI_SET_DOOR_OPEN)
QF_maGiC_VALUE
#endif
	],
	CDTYPE="$CDTYPE WIN32"
	CD_PLUGIN_TARGETS="$CD_PLUGIN_TARGETS cd_win.la"
)
if test "$CDTYPE"; then
	AC_MSG_RESULT([$CDTYPE])
else
	AC_MSG_RESULT([no])
fi

AC_SUBST(CD_LIBS)
AC_SUBST(CD_CFLAGS)

dnl ==================================================================
dnl Checks for networking
dnl ==================================================================

if test "x$ac_cv_func_connect" != "xyes"; then
	AC_CHECK_LIB(socket, connect,
					NET_LIBS="$NET_LIBS -lsocket"
					ac_cv_func_connect=yes
				)
fi
if test "x$ac_cv_func_gethostbyname" != "xyes"; then
	AC_CHECK_LIB(nsl, gethostbyname,
					NET_LIBS="$NET_LIBS -lnsl"
					ac_cv_func_gethostbyname=yes
				)
fi

AC_MSG_CHECKING([for connect in -lwsock32])
SAVELIBS="$LIBS"
LIBS="$LIBS -lwsock32"
AC_TRY_LINK([
#include <winsock.h>
],
[
connect(0, NULL, 42);
],
	NET_LIBS="$NET_LIBS -lwsock32 -lwinmm"
	ac_cv_func_connect=yes
	ac_cv_func_gethostbyname=yes
	HAVE_WSOCK=yes
	AC_MSG_RESULT(yes),
	AC_MSG_RESULT(no)
)
LIBS="$SAVELIBS"

AC_MSG_CHECKING(for UDP support)
if test "x$ac_cv_func_connect" = "xyes" -a "x$ac_cv_func_gethostbyname" = "xyes"; then
	HAVE_UDP=yes
	AC_MSG_RESULT(yes)
else
	AC_MSG_RESULT(no)
fi

if test "x$ac_cv_func_connect" != "xyes"; then
	AC_MSG_CHECKING([for connect in -lwsock32])
	SAVELIBS="$LIBS"
	LIBS="$LIBS -lwsock32"
	AC_TRY_LINK([
#include <winsock.h>
		],
		[
connect (0, NULL, 42);
		],
	    NET_LIBS="$NET_LIBS -lwsock32 -lwinmm"
	    AC_MSG_RESULT(yes),
	    AC_MSG_RESULT(no)
	)
	LIBS="$SAVELIBS"
fi
AC_SUBST(NET_LIBS)


dnl ==================================================================
dnl Misc checks
dnl ==================================================================

if test -d $srcdir/CVS; then
	cvs_def_enabled="!= xno"
	cvs_def_disabled="= xyes"
else
	cvs_def_enabled="= xyes"
	cvs_def_disabled="!= xno"
fi

dnl Set $prefix and $exec_prefix to $ac_default_prefix if they are not set
test "x$prefix" = "xNONE" && prefix=$ac_default_prefix
test "x$exec_prefix" = "xNONE" && exec_prefix="${prefix}"

if test "x$SYSTYPE" = "xWIN32"; then
	default_globalconf="~/${PACKAGE}.conf"
	default_userconf="~/${PACKAGE}rc"
	default_sharepath="."
	default_userpath="."
else
	default_globalconf="/etc/${PACKAGE}.conf"
	default_userconf="~/.${PACKAGE}rc"
	eval foo="$datadir"
	default_sharepath="$foo/games/$PACKAGE"
	default_userpath="~/.$PACKAGE"
fi

AC_ARG_WITH(global-cfg,
[  --with-global-cfg=FILE  If set will change the name and location of the
                          global config file used by QuakeForge.  Defaults to
                          /etc/quakeforge.conf.],
globalconf="$withval", globalconf="auto")
if test "x$globalconf" = "xauto" || test "x$globalconf" = "xyes" || \
	test "x$globalconf" = "xno"; then  dnl yes/no sanity checks
	globalconf="$default_globalconf"
fi
AC_DEFINE_UNQUOTED(FS_GLOBALCFG, "$globalconf", [Define this to the location of the global config file])

AC_ARG_WITH(user-cfg,
[  --with-user-cfg=FILE    If set will change the name and location of the
                          user-specific config file used by QuakeForge.
                          Defaults to ~/.quakeforgerc.],
userconf="$withval", userconf="auto")
if test "x$userconf" = "xauto" || test "x$userconf" = "xyes" || \
	test "x$userconf" = "xno"; then  dnl yes/no sanity checks
	userconf="$default_userconf"
fi
AC_DEFINE_UNQUOTED(FS_USERCFG, "$userconf", [Define this to the location of the user config file])

AC_ARG_WITH(sharepath,
[  --with-sharepath=DIR    Use DIR for shared game data, defaults to
                          '.' or \${datadir}/games/quakeforge (if new style)],
sharepath=$withval, sharepath="auto")
if test "x$sharepath" = "xauto" -o "x$sharepath" = "xyes" -o "x$sharepath" = "x"; then
	sharepath="$default_sharepath"
elif test "x$sharepath" = xno; then
	sharepath="."
fi
AC_DEFINE_UNQUOTED(FS_SHAREPATH, "$sharepath", [Define this to the shared game directory root])
AC_SUBST(sharepath)

AC_ARG_WITH(userpath,
[  --with-userpath=DIR     Use DIR for unshared game data, defaults to
                          '.' or ~/.quakeforge (if new style)],
userpath=$withval, userpath="auto")
if test "x$userpath" = "xauto" -o "x$userpath" = "xyes" -o "x$userpath" = "x"; then
	userpath="$default_userpath"
elif test "x$userpath" = xno; then
	userpath="."
fi
AC_DEFINE_UNQUOTED(FS_USERPATH, "$userpath", [Define this to the unshared game directory root])

AC_ARG_WITH(plugin-path,
[  --with-plugin-path=DIR  Use DIR for loading plugins, defaults to
                          \${libdir}/quakeforge],
plugindir=$withval, plugindir="auto")

PLUGINDIR="\${libdir}/quakeforge"
if test "x$plugindir" = "xauto" -o "x$plugindir" = "xyes" -o "x$plugindir" = "x"; then
	plugindir="$PLUGINDIR"
elif test "x$plugindir" = xno; then
	plugindir="."
else
	PLUGINDIR="$plugindir"
fi
eval expanded_plugindir="$plugindir"
eval expanded_plugindir="$expanded_plugindir"
AC_DEFINE_UNQUOTED(FS_PLUGINPATH, "$expanded_plugindir", [Define this to the path from which to load plugins])
AC_SUBST(plugindir)
AC_SUBST(PLUGINDIR)

AC_ARG_WITH(gl-driver,
	[  --with-gl-driver=NAME   Name of OpenGL driver DLL/DSO],
	gl_driver=$withval,
	gl_driver=auto
)
if test "x$gl_driver" = xauto -o "x$gl_driver" = xyes; then
	if test "$SYSTYPE" = WIN32; then
		gl_driver="OPENGL32.DLL"
	else
		gl_driver="libGL.so.1"
	fi
fi
AC_DEFINE_UNQUOTED(GL_DRIVER, "$gl_driver", [Define this to the default GL dynamic lib])

dnl CFLAGS for release and devel versions
AC_ARG_WITH(efence,
	[  --with-efence           link with -lefence],
	AC_CHECK_LIB(efence,malloc)
)

AC_ARG_ENABLE(debug,
	[  --disable-debug         compile without debugging],
	debug=$enable_debug
)
AC_MSG_CHECKING(for debugging)
if test "x$debug" != xno; then
	AC_MSG_RESULT(yes)
	BUILD_TYPE="$BUILD_TYPE Debug"
	CFLAGS="$CFLAGS -g"
else
	AC_MSG_RESULT(no)
fi

AC_ARG_ENABLE(optimize,
	[  --disable-optimize      compile without optimizations (for development)],
	optimize=$enable_optimize,
	optimize=yes
)

AC_ARG_ENABLE(typecheck-progs,
	[  --enable-typecheck-progs Enable type checking on progs field access])
if test "x$enable_typecheck_progs" = xyes; then
	AC_DEFINE(TYPECHECK_PROGS, 1, [Define this if you want progs typechecking])
fi

AC_MSG_CHECKING(for optimization)
if test "x$optimize" = xyes; then
	AC_MSG_RESULT(yes)
	BUILD_TYPE="$BUILD_TYPE Optimize"
	if test "x$GCC" = xyes; then
		set $CC
		shift
		args="$*"
		AC_MSG_CHECKING(for gcc version)
		CCVER="gcc `$CC --version | grep '[[0-9]]\.[[0-9]]' | sed -e 's/.*(GCC)//' -e 's/[[^0-9]]*\([[0-9.]]*\).*/\1/'`"
		set $CCVER
		IFS="."
		set $2
		CC_MAJ=$1
		CC_MIN=$2
		CC_SUB=$3
		IFS=" "
		AC_MSG_RESULT($CCVER)
		saved_cflags="$CFLAGS"
		CFLAGS=""
		QF_CC_OPTION(-frename-registers)
		QF_CC_OPTION(-finline-limit=32000 -Winline)
		heavy="-O2 $CFLAGS -ffast-math -funroll-loops -fomit-frame-pointer -fexpensive-optimizations"
		CFLAGS="$saved_cflags"
		light="-O2"
		AC_ARG_ENABLE(strict-aliasing,
	[  --enable-strict-aliasing enable the -fstrict-aliasing option of gcc])
		if test "x$enable_strict_aliasing" = "xyes"; then
			heavy="$heavy -fstrict-aliasing"
			light="$light -fstrict-aliasing"
		else
			if test "x$enable_strict_aliasing" != "xno"; then
				if test $CC_MAJ -ge 3; then
					heavy="$heavy -fstrict-aliasing"
				else
					if test $CC_MAJ = 2 -a $CC_MIN = 96; then
						light="$light -fno-strict-aliasing"
					fi
				fi
			else
				if test $CC_MAJ = 2 -a $CC_MIN = 96; then
					light="$light -fno-strict-aliasing"
				fi
			fi
		fi
		AC_MSG_CHECKING(for special compiler settings)
		AC_ARG_WITH(arch,
		[  --with-arch=ARCH        control compiler architecture directly],
			arch="$withval", arch=auto
		)
		case "$arch" in
			auto)
				case "${target_cpu}" in
					i?86)
						MORE_CFLAGS="-march=${target_cpu}"
						;;
					*)
						MORE_CFLAGS=""
						;;
				esac
				;;
			no|"")
				MORE_CFLAGS=""
				;;
			*)
				MORE_CFLAGS="-march=$arch"
				;;
		esac
		if test "x$MORE_CFLAGS" = x; then
			AC_MSG_RESULT(no)
		else
			save_CFLAGS="$CFLAGS"
			CFLAGS="$CFLAGS $MORE_CFLAGS"
			AC_TRY_COMPILE(
				[],
				[],
				AC_MSG_RESULT(yes),
				CFLAGS="$save_CFLAGS"
				AC_MSG_RESULT(no)
			)
		fi
		if test $CC_MAJ = 2 -a $CC_MIN = 96; then
			AC_MSG_CHECKING(if align options work)
			save_CFLAGS="$CFLAGS"
			CFLAGS="$CFLAGS -malign-loops=2 -malign-jumps=2 -malign-functions=2"
			AC_TRY_COMPILE(
				[],
				[],
				light="$light -malign-loops=2 -malign-jumps=2 -malign-functions=2"
				AC_MSG_RESULT(yes),
				AC_MSG_RESULT(no)
			)
			CFLAGS="$save_CFLAGS"
			CFLAGS="$CFLAGS $light"
		else
			CFLAGS="$CFLAGS $heavy"
		fi
	else
		CFLAGS=-O2
	fi
else
	AC_MSG_RESULT(no)
fi

dnl CFLAGS for release and devel versions
AC_ARG_ENABLE(profile,
	[  --enable-profile        compile with profiling (for development)],
	profile=$enable_profile
)
if test "x$profile" = xyes; then
	BUILD_TYPE="$BUILD_TYPE Profile"
	if test "x$GCC" = xyes; then
		CFLAGS="`echo $CFLAGS | sed -e 's/-fomit-frame-pointer//g'` -pg"
		LDFLAGS="$LDFLAGS -pg"
	else
		CFLAGS="$CFLAGS -p"
	fi
fi

check_pipe=no
if test "x$GCC" = xyes; then
	dnl Check for -pipe vs -save-temps.
	AC_MSG_CHECKING(for -pipe vs -save-temps)
	AC_ARG_ENABLE(save-temps,
		[  --enable-save-temps     save temporary files],
		AC_MSG_RESULT(-save-temps)
		CFLAGS="$CFLAGS -save-temps"
		BUILD_TYPE="$BUILD_TYPE Save-temps"
		,
		AC_MSG_RESULT(-pipe)
		check_pipe=yes
	)
fi
if test "x$check_pipe" = xyes; then
	QF_CC_OPTION(-pipe)
fi

dnl QuakeForge uses lots of BCPL-style (//) comments, which can cause problems
dnl with many compilers that don't support the latest ISO standards. Well,
dnl that is our cover story -- the reality is that we like them and don't want
dnl to give them up. :)
dnl Make the compiler swallow its pride...
if test "x$GCC" != xyes; then
   AC_MSG_CHECKING(for how to deal with BCPL-style comments)
   case "${host}" in
   *-aix*)
	CFLAGS="$CFLAGS -qcpluscmt"
        AC_MSG_RESULT([-qcpluscmt])
	;;
   *-irix6*)
	CFLAGS="$CFLAGS -Xcpluscomm"
        AC_MSG_RESULT([-Xcpluscomm])
	;;
   *-solaris*)
	CFLAGS="$CFLAGS -xCC"
        AC_MSG_RESULT([-xCC])
	;;
   *)
        AC_MSG_RESULT(nothing needed or no switch known)
	;;
  esac
fi

AC_ARG_ENABLE(Werror,
[  --enable-Werror         treat warnings as errors]
)
dnl We want warnings, lots of warnings...
if test "x$GCC" = "xyes"; then
	if test "x$enable_Werror" $cvs_def_enabled; then
		CFLAGS="$CFLAGS -Wall -Werror -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations"
	else
		CFLAGS="$CFLAGS -Wall"
	fi
	CFLAGS="$CFLAGS -fno-common"
fi

AS="$CC"
if test "x$SYSTYPE" = "xWIN32"; then
	ASFLAGS="\$(DEFS) \$(CFLAGS) \$(CPPFLAGS) \$(DEFAULT_INCLUDES) \$(INCLUDES) -D_WIN32"
	plugin_ldflags="-no-undefined"
	plugin_libadd="-luser32 -lgdi32 -lwinmm"
	case "$host_os" in
		mingw*)
			if test "x$host" != "x$build"; then
				case "$build_os" in
					cygwin*)
						plugin_libadd=" -L/usr/lib/w32api $plugin_libadd"
						;;
				esac
			fi
			;;
	esac

else
	ASFLAGS="\$(DEFS) \$(CFLAGS) \$(CPPFLAGS) \$(DEFAULT_INCLUDES) \$(INCLUDES)"
	plugin_ldflags=
	plugin_libadd=
fi
ASFLAGS="$ASFLAGS -DQFASM"
CCASFLAGS="$ASFLAGS"
CCAS="$AS"
AC_SUBST(AS)
AC_SUBST(ASFLAGS)
AC_SUBST(CCAS)
AC_SUBST(CCASFLAGS)

dnl silence automake about .r files
F77=touch
AC_SUBST(F77)

dnl ==================================================================
dnl Make sure we link against a working zlib
dnl ==================================================================

if test "x$HAVE_ZLIB" = xyes; then
	x11_old_libz=no
	if test -n "$X_LIBS"; then
		AC_MSG_CHECKING(for libz.a in X11)
		for d in `echo $X_LIBS | sed -e 's/-L//'`; do
			if test -f $d/libz.a; then
				x11_old_libz=$d
				break
			fi
		done
	fi
	if test "x$x11_old_libz" != xno; then
		AC_MSG_RESULT(yes)
		AC_MSG_WARN(there is an old libz.a in $x11_old_libz)
		AC_MSG_WARN($PROGRAM is unable to support compression)
		AC_MSG_WARN(due to library search order issues)
		AC_MSG_WARN(if you wish to have compression support)
		AC_MSG_WARN(please delete $x11_old_libz/libz.a)
		ac_cv_header_zlib_h=no
		HAVE_ZLIB="no, due to conflicting $x11_old_libz/libz.a"
		Z_LIBS=""
	else
		if test -n "$X_LIBS"; then
			AC_MSG_RESULT(no)
		fi
		AC_DEFINE(HAVE_ZLIB, 1, [Define if you have zlib])
	fi
fi
AC_SUBST(Z_LIBS)
AM_CONDITIONAL(HAVE_ZLIB, test "$HAVE_ZLIB" = "yes")

dnl ==================================================================
dnl Find out what to build
dnl ==================================================================

QF_WITH_TARGETS(
	clients,
	[  --with-clients=<list>   compile clients in <list>:],
	[3dfx,fbdev,glx,mgl,sdl,sdl32,sgl,svga,wgl,x11],dummy
)
QF_WITH_TARGETS(
	servers,
	[  --with-servers=<list>   compile dedicated server:],
	[master,nq,qw,qtv],dummy
)
QF_WITH_TARGETS(
	tools,
	[  --with-tools=<list>     compile qf tools:],
	[bsp2img,carne,pak,qfbsp,qfcc,qflight,qfmodelgen,qfvis,qwaq,wad,wav],dummy
)

unset CL_TARGETS
HW_TARGETS=""
QTV_TARGETS=""
QW_TARGETS=""
NQ_TARGETS=""

BUILD_GL=no
BUILD_SW32=no
BUILD_SW=no
CAN_BUILD_GL=no
CAN_BUILD_SW32=no
CAN_BUILD_SW=no

CD_TARGETS=""
SND_PLUGIN_TARGETS="snd_output_disk.la"
SND_REND_TARGETS=""
SND_TARGETS=""
VID_MODEL_TARGETS=""
VID_REND_TARGETS=""
VID_REND_NOINST_TARGETS=""
VID_TARGETS=""

if test "x$HAVE_FBDEV" = xyes; then
	if test "x$ENABLE_clients_fbdev" = xyes; then
		QW_TARGETS="$QW_TARGETS qw-client-fbdev\$(EXEEXT)"
		NQ_TARGETS="$NQ_TARGETS nq-fbdev\$(EXEEXT)"
		CL_TARGETS="$CL_TARGETS FBDEV"
		VID_TARGETS="$VID_TARGETS libQFfbdev.la"
		BUILD_SW=yes
		QF_NEED(vid, [common sw])
		QF_NEED(qw, [client common])
		QF_NEED(nq, [client common])
	fi
	CAN_BUILD_SW=yes
fi
if test "x$HAVE_X" = xyes; then
	CAN_BUILD_GL=yes
	CAN_BUILD_SW=yes
	if test "x$ENABLE_clients_glx" = xyes; then
		QW_TARGETS="$QW_TARGETS qw-client-glx\$(EXEEXT)"
		NQ_TARGETS="$NQ_TARGETS nq-glx\$(EXEEXT)"
		CL_TARGETS="$CL_TARGETS GLX"
		VID_TARGETS="$VID_TARGETS libQFglx.la"
		BUILD_GL=yes
		QF_NEED(vid, [common gl x11])
		QF_NEED(qw, [client common])
		QF_NEED(nq, [client common])
	fi
	if test "x$ENABLE_clients_x11" = xyes; then
		QW_TARGETS="$QW_TARGETS qw-client-x11\$(EXEEXT)"
		NQ_TARGETS="$NQ_TARGETS nq-x11\$(EXEEXT)"
		CL_TARGETS="$CL_TARGETS X11"
		VID_TARGETS="$VID_TARGETS libQFx11.la"
		BUILD_SW=yes
		QF_NEED(vid, [common sw x11])
		QF_NEED(qw, [client common])
		QF_NEED(nq, [client common])
	fi
fi
if test "x$HAVE_MGL" = xyes; then
	if test "x$ENABLE_clients_mgl" = xyes; then
		QW_TARGETS="$QW_TARGETS qw-client-mgl\$(EXEEXT)"
		NQ_TARGETS="$NQ_TARGETS nq-mgl\$(EXEEXT)"
		CL_TARGETS="$CL_TARGETS MGL"
		VID_TARGETS="$VID_TARGETS libQFwgl.la"
		BUILD_SW=yes
		QF_NEED(vid, [common sw])
		QF_NEED(qw, [client common])
		QF_NEED(nq, [client common])
	fi
	CAN_BUILD_SW=yes
fi
if test "x$HAVE_SDL" = xyes; then
	if test "x$ENABLE_clients_sdl" = xyes; then
		QW_TARGETS="$QW_TARGETS qw-client-sdl\$(EXEEXT)"
		NQ_TARGETS="$NQ_TARGETS nq-sdl\$(EXEEXT)"
		CL_TARGETS="$CL_TARGETS SDL"
		VID_TARGETS="$VID_TARGETS libQFsdl.la"
		BUILD_SW=yes
		QF_NEED(vid, [common sdl sw])
		QF_NEED(qw, [client common sdl])
		QF_NEED(nq, [client common sdl])
	fi
	CAN_BUILD_SW=yes
	if test "x$ENABLE_clients_sdl32" = xyes; then
		QW_TARGETS="$QW_TARGETS qw-client-sdl32\$(EXEEXT)"
		NQ_TARGETS="$NQ_TARGETS nq-sdl32\$(EXEEXT)"
		CL_TARGETS="$CL_TARGETS SDL32"
		VID_TARGETS="$VID_TARGETS libQFsdl32.la"
		BUILD_SW32=yes
		QF_NEED(vid, [common sdl sw32])
		QF_NEED(qw, [client common sdl])
		QF_NEED(nq, [client common sdl])
	fi
	CAN_BUILD_SW32=yes
	if test "x$ENABLE_clients_sgl" = xyes; then
		QW_TARGETS="$QW_TARGETS qw-client-sgl\$(EXEEXT)"
		NQ_TARGETS="$NQ_TARGETS nq-sgl\$(EXEEXT)"
		CL_TARGETS="$CL_TARGETS SDL-GL"
		VID_TARGETS="$VID_TARGETS libQFsgl.la"
		BUILD_GL=yes
		CAN_BUILD_GL=yes
		QF_NEED(vid, [common sdl gl])
		QF_NEED(qw, [client common sdl])
		QF_NEED(nq, [client common sdl])
	fi
fi
if test "x$HAVE_SVGA" = xyes; then
	if test "x$ENABLE_clients_svga" = xyes; then
		QW_TARGETS="$QW_TARGETS qw-client-svga\$(EXEEXT)"
		NQ_TARGETS="$NQ_TARGETS nq-svga\$(EXEEXT)"
		CL_TARGETS="$CL_TARGETS SVGAlib"
		VID_TARGETS="$VID_TARGETS libQFsvga.la"
		BUILD_SW=yes
		QF_NEED(vid, [asm common svga sw])
		QF_NEED(qw, [client common])
		QF_NEED(nq, [client common])
	fi
	CAN_BUILD_SW=yes
	if test "x$ENABLE_clients_3dfx" = xyes; then
		QW_TARGETS="$QW_TARGETS qw-client-3dfx\$(EXEEXT)"
		NQ_TARGETS="$NQ_TARGETS nq-3dfx\$(EXEEXT)"
		CL_TARGETS="$CL_TARGETS 3dfx"
		VID_TARGETS="$VID_TARGETS libQFtdfx.la"
		BUILD_GL=yes
		QF_NEED(vid, [asm common gl svga])
		QF_NEED(qw, [client common])
		QF_NEED(nq, [client common])
	fi
	CAN_BUILD_GL=yes
fi
if test "x$mingw" = xyes; then
	if test "x$ENABLE_clients_wgl" = xyes; then
		QW_TARGETS="$QW_TARGETS qw-client-wgl\$(EXEEXT)"
		NQ_TARGETS="$NQ_TARGETS nq-wgl\$(EXEEXT)"
		CL_TARGETS="$CL_TARGETS WGL"
		VID_TARGETS="$VID_TARGETS libQFwgl.la"
		BUILD_GL=yes
		QF_NEED(vid, [common gl])
		QF_NEED(qw, [client common])
		QF_NEED(nq, [client common])
	fi
	CAN_BUILD_GL=yes
fi

unset SV_TARGETS
if test "x$ENABLE_servers_nq" = xyes; then
	NQ_TARGETS="nq-server\$(EXEEXT) $NQ_TARGETS"
	SV_TARGETS="$SV_TARGETS nq"
	QF_NEED(nq, [common server])
fi
if test "x$ENABLE_servers_qtv" = xyes; then
	QTV_TARGETS="qtv\$(EXEEXT) $QTV_TARGETS"
	SV_TARGETS="$SV_TARGETS qtv"
#	QF_NEED(qtv, [common server])
fi
if test "x$ENABLE_servers_master" = xyes; then
	HW_TARGETS="hw-master\$(EXEEXT) $HW_TARGETS"
	QW_TARGETS="qw-master\$(EXEEXT) $QW_TARGETS"
	SV_TARGETS="$SV_TARGETS master"
fi
if test "x$ENABLE_servers_qw" = xyes; then
	QW_TARGETS="qw-server\$(EXEEXT) $QW_TARGETS"
	SV_TARGETS="$SV_TARGETS qw"
	QF_NEED(qw, [common server])
fi

unset TOOLS_TARGETS
if test "x$ENABLE_tools_bsp2img" = xyes; then
	TOOLS_TARGETS="$TOOLS_TARGETS bsp2img"
fi
if test "x$ENABLE_tools_carne" = xyes; then
	TOOLS_TARGETS="$TOOLS_TARGETS carne"
fi
if test "x$ENABLE_tools_pak" = xyes; then
	TOOLS_TARGETS="$TOOLS_TARGETS pak"
fi
if test "x$ENABLE_tools_qfbsp" = xyes; then
	TOOLS_TARGETS="$TOOLS_TARGETS qfbsp"
fi
if test "x$ENABLE_tools_qfcc" = xyes; then
	TOOLS_TARGETS="$TOOLS_TARGETS qfcc"
fi
if test "x$ENABLE_tools_qflight" = xyes; then
	TOOLS_TARGETS="$TOOLS_TARGETS qflight"
fi
if test "x$ENABLE_tools_qfmodelgen" = xyes; then
	TOOLS_TARGETS="$TOOLS_TARGETS qfmodelgen"
fi
if test "x$ENABLE_tools_qfvis" = xyes; then
	TOOLS_TARGETS="$TOOLS_TARGETS qfvis"
fi
if test "x$ENABLE_tools_qwaq" = xyes; then
	TOOLS_TARGETS="$TOOLS_TARGETS qwaq"
fi
if test "x$ENABLE_tools_wad" = xyes; then
	TOOLS_TARGETS="$TOOLS_TARGETS wad"
fi
if test "x$ENABLE_tools_wav" = xyes; then
	TOOLS_TARGETS="$TOOLS_TARGETS wav"
fi

AM_CONDITIONAL(BUILD_BSP2IMG, test "$ENABLE_tools_bsp2img" = "yes")
AM_CONDITIONAL(BUILD_CARNE, test "$ENABLE_tools_carne" = "yes")
AM_CONDITIONAL(BUILD_PAK, test "$ENABLE_tools_pak" = "yes")
AM_CONDITIONAL(BUILD_QFBSP, test "$ENABLE_tools_qfbsp" = "yes")
AM_CONDITIONAL(BUILD_QFCC, test "$ENABLE_tools_qfcc" = "yes")
AM_CONDITIONAL(BUILD_QFLIGHT, test "$ENABLE_tools_qflight" = "yes")
AM_CONDITIONAL(BUILD_QFMODELGEN, test "$ENABLE_tools_qfmodelgen" = "yes")
AM_CONDITIONAL(BUILD_QFVIS, test "$ENABLE_tools_qfvis" = "yes")
AM_CONDITIONAL(BUILD_QWAQ, test "$ENABLE_tools_qwaq" = "yes" -a "$ENABLE_tools_qfcc" = "yes")
AM_CONDITIONAL(BUILD_WAD, test "$ENABLE_tools_wad" = "yes")
AM_CONDITIONAL(BUILD_WAV, test "$ENABLE_tools_wav" = "yes")

AM_CONDITIONAL(BUILD_RUAMOKO, test "$ENABLE_tools_qfcc" = "yes" -a "$ENABLE_tools_pak" = "yes")

if test "x$BUILD_SW" = xyes; then
	VID_REND_NOINST_TARGETS="$VID_REND_NOINST_TARGETS libQFrenderer_sw.la"
	VID_MODEL_TARGETS="$VID_MODEL_TARGETS libQFmodels_sw.la"
fi
if test "x$BUILD_SW32" = xyes; then
	VID_REND_TARGETS="$VID_REND_TARGETS libQFrenderer_sw32.la"
	if test "x$BUILD_SW" != xyes; then
		VID_MODEL_TARGETS="$VID_MODEL_TARGETS libQFmodels_sw.la"
	fi
fi
if test "x$BUILD_GL" = xyes; then
	VID_REND_TARGETS="$VID_REND_TARGETS libQFrenderer_gl.la"
	VID_MODEL_TARGETS="$VID_MODEL_TARGETS libQFmodels_gl.la"
fi

QF_PROCESS_NEED(vid, [asm common gl sdl sw sw32 svga x11])
QF_PROCESS_NEED(qw, [client common sdl server], a)
QF_PROCESS_NEED(nq, [client common sdl server], a)

AC_SUBST(CAN_BUILD_GL)
AC_SUBST(CAN_BUILD_SW)
AC_SUBST(CAN_BUILD_SW32)

AC_SUBST(HAVE_FBDEV)
AC_SUBST(HAVE_SDL)
AC_SUBST(HAVE_SVGA)
AC_SUBST(HAVE_X)

ALSA_PLUGIN="snd_output_alsa"
if test "`echo $SOUND_TYPES | grep ALSA0_9`"; then
	ALSA_PLUGIN="snd_output_alsa0_9"
fi

if test -n "$CL_TARGETS"; then
	CD_TARGETS="libQFcd.la"
	SND_TARGETS="libQFsound.la"
	JOY_TARGETS="libQFjs.la"
	# the following will catch both ALSA and ALSA0_9
	if test "`echo $SOUND_TYPES | grep ALSA`"; then
		SND_PLUGIN_TARGETS="$SND_PLUGIN_TARGETS $ALSA_PLUGIN.la"
	fi
	if test "`echo $SOUND_TYPES | grep MME`"; then
		SND_PLUGIN_TARGETS="$SND_PLUGIN_TARGETS snd_output_mme.la"
	fi
	if test "`echo $SOUND_TYPES | grep OSS`"; then
		SND_PLUGIN_TARGETS="$SND_PLUGIN_TARGETS snd_output_oss.la"
	fi
	if test "`echo $SOUND_TYPES | grep SDL`"; then
		SND_PLUGIN_TARGETS="$SND_PLUGIN_TARGETS snd_output_sdl.la"
	fi
	if test "`echo $SOUND_TYPES | grep SGI`"; then
		SND_PLUGIN_TARGETS="$SND_PLUGIN_TARGETS snd_output_sgi.la"
	fi
	if test "`echo $SOUND_TYPES | grep SUN`"; then
		SND_PLUGIN_TARGETS="$SND_PLUGIN_TARGETS snd_output_sun.la"
	fi
	if test "`echo $SOUND_TYPES | grep Win32`"; then
		SND_PLUGIN_TARGETS="$SND_PLUGIN_TARGETS snd_output_win.la"
	fi
	if test "`echo $SOUND_TYPES | grep DirectX`"; then
		SND_PLUGIN_TARGETS="$SND_PLUGIN_TARGETS snd_output_dx.la"
	fi
	if test "$SOUND_TYPES"; then
		SND_REND_TARGETS="$SND_REND_TARGETS snd_render_default.la"
	fi
else
	unset CDTYPE
	CD_PLUGIN_TARGETS=""
	CD_TARGETS=""
	JOY_TARGETS=""
	SND_PLUGIN_TARGETS=""
	SND_REND_TARGETS=""
	SND_TARGETS=""
	unset SOUND_TYPES
fi
AC_SUBST(ALSA_PLUGIN)

SERVER_PLUGIN_TARGETS="console_server.la"
SERVER_PLUGIN_STATIC=""
CLIENT_PLUGIN_TARGETS="console_client.la"
CLIENT_PLUGIN_STATIC=""
CD_PLUGIN_STATIC=""
SND_PLUGIN_STATIC=""
SND_REND_STATIC=""

if test "x$enable_shared" = xno; then
	PREFER_PIC=
	PREFER_NON_PIC=
else
	PREFER_PIC=-prefer-pic
	PREFER_NON_PIC=-prefer-non-pic
fi
if test "x$enable_static" = xno; then
	STATIC=
else
	STATIC=-static
fi
AC_SUBST(PREFER_PIC)
AC_SUBST(PREFER_NON_PIC)
AC_SUBST(STATIC)

AC_ARG_WITH(static-plugins,
[  --with-static-plugins   build plugins into executable rather than separate],
	static_plugins="$withval", static_plugins=auto)
if test "x$static_plugins" = xauto; then
	if test "x$enable_shared" = xno -o "x$SYSTYPE" = xWIN32; then
		static_plugins=yes
	fi
fi
if test "x$static_plugins" = xyes; then
	AC_DEFINE(STATIC_PLUGINS, 1, [Define this if you are building static plugins])
	SERVER_PLUGIN_STATIC="$SERVER_PLUGIN_TARGETS"
	SERVER_PLUGIN_TARGETS=""
	CLIENT_PLUGIN_STATIC="$CLIENT_PLUGIN_TARGETS"
	CLIENT_PLUGIN_TARGETS=""
	CD_PLUGIN_STATIC="$CD_PLUGIN_TARGETS"
	CD_PLUGIN_TARGETS=""
	SND_PLUGIN_STATIC="$SND_PLUGIN_TARGETS"
	SND_PLUGIN_TARGETS=""
	SND_REND_STATIC="$SND_REND_TARGETS"
	SND_REND_TARGETS=""
	if test -n "$SOUND_TYPES"; then
		SOUND_TYPES="$SOUND_TYPES (static)"
		CDTYPE="$CDTYPE (static)"
	fi
	plugin_ldflags="$plugin_ldflags"
else
	plugin_ldflags="$plugin_ldflags"' -version-info $(plugin_version) -rpath $(plugindir)'
fi
plugin_ldflags="$plugin_ldflags -module -avoid-version"
SERVER_PLUGIN_STATIC_LIBS=""
CLIENT_PLUGIN_STATIC_LIBS=""
CD_PLUGIN_STATIC_LIBS=""
SND_PLUGIN_STATIC_LIBS=""
SND_REND_STATIC_LIBS=""
SERVER_PLUGIN_LIST="{0, 0}"
CLIENT_PLUGIN_LIST="{0, 0}"
CD_PLUGIN_LIST="{0, 0}"
SND_OUTPUT_LIST="{0, 0}"
SND_RENDER_LIST="{0, 0}"
SERVER_PLUGIN_PROTOS=""
CLIENT_PLUGIN_PROTOS=""
CD_PLUGIN_PROTOS=""
SND_OUTPUT_PROTOS=""
SND_RENDER_PROTOS=""
for l in $SERVER_PLUGIN_STATIC; do
	SERVER_PLUGIN_STATIC_LIBS="$SERVER_PLUGIN_STATIC_LIBS "'$(top_builddir)'"/libs/console/$l"
	n="`echo $l | sed -e 's/\(.*\)\.la/\1/'`"
	SERVER_PLUGIN_LIST='{"'"$n"'"'", ${n}_PluginInfo},$SERVER_PLUGIN_LIST"
	SERVER_PLUGIN_PROTOS="$SERVER_PLUGIN_PROTOS extern plugin_t *${n}_PluginInfo (void);"
done
for l in $CLIENT_PLUGIN_STATIC; do
	CLIENT_PLUGIN_STATIC_LIBS="$CLIENT_PLUGIN_STATIC_LIBS "'$(top_builddir)'"/libs/console/$l"
	n="`echo $l | sed -e 's/\(.*\)\.la/\1/'`"
	CLIENT_PLUGIN_LIST='{"'"$n"'"'", ${n}_PluginInfo},$CLIENT_PLUGIN_LIST"
	CLIENT_PLUGIN_PROTOS="$CLIENT_PLUGIN_PROTOS extern plugin_t *${n}_PluginInfo (void);"
done
for l in $CD_PLUGIN_STATIC; do
	CD_PLUGIN_STATIC_LIBS="$CD_PLUGIN_STATIC_LIBS cd/$l"
	n="`echo $l | sed -e 's/\(.*\)\.la/\1/'`"
	CD_PLUGIN_LIST='{"'"$n"'"'", ${n}_PluginInfo},$CD_PLUGIN_LIST"
	CD_PLUGIN_PROTOS="$CD_PLUGIN_PROTOS extern plugin_t *${n}_PluginInfo (void);"
done
for l in $SND_PLUGIN_STATIC; do
	SND_PLUGIN_STATIC_LIBS="$SND_PLUGIN_STATIC_LIBS targets/$l"
	n="`echo $l | sed -e 's/\(.*\)\.la/\1/'`"
	SND_OUTPUT_LIST='{"'"$n"'"'", ${n}_PluginInfo},$SND_OUTPUT_LIST"
	SND_OUTPUT_PROTOS="$SND_OUTPUT_PROTOS extern plugin_t *${n}_PluginInfo (void);"
done
for l in $SND_REND_STATIC; do
	SND_REND_STATIC_LIBS="$SND_REND_STATIC_LIBS renderer/$l"
	n="`echo $l | sed -e 's/\(.*\)\.la/\1/'`"
	SND_RENDER_LIST='{"'"$n"'"'", ${n}_PluginInfo},$SND_RENDER_LIST"
	SND_RENDER_PROTOS="$SND_RENDER_PROTOS extern plugin_t *${n}_PluginInfo (void);"
done
AC_DEFINE_UNQUOTED(SERVER_PLUGIN_LIST, $SERVER_PLUGIN_LIST, [list of server plugins])
AC_DEFINE_UNQUOTED(SERVER_PLUGIN_PROTOS, $SERVER_PLUGIN_PROTOS, [list of server prototypes])
AC_DEFINE_UNQUOTED(CLIENT_PLUGIN_LIST, $CLIENT_PLUGIN_LIST, [list of client plugins])
AC_DEFINE_UNQUOTED(CLIENT_PLUGIN_PROTOS, $CLIENT_PLUGIN_PROTOS, [list of client prototypes])
AC_DEFINE_UNQUOTED(CD_PLUGIN_LIST, $CD_PLUGIN_LIST, [list of cd plugins])
AC_DEFINE_UNQUOTED(CD_PLUGIN_PROTOS, $CD_PLUGIN_PROTOS, [list of cd prototypes])
AC_DEFINE_UNQUOTED(SND_OUTPUT_LIST, $SND_OUTPUT_LIST, [list of sound output plugins])
AC_DEFINE_UNQUOTED(SND_OUTPUT_PROTOS, $SND_OUTPUT_PROTOS, [list of sound output prototypes])
AC_DEFINE_UNQUOTED(SND_RENDER_LIST, $SND_RENDER_LIST, [list of sound render plugins])
AC_DEFINE_UNQUOTED(SND_RENDER_PROTOS, $SND_RENDER_PROTOS, [list of sound render prototypes])

AC_SUBST(HW_TARGETS)
AC_SUBST(NQ_TARGETS)
AC_SUBST(QTV_TARGETS)
AC_SUBST(QW_TARGETS)
AC_SUBST(SERVER_PLUGIN_STATIC)
AC_SUBST(SERVER_PLUGIN_STATIC_LIBS)
AC_SUBST(SERVER_PLUGIN_TARGETS)
AC_SUBST(CLIENT_PLUGIN_STATIC)
AC_SUBST(CLIENT_PLUGIN_STATIC_LIBS)
AC_SUBST(CLIENT_PLUGIN_TARGETS)
AC_SUBST(CD_PLUGIN_STATIC)
AC_SUBST(CD_PLUGIN_STATIC_LIBS)
AC_SUBST(CD_PLUGIN_TARGETS)
AC_SUBST(CD_TARGETS)
AC_SUBST(JOY_TARGETS)
AC_SUBST(SND_PLUGIN_STATIC)
AC_SUBST(SND_PLUGIN_STATIC_LIBS)
AC_SUBST(SND_PLUGIN_TARGETS)
AC_SUBST(SND_REND_STATIC)
AC_SUBST(SND_REND_STATIC_LIBS)
AC_SUBST(SND_REND_TARGETS)
AC_SUBST(SND_TARGETS)
AC_SUBST(VID_MODEL_TARGETS)
AC_SUBST(VID_REND_TARGETS)
AC_SUBST(VID_REND_NOINST_TARGETS)
AC_SUBST(VID_TARGETS)
AC_SUBST(plugin_ldflags)
AC_SUBST(plugin_libadd)

QF_DEPS(BSP2IMG,
	[],
	[$(top_builddir)/libs/image/libQFimage.la $(top_builddir)/libs/util/libQFutil.la],
	[$(WIN32_LIBS)],
)
QF_DEPS(QFBSP,
	[-I$(top_srcdir)/tools/qfbsp/include],
	[$(top_builddir)/libs/util/libQFutil.la],
	[$(WIN32_LIBS)],
)
QF_DEPS(QFCC,
	[-I$(top_srcdir)/tools/qfcc/include],
	[$(top_builddir)/libs/gamecode/engine/libQFgamecode.la $(top_builddir)/libs/util/libQFutil.la],
	[$(WIN32_LIBS)],
)
QF_DEPS(QFLIGHT,
	[-I$(top_srcdir)/tools/qflight/include],
	[$(top_builddir)/libs/util/libQFutil.la],
	[$(WIN32_LIBS)],
)
QF_DEPS(QFMODELGEN,
	[-I$(top_srcdir)/tools/qfmodelgen/include],
	[$(top_builddir)/libs/util/libQFutil.la],
	[$(WIN32_LIBS)],
)
QF_DEPS(QFVIS,
	[-I$(top_srcdir)/tools/qfvis/include],
	[$(top_builddir)/libs/util/libQFutil.la],
	[$(WIN32_LIBS)],
)
QF_DEPS(QWAQ,
	[],
	[$(top_builddir)/libs/ruamoko/libQFruamoko.la $(top_builddir)/libs/gamecode/engine/libQFgamecode.la $(top_builddir)/libs/util/libQFutil.la],
	[$(WIN32_LIBS)],
)
QF_DEPS(CARNE,
	[],
	[$(top_builddir)/libs/gib/libQFgib.la $(top_builddir)/libs/util/libQFutil.la],
	[$(WIN32_LIBS)],
)
QF_DEPS(PAK,
	[],
	[$(top_builddir)/libs/util/libQFutil.la],
	[$(WIN32_LIBS)],
)
QF_DEPS(WAD,
	[],
	[$(top_builddir)/libs/util/libQFutil.la],
	[$(WIN32_LIBS)],
)
QF_DEPS(WAV,
	[],
	[$(top_builddir)/libs/util/libQFutil.la],
	[$(WIN32_LIBS)],
)

AC_DEFINE_UNQUOTED(PATH_SEPARATOR, '/', [Define this to your operating system's path separator character])
AC_DEFINE_UNQUOTED(CPP_NAME, "$CPP_NAME", [Define this to the command line for the C preprocessor])
AC_DEFINE_UNQUOTED(QFCC_INCLUDE_PATH, "$prefix/include/QF/ruamoko", [Define this to where qfcc should look for header files])
AC_DEFINE_UNQUOTED(QFCC_LIB_PATH, "$prefix/lib/ruamoko", [Define this to where qfcc should look for lib files])

AM_CONDITIONAL(BUILD_GL, test "$BUILD_GL" = "yes")
AM_CONDITIONAL(BUILD_SW, test "$BUILD_SW" = "yes")
AM_CONDITIONAL(BUILD_SW_ASM, test "$BUILD_SW" = "yes" -a "$ASM_ARCH" = "yes")
AM_CONDITIONAL(BUILD_SW_MOD, test "$BUILD_SW" = "yes" -o "$BUILD_SW32" = "yes")
AM_CONDITIONAL(BUILD_SW32, test "$BUILD_SW32" = "yes")

dnl Finalization of CFLAGS, LDFLAGS, and LIBS
AC_SUBST(CFLAGS)
AC_SUBST(LDFLAGS)
AC_SUBST(LIBS)

AC_SUBST(AS)
AC_SUBST(AR)
AC_SUBST(RANLIB)

dnl Output files
AC_OUTPUT(
	Makefile
	quakeforge.lsm

	include/Makefile
	include/QF/Makefile

	libs/Makefile
	libs/audio/Makefile
	libs/audio/cd/Makefile
	libs/audio/targets/Makefile
	libs/audio/renderer/Makefile
	libs/console/Makefile
	libs/gamecode/Makefile
	libs/gamecode/engine/Makefile
	libs/gamecode/builtins/Makefile
	libs/gib/Makefile
	libs/image/Makefile
	libs/models/Makefile
	libs/models/alias/Makefile
	libs/models/brush/Makefile
	libs/models/sprite/Makefile
	libs/net/Makefile
	libs/net/nc/Makefile
	libs/net/nm/Makefile
	libs/qw/Makefile
	libs/ruamoko/Makefile
	libs/util/Makefile
	libs/video/Makefile
	libs/video/renderer/Makefile
	libs/video/renderer/gl/Makefile
	libs/video/renderer/sw/Makefile
	libs/video/renderer/sw32/Makefile
	libs/video/targets/Makefile

	hw/Makefile
	hw/include/Makefile
	hw/source/Makefile

	nq/Makefile
	nq/include/Makefile
	nq/source/Makefile

	qtv/Makefile
	qtv/include/Makefile
	qtv/source/Makefile

	qw/Makefile
	qw/include/Makefile
	qw/source/Makefile

	tools/Makefile
	tools/bsp2img/Makefile
	tools/carne/Makefile
	tools/pak/Makefile
	tools/qfbsp/Makefile
	tools/qfbsp/include/Makefile
	tools/qfbsp/source/Makefile
	tools/qfcc/Makefile
	tools/qfcc/doc/Makefile
	tools/qfcc/doc/man/Makefile
	tools/qfcc/include/Makefile
	tools/qfcc/source/Makefile
	tools/qflight/Makefile
	tools/qflight/include/Makefile
	tools/qflight/source/Makefile
	tools/qfmodelgen/Makefile
	tools/qfmodelgen/include/Makefile
	tools/qfmodelgen/source/Makefile
	tools/qfvis/Makefile
	tools/qfvis/include/Makefile
	tools/qfvis/source/Makefile
	tools/qwaq/Makefile
	tools/qwaq/progs.src
	tools/wad/Makefile
	tools/wav/Makefile

	ruamoko/Makefile
	ruamoko/include/Makefile
	ruamoko/lib/Makefile
	ruamoko/game/Makefile
	ruamoko/gui/Makefile
	ruamoko/cl_menu/Makefile

	doc/Makefile
	doc/quakeforge.dox
	doc/man/Makefile

	debian/Makefile

	RPM/Makefile
	RPM/build_rpm
	RPM/quakeforge.conf
	RPM/quakeforge.spec,

	chmod +x RPM/build_rpm
	if test -n "$AR" -a "$AR" != "ar"; then
		sed -e 's/^AR="ar"$/AR="'$AR'"/' libtool > libtool.qf
		if cmp -s libtool.qf libtool; then
			echo libtool is unchanged
			rm -f libtool.qf
		else
			rm -f libtool
			mv libtool.qf libtool
		fi
	fi
)

if test "x$CCVER" = "x"; then
	CCVER=$CC
else
	if test "x$CCVER" = "xgcc 2.96"; then
		CCVER="$CCVER
(Note: You might need CC=kgcc ./configure on some systems)"
	fi
fi

AC_MSG_RESULT([
	QuakeForge has been configured successfully.

	Build type         :$BUILD_TYPE
	Server support     :${SV_TARGETS- no}
	Client support     :${CL_TARGETS- no}
	Tools support      :${TOOLS_TARGETS- no}
	Sound support      :${SOUND_TYPES- no}
	CD Audio system    :${CDTYPE- no}
	IPv6 networking    : $NETTYPE_IPV6
	Compression support: gz=$HAVE_ZLIB ogg=$HAVE_VORBIS png=$HAVE_PNG
	Compiler version   : $CCVER
	Compiler flags     : $CFLAGS
	qfcc cpp invocation: $CPP_NAME

	Shared game data directory  : $sharepath
	Per-user game data directory: $userpath
	Plugin load directory       : $expanded_plugindir
	Global configuration file   : $globalconf
	User configuration file     : $userconf
	OpenGL dynamic lib          : $gl_driver
	libWildMidi Support         : $HAVE_WILDMIDI
])

if test -d $srcdir/CVS; then
	echo "WARNING: Hackers at work, watch for falling bits of code."
	echo "(This is from a development CVS tree. Expect problems)"
	echo
fi
