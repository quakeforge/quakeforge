AUTOMAKE_OPTIONS= foreign

AM_CFLAGS= @PREFER_PIC@
INCLUDES= -I$(top_srcdir)/include

lib_LTLIBRARIES= libQFruamoko.la

libQFruamoko_la_LDFLAGS=	-version-info 1:0:0 -no-undefined
libQFruamoko_la_LIBADD=		$(top_builddir)/libs/gamecode/engine/libQFgamecode.la $(top_builddir)/libs/util/libQFutil.la
libQFruamoko_la_SOURCES= \
		rua_cbuf.c rua_cmd.c rua_cvar.c rua_file.c rua_hash.c rua_init.c \
		rua_obj.c rua_plist.c rua_qfile.c rua_qfs.c rua_string.c
