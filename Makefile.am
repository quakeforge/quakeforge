## Process this file with automake to produce Makefile.in
AUTOMAKE_OPTIONS= foreign

SUBDIRS=	include libs qw nq

EXTRA_DIST=	ChangeLog RPM/build_rpm.in \
		tools/gas2masm/Makefile tools/gas2masm/gas2masm.c \
		tools/gas2masm/gas2masm.dsp tools/gas2masm/gas2masm.dsw \
		tools/gas2masm/gas2masm.mak tools/gas2masm/gas2masm.mdp \
		tools/zpak

changelog::
	-touch ChangeLog
	-tools/cvs2cl/cvs2cl.pl -b --utc
	-rm -f ChangeLog.bak
