AUTOMAKE_OPTIONS= foreign

AM_CFLAGS= @PREFER_PIC@
INCLUDES= -I$(top_srcdir)/include

lib_LTLIBRARIES= libQFgamecode_builtins.la libQFcsqc.la

libQFgamecode_builtins_la_LDFLAGS=	-version-info 1:0:0
libQFgamecode_builtins_la_SOURCES=	pr_cmds.c

libQFcsqc_la_LDFLAGS=	-version-info 1:0:0
libQFcsqc_la_SOURCES=\
		bi_cbuf.c bi_cmd.c bi_cvar.c bi_file.c bi_hash.c bi_init.c \
		bi_inputline.c bi_plist.c \
		bi_qfile.c bi_qfs.c
