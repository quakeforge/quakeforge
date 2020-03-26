AUTOMAKE_OPTIONS=	foreign

QFCC_DEP=$(top_builddir)/tools/qfcc/source/qfcc$(EXEEXT)
QFCC=$(QFCC_DEP)

ruamoko_libs=libr.a libqw.a libnq.a libcsqc.a
libs=$(ruamoko_libs)

pkglibdir=$(datarootdir)/qfcc/lib

pkglib_LIBRARIES= $(libs)
EXTRA_LIBRARIES= $(ruamoko_libs)

noinst_HEADERS= \
	Array+Private.h

QCFLAGS=-qq -O -g -Wall -Wno-integer-divide -Werror
QCPPFLAGS=--no-default-paths -I$(top_srcdir)/ruamoko/include -I$(top_srcdir)/include
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

libr_a_SOURCES=\
	cbuf.r cmd.r cvar.r hash.r msgbuf.r plist.r qfile.r qfs.r script.r \
	sound.r string.r math.r types.r \
	obj_forward.r \
	Object.r Protocol.r \
	AutoreleasePool.r Array.r Array+Private.r Entity.r PropertyList.r Set.r
libr_a_obj=$(libr_a_SOURCES:.r=.o)
libr_a_dep=$(addprefix ./$(DEPDIR)/,$(libr_a_obj:.o=.Qo))
libr_a_AR=$(PAK) -cf
include $(libr_a_dep) # am--include-marker
r_depfiles_remade += $(libr_a_dep)

common_src=debug.r system.r legacy_string.r
server_src= \
	crudefile.r entities.r infokey.r message.r \
	physics.r server.r sv_sound.r
libqw_a_src= \
	qw_message.r qw_physics.r qw_sys.r
libnq_a_src= \
	nq_message.r
libcsqc_a_src= draw.r gib.r key.r

common_obj=$(common_src:.r=.o)
common_dep=$(addprefix ./$(DEPDIR)/,$(common_obj:.o=.Qo))
include $(common_dep) # am--include-marker
r_depfiles_remade += $(common_dep)

server_obj=$(server_src:.r=.o)
server_dep=$(addprefix ./$(DEPDIR)/,$(server_obj:.o=.Qo))
include $(server_dep) # am--include-marker
r_depfiles_remade += $(server_dep)

libqw_a_SOURCES=$(libqw_a_src) $(common_src) $(server_src) math.r
libqw_a_obj=$(libqw_a_src:.r=.o)
libqw_a_dep=$(addprefix ./$(DEPDIR)/,$(libqw_a_obj:.o=.Qo))
libqw_a_AR=$(PAK) -cf
include $(libqw_a_dep) # am--include-marker
r_depfiles_remade += $(libqw_a_dep)

libnq_a_SOURCES=$(libnq_a_src) $(common_src) $(server_src) math.r
libnq_a_obj=$(libnq_a_src:.r=.o)
libnq_a_dep=$(addprefix ./$(DEPDIR)/,$(libnq_a_obj:.o=.Qo))
libnq_a_AR=$(PAK) -cf
include $(libnq_a_dep) # am--include-marker
r_depfiles_remade += $(libnq_a_dep)

libcsqc_a_SOURCES=$(libcsqc_a_src) $(common_src)
libcsqc_a_obj=$(libcsqc_a_src:.r=.o)
libcsqc_a_dep=$(addprefix ./$(DEPDIR)/,$(libcsqc_a_obj:.o=.Qo))
libcsqc_a_AR=	$(PAK) -cf
include $(libcsqc_a_dep) # am--include-marker
r_depfiles_remade += $(libcsqc_a_dep)

$(r_depfiles_remade):
	$(MKDIR_P) $(@D)
	echo '# dummy' >$@-t && $(am__mv) $@-t $@

am--depfiles: $(am__depfiles_remade) $(r_depfiles_remade)

CLEANFILES=	*.qfo *.o
