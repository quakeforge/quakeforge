dnl ==================================================================
dnl Checks for header files.
dnl ==================================================================

AC_HEADER_DIRENT
AC_HEADER_MAJOR
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS(
	arpa/inet.h asm/io.h assert.h conio.h \
	ctype.h ddraw.h dinput.h direct.h dirent.h dlfcn.h dmedia/audio.h \
	dmedia/cdaudio.h dpmi.h dsound.h errno.h fcntl.h io.h \
	ifaddrs.h libc.h limits.h linux/cdrom.h linux/joystick.h \
	linux/soundcard.h machine/soundcard.h malloc.h math.h mgraph.h _mingw.h \
	netdb.h net/if.h netinet/in.h process.h pthread.h pwd.h \
	setjmp.h signal.h stdarg.h stdio.h stdlib.h string.h strings.h \
	sys/asoundlib.h sys/audioio.h sys/filio.h sys/ioctl.h sys/io.h sys/ipc.h \
	sys/mman.h sys/param.h sys/poll.h sys/select.h sys/shm.h sys/signal.h \
	sys/socket.h sys/soundcard.h sys/stat.h sys/time.h sys/types.h sys/uio.h \
	termios.h time.h unistd.h vgakeyboard.h vgamouse.h windows.h winsock.h
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

AC_FUNC_ALLOCA

AC_MSG_CHECKING(for fnmatch in fnmatch.h)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include "fnmatch.h"]], [[int (*foo)() = fnmatch;]])],[AC_DEFINE(HAVE_FNMATCH_PROTO, 1, Define this if fnmatch is prototyped in fnmatch.h)
	AC_MSG_RESULT(yes)],[AC_MSG_RESULT(no)
])

AC_MSG_CHECKING(for strnlen in string.h)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include "string.h"]], [[int (*foo)() = strnlen;]])],[AC_DEFINE(HAVE_STRNLEN_PROTO, 1, Define this if strnlen is prototyped in string.h)
	AC_MSG_RESULT(yes)],[AC_MSG_RESULT(no)
])

AC_MSG_CHECKING(for strndup in string.h)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include "string.h"]], [[int (*foo)() = strndup;]])],[AC_DEFINE(HAVE_STRNDUP_PROTO, 1, Define this if strndup is prototyped in string.h)
	AC_MSG_RESULT(yes)],[AC_MSG_RESULT(no)
])

AC_MSG_CHECKING(for strcasestr in string.h)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include "string.h"]], [[int (*foo)() = strcasestr;]])],[AC_DEFINE(HAVE_STRCASESTR_PROTO, 1, Define this if strcasestr is prototyped in string.h)
	AC_MSG_RESULT(yes)],[AC_MSG_RESULT(no)
])
