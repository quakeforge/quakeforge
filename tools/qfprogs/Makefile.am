AUTOMAKE_OPTIONS= foreign

SUBDIRS=        include source

#EXTRA_DIST=qfcc.lsm.in

dist-zip: distdir
	-chmod -R a+r $(distdir)
	ZIP="-r9q" zip $(distdir).zip $(NOCONV_DIST)
	ZIP="-r9ql" zip $(distdir).zip $(distdir) -x $(NOCONV_DIST)
	-rm -rf $(distdir)

dist-bz2: distdir
	-chmod -R a+r $(distdir)
	BZIP2="-9" $(TAR) Ichof $(distdir).tar.bz2 $(distdir)
	-rm -rf $(distdir)

dist-all-local: distdir
	-chmod -R a+r $(distdir)
	GZIP=$(GZIP_ENV) $(TAR) chozf $(distdir).tar.gz $(distdir)
	BZIP2="-9" $(TAR) Ichof $(distdir).tar.bz2 $(distdir)
	ZIP="-r9q" zip $(distdir).zip $(NOCONV_DIST)
	ZIP="-r9ql" zip $(distdir).zip $(distdir) -x $(NOCONV_DIST)
	-rm -rf $(distdir)
