INCLUDES= -I$(top_srcdir)/include

lib_LTLIBRARIES = libQFutil.la

libQFutil_la_LDFLAGS = -version-info 1:0:0 $(Z_LIBS) $(DL_LIBS)
libQFutil_la_SOURCES = \
	checksum.c cmd.c console.c con_print.c crc.c cvar.c hash.c \
	info.c link.c math.S \
	mathlib.c \
	mdfour.c msg.c plugin.c qargs.c qendian.c qfplist.c quakefs.c \
	quakeio.c sizebuf.c sys.c sys_error.c va.c ver_check.c zone.c

LIBLIST = libQFutil.la @LIBRARY_SEARCH_PATH@
