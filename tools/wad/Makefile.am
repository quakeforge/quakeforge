AUTOMAKE_OPTIONS= foreign

WAD_LIBS=@WAD_LIBS@
WAD_DEPS=@WAD_DEPS@
WAD_INCS=@WAD_INCS@

INCLUDES= -I$(top_srcdir)/include $(WAD_INCS)


bin_PROGRAMS=	$(wad)

mans=wad.1
man_MANS=	$(mans)

wad_SOURCES=	grab.c script.c wad.c
wad_LDADD=	$(WAD_LIBS)
wad_DEPENDENCIES= $(WAD_DEPS)

EXTRA_DIST=		wad.h $(mans)
