AUTOMAKE_OPTIONS= foreign

AM_CFLAGS= @PREFER_PIC@
INCLUDES= -I$(top_srcdir)/include

lib_LTLIBRARIES= libQFgamecode_builtins.la

libQFgamecode_builtins_la_LDFLAGS=	-version-info 1:0:0
libQFgamecode_builtins_la_SOURCES=	pr_cmds.c bi_gib.c bi_inputline.c
