INCLUDES= -I$(top_srcdir)/include

pkglibdir = @PLUGINDIR@

lib_LTLIBRARIES = libQFconsole.la
pkglib_LTLIBRARIES = libconsole_client.la libconsole_server.la

common_sources = complete.c console.c inputline.c list.c
client_sources = client.c
server_sources = server.c

libQFconsole_la_LDFLAGS = -version-info 1:0:0
libQFconsole_la_LIBADD =
libQFconsole_la_SOURCES = $(common_sources)
libQFconsole_la_DEPENDENCIES =

libconsole_client_la_LDFLAGS = -version-info 1:0:0
libconsole_client_la_LIBADD =
libconsole_client_la_SOURCES = $(client_sources)
libconsole_client_la_DEPENDENCIES =

libconsole_server_la_LDFLAGS = -version-info 1:0:0
libconsole_server_la_LIBADD = $(CURSES_LIBS)
libconsole_server_la_SOURCES = $(server_sources)
libconsole_server_la_DEPENDENCIES =

LIBLIST = libQFconsole.la
