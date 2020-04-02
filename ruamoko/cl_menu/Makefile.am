## Process this file with automake to produce Makefile.in
AUTOMAKE_OPTIONS= foreign no-exeext

QFCC_DEP=$(top_builddir)/tools/qfcc/source/qfcc$(EXEEXT)
QFCC=$(QFCC_DEP)

GZ=@progs_gz@
menu_libexec=menu.dat
menu_data=menu.plist menu.sym

pkglibexecdir=@sharepath@/QF
pkgdatadir=@sharepath@/QF

pkgdata_DATA= $(menu_data)
pkglibexec_PROGRAMS= $(menu_libexec)
EXTRA_PROGRAMS= $(menu_libexec)

QCFLAGS=-qq -O -g -Wall -Werror -Wno-integer-divide
QCPPFLAGS=--no-default-paths -I$(top_srcdir)/ruamoko/include -I$(top_srcdir)/include
QCLINKFLAGS=--no-default-paths -L$(top_builddir)/ruamoko/lib
QCOMPILE=$(QFCC) $(QCFLAGS) $(QCPPFLAGS)
QLINK=$(QFCC) $(QCFLAGS) $(QCLINKFLAGS)

MKDIR_P = @MKDIR_P@
am__mv = mv -f

GZIP=if echo $@ | grep -q .gz; then gzip -f `basename $@ .gz`; if test -f `basename $@ .dat.gz`.sym; then gzip -f `basename $@ .dat.gz`.sym; fi; fi
# BSD make can't handle $(shell foo) directives, and GNU make can't handle |=
# so we have to bite the bullet and pass this to the shell every time.

SUFFIXES=.o .r
.r.o:
	$(QCOMPILE) -MT $@ -MD -MP -MF $(DEPDIR)/$*.Tqo -c -o $@ $<
	@sed -i -e '1s@:@: $(QFCC_DEP)@' $(DEPDIR)/$*.Tqo
	@$(am__mv) $(DEPDIR)/$*.Tqo $(DEPDIR)/$*.Qo

menu_src= \
	client_menu.r controls_o.r options.r options_util.r servlist.r \
	Frame.r HUD.r menu.r plistmenu.r \
	\
	CrosshairCvar.r CrosshairView.r CvarColor.r CvarColorView.r \
	CvarObject.r CvarRange.r CvarRangeView.r CvarString.r CvarStringView.r \
	CvarToggle.r CvarToggleView.r \
	MenuGroup.r MouseToggle.r ProxyView.r RunToggle.r SubMenu.r

r_depfiles_remade=

menu_dat_SOURCES=$(menu_src)
menu_obj=$(menu_src:.r=.o)
menu_dep=$(addprefix ./$(DEPDIR)/,$(menu_obj:.o=.Qo))
menu.dat: $(menu_obj) $(QFCC_DEP) ../lib/libcsqc.a ../lib/libr.a ../gui/libgui.a
	$(QLINK) -o menu.dat $(menu_obj) ../gui/libgui.a ../lib/libcsqc.a ../lib/libr.a
include $(menu_dep) # am--include-marker
r_depfiles_remade += $(menu_dep)

menu.sym: menu.dat

$(r_depfiles_remade):
	$(MKDIR_P) $(@D)
	echo '# dummy' >$@-t && $(am__mv) $@-t $@

am--depfiles: $(am__depfiles_remade) $(r_depfiles_remade)

EXTRA_DIST= $(menu_src) \
	CrosshairCvar.h CrosshairView.h CvarColor.h CvarColorView.h CvarObject.h \
	CvarRange.h CvarRangeView.h CvarString.h CvarStringView.h \
	CvarToggle.h CvarToggleView.h Frame.h HUD.h \
	MenuGroup.h MouseToggle.h ProxyView.h RunToggle.h SubMenu.h client_menu.h \
	controls_o.h menu.h options.h options_util.h plistmenu.h servlist.h \
	menu.plist
CLEANFILES= *.dat *.sym *.gz *.qfo *.o
