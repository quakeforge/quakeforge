AUTOMAKE_OPTIONS= foreign
AM_CFLAGS= @PREFER_PIC@
INCLUDES= -I$(top_srcdir)/include

lib_LTLIBRARIES= libQFobject.la

libQFobject_la_LDFLAGS=		-version-info $(QUAKE_LIBRARY_VERSION_INFO) -no-undefined
libQFobject_la_LIBADD=		$(top_builddir)/libs/util/libQFutil.la
libQFobject_la_SOURCES= \
	ArrayList.c Collection.c Double.c Integer.c Iterator.c \
	List.c Number.c Selector.c String.c garbage.c object.c
