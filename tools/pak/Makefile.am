AUTOMAKE_OPTIONS= foreign

PAK_LIBS=@PAK_LIBS@
PAK_DEPS=@PAK_DEPS@
PAK_INCS=@PAK_INCS@

INCLUDES= -I$(top_srcdir)/include $(PAK_INCS)

if BUILD_PAK
pak=pak
mans=pak.1
else
pak=
mans=
endif

bin_PROGRAMS=	$(pak)
EXTRA_PROGRAMS= pak

man_MANS=	$(mans)
EXTRA_MANS= pak.1

pak_SOURCES=	pak.c
pak_LDADD=	$(PAK_LIBS)
pak_DEPENDENCIES= $(PAK_DEPS)

EXTRA_DIST=		pak.h
