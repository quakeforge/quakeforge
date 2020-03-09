AUTOMAKE_OPTIONS= foreign

QWAQ_LIBS=@QWAQ_LIBS@
QWAQ_DEPS=@QWAQ_DEPS@
QWAQ_INCS=@QWAQ_INCS@

AM_CPPFLAGS=	-I$(top_srcdir)/include $(QWAQ_INCS) $(PTHREAD_CFLAGS)

noinst_PROGRAMS=@QWAQ_TARGETS@ qwaq-app.dat$(EXEEXT)

QFCC_DEP=$(top_builddir)/tools/qfcc/source/qfcc$(EXEEXT)
QFCC=$(top_builddir)/tools/qfcc/source/qfcc

QCFLAGS=-qq -O -g -Werror
QCPPFLAGS=--no-default-paths -I$(top_srcdir)/ruamoko/include
QCLINKFLAGS=--no-default-paths -L$(top_builddir)/ruamoko/lib
QCOMPILE=$(QFCC) $(QCFLAGS) $(QCPPFLAGS)
QLINK=$(QFCC) $(QCFLAGS) $(QCLINKFLAGS)

SUFFIXES=.o .r
.r.o:
	$(QCOMPILE) -MT $@ -MD -MP -MF $(DEPDIR)/$*.Tqo -c -o $@ $<
	sed -i -e '1s@:@: $(QFCC_DEP)@' $(DEPDIR)/$*.Tqo
	$(am__mv) $(DEPDIR)/$*.Tqo $(DEPDIR)/$*.Qo

qwaq_app_dat_src= \
	qwaq-app.r \
	qwaq-draw.r \
	qwaq-group.r \
	qwaq-rect.r \
	qwaq-screen.r \
	qwaq-textcontext.r \
	qwaq-view.r \
	qwaq-window.r

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

r_depfiles_remade=

qwaq_app_dat_SOURCES=$(qwaq_app_dat_src)
qwaq_app_obj=$(qwaq_app_dat_SOURCES:.r=.o)
qwaq_app_dep=$(addprefix ./$(DEPDIR)/,$(qwaq_app_obj:.o=.Qo))
qwaq-app.dat$(EXEEXT): $(qwaq_app_obj) $(QFCC_DEP)
	$(QLINK) -o $@ $(qwaq_app_obj) -lr
include $(qwaq_app_dep) # am--include-marker
r_depfiles_remade += $(qwaq_app_dep)

$(r_depfiles_remade):
	@$(MKDIR_P) $(@D)
	@echo '# dummy' >$@-t && $(am__mv) $@-t $@

am--depfiles: $(am__depfiles_remade) $(r_depfiles_remade)

EXTRA_PROGRAMS=qwaq-curses qwaq-x11
EXTRA_DIST=$(qwaq_dat_src) qwaq.h
CLEANFILES= *.dat *.sym qwaq-curses.log
