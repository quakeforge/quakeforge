dnl Process this file with autoconf to produce a configure script.
AC_PREREQ(2.13)
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
AM_INIT_AUTOMAKE(quakeforge, 0.5.1)

dnl Define the proper name and extra version numbers for package
PROGRAM=QuakeForge
NQ_VERSION=1.09
NQ_QSG_VERSION=1.0
QW_VERSION=2.40
QW_QSG_VERSION=2.0

AC_DEFINE_UNQUOTED(PROGRAM, 		"$PROGRAM")
AC_DEFINE_UNQUOTED(NQ_VERSION,		"$NQ_VERSION")
AC_DEFINE_UNQUOTED(NQ_QSG_VERSION,	"$NQ_QSG_VERSION")
AC_DEFINE_UNQUOTED(QW_VERSION,		"$QW_VERSION")
AC_DEFINE_UNQUOTED(QW_QSG_VERSION,	"$QW_QSG_VERSION")

AC_SUBST(PROGRAM)
AC_SUBST(NQ_VERSION)
AC_SUBST(NQ_QSG_VERSION)
AC_SUBST(QW_VERSION)
AC_SUBST(QW_QSG_VERSION)

ISODATE=`date +%Y-%m-%d`
AC_SUBST(ISODATE)

AC_LANG_C

if test ${AR-unset} = unset; then
	AR="ar"
fi
export AR

endian=""
case "$host_os" in
	mingw32*)
		mingw=yes
		CFLAGS="$CFLAGS -I\$(top_srcdir)/include/win32"
		CFLAGS="$CFLAGS -I\$(top_srcdir)/include/win32/resources"
		if test "$host" != "$build"; then
			CC="$host_cpu-$host_os-gcc"
			AS="$CC"
			AR="$host_cpu-$host_os-ar"
			RANLIB="$host_cpu-$host_os-ranlib"
			endian="little"
		fi
		;;
	cygwin*)
		cygwin=yes
		if test "$host" != "$build"; then
			CC="$host_cpu-$host_os-gcc"
			AS="$CC"
			AR="$host_cpu_$host_os-ar"
			RANLIB="$host_cpu-$host_os-ranlib"
                        endian="little"
		fi
		if test "$host" != "$target"; then
			case "$target_os" in
				mingw32*)
					mingw=yes
					CPPFLAGS="$CPPFLAGS -mno-cygwin -mwindows"
					CPPFLAGS="$CPPFLAGS -I\$(top_srcdir)/include/win32"
					CPPFLAGS="$CPPFLAGS -I\$(top_srcdir)/include/win32/resources"
					LDFLAGS="$LDFLAGS -mno-cygwin -mwindows"
					endian="little"
				;;
			esac
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
AM_PROG_LIBTOOL
AC_PROG_YACC
AM_PROG_LEX

AC_CHECK_LIB(l, main, LEXLIB="-ll", AC_CHECK_LIB(fl, main, LEXLIB="-lfl"))
AC_SUBST(LEXLIB)

set $CC
if test "$1" = gcc; then
	shift
	args="$*"
	AC_MSG_CHECKING(for gcc version)
	CCVER="gcc `gcc --version`"
	AC_MSG_RESULT($CCVER)
fi

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
	alsa/asoundlib.h \
	arpa/inet.h asm/io.h assert.h conio.h ctype.h ddraw.h dinput.h \
	direct.h dirent.h dlfcn.h dmedia/audio.h dmedia/cdaudio.h dpmi.h \
	dsound.h errno.h execinfo.h fcntl.h fnmatch.h io.h \
	libc.h limits.h linux/cdrom.h linux/joystick.h linux/soundcard.h \
	machine/soundcard.h malloc.h math.h mgraph.h _mingw.h netdb.h \
	netinet/in.h pwd.h rpc/types.h setjmp.h signal.h stdarg.h stdio.h \
	stdlib.h string.h strings.h sys/asoundlib.h sys/audioio.h sys/filio.h \
	sys/ioctl.h sys/io.h sys/ipc.h sys/mman.h sys/param.h sys/poll.h \
	sys/shm.h sys/signal.h sys/socket.h sys/soundcard.h sys/stat.h \
	sys/time.h sys/types.h sys/wait.h termios.h time.h unistd.h vga.h \
	vgakeyboard.h vgamouse.h windows.h winsock.h zlib.h
)
if test "x$mingw" = xyes; then
	AC_MSG_CHECKING(for fnmatch.h)
	AC_MSG_RESULT(yes)
	AC_DEFINE(HAVE_FNMATCH_H)
else
	AC_CHECK_HEADERS(fnmatch.h)
fi

