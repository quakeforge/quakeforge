AUTOMAKE_OPTIONS= foreign
AM_CFLAGS= @PREFER_PIC@ -DBUILD_QFutil
AM_CCASFLAGS= @PREFER_PIC@ -DBUILD_QFutil
INCLUDES= -I$(top_srcdir)/include

lib_LTLIBRARIES= libQFutil.la

if ASM_ARCH
noinst_LTLIBRARIES=	libasm.la
else
noinst_LTLIBRARIES=
endif

asm_src= math.S sys_ia32.S

libasm_la_SOURCES=	$(asm_src)


fnmatch_src=	fnmatch.c dirent.c
getopt_src=		getopt.c getopt1.c
if BUILD_FNMATCH
fnmatch= $(fnmatch_src)
else
fnmatch=
endif

if BUILD_GETOPT
getopt= $(getopt_src)
else
getopt=
endif

libQFutil_la_LDFLAGS=		-version-info 1:0:0 -no-undefined
libQFutil_la_LIBADD=		libasm.la $(Z_LIBS) $(DL_LIBS) $(WIN32_LIBS)
libQFutil_la_DEPENDENCIES=	libasm.la
libQFutil_la_SOURCES= \
	bspfile.c buildnum.c cbuf.c checksum.c cmd.c crc.c cvar.c dstring.c \
	fendian.c hash.c idparse.c info.c link.c llist.c \
	mathlib.c mdfour.c msg.c pakfile.c plugin.c qargs.c qendian.c \
	qfplist.c quakefs.c quakeio.c riff.c sizebuf.c string.c sys.c \
	va.c ver_check.c wad.c wadfile.c zone.c $(fnmatch) $(getopt)

EXTRA_DIST= $(asm_src) $(fnmatch_src) $(getopt_src)
