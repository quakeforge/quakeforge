## Process this file with automake to produce Makefile.in
AUTOMAKE_OPTIONS= foreign

SUBDIRS=	include source debian doc

EXTRA_DIST=	README.WIN newtree.dsw ChangeLog \
		RPM/build_rpm.in RPM/quakeforge.spec.in \
		tools/gas2masm/Makefile tools/gas2masm/gas2masm.c \
		tools/gas2masm/gas2masm.dsp tools/gas2masm/gas2masm.dsw \
		tools/gas2masm/gas2masm.mak tools/gas2masm/gas2masm.mdp \
		tools/zpak

NOCONV_DIST=	$(distdir)/include/win32/resources/icon1.ico

changelog::
	-touch ChangeLog
	-tools/cvs2cl/cvs2cl.pl -b --utc
	-rm -f ChangeLog.bak

dist-zip: distdir
	-chmod -R a+r $(distdir)
	ZIP="-r9q" zip $(distdir).zip $(NOCONV_DIST)
	ZIP="-r9ql" zip $(distdir).zip $(distdir) -x $(NOCONV_DIST)
	-rm -rf $(distdir)

dist-bz2: distdir
	-chmod -R a+r $(distdir)
	$(TAR) -cho $(distdir) | bzip2 -9 > $(distdir).tar.bz2
	-rm -rf $(distdir)

dist-all-local: distdir
	-chmod -R a+r $(distdir)
	GZIP=$(GZIP_ENV) $(TAR) chozf $(distdir).tar.gz $(distdir)
	$(TAR) -cho $(distdir) | bzip2 -9 > $(distdir).tar.bz2
	ZIP="-r9q" zip $(distdir).zip $(NOCONV_DIST)
	ZIP="-r9ql" zip $(distdir).zip $(distdir) -x $(NOCONV_DIST)
	-rm -rf $(distdir)
