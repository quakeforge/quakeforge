## Process this file with automake to produce Makefile.in

ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST = \
		ChangeLog configure.ac bootstrap \
		$(top_srcdir)/.version \
		config.d/git-version-gen \
		tools/cross/droid/cross-configure.sh \
		tools/cross/droid/cross-make.sh \
		tools/cross/mingw/cross-configure.sh \
		tools/cross/mingw/cross-make.sh \
		tools/cross/mingw64/cross-configure.sh \
		tools/cross/mingw64/cross-make.sh \
		tools/cross/mipsel-linux/cross-configure.sh \
		tools/cross/mipsel-linux/cross-make.sh \
		tools/cross/ps3-elf/cross-configure.sh \
		tools/cross/ps3-elf/cross-make.sh \
		tools/gas2masm/Makefile tools/gas2masm/gas2masm.c \
		tools/gas2masm/gas2masm.dsp tools/gas2masm/gas2masm.dsw \
		tools/gas2masm/gas2masm.mak tools/gas2masm/gas2masm.mdp

NOCONV_DIST= \
	$(distdir)/include/win32/resources/icon1Vista.ico \
	$(distdir)/include/win32/resources/icon1XP.ico

BUILT_SOURCES = $(top_srcdir)/.version
AM_CFLAGS= $(TRACY_CFLAGS) -fexceptions -include qftracy.h
AM_CXXFLAGS= $(TRACY_CFLAGS) -include qftracy.h
AM_CPPFLAGS= -I$(top_srcdir)/include $(UNWIND_CFLAGS) $(PTHREAD_CFLAGS) $(FNM_FLAGS) $(NCURSES_CFLAGS) $(FREETYPE_CFLAGS) $(HARFBUZZ_CFLAGS) $(VULKAN_CPPFLAGS) $(LIBCURL_CFLAGS)

common_ldflags= -export-dynamic @STATIC@ @PTHREAD_LDFLAGS@

SUFFICES =
TESTS =
XFAIL_TESTS =
bin_PROGRAMS =
bin_SCRIPTS =
check_PROGRAMS =
lib_LTLIBRARIES =
man_MANS =
noinst_LTLIBRARIES =
noinst_LIBRARIES =
noinst_PROGRAMS =
noinst_HEADERS =
noinst_PYTHON =
plugin_LTLIBRARIES =

RANLIB=touch
ARFLAGS=cr

EXTRA_HEADERS =
EXTRA_LTLIBRARIES =
EXTRA_PROGRAMS =
EXTRA_LIBRARIES =

CLEANFILES =
DISTCLEANFILES =

YFLAGS = -v -d -Wno-yacc -Werror
PTHREAD_LDFLAGS=@PTHREAD_LDFLAGS@
PTHREAD_CFLAGS=@PTHREAD_CFLAGS@

if HAVE_TRACY
tracy_src=tools/tracy/qftracy.cc tools/tracy/qftracy_mem.c
tracyvk_src=tools/tracy/qftracyvk.cc
else
tracy_src=
tracyvk_src=
endif

lib_ldflags=-version-info $(QUAKE_LIBRARY_VERSION_INFO) \
			-rpath $(libdir) -no-undefined
plugin_ldflags= @plugin_ldflags@ -avoid-version -module -rpath $(plugindir)
plugin_libadd= @plugin_libadd@

SDL_LIBS= @SDL_LIBS@
XMMS_LIBS= @XMMS_LIBS@

PAK=$(top_builddir)/pak$(EXEEXT)
QFCC_DEP=qfcc$(EXEEXT)
QFCC=$(top_builddir)/$(QFCC_DEP)
QWAQ=$(top_builddir)/ruamoko/qwaq/qwaq-cmd$(EXEEXT)

GZ=@progs_gz@

V_QFCC = $(V_QFCC_@AM_V@)
V_QFCC_ = $(V_QFCC_@AM_DEFAULT_V@)
V_QFCC_0 = @echo "  QFCC    " $@;
V_QFCC_1 =

V_QFCCLD = $(V_QFCCLD_@AM_V@)
V_QFCCLD_ = $(V_QFCCLD_@AM_DEFAULT_V@)
V_QFCCLD_0 = @echo "  QFCCLD  " $@;
V_QFCCLD_1 =

QCSYSTEM=--no-default-paths -I$(top_srcdir) -I$(top_srcdir)/ruamoko/include -I$(top_srcdir)/include
QCFLAGS=-qq -O -g -Werror -Wall -Wno-integer-divide
QCPPFLAGS=$(QCSYSTEM)
QCLINKFLAGS=--no-default-paths -Lruamoko/lib
QCOMPILE=$(QFCC) $(QCFLAGS) $(QCPPFLAGS)
QLINK=$(QFCC) $(QCFLAGS) $(QCLINKFLAGS)
MKDIR_P = @MKDIR_P@

qfcc_compile=\
	$(V_QFCC)depbase=`echo $@ | sed 's|[^/]*$$|$(DEPDIR)/&|;s|\.o$$||'`;\
	$(QCOMPILE) $(1) -MT $@ -MD -MP -MF $$depbase.Tqo -c -o $@ $< &&\
	sed -i -e '1s@:@: $(QFCC_DEP)@' $$depbase.Tqo &&\
	$(am__mv) $$depbase.Tqo $$depbase.Qo

am__mv = mv -f

SUFFIXES=.o .r .pas
%_v6p.o: %.r
	$(call qfcc_compile,-Ctarget=v6p)
