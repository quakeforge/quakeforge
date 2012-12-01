## Process this file with automake to produce Makefile.in
AUTOMAKE_OPTIONS= foreign

#FIXME should qf data be installed somewhere other than id1 that gets
#searched after everything else?
pkgdatadir=@sharepath@/id1

QFCC_DEP=$(top_builddir)/tools/qfcc/source/qfcc$(EXEEXT)
QFCC=$(QFCC_DEP)
QCFLAGS=-qq -O -g -Werror -Wall -Wno-integer-divide --no-default-paths
QCPPFLAGS=-I. -I$(srcdir) -I$(top_builddir)/ruamoko/include -I$(top_srcdir)/ruamoko/include
GZIP=if echo $@ | grep -q .gz; then gzip -f `basename $@ .gz`; if test -f `basename $@ .dat.gz`.sym; then gzip -f `basename $@ .dat.gz`.sym; fi; fi
GZ=@progs_gz@
# BSD make can't handle $(shell foo) directives, and GNU make can't handle |=
# so we have to bite the bullet and pass this to the shell every time.
STRIP=`echo -n $(srcdir)/ | sed -e 's/[^/]//g' | wc -c`

data=game.dat$(GZ)

noinst_DATA= $(data)
EXTRA_DATA= game.dat

game_src= Axe.r GameEntity.r World.r tempent.r

SUFFIXES=.qfo .r
.r.qfo:
	$(QFCC) $(QCFLAGS) $(QCPPFLAGS) -p $(STRIP) -c -o $@ $<

game_obj=$(game_src:.r=.qfo)
game.dat$(GZ): $(game_obj) ../lib/libr.a ../lib/libqw.a
	$(QFCC) $(QCFLAGS) -p $(STRIP) -o game.dat $(game_obj) ../lib/libr.a ../lib/libqw.a ../lib/libr.a
	$(GZIP)

EXTRA_DIST= $(game_src) Axe.h GameEntity.h tempent.h Weapon.h World.h
CLEANFILES= *.dat *.sym *.gz *.qfo
