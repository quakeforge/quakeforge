AUTOMAKE_OPTIONS= foreign

WAD_LIBS=@WAD_LIBS@
WAD_DEPS=@WAD_DEPS@
WAD_INCS=@WAD_INCS@

INCLUDES= -I$(top_srcdir)/include $(WAD_INCS)

if BUILD_WAD
wad=wad
mans=wad.1
else
wad=
mans=
endif

bin_PROGRAMS=	$(wad)
EXTRA_PROGRAMS= wad

man_MANS=	$(mans)

wad_SOURCES=	grab.c script.c wad.c
wad_LDADD=	$(WAD_LIBS)
wad_DEPENDENCIES= $(WAD_DEPS)

EXTRA_DIST=		wad.h wad.1