dnl ==================================================================
dnl Checks for typedefs, structures, and compiler characteristics
dnl ==================================================================

AC_C_CONST
AC_C_INLINE
AC_TYPE_SIZE_T
AC_STRUCT_ST_BLKSIZE
AC_HEADER_TIME
AC_STRUCT_TM

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
AM_CONDITIONAL(BUILD_FNMATCH, test "$BUILD_FNMATCH" = "yes")

AC_MSG_CHECKING(for fnmatch in fnmatch.h)
AC_TRY_COMPILE(
	[#include "fnmatch.h"],
	[int (*foo)() = fnmatch;],
	AC_DEFINE(HAVE_FNMATCH_PROTO)
	AC_MSG_RESULT(yes),
	AC_MSG_RESULT(no)
)

AC_MSG_CHECKING(for type of fpos_t)
AC_TRY_COMPILE(
	[#include <stdio.h>],
	[fpos_t x = 0],
	AC_MSG_RESULT(off_t),
	AC_DEFINE(HAVE_FPOS_T_STRUCT)
	AC_MSG_RESULT(struct)
)

AC_MSG_CHECKING(for socklen_t in sys/types.h)
AC_TRY_COMPILE(
	[#include <sys/types.h>],
	[ socklen_t x = 0;],
	AC_DEFINE(HAVE_SOCKLEN_T)
	AC_MSG_RESULT(yes),
	AC_MSG_RESULT(no)
	dnl FreeBSD 4.0 has it in sys/socket.h
	AC_MSG_CHECKING(for socklen_t in sys/socket.h)
	AC_TRY_COMPILE(
		[#include <sys/types.h>
		#include <sys/socket.h>],
		[ socklen_t x = 0;],
		AC_DEFINE(HAVE_SOCKLEN_T) AC_MSG_RESULT(yes),
		AC_MSG_RESULT(no)
	)
)

AC_MSG_CHECKING(for underscore prefix in names)
AC_TRY_LINK(
	[asm(".long _bar");
	int bar;],
	[],
	AC_DEFINE(HAVE_SYM_PREFIX_UNDERSCORE) AC_MSG_RESULT(yes),
	AC_MSG_RESULT(no)
)

AC_MSG_CHECKING(for size_t in sys/types.h)
AC_TRY_COMPILE(
	[#include <sys/types.h>],
	[ size_t x = 0;],
	AC_DEFINE(HAVE_SIZE_T) AC_MSG_RESULT(yes),
	AC_MSG_RESULT(no)
)

dnl maybe these two (at least the 2nd) should only be checked if ipv6 is enabled?
AC_MSG_CHECKING(for ss_len in struct sockaddr_storage)
AC_TRY_COMPILE(
	[#include <sys/types.h>
	#include <sys/socket.h>],
	[ void f(void) { struct sockaddr_storage ss; ss.ss_len=0; }],
	AC_DEFINE(HAVE_SS_LEN) AC_MSG_RESULT(yes),
	AC_MSG_RESULT(no)
)

AC_MSG_CHECKING(for sin6_len in struct sockaddr_in6)
AC_TRY_COMPILE(
	[#include <sys/types.h>
	#include <netinet/in.h>],
	[ void f(void) { struct sockaddr_in6 s6; s6.sin6_len=0; }],
	AC_DEFINE(HAVE_SIN6_LEN) AC_MSG_RESULT(yes),
	AC_MSG_RESULT(no)
)

AC_MSG_CHECKING(for sa_len in struct sockaddr)
AC_TRY_COMPILE(
	[#include <sys/types.h>
	#include <netinet/in.h>],
	[ void f(void) { struct sockaddr sa; sa.sa_len=0; }],
	AC_DEFINE(HAVE_SA_LEN) AC_MSG_RESULT(yes),
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
AC_CHECK_FUNCS(
	access _access \
	gethostname gethostbyname connect gettimeofday getwd mkdir _mkdir \
	ftime _ftime fcntl stat putenv select socket strerror strstr \
	snprintf _snprintf vsnprintf _vsnprintf strsep dlopen getaddrinfo \
	getnameinfo mprotect
)

DL_LIBS=""
if test "x$ac_cv_func_dlopen" != "xyes"; then
	AC_CHECK_LIB(dl, dlopen,
		AC_DEFINE(HAVE_DLOPEN) DL_LIBS="-ldl"
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
if test $strcasecmp = no; then
	AC_CHECK_FUNC(stricmp,
		AC_DEFINE(strcasecmp, stricmp),
		AC_MSG_ERROR([Neither stricmp nor strcasecmp found])
	)
fi

dnl Check for vsnprintf
if test "x$ac_cv_func_vsnprintf" = "xno" -a \
	"x$ac_cv_func__vsnprintf" = "xno"; then
	dnl libdb may have this
	AC_CHECK_LIB(db,vsnprintf)
fi

dnl Checks for working -lm
AC_CHECK_LIB(m, pow,, AC_MSG_ERROR([math library (-lm) appears broken]))

AC_ARG_ENABLE(zlib,
[  --disable-zlib          disable zlib support],
)

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


AC_ARG_WITH(ipv6,
[  --with-ipv6=DIR         enable IPv6 support. Optional argument specifies
                          location of inet6 libraries.],
	if test "x$withval" = xno ; then
		NETTYPE_IPV6=no
	else
		AC_DEFINE(HAVE_IPV6)
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
		AC_DEFINE(HAVE_FB_AUX_VGA_PLANES_VGA4)
		AC_MSG_RESULT(yes),
		AC_MSG_RESULT(no)
	)
	AC_MSG_CHECKING(for FB_AUX_VGA_PLANES_CFB4)
	AC_TRY_COMPILE(
		[#include "linux/fb.h"],
		[int foo = FB_AUX_VGA_PLANES_CFB4;],
		AC_DEFINE(HAVE_FB_AUX_VGA_PLANES_CFB4)
		AC_MSG_RESULT(yes),
		AC_MSG_RESULT(no)
	)
	AC_MSG_CHECKING(for FB_AUX_VGA_PLANES_CFB8)
	AC_TRY_COMPILE(
		[#include "linux/fb.h"],
		[int foo = FB_AUX_VGA_PLANES_CFB8;],
		AC_DEFINE(HAVE_FB_AUX_VGA_PLANES_CFB8)
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
[  --enable-vidmode        use XFree86 VidMode extension, if available],
	HAVE_VIDMODE=$enable_vidmode, HAVE_VIDMODE=auto)
if test "x$HAVE_VIDMODE" != xno; then
	save_CPPFLAGS="$CPPFLAGS"
	CPPFLAGS="$X_CFLAGS $CPPFLAGS"
	AC_CHECK_HEADER(X11/extensions/xf86vmode.h,
		dnl Make sure the library works
		AC_CHECK_LIB(Xxf86vm, XF86VidModeSwitchToMode,
			AC_DEFINE(HAVE_VIDMODE)
			VIDMODE_LIBS="-lXxf86vm",,
			[$X_LIBS -lXext -lX11 $X_EXTRA_LIBS]
		)
	)
	CPPFLAGS="$save_CPPFLAGS"
fi
AC_SUBST(VIDMODE_LIBS)

dnl Check for DGA support
AC_ARG_ENABLE(dga,
[  --enable-dga            use XFree86 DGA extension, if available],
HAVE_DGA=$enable_dga, HAVE_DGA=auto)
if test "x$HAVE_DGA" != xno; then
	save_CPPFLAGS="$CPPFLAGS"
	CPPFLAGS="$X_CFLAGS $CPPFLAGS"
	AC_CHECK_HEADER(X11/extensions/xf86dga.h,
		dnl Make sure the library works
		AC_CHECK_LIB(Xxf86dga, XF86DGAQueryVersion,
			AC_DEFINE(HAVE_DGA)
			DGA_LIBS="-lXxf86dga",,
			[$X_LIBS -lXext -lX11 $X_EXTRA_LIBS]
		)
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
		;;
	i?86-*-cygwin*)
		SYSTYPE=WIN32
		AC_MSG_RESULT([Win32 driver])
		;;
	*)
		SYSTYPE=POSIX
		AC_MSG_RESULT([default POSIX driver])
		;;
esac
AM_CONDITIONAL(SYSTYPE_WIN32, test "$SYSTYPE" = "WIN32")

dnl Check for ia32
AC_MSG_CHECKING(for an ia32 machine)
case "${host}" in
	i?86-*-*)
		AC_MSG_RESULT(yes)
		AC_MSG_CHECKING(to see if we should disable asm optimizations)
		AC_ARG_ENABLE(asmopt,
			[  --disable-asmopt        disable assembler optimization],
			AC_MSG_RESULT(yes),
 			AC_DEFINE(USE_INTEL_ASM)
			ASM_ARCH=yes
			AC_MSG_RESULT(no)
		)
		;;
	*) AC_MSG_RESULT(no)
esac
AM_CONDITIONAL(ASM_ARCH, test "$ASM_ARCH" = "yes")

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

SOUND_TYPES=""
AC_MSG_CHECKING(for sound support)

HAVE_ALSA=no
if test "x$enable_alsa" != "xno"; then
	if test "x$ac_cv_header_sys_asoundlib_h" = "xyes" -o \
		"x$ac_cv_header_alsa_asoundlib_h" = "xyes"; then
			AC_EGREP_CPP([QF_maGiC_VALUE],
				[
#include <sys/asoundlib.h>
#if defined(SND_LIB_MAJOR) && defined(SND_LIB_MINOR)
#if SND_LIB_MAJOR > 0 || (SND_LIB_MAJOR == 0 && SND_LIB_MINOR == 5)
QF_maGiC_VALUE
#endif
#endif
				],
				SOUND_TYPES="$SOUND_TYPES ALSA0.5"
				ALSA_LIBS="-lasound"
				HAVE_ALSA=yes,
				AC_EGREP_CPP([QF_maGiC_VALUE],
				[
#include <alsa/asoundlib.h>
#if defined(SND_LIB_MAJOR) && defined(SND_LIB_MINOR)
#if SND_LIB_MAJOR > 0 || (SND_LIB_MAJOR == 0 && SND_LIB_MINOR >= 9)
QF_maGiC_VALUE
#endif
#endif
				],
				SOUND_TYPES="$SOUND_TYPES ALSA0.9"
				ALSA_LIBS="-lasound"
				HAVE_ALSA=yes,
				AC_EGREP_CPP([QF_maGiC_VALUE],
					[
#include <sys/asoundlib.h>
#if defined(SND_LIB_MAJOR) && defined(SND_LIB_MINOR)
#if SND_LIB_MAJOR > 0 || (SND_LIB_MAJOR == 0 && SND_LIB_MINOR >= 9)
QF_maGiC_VALUE
#endif
#endif
					],
					SOUND_TYPES="$SOUND_TYPES ALSA0.9"
					ALSA_LIBS="-lasound"
					HAVE_ALSA=yes
				)
		    )
	    )
	fi
fi
AC_SUBST(ALSA_LIBS)
AC_SUBST(HAVE_ALSA)

SOUND_TYPES="$SOUND_TYPES DISK"

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
if test "x$ac_cv_header_windows_h" = "xyes" -a \
		"x$ac_cv_header_dsound_h" = "xyes"; then
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
		SOUND_TYPES="$SOUND_TYPES Win32"
		WINSND_LIBS="-lwinmm"
	)
fi
AC_SUBST(WINSND_LIBS)

if test "x$enable_sound" = "xno"; then
	SOUND_TYPES=""
fi

if test "$SOUND_TYPES"; then
	AC_MSG_RESULT([$SOUND_TYPES])
else
	AC_MSG_RESULT([no, using null sound driver])
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
CD_PLUGIN_TARGETS="libcd_null.la"
CDTYPE=""

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
	CD_PLUGIN_TARGETS="$CD_PLUGIN_TARGETS libcd_linux.la"
)

dnl SDL CD audio
if test "x$HAVE_SDL" = "xyes"; then
	CDTYPE="$CDTYPE SDL"
	CD_PLUGIN_TARGETS="$CD_PLUGIN_TARGETS libcd_sdl.la"
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
	CD_PLUGIN_TARGETS="$CD_PLUGIN_TARGETS libcd_sgi.la"
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
	CD_PLUGIN_TARGETS="$CD_PLUGIN_TARGETS libcd_win.la"
)
if test "$CDTYPE"; then
	AC_MSG_RESULT([$CDTYPE])
else
	AC_MSG_RESULT([no, using null CD driver])
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
AC_DEFINE_UNQUOTED(FS_GLOBALCFG, "$globalconf")

AC_ARG_WITH(user-cfg,
[  --with-user-cfg=FILE    If set will change the name and location of the
                          user-specific config file used by QuakeForge.
                          Defaults to ~/.quakeforgerc.],
globalconf="$withval", userconf="auto")
if test "x$userconf" = "xauto" || test "x$userconf" = "xyes" || \
	test "x$userconf" = "xno"; then  dnl yes/no sanity checks
	userconf="$default_userconf"
fi
AC_DEFINE_UNQUOTED(FS_USERCFG, "$userconf")

AC_ARG_WITH(sharepath,
[  --with-sharepath=DIR    Use DIR for shared game data, defaults to
                          '.' or \${datadir}/games/quakeforge (if new style)],
sharepath=$withval, sharepath="auto")
if test "x$sharepath" = "xauto" -o "x$sharepath" = "xyes" -o "x$sharepath" = "x"; then
	sharepath="$default_sharepath"
elif test "x$sharepath" = xno; then
	sharepath="."
fi
AC_DEFINE_UNQUOTED(FS_SHAREPATH, "$sharepath")

AC_ARG_WITH(userpath,
[  --with-userpath=DIR     Use DIR for unshared game data, defaults to
                          '.' or ~/.quakeforge (if new style)],
userpath=$withval, userpath="auto")
if test "x$userpath" = "xauto" -o "x$userpath" = "xyes" -o "x$userpath" = "x"; then
	userpath="$default_userpath"
elif test "x$userpath" = xno; then
	userpath="."
fi
AC_DEFINE_UNQUOTED(FS_USERPATH, "$userpath")

AC_ARG_WITH(plugin-path,
[  --with-plugin-path=DIR  Use DIR for loading plugins, defaults to
                          \${libdir}/quakeforge],
plugindir=$withval, plugindir="auto")

PLUGINDIR="$libdir/$PACKAGE"
if test "x$plugindir" = "xauto" -o "x$plugindir" = "xyes" -o "x$plugindir" = "x"; then
	eval plugindir="$PLUGINDIR"
elif test "x$plugindir" = xno; then
	plugindir="."
else
	PLUGINDIR=$plugindir
	eval plugindir="$PLUGINDIR"
fi
AC_DEFINE_UNQUOTED(FS_PLUGINPATH, "$plugindir")
AC_SUBST(plugindir)
AC_SUBST(PLUGINDIR)

dnl CFLAGS for release and devel versions
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
	optimize=$disable_optimize,
	optimize=yes
)

AC_ARG_ENABLE(typecheck-progs,
	[  --enable-typecheck-progs Enable type checking on progs field access])
if test "x$enable_typecheck_progs" = xyes; then
	AC_DEFINE(TYPECHECK_PROGS)
fi

AC_ARG_WITH(amd,
	[  --with-amd              Optimize for AMD processors instead of Intel],
	HAVE_AMD="yes",
	HAVE_AMD="no"
)

AC_MSG_CHECKING(for optimization)
if test "x$optimize" = xyes; then
	AC_MSG_RESULT(yes)
	BUILD_TYPE="$BUILD_TYPE Optimize"
	if test "x$GCC" = xyes; then
		CFLAGS="$CFLAGS -O3 -ffast-math -funroll-loops -fomit-frame-pointer -fexpensive-optimizations"
		AC_MSG_CHECKING(for special compiler settings)
		case "${host_cpu}" in
			i?86)
				if test "x$HAVE_AMD" = "xyes"; then
					MORE_CFLAGS="-march=k6"
				else
					MORE_CFLAGS="-march=${host_cpu}"
				fi
				;;
			*)
				MORE_CFLAGS=""
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
			)
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
	AC_MSG_CHECKING(whether -pipe works)
	save_CFLAGS="$CFLAGS"
	CFLAGS="$CFLAGS -pipe"
	pipe_ok=no
	AC_TRY_COMPILE(
		[],
		[],
		pipe_ok=yes
		AC_MSG_RESULT(yes),
		AC_MSG_RESULT(no)
	)
	CFLAGS="$save_CFLAGS"
	if test "x$pipe_ok" = xyes; then
		CFLAGS="$CFLAGS -pipe"
	fi
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
		CFLAGS="$CFLAGS -Wall -Werror"
	else
		CFLAGS="$CFLAGS -Wall"
	fi
#	CFLAGS="$CFLAGS -Wall -pedantic"
fi

AS="$CC"
ASFLAGS="\$(DEFS) \$(CFLAGS) \$(CPPFLAGS) \$(DEFAULT_INCLUDES) \$(INCLUDES)"
AC_SUBST(AS)
AC_SUBST(ASFLAGS)

dnl ==================================================================
dnl Make sure we link against a working zlib
dnl ==================================================================

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
	AC_MSG_RESULT(no)
	AC_DEFINE(HAVE_ZLIB)
fi
AC_SUBST(Z_LIBS)

dnl ==================================================================
dnl Find out what to build
dnl ==================================================================

AC_ARG_WITH(clients,
	[  --with-clients=<list>   compile clients in <list>;
                          fbdev,glx,mgl,sdl,sdl32,sgl,svga,wgl,x11],
	clients="$withval",
	clients="all"
)
if test "$clients" = "all"; then
	ENABLE_3DFX=yes
	ENABLE_FBDEV=yes
	ENABLE_GLX=yes
	ENABLE_MGL=yes
	ENABLE_SDL=yes
	ENABLE_SDL32=yes
	ENABLE_SGL=yes
	ENABLE_SVGA=yes
	ENABLE_WGL=yes
	ENABLE_X11=yes
else
	ENABLE_3DFX=no
	ENABLE_FBDEV=no
	ENABLE_GLX=no
	ENABLE_MGL=no
	ENABLE_SDL=no
	ENABLE_SDL32=no
	ENABLE_SGL=no
	ENABLE_SVGA=no
	ENABLE_WGL=no
	ENABLE_X11=no
	IFS=","
	for client in $clients; do
		case "$client" in
			fbdev)
				ENABLE_FBDEV=yes
				;;
			glx)
				ENABLE_GLX=yes
				;;
			mgl)
				ENABLE_MGL=yes
				;;
			sdl)
				ENABLE_SDL=yes
				;;
			sdl32)
				ENABLE_SDL32=yes
				;;
			sgl)
				ENABLE_SGL=yes
				;;
			svga)
				ENABLE_SVGA=yes
				;;
			wgl)
				ENABLE_WGL=yes
				;;
			x11)
				ENABLE_X11=yes
				;;
		esac
	done
	IFS=" "
