AUTOMAKE_OPTIONS= foreign

INCLUDES= -I$(top_srcdir)/include

lib_LTLIBRARIES=	libQFgamecode.la

libQFgamecode_la_LDFLAGS=	-version-info 1:0:0
libQFgamecode_la_SOURCES= \
		pr_builtins.c pr_edict.c pr_debug.c pr_exec.c pr_load.c pr_obj.c \
		pr_opcode.c pr_resolve.c pr_resource.c pr_strings.c pr_zone.c
