## Process this file with automake to produce Makefile.in
AUTOMAKE_OPTIONS= foreign

SUBDIRS=	include source

NOCONV_DIST=	$(distdir)/include/win32/resources/icon1.ico

changelog::
	-touch $(top_srcdir)/ChangeLog
	-$(top_srcdir)/tools/cvs2cl/cvs2cl.pl -b --utc
	-rm -f $(top_srcdir)/ChangeLog.bak

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