fi

AC_ARG_WITH(servers,
	[  --with-servers=<list>   compile dedicated server
                          master,nq,qw],
	servers="$withval",
	servers=all
)
if test "$servers" = "all"; then
	ENABLE_NQ_SERVER=yes
	ENABLE_QW_MASTER=yes
	ENABLE_QW_SERVER=yes
else
	ENABLE_NQ_SERVER=no
	ENABLE_QW_MASTER=no
	ENABLE_QW_SERVER=no
	IFS=","
	for server in $servers; do
		case "$server" in
			nq)
				ENABLE_NQ_SERVER=yes
				;;
			master)
				ENABLE_QW_MASTER=yes
				;;
			qw)
				ENABLE_QW_SERVER=yes
				;;
		esac
	done
	IFS=" "
fi

CL_TARGETS=""
QW_TARGETS=""
NQ_TARGETS=""

BUILD_GL=no
BUILD_SW32=no
BUILD_SW=no
CAN_BUILD_GL=no
CAN_BUILD_SW32=no
CAN_BUILD_SW=no

CD_TARGETS=""
SND_PLUGIN_TARGETS="libsnd_output_disk.la"
SND_REND_TARGETS=""
SND_TARGETS=""
VID_MODEL_TARGETS=""
VID_REND_TARGETS=""
VID_TARGETS=""
VID_TARGETS_NOINST=""

