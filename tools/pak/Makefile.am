AUTOMAKE_OPTIONS= foreign

PAK_LIBS=@PAK_LIBS@
PAK_DEPS=@PAK_DEPS@
PAK_INCS=@PAK_INCS@

INCLUDES= -I$(top_srcdir)/include $(PAK_INCS)

bin_PROGRAMS=	pak

man_MANS=	pak.1

pak_SOURCES=	pak.c
pak_LDADD=	$(PAK_LIBS)
pak_DEPENDENCIES= $(PAK_DEPS)

EXTRA_DIST=		pak.h pak.1
