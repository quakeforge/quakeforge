INCLUDES= -I$(top_srcdir)/include

lib_LTLIBRARIES=	libQFconsole.la
plugin_LTLIBRARIES= libconsole_server.la #libconsole_client.la

common_sources= buffer.c complete.c console.c inputline.c list.c maplist.c
client_sources= client.c
server_sources= server.c

libQFconsole_la_LDFLAGS=	$(plugin_LDFLAGS) -version-info 1:0:0
libQFconsole_la_LIBADD=		$(plugin_LIBADD)
libQFconsole_la_SOURCES=	$(common_sources)

#libconsole_client_la_LDFLAGS=	-version-info 1:0:0
#libconsole_client_la_SOURCES=	$(client_sources)

libconsole_server_la_LDFLAGS=	$(plugin_LDFLAGS) -version-info 1:0:0
libconsole_server_la_LIBADD=	$(CURSES_LIBS) $(plugin_LIBADD)
libconsole_server_la_SOURCES=	$(server_sources)
