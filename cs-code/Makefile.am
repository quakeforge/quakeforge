## Process this file with automake to produce Makefile.in
AUTOMAKE_OPTIONS= foreign

#FIXME should qf data be installed somewhere other than id1 that gets
#searched after everything else?
pkgdatadir=@sharepath@/id1

QFCC=$(top_builddir)/tools/qfcc/source/qfcc
QCFLAGS=-g -Werror
QCPPFLAGS=-I. -I$(srcdir) -I$(top_builddir)/include -I$(top_srcdir)/include

pkgdata_DATA= menu.dat

menu_src= menu.qc menu_pics.qc menu_util.qc servlist.qc options.qc cbuf_def.qc cvar_def.qc draw_def.qc file_def.qc game_def.qc inputline_def.qc inputline_util.qc key_defs.qc menu_def.qc options_util.qc string_def.qc controls_o.qc stringh_def.qc

menu.dat: menu.src $(menu_src)
	$(QFCC) $(QCFLAGS) $(QCPPFLAGS) -P $<

EXTRA_DIST= $(menu_src)
CLEANFILES= *.dat *.sym
