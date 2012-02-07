AUTOMAKE_OPTIONS= foreign
AM_CFLAGS= @PREFER_PIC@
INCLUDES= -I$(top_srcdir)/include

lib_ldflags=-version-info $(QUAKE_LIBRARY_VERSION_INFO) \
			-rpath $(libdir) -no-undefined

gib_deps=$(top_builddir)/libs/util/libQFutil.la

lib_LTLIBRARIES= libQFgib.la

libQFgib_la_LDFLAGS=		$(lib_ldflags)
libQFgib_la_LIBADD=			$(gib_deps)
libQFgib_la_DEPENDENCIES=	$(gib_deps)
libQFgib_la_SOURCES= \
	gib_buffer.c gib_builtin.c gib_classes.c gib_execute.c gib_function.c \
	gib_parse.c gib_handle.c  gib_object.c gib_process.c gib_regex.c \
	gib_thread.c gib_vars.c gib_init.c gib_tree.c \
	gib_semantics.c ops.c exp.c regex.c
