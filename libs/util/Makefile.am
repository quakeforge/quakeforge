AUTOMAKE_OPTIONS= foreign

SUBDIRS= . test

AM_CFLAGS= @PREFER_PIC@
CCASFLAGS+= @PREFER_PIC@
AM_CPPFLAGS= -I$(top_srcdir)/include $(Z_CFLAGS) $(FNM_FLAGS)

lib_LTLIBRARIES= libQFutil.la

lib_ldflags=-version-info $(QUAKE_LIBRARY_VERSION_INFO) \
			-rpath $(libdir) -no-undefined

if ASM_ARCH
asm= libasm.la
else
asm=
endif

noinst_LTLIBRARIES=	$(asm)
#EXTRA_LTLIBRARIES= libasm.la

libasm_la_SOURCES=	math.S sys_ia32.S


dirent_src=		dirent.c
fnmatch_src=	fnmatch.c
getopt_src=		getopt.c getopt1.c

if BUILD_DIRENT
dirent= $(dirent_src)
else
dirent=
endif

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

libQFutil_la_LDFLAGS=		$(lib_ldflags)
libQFutil_la_LIBADD=		$(asm) $(Z_LIBS) $(DL_LIBS) $(WIN32_LIBS)
libQFutil_la_DEPENDENCIES=	$(asm)
libQFutil_la_SOURCES= \
	bspfile.c buildnum.c cbuf.c checksum.c cmd.c crc.c cvar.c dstring.c \
	fendian.c hash.c idparse.c info.c link.c llist.c \
	mathlib.c mdfour.c mersenne.c msg.c pakfile.c plugin.c qargs.c qendian.c \
	qfplist.c quakefs.c quakeio.c riff.c script.c segtext.c set.c sizebuf.c \
	string.c sys.c txtbuffer.c va.c ver_check.c vrect.c wad.c wadfile.c \
	zone.c \
	$(dirent) $(fnmatch) $(getopt)

EXTRA_DIST= $(fnmatch_src) $(getopt_src)
