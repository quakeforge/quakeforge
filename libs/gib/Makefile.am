INCLUDES= -I$(top_srcdir)/include

lib_LTLIBRARIES = libQFgib.la

libQFgib_la_LDFLAGS = -version-info 1:0:0
libQFgib_la_SOURCES = gib.c gib_instructions.c gib_interpret.c \
	gib_modules.c gib_parse.c gib_stack.c gib_vars.c
EXTRA_DIST = gib_error.h gib_instructions.h gib_interpret.h gib_modules.h \
	gib_parse.h gib_stack.h gib_vars.h

LIBLIST = libQFgib.la @LIBRARY_SEARCH_PATH@