%_v6.o: %.r
	$(call qfcc_compile,-Ctarget=v6)
.r.o:
	$(qfcc_compile)
.pas.o:
	$(qfcc_compile)

qcautodep = $(join $(addsuffix $(DEPDIR)/,$(dir $(basename $(1)))),$(addsuffix $(2).Qo,$(notdir $(basename $(basename $(1))))))
r_depfiles_remade=
pas_depfiles_remade=

V_PY = $(V_PY_@AM_V@)
V_PY_ = $(V_PY_@AM_DEFAULT_V@)
V_PY_0 = @echo "  PY      " $@;
V_PY_1 =

V_GLSLANG = $(V_GLSLANG_@AM_V@)
V_GLSLANG_ = $(V_GLSLANG_@AM_DEFAULT_V@)
V_GLSLANG_0 = @echo "  GLSLANG " $@;
V_GLSLANG_1 =

V_XXD = $(V_XXD_@AM_V@)
V_XXD_ = $(V_XXD_@AM_DEFAULT_V@)
V_XXD_0 = @echo "  XXD     " $@;
V_XXD_1 =

V_SED = $(V_SED_@AM_V@)
V_SED_ = $(V_SED_@AM_DEFAULT_V@)
V_SED_0 = @echo "  SED     " $@;
V_SED_1 =

SUFFICES += .plist .plc
.plist.plc:
	$(V_SED)sed -e 's/\\/\\\\/g' -e 's/"/\\"/g' -e 's/^/"/' -e 's/$$/\\n"/' $< > $@.t &&\
	$(am__mv) $@.t $@

%.spv: %
	@$(mkdir_p) $(builddir)/`dirname $@`
	$(V_GLSLANG)(((($(GLSLANGVALIDATOR) -V $< -o $@; echo $$? >&3) | sed -e '1d' 1>&2) 3>&1) | (read xs; exit $$xs))

%.spvc: %
	@$(mkdir_p) $(builddir)/`dirname $@`
	$(V_GLSLANG)(((($(GLSLANGVALIDATOR) --vn `basename $< | tr . _` -V $< -o $@; echo $$? >&3) | sed -e '1d' 1>&2) 3>&1) | (read xs; exit $$xs))

sharepath = @sharepath@
shaderdir = @shaderdir@
shader_DATA =

include doc/Makemodule.am
include RPM/Makemodule.am
include debian/Makemodule.am
include desktop/Makemodule.am
include pkg-config/Makemodule.am
include include/Makemodule.am
include libs/Makemodule.am
include hw/Makemodule.am
include nq/Makemodule.am
include qtv/Makemodule.am
include qw/Makemodule.am
include tools/Makemodule.am
include ruamoko/Makemodule.am

DISTCLEANFILES += $(r_depfiles_remade) $(pas_depfiles_remade)
CLEANFILES += $(shader_DATA)

$(r_depfiles_remade):
	$(MKDIR_P) $(@D)
	echo '$@' | sed -e 's@\$(DEPDIR)/@@' -e 's@\(.*\)\.Qo$$@\1.o: $(top_srcdir)/\1.r $(QFCC)@' >$@-t && $(am__mv) $@-t $@

$(pas_depfiles_remade):
	$(MKDIR_P) $(@D)
	echo '$@' | sed -e 's@\$(DEPDIR)/@@' -e 's@\(.*\)\.Qo$$@\1.o: $(top_srcdir)/\1.pas $(QFCC)@' >$@-t && $(am__mv) $@-t $@

am--depfiles: $(am__depfiles_remade) $(r_depfiles_remade) $(pas_depfiles_remade)
	echo findme $(ruamoko_gui_libgui_a_dep)

changelog: ChangeLog
ChangeLog: FORCE
	@if test -d "$(top_srcdir)/.git"; then \
		echo "creating ChangeLog" && \
		(	cd "$(top_srcdir)" && \
			echo 'This file is generated by Makefile; do not edit.'; \
			echo; \
			./missing --run git log --abbrev-commit --stat --no-merges \
		) > $(top_builddir)/ChangeLog; \
	else \
		echo 'A git clone is required to generate ChangeLog' >&2; \
	fi

dist-zip: distdir
	-chmod -R a+r $(distdir)
	ZIP="-r9q" zip $(distdir).zip $(NOCONV_DIST)
	-rm -f $(NOCONV_DIST)
	ZIP="-r9ql" zip $(distdir).zip $(distdir)
	-rm -rf $(distdir)

dist-bz2: distdir
	-chmod -R a+r $(distdir)
	$(AMTAR) -cho $(distdir) | bzip2 -9 > $(distdir).tar.bz2
	-rm -rf $(distdir)

dist-all-local: distdir
	-chmod -R a+r $(distdir)
	GZIP=$(GZIP_ENV) $(AMTAR) chozf $(distdir).tar.gz $(distdir)
	$(AMTAR) -cho $(distdir) | bzip2 -9 > $(distdir).tar.bz2
	ZIP="-r9q" zip $(distdir).zip $(NOCONV_DIST)
	-rm -f $(NOCONV_DIST)
	ZIP="-r9ql" zip $(distdir).zip $(distdir)
	-rm -rf $(distdir)

$(top_srcdir)/.version:
	echo $(VERSION) > $@-t && mv $@-t $@
dist-hook:
	echo $(VERSION) > $(distdir)/.tarball-version

FORCE:
