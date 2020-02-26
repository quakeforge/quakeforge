QWAQ_LIBS=@QWAQ_LIBS@
QWAQ_DEPS=@QWAQ_DEPS@
QWAQ_INCS=@QWAQ_INCS@

AM_CPPFLAGS=	-I$(top_srcdir)/include $(QWAQ_INCS) $(PTHREAD_CFLAGS)

QFCC_DEP=$(top_builddir)/tools/qfcc/source/qfcc$(EXEEXT)
QFCC=$(top_builddir)/tools/qfcc/source/qfcc
QCFLAGS=-qq -O -g -Werror --advanced --no-default-paths

noinst_PROGRAMS=@QWAQ_TARGETS@
noinst_DATA= #qwaq.dat

qwaq_dat_src= \
	$e

qwaq_curses_libs=
qwaq_curses_SOURCES=main.c qwaq-curses.c
qwaq_curses_LDADD= $(qwaq_curses_libs) $(QWAQ_LIBS) \
				$(PANEL_LIBS) $(CURSES_LIBS) $(PTHREAD_LDFLAGS) $(DL_LIBS)
qwaq_curses_LDFLAGS=
qwaq_curses_DEPENDENCIES= $(qwaq_curses_libs) $(QWAQ_DEPS)

cl_plugin_libs= \
	@client_static_plugin_libs@

client_libs= \
	$(top_builddir)/libs/console/libQFconsole.la \
	$(top_builddir)/libs/video/targets/libQFjs.la \
	$(top_builddir)/libs/audio/libQFcd.la \
	$(top_builddir)/libs/audio/libQFsound.la \
	$(top_builddir)/libs/image/libQFimage.la

qwaq_x11_libs= \
	$(cl_plugin_libs) \
	$(top_builddir)/libs/video/renderer/libQFrenderer.la \
	$(top_builddir)/libs/models/libQFmodels.la \
	$(top_builddir)/libs/video/targets/libQFx11.la \
	$(client_libs) \
	$(top_builddir)/libs/gib/libQFgib.la
qwaq_x11_SOURCES=qwaq.c qwaq-bi.c
qwaq_x11_LDADD= $(qwaq_x11_libs) $(QWAQ_LIBS) \
				$(VIDMODE_LIBS) $(DGA_LIBS) $(X_LIBS) -lX11 \
				$(X_EXTRA_LIBS) $(X_SHM_LIB) $(DL_LIBS)
qwaq_x11_LDFLAGS=
qwaq_x11_DEPENDENCIES= $(qwaq_x11_libs) $(QWAQ_DEPS)

#qwaq.dat: $(qwaq_dat_src) $(QFCC_DEP) $(top_srcdir)/ruamoko/lib/Object.r
#	$(QFCC) $(QCFLAGS) -I$(top_srcdir)/ruamoko/include

EXTRA_PROGRAMS=qwaq-curses qwaq-x11
EXTRA_DIST=$(qwaq_dat_src) qwaq.h
CLEANFILES= *.dat *.sym