if test "x$HAVE_FBDEV" = xyes; then
	if test "x$ENABLE_FBDEV" = xyes; then
		QW_TARGETS="$QW_TARGETS qw-client-fbdev\$(EXEEXT)"
		NQ_TARGETS="$NQ_TARGETS nq-fbdev\$(EXEEXT)"
		CL_TARGETS="$CL_TARGETS FBDEV"
		VID_TARGETS="$VID_TARGETS libQFfbdev.la"
		BUILD_SW=yes
	fi
	CAN_BUILD_SW=yes
fi
if test "x$HAVE_X" = xyes; then
	CAN_BUILD_GL=yes
	CAN_BUILD_SW=yes
	if test "x$ENABLE_GLX" = xyes; then
		QW_TARGETS="$QW_TARGETS qw-client-glx\$(EXEEXT)"
		NQ_TARGETS="$NQ_TARGETS nq-glx\$(EXEEXT)"
		CL_TARGETS="$CL_TARGETS GLX"
		VID_TARGETS="$VID_TARGETS libQFglx.la"
		BUILD_GL=yes
	fi
	if test "x$ENABLE_X11" = xyes; then
		QW_TARGETS="$QW_TARGETS qw-client-x11\$(EXEEXT)"
		NQ_TARGETS="$NQ_TARGETS nq-x11\$(EXEEXT)"
		CL_TARGETS="$CL_TARGETS X11"
		VID_TARGETS="$VID_TARGETS libQFx11.la"
		BUILD_SW=yes
	fi
