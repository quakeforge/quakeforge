## Process this file with automake to produce Makefile.in
AUTOMAKE_OPTIONS= foreign

#FIXME should qf data be installed somewhere other than id1 that gets
#searched after everything else?
pkgdatadir=@sharepath@/id1

QFCC_DEP=$(top_builddir)/tools/qfcc/source/qfcc$(EXEEXT)
QFCC=$(QFCC_DEP)
QCFLAGS=-qq -g -Werror
QCPPFLAGS=-I. -I$(srcdir) -I$(top_builddir)/ruamoko/include -I$(top_srcdir)/ruamoko/include -I$(top_builddir)/include -I$(top_srcdir)/include
GZIP=if echo $@ | grep -q .gz; then gzip -f `basename $@ .gz`; if test -f `basename $@ .dat.gz`.sym; then gzip -f `basename $@ .dat.gz`.sym; fi; fi
if HAVE_ZLIB
GZ=.gz
else
GZ=
endif
STRIP=$(shell echo `echo -n $(srcdir)/ | sed -e 's/[^/]//g' | wc -c`)

pkgdata_DATA= menu.dat$(GZ)

menu_src= \
	client_menu.qc controls_o.qc \
	inputline_util.qc menu.r options.qc \
	options_util.qc servlist.qc string.r stringh.r

%.qfo: %.r
	$(QFCC) $(QCFLAGS) $(QCPPFLAGS) -c -o $@ $<

%.qfo: %.qc
	$(QFCC) $(QCFLAGS) $(QCPPFLAGS) -c -o $@ $<

menu_obj=$(addsuffix .qfo,$(basename $(menu_src)))

menu.dat$(GZ): $(menu_obj) $(QFCC_DEP) ../lib/libgui.a ../lib/libcsqc.a ../lib/libr.a
	$(QFCC) $(QCFLAGS) -p $(STRIP) -o menu.dat $(menu_obj) -l../lib/libgui.a -l../lib/libcsqc.a -l../lib/libr.a
	$(GZIP)

EXTRA_DIST= $(menu_src) \
	client_menu.h controls_o.h \
	menu.h options.h \
	options_util.h servlist.h string.h stringh.h
CLEANFILES= *.dat *.sym *.gz *.qfo
