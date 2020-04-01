AUTOMAKE_OPTIONS= foreign

AM_CFLAGS= @PREFER_PIC@
AM_CPPFLAGS= -I$(top_srcdir)/include $(FNM_FLAGS)

lib_ldflags=-version-info $(QUAKE_LIBRARY_VERSION_INFO) \
			-rpath $(libdir) -no-undefined
rua_libs= \
	$(top_builddir)/libs/gamecode/libQFgamecode.la \
	$(top_builddir)/libs/util/libQFutil.la

lib_LTLIBRARIES= libQFruamoko.la

libQFruamoko_la_LDFLAGS=		$(lib_ldflags)
libQFruamoko_la_LIBADD=			$(rua_libs)
libQFruamoko_la_DEPENDENCIES=	$(rua_libs)
libQFruamoko_la_SOURCES= \
		pr_cmds.c \
		rua_cbuf.c rua_cmd.c rua_cvar.c rua_hash.c rua_init.c \
		rua_math.c rua_msgbuf.c rua_obj.c rua_plist.c rua_qfile.c rua_qfs.c \
		rua_runtime.c rua_script.c rua_set.c rua_string.c
