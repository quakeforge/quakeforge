AUTOMAKE_OPTIONS= foreign

INCLUDES= -I$(top_srcdir)/include
plugin_version= 1:0:0
plugin_ldflags= @plugin_ldflags@
plugin_libadd= @plugin_libadd@

lib_LTLIBRARIES=	libQFconsole.la
plugin_LTLIBRARIES= @SERVER_PLUGIN_TARGETS@ @CLIENT_PLUGIN_TARGETS@
noinst_LTLIBRARIES= @SERVER_PLUGIN_STATIC@ @CLIENT_PLUGIN_STATIC@
EXTRA_LTLIBRARIES=  libconsole_server.la libconsole_client.la

common_sources= buffer.c complete.c console.c inputline.c list.c filelist.c
client_sources= client.c menu.c
server_sources= server.c

libQFconsole_la_LDFLAGS=	-version-info 1:0:0 -rpath $(libdir)
libQFconsole_la_LIBADD=		$(plugin_libadd)
libQFconsole_la_SOURCES=	$(common_sources)

libconsole_client_la_LDFLAGS=	$(plugin_ldflags)
libconsole_client_la_LIBADD=	$(plugin_libadd)
libconsole_client_la_SOURCES=	$(client_sources)

libconsole_server_la_LDFLAGS=	$(plugin_ldflags)
libconsole_server_la_LIBADD=	$(CURSES_LIBS) $(plugin_libadd)
libconsole_server_la_SOURCES=	$(server_sources)
