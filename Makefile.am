## Process this file with automake to produce Makefile.in
AUTOMAKE_OPTIONS= foreign

SUBDIRS=	RPM debian doc include libs hw nq qtv qw tools ruamoko

## configure.ac needs to be listed here for older autoconfs
EXTRA_DIST=	ChangeLog configure.ac \
		tools/cross/cross-configure.sh tools/cross/cross-make.sh \
		tools/cross/cross.sh \
		tools/gas2masm/Makefile tools/gas2masm/gas2masm.c \
		tools/gas2masm/gas2masm.dsp tools/gas2masm/gas2masm.dsw \
		tools/gas2masm/gas2masm.mak tools/gas2masm/gas2masm.mdp

changelog::
	( cd $(top_srcdir); \
	tools/cvs2cl/cvs2cl.pl --stdout -b --utc -S --no-wrap) > ChangeLog

NOCONV_DIST=	$(distdir)/include/win32/resources/icon1.ico

dist-zip: distdir
	-chmod -R a+r $(distdir)
	ZIP="-r9q" zip $(distdir).zip $(NOCONV_DIST)
	ZIP="-r9ql" zip $(distdir).zip $(distdir) -x $(NOCONV_DIST)
	-rm -rf $(distdir)

dist-bz2: distdir
	-chmod -R a+r $(distdir)
	$(AMTAR) -cho $(distdir) | bzip2 -9 > $(distdir).tar.bz2
	-rm -rf $(distdir)

dist-all-local: distdir
	-chmod -R a+r $(distdir)
	GZIP=$(GZIP_ENV) $(AMTAR) chozf $(distdir).tar.gz $(distdir)
	$(AMTAR) -cho $(distdir) | bzip2 -9 > $(distdir).tar.bz2
	ZIP="-r9q" zip $(distdir).zip $(NOCONV_DIST)
	ZIP="-r9ql" zip $(distdir).zip $(distdir) -x $(NOCONV_DIST)
	-rm -rf $(distdir)
