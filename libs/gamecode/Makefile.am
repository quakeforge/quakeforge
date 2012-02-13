AUTOMAKE_OPTIONS= foreign

AM_CFLAGS= @PREFER_PIC@
INCLUDES= -I$(top_srcdir)/include
lib_ldflags=

gc_deps=$(top_builddir)/libs/util/libQFutil.la

noinst_LTLIBRARIES=	libQFgamecode.la

libQFgamecode_la_LDFLAGS=		$(lib_ldflags)
libQFgamecode_la_LIBADD=		$(gc_deps)
libQFgamecode_la_DEPENDENCIES=	$(gc_deps)
libQFgamecode_la_SOURCES= \
		pr_builtins.c pr_edict.c pr_debug.c pr_exec.c pr_load.c pr_parse.c \
		pr_opcode.c pr_resolve.c pr_resource.c pr_strings.c pr_zone.c
