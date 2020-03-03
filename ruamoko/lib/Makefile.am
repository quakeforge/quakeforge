AUTOMAKE_OPTIONS=	foreign

pkglibdir=$(datarootdir)/qfcc/lib

QFCC=$(top_builddir)/tools/qfcc/source/qfcc$(EXEEXT)
QCFLAGS=-qq -O -g -Wall -Wno-integer-divide -Werror --no-default-paths
QCPPFLAGS=--no-default-paths -I$(top_srcdir)/ruamoko/include -I$(top_srcdir)/include
QCLINKFLAGS=--no-default-paths -L$(top_builddir)/ruamoko/lib
QCOMPILE=$(QFCC) $(QCFLAGS) $(QCPPFLAGS)
QLINK=$(QFCC) $(QCFLAGS) $(QCLINKFLAGS)
PAK=$(top_builddir)/tools/pak/pak$(EXEEXT)
RANLIB=touch

AM_CPPFLAGS=	-I$(top_srcdir)/ruamoko/include -I$(top_srcdir)/include

noinst_HEADERS= \
	Array+Private.h

ruamoko_libs=libr.a libqw.a libnq.a libcsqc.a
libs=$(ruamoko_libs)

pkglib_LIBRARIES= $(libs)
EXTRA_LIBRARIES= $(ruamoko_libs)

SUFFIXES= .o .r .qc
.r.o:
	$(QFCC) $(QCFLAGS) $(QCPPFLAGS) -c -o $@ $<
.qc.o:
	$(QFCC) $(QCFLAGS) $(QCPPFLAGS) -c -o $@ $<

libr_a_SOURCES=\
	cbuf.r cmd.r cvar.r hash.r msgbuf.r plist.r qfile.r qfs.r script.r \
	sound.r string.r math.r types.r \
	Object.r Protocol.r \
	AutoreleasePool.r Array.r Array+Private.r Entity.r PropertyList.r Set.r
libr_a_AR=$(PAK) -cf

libqw_a_SOURCES=\
	crudefile.r debug.r entities.r infokey.r math.r message.r \
	physics.r qw_message.r qw_physics.r qw_sys.r \
	server.r sv_sound.r system.r
libqw_a_AR=$(PAK) -cf

libnq_a_SOURCES=\
	crudefile.r debug.r entities.r infokey.r math.r message.r \
	nq_message.r physics.r server.r sv_sound.r system.r
libnq_a_AR=$(PAK) -cf

libcsqc_a_SOURCES= \
	debug.r draw.r gib.r key.r system.r
libcsqc_a_AR=	$(PAK) -cf

CLEANFILES=	*.qfo *.o
