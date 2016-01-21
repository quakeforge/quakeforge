AUTOMAKE_OPTIONS= foreign

WAV_LIBS=@WAV_LIBS@
WAV_DEPS=@WAV_DEPS@
WAV_INCS=@WAV_INCS@

AM_CPPFLAGS= -I$(top_srcdir)/include $(WAV_INCS)

bin_PROGRAMS=	qfwavinfo

mans=qfwavinfo.1
man_MANS=	#$(mans)

qfwavinfo_SOURCES=	qfwavinfo.c
qfwavinfo_LDADD=	$(WAV_LIBS)
qfwavinfo_DEPENDENCIES= $(WAV_DEPS)

EXTRA_DIST=#$(mans)
