AUTOMAKE_OPTIONS=	foreign

libdir=$(prefix)/lib/ruamoko

QFCC=$(top_builddir)/tools/qfcc/source/qfcc$(EXEEXT)
QCFLAGS=-qq -g -Werror
QCPPFLAGS=$(INCLUDES)
PAK=$(top_builddir)/tools/pak/pak$(EXEEXT)
RANLIB=touch

INCLUDES=	-I$(top_srcdir)/ruamoko/include -I$(top_srcdir)/include

ruamoko_libs=libr.a libgui.a libcsqc.a
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
	crudefile.r debug.r entities.r infokey.r math.r message.r nq_message.r \
	physics.r qw_message.r qw_physics.r qw_sys.r sound.r string.r system.r \
	Object.r Array.r Entity.r Point.r Size.r Rect.r
libr_a_AR=$(PAK) -cf

libgui_a_SOURCES=\
	draw.r InputLine.r key.r
libgui_a_AR=$(PAK) -cf

libcsqc_a_SOURCES= \
	cbuf.r cmd.r cvar.r file.r
libcsqc_a_AR=	$(PAK) -cf

CLEANFILES=	*.qfo *.o
