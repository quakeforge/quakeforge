## Process this file with automake to produce Makefile.in
AUTOMAKE_OPTIONS= foreign

#FIXME should qf data be installed somewhere other than id1 that gets
#searched after everything else?
pkgdatadir=@sharepath@/id1

QFCC=$(top_builddir)/tools/qfcc/source/qfcc$(EXEEXT)
QCFLAGS=-qq -g -Werror
QCPPFLAGS=-I. -I$(srcdir) -I$(top_builddir)/ruamoko/include -I$(top_srcdir)/ruamoko/include
STRIP=$(shell echo `echo -n $(srcdir)/ | sed -e 's/[^/]//g' | wc -c`)

pkgdata_DATA= game.dat

game_src= Axe.r GameEntity.r World.r tempent.r

%.qfo: %.r
	$(QFCC) $(QCFLAGS) $(QCPPFLAGS) -p $(STRIP) -c -o $@ $<

%.qfo: %.qc
	$(QFCC) $(QCFLAGS) $(QCPPFLAGS) -p $(STRIP) -c -o $@ $<

game_obj=$(addsuffix .qfo,$(basename $(game_src)))

game.dat: $(game_obj) ../lib/libr.a
	$(QFCC) $(QCFLAGS) -p $(STRIP) -o game.dat $(game_obj) -l../lib/libr.a

EXTRA_DIST= $(game_src) Axe.h GameEntity.h tempent.h Weapon.h World.h
CLEANFILES= *.dat *.sym *.gz *.qfo
