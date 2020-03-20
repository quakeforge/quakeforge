## Process this file with automake to produce Makefile.in
AUTOMAKE_OPTIONS= foreign no-exeext

QFCC_DEP=$(top_builddir)/tools/qfcc/source/qfcc$(EXEEXT)
QFCC=$(QFCC_DEP)

GZ=@progs_gz@
game_libexec=game.dat

pkglibexecdir=@sharepath@/QF

# this will eventually go into pkglibexecdir, but for now...
noinst_PROGRAMS= $(game_libexec)
EXTRA_PROGRAMS= $(game_libexec)

QCFLAGS=-qq -O -g -Werror -Wall -Wno-integer-divide
QCPPFLAGS=--no-default-paths -I$(top_srcdir)/ruamoko/include
QCLINKFLAGS=--no-default-paths -L$(top_builddir)/ruamoko/lib
QCOMPILE=$(QFCC) $(QCFLAGS) $(QCPPFLAGS)
QLINK=$(QFCC) $(QCFLAGS) $(QCLINKFLAGS)

MKDIR_P = @MKDIR_P@
am__mv = mv -f

GZIP=if echo $@ | grep -q .gz; then gzip -f `basename $@ .gz`; if test -f `basename $@ .dat.gz`.sym; then gzip -f `basename $@ .dat.gz`.sym; fi; fi
# BSD make can't handle $(shell foo) directives, and GNU make can't handle |=
# so we have to bite the bullet and pass this to the shell every time.
STRIP=`echo -n $(srcdir)/ | sed -e 's/[^/]//g' | wc -c`

SUFFIXES=.o .r
.r.o:
	$(QCOMPILE) -MT $@ -MD -MP -MF $(DEPDIR)/$*.Tqo -p $(STRIP) -c -o $@ $<
	sed -i -e '1s@:@: $(QFCC_DEP)@' $(DEPDIR)/$*.Tqo
	$(am__mv) $(DEPDIR)/$*.Tqo $(DEPDIR)/$*.Qo

game_src= Axe.r GameEntity.r World.r tempent.r

r_depfiles_remade=

game_dat_SOURCES=$(game_src)
game_obj=$(game_src:.r=.o)
game_dep=$(addprefix ./$(DEPDIR)/,$(game_obj:.o=.Qo))
game.dat: $(game_obj) $(QFCC_DEP) ../lib/libr.a ../lib/libqw.a
	$(QLINK) -p $(STRIP) -o game.dat $(game_obj) ../lib/libr.a ../lib/libqw.a ../lib/libr.a
include $(game_dep) # am--include-marker
r_depfiles_remade += $(game_dep)

game.sym: game.dat

$(r_depfiles_remade):
	$(MKDIR_P) $(@D)
	echo '# dummy' >$@-t && $(am__mv) $@-t $@

am--depfiles: $(am__depfiles_remade) $(r_depfiles_remade)

EXTRA_DIST= $(game_src) Axe.h GameEntity.h tempent.h Weapon.h World.h
CLEANFILES= *.dat *.sym *.gz *.o *.qfo
