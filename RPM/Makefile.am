## Process this file with automake to produce Makefile.in
AUTOMAKE_OPTIONS=	foreign

EXTRA_DIST=	build_rpm.in quakeforge.conf.in quakeforge.spec.in rpmrc

rpm: build_rpm quakeforge.conf quakeforge.spec rpmrc
	./build_rpm

CLEANFILES = *.rpm
