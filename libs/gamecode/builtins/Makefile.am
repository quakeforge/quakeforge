AUTOMAKE_OPTIONS= foreign

INCLUDES= -I$(top_srcdir)/include

lib_LTLIBRARIES= libQFgamecode_builtins.la libQFcsqc.la

libQFgamecode_builtins_la_LDFLAGS=	-version-info 1:0:0
libQFgamecode_builtins_la_SOURCES=	pr_cmds.c

libQFcsqc_la_LDFLAGS=	-version-info 1:0:0
libQFcsqc_la_SOURCES= bi_init.c bi_cbuf.c bi_file.c bi_inputline.c bi_string.c bi_cvar.c bi_keys.c bi_strhash.c
