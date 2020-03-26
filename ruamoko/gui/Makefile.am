AUTOMAKE_OPTIONS=	foreign

QFCC_DEP=$(top_builddir)/tools/qfcc/source/qfcc$(EXEEXT)
QFCC=$(QFCC_DEP)

gui_libs=libgui.a
libs=$(gui_libs)

pkglibdir=$(datarootdir)/qfcc/lib

pkglib_LIBRARIES= $(libs)
EXTRA_LIBRARIES= $(gui_libs)

QFCC=$(top_builddir)/tools/qfcc/source/qfcc$(EXEEXT)
QCFLAGS=-qq -O -g -Werror -Wall -Wno-integer-divide --no-default-paths
QCPPFLAGS=--no-default-paths -I$(top_srcdir)/ruamoko/include
QCLINKFLAGS=--no-default-paths -L$(top_builddir)/ruamoko/lib
QCOMPILE=$(QFCC) $(QCFLAGS) $(QCPPFLAGS)
QLINK=$(QFCC) $(QCFLAGS) $(QCLINKFLAGS)

MKDIR_P = @MKDIR_P@
am__mv = mv -f

PAK=$(top_builddir)/tools/pak/pak$(EXEEXT)
RANLIB=touch

SUFFIXES= .o .r
.r.o:
	$(QCOMPILE) -MT $@ -MD -MP -MF $(DEPDIR)/$*.Tqo -p $(STRIP) -c -o $@ $<
	@sed -i -e '1s@:@: $(QFCC_DEP)@' $(DEPDIR)/$*.Tqo
	@$(am__mv) $(DEPDIR)/$*.Tqo $(DEPDIR)/$*.Qo

r_depfiles_remade=

libgui_a_SOURCES= \
	Group.r InputLine.r Pic.r Point.r Rect.r Size.r Slider.r Text.r View.r
libgui_a_obj=$(libgui_a_SOURCES:.r=.o)
libgui_a_dep=$(addprefix ./$(DEPDIR)/,$(libgui_a_obj:.o=.Qo))
libgui_a_AR=	$(PAK) -cf
include $(libgui_a_dep) # am--include-marker
r_depfiles_remade += $(libgui_a_dep)

$(r_depfiles_remade):
	$(MKDIR_P) $(@D)
	echo '# dummy' >$@-t && $(am__mv) $@-t $@

am--depfiles: $(am__depfiles_remade) $(r_depfiles_remade)


CLEANFILES=	*.qfo *.o
