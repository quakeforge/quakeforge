AUTOMAKE_OPTIONS=	foreign

libdir=$(prefix)/lib/ruamoko

QFCC=$(top_builddir)/tools/qfcc/source/qfcc$(EXEEXT)
QCFLAGS=-qq -g -Werror
QCPPFLAGS=$(INCLUDES)
PAK=$(top_builddir)/tools/pak/pak$(EXEEXT)
RANLIB=touch

INCLUDES=	-I$(top_srcdir)/ruamoko/include -I$(top_srcdir)/include

gui_libs=libgui.a
if BUILD_RUAMOKO
libs=$(gui_libs)
else
libs=
endif

lib_LIBRARIES= $(libs)
EXTRA_LIBRARIES= $(gui_libs)

%.qfo: %.r
	$(QFCC) $(QCFLAGS) $(QCPPFLAGS) -c -o $@ $<

%.o: %.r
	$(QFCC) $(QCFLAGS) $(QCPPFLAGS) -c -o $@ $<

libgui_a_SOURCES= \
	Group.r InputLine.r Point.r Rect.r Size.r Slider.r Text.r View.r
libgui_a_AR=	$(PAK) -cf

CLEANFILES=	*.qfo *.o