fi
if test "x$HAVE_MGL" = xyes; then
	if test "x$ENABLE_MGL" = xyes; then
		QW_TARGETS="$QW_TARGETS qw-client-mgl\$(EXEEXT)"
		NQ_TARGETS="$NQ_TARGETS nq-mgl\$(EXEEXT)"
		CL_TARGETS="$CL_TARGETS MGL"
		VID_TARGETS="$VID_TARGETS libQFwgl.la"
		BUILD_SW=yes
	fi
	CAN_BUILD_SW=yes
fi
if test "x$HAVE_SDL" = xyes; then
	if test "x$ENABLE_SDL" = xyes; then
		QW_TARGETS="$QW_TARGETS qw-client-sdl\$(EXEEXT)"
		NQ_TARGETS="$NQ_TARGETS nq-sdl\$(EXEEXT)"
		CL_TARGETS="$CL_TARGETS SDL"
		VID_TARGETS="$VID_TARGETS libQFsdl.la"
		BUILD_SW=yes
	fi
	CAN_BUILD_SW=yes
	if test "x$ENABLE_SDL32" = xyes; then
		QW_TARGETS="$QW_TARGETS qw-client-sdl32\$(EXEEXT)"
		NQ_TARGETS="$NQ_TARGETS nq-sdl32\$(EXEEXT)"
		CL_TARGETS="$CL_TARGETS SDL32"
		VID_TARGETS="$VID_TARGETS libQFsdl32.la"
		BUILD_SW32=yes
	fi
	CAN_BUILD_SW32=yes
	if test "x$ENABLE_SGL" = xyes; then
		QW_TARGETS="$QW_TARGETS qw-client-sgl\$(EXEEXT)"
		NQ_TARGETS="$NQ_TARGETS nq-sgl\$(EXEEXT)"
		CL_TARGETS="$CL_TARGETS SDL-GL"
		VID_TARGETS="$VID_TARGETS libQFsgl.la"
		BUILD_GL=yes
		CAN_BUILD_GL=yes
	fi
