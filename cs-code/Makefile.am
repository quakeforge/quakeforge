## Process this file with automake to produce Makefile.in
AUTOMAKE_OPTIONS= foreign

#FIXME should qf data be installed somewhere other than id1 that gets
#searched after everything else?
pkgdatadir=@sharepath@/id1

QFCC=$(top_builddir)/tools/qfcc/source/qfcc
QCFLAGS=-g -Werror
QCPPFLAGS=-I. -I$(srcdir) -I$(top_builddir)/include -I$(top_srcdir)/include
GZIP=if echo $@ | grep -q .gz; then gzip -fv `basename $@ .gz`; if test -f `basename $@ .dat.gz`.sym; then gzip -fv `basename $@ .dat.gz`.sym; fi; fi
if HAVE_ZLIB
GZ=.gz
else
GZ=
endif
STRIP=$(shell echo -n $(srcdir)/ | sed -e 's/[^/]//g' | wc -c)

pkgdata_DATA= menu.dat$(GZ)

menu_src= \
	cbuf_def.qc controls_o.qc cvar_def.qc draw_def.qc file_def.qc \
	game_def.qc inputline_def.qc inputline_util.qc key_defs.qc menu.qc \
	menu_def.qc menu_pics.qc menu_util.qc options.qc options_util.qc \
	servlist.qc string_def.qc stringh_def.qc

menu.dat$(GZ): menu.src $(menu_src)
	$(QFCC) $(QCFLAGS) $(QCPPFLAGS) -p $(STRIP) -P $<
	$(GZIP)

EXTRA_DIST= $(menu_src)
CLEANFILES= *.dat *.sym *.gz
