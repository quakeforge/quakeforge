AUTOMAKE_OPTIONS= foreign

AM_CFLAGS= @PREFER_PIC@
INCLUDES= -I$(top_srcdir)/include
plugin_ldflags= @plugin_ldflags@ -avoid-version -module -rpath $(plugindir)
plugin_libadd= @plugin_libadd@
EXEEXT=

lib_LTLIBRARIES=	libQFconsole.la
plugin_LTLIBRARIES= @SERVER_PLUGIN_TARGETS@ @CLIENT_PLUGIN_TARGETS@
noinst_LTLIBRARIES= @SERVER_PLUGIN_STATIC@ @CLIENT_PLUGIN_STATIC@
EXTRA_LTLIBRARIES=  console_server.la console_client.la

common_sources= \
	buffer.c complete.c console.c inputline.c list.c filelist.c view.c
client_sources= bi_inputline.c client.c menu.c pr_keys.c
server_sources= server.c

libQFconsole_la_LDFLAGS=	-version-info 1:0:0 -rpath $(libdir) -no-undefined
libQFconsole_la_LIBADD=		$(plugin_libadd) $(top_builddir)/libs/util/libQFutil.la
libQFconsole_la_SOURCES=	$(common_sources)

console_client_la_LDFLAGS=	$(plugin_ldflags)
console_client_la_LIBADD=	$(plugin_libadd) libQFconsole.la $(top_builddir)/libs/audio/libQFsound.la $(top_builddir)/libs/ruamoko/libQFruamoko.la
console_client_la_SOURCES=	$(client_sources)

console_server_la_LDFLAGS=	$(plugin_ldflags)
console_server_la_LIBADD=	$(CURSES_LIBS) $(plugin_libadd) libQFconsole.la
console_server_la_SOURCES=	$(server_sources)
