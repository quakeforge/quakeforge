AUTOMAKE_OPTIONS= foreign

WAV_LIBS=@WAV_LIBS@
WAV_DEPS=@WAV_DEPS@
WAV_INCS=@WAV_INCS@

INCLUDES= -I$(top_srcdir)/include $(WAV_INCS)

bin_PROGRAMS=	qfwavinfo

#man_MANS=	qfwavinfo.1

qfwavinfo_SOURCES=	qfwavinfo.c
qfwavinfo_LDADD=	$(WAV_LIBS)
qfwavinfo_DEPENDENCIES= $(WAV_DEPS)

EXTRA_DIST=
