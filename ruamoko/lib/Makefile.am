AUTOMAKE_OPTIONS=	foreign

libdir=$(prefix)/lib/ruamoko

QFCC=$(top_builddir)/tools/qfcc/source/qfcc$(EXEEXT)
QCFLAGS=-qq -g -Werror
QCPPFLAGS=$(INCLUDES)
PAK=$(top_builddir)/tools/pak/pak$(EXEEXT)
RANLIB=touch

INCLUDES=	-I$(top_srcdir)/ruamoko/include -I$(top_srcdir)/include

ruamoko_libs=libr.a libqw.a libnq.a libcsqc.a
if BUILD_RUAMOKO
libs=$(ruamoko_libs)
else
libs=
endif

lib_LIBRARIES= $(libs)
EXTRA_LIBRARIES= $(ruamoko_libs)

%.qfo: %.r
	$(QFCC) $(QCFLAGS) $(QCPPFLAGS) -c -o $@ $<

%.o: %.r
	$(QFCC) $(QCFLAGS) $(QCPPFLAGS) -c -o $@ $<

libr_a_SOURCES=\
	cbuf.r cmd.r cvar.r file.r qfile.r string.r \
	Object.r AutoreleasePool.r Array.r Entity.r List.r ListNode.r Stack.r
libr_a_AR=$(PAK) -cf

libqw_a_SOURCES=\
	crudefile.r debug.r hash.r entities.r infokey.r math.r message.r \
	physics.r plist.r qw_message.r qw_physics.r qw_sys.r \
	server.r sound.r system.r
libqw_a_AR=$(PAK) -cf

libnq_a_SOURCES=\
	crudefile.r debug.r hash.r entities.r infokey.r math.r message.r \
	nq_message.r physics.r plist.r server.r sound.r system.r
libnq_a_AR=$(PAK) -cf

libcsqc_a_SOURCES= \
	draw.r gib.r key.r system.r
libcsqc_a_AR=	$(PAK) -cf

CLEANFILES=	*.qfo *.o