fi
if test "x$HAVE_SVGA" = xyes; then
	if test "x$ENABLE_SVGA" = xyes; then
		QW_TARGETS="$QW_TARGETS qw-client-svga\$(EXEEXT)"
		NQ_TARGETS="$NQ_TARGETS nq-svga\$(EXEEXT)"
		CL_TARGETS="$CL_TARGETS SVGAlib"
		VID_TARGETS_NOINST="$VID_TARGETS_NOINST libQFsvga.la"
		BUILD_SW=yes
	fi
	CAN_BUILD_SW=yes
fi
if test "x$mingw" = xyes; then
	if test "x$ENABLE_WGL" = xyes; then
		QW_TARGETS="$QW_TARGETS qw-client-wgl\$(EXEEXT)"
		NQ_TARGETS="$NQ_TARGETS nq-wgl\$(EXEEXT)"
		CL_TARGETS="$CL_TARGETS WGL"
		VID_TARGETS="$VID_TARGETS libQFwgl.la"
		BUILD_GL=yes
	fi
	CAN_BUILD_GL=yes
fi

SV_TARGETS=""
if test "x$ENABLE_NQ_SERVER" = xyes; then
	NQ_TARGETS="nq-server\$(EXEEXT) $NQ_TARGETS"
	SV_TARGETS="$SV_TARGETS nq"
