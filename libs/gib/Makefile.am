AUTOMAKE_OPTIONS= foreign
AM_CFLAGS= @PREFER_PIC@
INCLUDES= -I$(top_srcdir)/include

lib_LTLIBRARIES= libQFgib.la

libQFgib_la_LDFLAGS=		-version-info 1:0:0
libQFgib_la_SOURCES= \
	gib_buffer.c gib_builtin.c gib_function.c gib_parse.c gib_process.c \
	gib_regex.c gib_thread.c gib_vars.c gib_init.c ops.c exp.c regex.c
