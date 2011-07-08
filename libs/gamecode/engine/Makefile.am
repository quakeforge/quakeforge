AUTOMAKE_OPTIONS= foreign

AM_CFLAGS= @PREFER_PIC@
INCLUDES= -I$(top_srcdir)/include

lib_LTLIBRARIES=	libQFgamecode.la

libQFgamecode_la_LDFLAGS=	-version-info $(QUAKE_LIBRARY_VERSION_INFO) -no-undefined
libQFgamecode_la_LIBADD=	$(top_builddir)/libs/util/libQFutil.la
libQFgamecode_la_SOURCES= \
		pr_builtins.c pr_edict.c pr_debug.c pr_exec.c pr_load.c pr_parse.c \
		pr_opcode.c pr_resolve.c pr_resource.c pr_strings.c pr_zone.c
