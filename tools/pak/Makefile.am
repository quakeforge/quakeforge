AUTOMAKE_OPTIONS= foreign

PAK_LIBS=@PAK_LIBS@
PAK_DEPS=@PAK_DEPS@
PAK_INCS=@PAK_INCS@

AM_CPPFLAGS= -I$(top_srcdir)/include $(PAK_INCS)

bin_PROGRAMS=		pak
bin_SCRIPTS=		zpak

mans=				pak.1 zpak.1
man_MANS=			$(mans)

pak_SOURCES=		pak.c
pak_LDADD=			$(PAK_LIBS)
pak_DEPENDENCIES=	$(PAK_DEPS)

EXTRA_DIST=pak.h zpak $(mans)
