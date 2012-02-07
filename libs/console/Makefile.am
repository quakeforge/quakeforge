AUTOMAKE_OPTIONS= foreign

AM_CFLAGS= @PREFER_PIC@
INCLUDES= -I$(top_srcdir)/include
plugin_ldflags= @plugin_ldflags@ -avoid-version -module -rpath $(plugindir)
plugin_libadd= @plugin_libadd@
lib_ldflags=-version-info $(QUAKE_LIBRARY_VERSION_INFO) \
			-rpath $(libdir) -no-undefined
EXEEXT=

lib_LTLIBRARIES=	libQFconsole.la
plugin_LTLIBRARIES= @SERVER_PLUGIN_TARGETS@ @CLIENT_PLUGIN_TARGETS@
noinst_LTLIBRARIES= @SERVER_PLUGIN_STATIC@ @CLIENT_PLUGIN_STATIC@
EXTRA_LTLIBRARIES=  console_server.la console_client.la

common_sources= \
	buffer.c complete.c console.c inputline.c list.c filelist.c view.c
client_sources= bi_inputline.c client.c menu.c pr_keys.c
server_sources= server.c

console_deps=$(top_builddir)/libs/util/libQFutil.la
client_deps= libQFconsole.la \
	$(top_builddir)/libs/audio/libQFsound.la \
	$(top_builddir)/libs/ruamoko/libQFruamoko.la \
	$(console_deps)
server_deps= libQFconsole.la $(console_deps)

libQFconsole_la_LDFLAGS=		$(lib_ldflags)
libQFconsole_la_LIBADD=			$(console_deps) $(plugin_libadd)
libQFconsole_la_DEPENDENCIES=	$(console_deps)
libQFconsole_la_SOURCES=		$(common_sources)

console_client_la_LDFLAGS=		$(plugin_ldflags)
console_client_la_LIBADD=		$(client_deps) $(plugin_libadd)
console_client_la_DEPENDENCIES=	$(client_deps)
console_client_la_SOURCES=		$(client_sources)

console_server_la_LDFLAGS=		$(plugin_ldflags)
console_server_la_LIBADD=		$(server_deps) $(CURSES_LIBS) $(plugin_libadd)
console_server_la_DEPENDENCIES=	$(server_deps)
console_server_la_SOURCES=		$(server_sources)
