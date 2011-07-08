AUTOMAKE_OPTIONS= foreign
AM_CFLAGS= @PREFER_PIC@
INCLUDES= -I$(top_srcdir)/include

lib_LTLIBRARIES= libQFgib.la

libQFgib_la_LDFLAGS=	-version-info $(QUAKE_LIBRARY_VERSION_INFO) -no-undefined
libQFgib_la_LIBADD=		$(top_builddir)/libs/util/libQFutil.la
libQFgib_la_SOURCES= \
	gib_buffer.c gib_builtin.c gib_classes.c gib_execute.c gib_function.c gib_parse.c gib_handle.c \
	gib_object.c gib_process.c gib_regex.c gib_thread.c gib_vars.c gib_init.c gib_tree.c \
	gib_semantics.c ops.c exp.c regex.c
