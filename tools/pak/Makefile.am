AUTOMAKE_OPTIONS= foreign

PAK_LIBS=@PAK_LIBS@
PAK_DEPS=@PAK_DEPS@
PAK_INCS=@PAK_INCS@

INCLUDES= -I$(top_srcdir)/include $(PAK_INCS)

if BUILD_PAK
pak=pak
zpak=zpak
mans=pak.1
else
pak=
zpak=
mans=
endif

bin_PROGRAMS=	$(pak)
bin_SCRIPTS=	$(zpak)
EXTRA_PROGRAMS=	pak
EXTRA_SCRIPTS=	zpak

man_MANS=	$(mans)

pak_SOURCES=	pak.c
pak_LDADD=	$(PAK_LIBS)
pak_DEPENDENCIES= $(PAK_DEPS)

EXTRA_DIST=		pak.h pak.1
