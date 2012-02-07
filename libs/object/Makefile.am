AUTOMAKE_OPTIONS= foreign
AM_CFLAGS= @PREFER_PIC@
INCLUDES= -I$(top_srcdir)/include

lib_ldflags=-version-info $(QUAKE_LIBRARY_VERSION_INFO) \
			-rpath $(libdir) -no-undefined
object_libs=$(top_builddir)/libs/util/libQFutil.la

lib_LTLIBRARIES= libQFobject.la

libQFobject_la_LDFLAGS=		$(lib_ldflags)
libQFobject_la_LIBADD=		$(object_libs)
libQFobject_la_DEPENDENCIES=$(object_libs)
libQFobject_la_SOURCES= \
	ArrayList.c Collection.c Double.c Integer.c Iterator.c \
	List.c Number.c Selector.c String.c garbage.c object.c
