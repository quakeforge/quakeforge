INCLUDES= -I$(top_srcdir)/include

lib_LTLIBRARIES= libQFutil.la

if ASM_ARCH
noinst_LTLIBRARIES=	libasm.la
else
noinst_LTLIBRARIES=
endif

asm_src= math.S

libasm_la_SOURCES=	$(asm_src)


fnmatch_src=	fnmatch.c dirent.c
if BUILD_FNMATCH
fnmatch= $(fnmatch_src)
else
fnmatch=
endif

libQFutil_la_LDFLAGS=		-version-info 1:0:0
libQFutil_la_LIBADD=		libasm.la $(Z_LIBS) $(DL_LIBS)
libQFutil_la_DEPENDENCIES=	libasm.la
libQFutil_la_SOURCES= \
	checksum.c cmd.c console.c con_print.c crc.c cvar.c fendian.c hash.c \
	info.c link.c mathlib.c mdfour.c msg.c pcx.c plugin.c qargs.c \
	qendian.c qfplist.c quakefs.c quakeio.c sizebuf.c string.c sys.c \
	tga.c va.c ver_check.c wad.c zone.c $(fnmatch)

EXTRA_DIST= $(asm_src) $(fnmatch_src)
