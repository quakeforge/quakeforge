AUTOMAKE_OPTIONS=	foreign

pkglibdir=$(datarootdir)/qfcc/lib

QFCC=$(top_builddir)/tools/qfcc/source/qfcc$(EXEEXT)
QCFLAGS=-qq -g -Werror -Wall -Wno-integer-divide --no-default-paths
QCPPFLAGS=$(INCLUDES)
PAK=$(top_builddir)/tools/pak/pak$(EXEEXT)
RANLIB=touch

INCLUDES=	-I$(top_srcdir)/ruamoko/include -I$(top_srcdir)/include

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
