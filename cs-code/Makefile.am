## Process this file with automake to produce Makefile.in
AUTOMAKE_OPTIONS= foreign

#FIXME should qf data be installed somewhere other than id1 that gets
#searched after everything else?
pkgdatadir=@sharepath@/id1

QFCC_DEP=$(top_builddir)/tools/qfcc/source/qfcc$(EXEEXT)
QFCC=$(QFCC_DEP)
QCFLAGS=-qq -g -Werror
QCPPFLAGS=-I. -I$(srcdir) -I$(top_builddir)/include -I$(top_srcdir)/include
GZIP=if echo $@ | grep -q .gz; then gzip -f `basename $@ .gz`; if test -f `basename $@ .dat.gz`.sym; then gzip -f `basename $@ .dat.gz`.sym; fi; fi
if HAVE_ZLIB
GZ=.gz
else
GZ=
endif
STRIP=$(shell echo `echo -n $(srcdir)/ | sed -e 's/[^/]//g' | wc -c`)

pkgdata_DATA= menu.dat$(GZ)

menu_src= \
	cbuf.r client_menu.qc cmd.r controls_o.qc cvar.r draw.r file.r \
	game.r inputline.r inputline_util.qc key.r  menu.r options.qc \
	options_util.qc servlist.qc string.r stringh.r

%.qfo: %.r
	$(QFCC) $(QCFLAGS) $(QCPPFLAGS) -p $(STRIP) -c -o $@ $<

%.qfo: %.qc
	$(QFCC) $(QCFLAGS) $(QCPPFLAGS) -p $(STRIP) -c -o $@ $<

menu_obj=$(addsuffix .qfo,$(basename $(menu_src)))

menu.dat$(GZ): $(menu_obj) $(QFCC_DEP)
	$(QFCC) $(QCFLAGS) -p $(STRIP) -o menu.dat $(menu_obj)
	$(GZIP)

EXTRA_DIST= $(menu_src) cbuf.h client_menu.h cmd.h controls_o.h cvar.h \
		draw.h file.h game.h inputline.h key.h menu.h options.h \
		options_util.h servlist.h string.h stringh.h
CLEANFILES= *.dat *.sym *.gz *.qfo