fi
if test "x$ENABLE_QW_MASTER" = xyes; then
	QW_TARGETS="qw-master\$(EXEEXT) $QW_TARGETS"
	SV_TARGETS="$SV_TARGETS master"
fi
if test "x$ENABLE_QW_SERVER" = xyes; then
	QW_TARGETS="qw-server\$(EXEEXT) $QW_TARGETS"
	SV_TARGETS="$SV_TARGETS qw"
fi

if test "x$BUILD_SW" = xyes; then
	VID_REND_TARGETS="$VID_REND_TARGETS libQFrenderer_sw.la"
	VID_MODEL_TARGETS="$VID_MODEL_TARGETS libQFmodels_sw.la"
fi
if test "x$BUILD_SW32" = xyes; then
	VID_REND_TARGETS="$VID_REND_TARGETS libQFrenderer_sw32.la"
	VID_MODEL_TARGETS="$VID_MODEL_TARGETS libQFmodels_sw32.la"
fi
if test "x$BUILD_GL" = xyes; then
	VID_REND_TARGETS="$VID_REND_TARGETS libQFrenderer_gl.la"
	VID_MODEL_TARGETS="$VID_MODEL_TARGETS libQFmodels_gl.la"
fi

AC_SUBST(CAN_BUILD_GL)
AC_SUBST(CAN_BUILD_SW)
AC_SUBST(CAN_BUILD_SW32)

AC_SUBST(HAVE_FBDEV)
AC_SUBST(HAVE_SDL)
AC_SUBST(HAVE_SVGA)
AC_SUBST(HAVE_X)

