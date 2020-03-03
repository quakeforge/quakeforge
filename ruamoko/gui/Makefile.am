AUTOMAKE_OPTIONS=	foreign

pkglibdir=$(datarootdir)/qfcc/lib

QFCC=$(top_builddir)/tools/qfcc/source/qfcc$(EXEEXT)
QCFLAGS=-qq -O -g -Werror -Wall -Wno-integer-divide --no-default-paths
QCPPFLAGS=--no-default-paths -I$(top_srcdir)/ruamoko/include
QCLINKFLAGS=--no-default-paths -L$(top_builddir)/ruamoko/lib
QCOMPILE=$(QFCC) $(QCFLAGS) $(QCPPFLAGS)
QLINK=$(QFCC) $(QCFLAGS) $(QCLINKFLAGS)
PAK=$(top_builddir)/tools/pak/pak$(EXEEXT)
RANLIB=touch

AM_CPPFLAGS=	-I$(top_srcdir)/ruamoko/include -I$(top_srcdir)/include

gui_libs=libgui.a
libs=$(gui_libs)

pkglib_LIBRARIES= $(libs)
EXTRA_LIBRARIES= $(gui_libs)

SUFFIXES= .qfo .r .qc
.r.o:
	$(QFCC) $(QCFLAGS) $(QCPPFLAGS) -c -o $@ $<

libgui_a_SOURCES= \
	Group.r InputLine.r Pic.r Point.r Rect.r Size.r Slider.r Text.r View.r
libgui_a_AR=	$(PAK) -cf

CLEANFILES=	*.qfo *.o
