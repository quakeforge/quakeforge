## Process this file with automake to produce Makefile.in
AUTOMAKE_OPTIONS= foreign

pkgdatadir=@sharepath@/QF

QFCC_DEP=$(top_builddir)/tools/qfcc/source/qfcc$(EXEEXT)
QFCC=$(QFCC_DEP)
QCFLAGS=-qq -g -Wall -Werror -Wno-integer-divide --no-default-paths
QCPPFLAGS=-I. -I$(srcdir) -I$(top_builddir)/ruamoko/include -I$(top_srcdir)/ruamoko/include -I$(top_builddir)/include -I$(top_srcdir)/include
GZIP=if echo $@ | grep -q .gz; then gzip -f `basename $@ .gz`; if test -f `basename $@ .dat.gz`.sym; then gzip -f `basename $@ .dat.gz`.sym; fi; fi
GZ=@progs_gz@
# BSD make can't handle $(shell foo) directives, and GNU make can't handle |=
# so we have to bite the bullet and pass this to the shell every time.
STRIP=`echo -n $(srcdir)/ | sed -e 's/[^/]//g' | wc -c`

menu_data=menu.dat$(GZ) menu.sym$(GZ) menu.plist

data=$(menu_data)

pkgdata_DATA= $(data)
EXTRA_DATA= $(menu_data)

menu_src= \
	client_menu.r controls_o.r options.r options_util.r servlist.r \
	Frame.r menu.r HUD.r plistmenu.r ../lib/debug.r \
	\
	CrosshairCvar.r CrosshairView.r CvarColor.r CvarColorView.r \
	CvarObject.r CvarRange.r CvarRangeView.r CvarString.r CvarStringView.r \
	CvarToggle.r CvarToggleView.r \
	MenuGroup.r MouseToggle.r ProxyView.r RunToggle.r SubMenu.r

SUFFIXES=.qfo .r
.r.qfo:
	$(QFCC) $(QCFLAGS) $(QCPPFLAGS) -p $(STRIP) -c -o $@ $<
menu_obj=$(menu_src:.r=.qfo)

menu.dat$(GZ): $(menu_obj) $(QFCC_DEP) ../lib/libcsqc.a ../lib/libr.a ../gui/libgui.a
	$(QFCC) $(QCFLAGS) -p $(STRIP) -o menu.dat $(menu_obj) ../gui/libgui.a ../lib/libcsqc.a ../lib/libr.a
	$(GZIP)
menu.sym$(GZ): menu.dat$(GZ)

EXTRA_DIST= $(menu_src) \
	CrosshairCvar.h CrosshairView.h CvarColor.h CvarColorView.h CvarObject.h \
	CvarRange.h CvarRangeView.h CvarString.h CvarStringView.h \
	CvarToggle.h CvarToggleView.h Frame.h HUD.h \
	MenuGroup.h MouseToggle.h ProxyView.h RunToggle.h SubMenu.h client_menu.h \
	controls_o.h menu.h options.h options_util.h plistmenu.h servlist.h \
	menu.plist
CLEANFILES= *.dat *.sym *.gz *.qfo