if test -n "$CL_TARGETS"; then
	CD_TARGETS="libQFcd.la"
	SND_TARGETS="libQFsound.la"
	VID_TARGETS="libQFjs.la $VID_TARGETS"
	if test "`echo $SOUND_TYPES | grep ALSA0.5`"; then
		SND_PLUGIN_TARGETS="$SND_PLUGIN_TARGETS libsnd_output_alsa0_5.la"
	fi
	if test "`echo $SOUND_TYPES | grep ALSA0.9`"; then
		SND_PLUGIN_TARGETS="$SND_PLUGIN_TARGETS libsnd_output_alsa0_9.la"
	fi
	if test "`echo $SOUND_TYPES | grep MME`"; then
		SND_PLUGIN_TARGETS="$SND_PLUGIN_TARGETS libsnd_output_mme.la"
	fi
	if test "`echo $SOUND_TYPES | grep OSS`"; then
		SND_PLUGIN_TARGETS="$SND_PLUGIN_TARGETS libsnd_output_oss.la"
	fi
	if test "`echo $SOUND_TYPES | grep SDL`"; then
		SND_PLUGIN_TARGETS="$SND_PLUGIN_TARGETS libsnd_output_sdl.la"
	fi
	if test "`echo $SOUND_TYPES | grep SGI`"; then
		SND_PLUGIN_TARGETS="$SND_PLUGIN_TARGETS libsnd_output_sgi.la"
	fi
	if test "`echo $SOUND_TYPES | grep SUN`"; then
		SND_PLUGIN_TARGETS="$SND_PLUGIN_TARGETS libsnd_output_sun.la"
	fi
	if test "`echo $SOUND_TYPES | grep Win32`"; then
		SND_PLUGIN_TARGETS="$SND_PLUGIN_TARGETS libsnd_output_win.la"
	fi
	if test "$SOUND_TYPES"; then
		SND_REND_TARGETS="$SND_REND_TARGETS libsnd_render_default.la"
	fi
else
	CDTYPE=""
	CD_PLUGIN_TARGETS=""
	CD_TARGETS=""
	SND_PLUGIN_TARGETS=""
	SND_REND_TARGETS=""
	SND_TARGETS=""
	SOUND_TYPES=""
fi

AC_SUBST(NQ_TARGETS)
AC_SUBST(QW_TARGETS)
AC_SUBST(CD_PLUGIN_TARGETS)
AC_SUBST(CD_TARGETS)
AC_SUBST(SND_PLUGIN_TARGETS)
AC_SUBST(SND_REND_TARGETS)
AC_SUBST(SND_TARGETS)
AC_SUBST(VID_MODEL_TARGETS)
AC_SUBST(VID_REND_TARGETS)
AC_SUBST(VID_TARGETS)
AC_SUBST(VID_TARGETS_NOINST)

AM_CONDITIONAL(BUILD_GL, test "$BUILD_GL" = "yes")
AM_CONDITIONAL(BUILD_SW, test "$BUILD_SW" = "yes")
AM_CONDITIONAL(BUILD_SW_ASM, test "$BUILD_SW" = "yes" -a "$ASM_ARCH" = "yes")
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
	quakeforge-config

	include/Makefile
	include/QF/Makefile
	include/QF/GL/Makefile
	include/QF/plugin/Makefile
	include/win32/version.h

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
	libs/models/Makefile
	libs/models/alias/Makefile
	libs/models/brush/Makefile
	libs/models/sprite/Makefile
	libs/util/Makefile
	libs/video/Makefile
	libs/video/renderer/Makefile
	libs/video/renderer/gl/Makefile
	libs/video/renderer/sw/Makefile
	libs/video/renderer/sw32/Makefile
	libs/video/targets/Makefile

	qw/Makefile
	qw/quakeforge.lsm
	qw/include/Makefile
	qw/source/Makefile
	
	nq/Makefile
	nq/nuq.lsm
	nq/include/Makefile
	nq/source/Makefile

	doc/Makefile
	doc/man/Makefile
	doc/texinfo/Makefile

	debian/Makefile

	RPM/Makefile
	RPM/build_rpm
	RPM/quakeforge.conf
	RPM/quakeforge.spec,

	chmod +x quakeforge-config RPM/build_rpm
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
	if test "$CCVER" = "gcc 2.96"; then
		CCVER="$CCVER
(Note: You might need CC=kgcc ./configure on some systems)"
	fi
fi

AC_MSG_RESULT([
	QuakeForge has been configured successfully.

	Build type:$BUILD_TYPE
	Server support:$SV_TARGETS
	Client support:$CL_TARGETS
	Sound support:$SOUND_TYPES
	CD Audio system:$CDTYPE
	IPv6 networking: $NETTYPE_IPV6
	Compression support: $HAVE_ZLIB
	Compiler version: $CCVER

	Shared game data directory: $sharepath
	Per-user game data directory: $userpath
	Plugin load directory: $plugindir
	Global configuration file: $globalconf
	User configuration file: $userconf
])

if test -d $srcdir/CVS; then
	echo "WARNING: Hackers at work, watch for falling bits of code."
	echo "(This is from a development CVS tree. Expect problems)"
	echo
fi
