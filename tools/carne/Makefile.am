AUTOMAKE_OPTIONS= foreign

CARNE_LIBS=@CARNE_LIBS@
CARNE_DEPS=@CARNE_DEPS@
PAK_INCS=@CARNE_INCS@

INCLUDES= -I$(top_srcdir)/include $(CARNE_INCS)

if BUILD_CARNE
carne=carne
else
carne=
endif

noinst_PROGRAMS= $(carne)
EXTRA_PROGRAMS= carne

carne_SOURCES=	main.c
carne_LDADD=	$(CARNE_LIBS)
carne_DEPENDENCIES= $(CARNE_DEPS)

EXTRA_DIST=
