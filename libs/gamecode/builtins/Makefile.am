AUTOMAKE_OPTIONS= foreign

CFLAGS+= @PREFER_PIC@
INCLUDES= -I$(top_srcdir)/include

lib_LTLIBRARIES= libQFgamecode_builtins.la

libQFgamecode_builtins_la_LDFLAGS=	-version-info 1:0:0 -no-undefined
libQFgamecode_builtins_la_LIBADD=	$(top_builddir)/libs/gamecode/engine/libQFgamecode.la $(top_builddir)/libs/gib/libQFgib.la
libQFgamecode_builtins_la_SOURCES=	pr_cmds.c bi_gib.c
