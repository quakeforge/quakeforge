INCLUDES= -I$(top_srcdir)/include

lib_LTLIBRARIES = libQFgib.la

libQFgib_la_LDFLAGS = -version-info 1:0:0
libQFgib_la_SOURCES = gib.c gib_instructions.c gib_interpret.c \
	gib_modules.c gib_parse.c gib_stack.c gib_vars.c

LIBLIST = libQFgib.la @LIBRARY_SEARCH_PATH@
