INCLUDES= -I$(top_srcdir)/include

lib_LTLIBRARIES = libQFutil.la

if BUILD_FNMATCH
fnmatch_SRC = fnmatch.c dirent.c
else
fnmatch_SRC =
endif

noinst_LTLIBRARIES = libasm.la

if ASM_ARCH
libasm_la_SOURCES = math.S
else
libasm_la_SOURCES =
endif

libQFutil_la_LDFLAGS = -version-info 1:0:0 $(Z_LIBS) $(DL_LIBS)
libQFutil_la_LIBADD = libasm.la
libQFutil_la_DEPENDENCIES = libasm.la
libQFutil_la_SOURCES = \
	checksum.c cmd.c console.c con_print.c crc.c cvar.c fendian.c hash.c \
	info.c link.c \
	mathlib.c \
	mdfour.c msg.c pcx.c plugin.c qargs.c qendian.c qfplist.c quakefs.c \
	quakeio.c sizebuf.c sys.c tga.c va.c ver_check.c wad.c \
	zone.c $(fnmatch_SRC)

LIBLIST = libQFutil.la @LIBRARY_SEARCH_PATH@

EXTRA_DIST = fnmatch.c dirent.c
