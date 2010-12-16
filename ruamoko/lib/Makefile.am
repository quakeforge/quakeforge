AUTOMAKE_OPTIONS=	foreign

pkglibdir=$(libdir)/ruamoko

QFCC=$(top_builddir)/tools/qfcc/source/qfcc$(EXEEXT)
QCFLAGS=-qq -g -Wall -Wno-integer-divide -Werror
QCPPFLAGS=$(INCLUDES)
PAK=$(top_builddir)/tools/pak/pak$(EXEEXT)
RANLIB=touch

INCLUDES=	-I$(top_srcdir)/ruamoko/include -I$(top_srcdir)/include

noinst_HEADERS= \
	Array+Private.h

ruamoko_libs=libr.a libqw.a libnq.a libcsqc.a
if BUILD_RUAMOKO
libs=$(ruamoko_libs)
else
libs=
endif

pkglib_LIBRARIES= $(libs)
EXTRA_LIBRARIES= $(ruamoko_libs)

%.qfo: %.r
	$(QFCC) $(QCFLAGS) $(QCPPFLAGS) -c -o $@ $<

%.o: %.r
	$(QFCC) $(QCFLAGS) $(QCPPFLAGS) -c -o $@ $<

libr_a_SOURCES=\
	cbuf.r cmd.r cvar.r file.r hash.r plist.r qfile.r qfs.r script.r sound.r \
	string.r \
	AutoreleasePool.r Array.r Array+Private.r Entity.r List.r ListNode.r Object.r \
	PropertyList.r Protocol.r Stack.r
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
	draw.r gib.r key.r system.r
libcsqc_a_AR=	$(PAK) -cf

CLEANFILES=	*.qfo *.o
