INCLUDES= -I$(top_srcdir)/include $(GGI_CFLAGS) $(MGL_CFLAGS) $(SDL_CFLAGS) $(SVGA_CFLAGS) $(X_CFLAGS) $(GLX_CFLAGS) $(TDFXGL_CFLAGS)

lib_LTLIBRARIES =	@VID_TARGETS@
EXTRA_LTLIBRARIES = libQFjs.la libQFfbdev.la libQFsvga.la libQFx11.la libQFglx.la libQFsdl.la libQFsgl.la libQFtdfx.la libQFwgl.la libQFmgl.la

libQFjs_la_LDFLAGS=	-version-info 1:0:0

if JOYTYPE_LINUX
libQFjs_la_SOURCES=      joy.c joy_linux.c
endif
if JOYTYPE_WIN32
libQFjs_la_SOURCES=      joy.c joy_win.c
endif
if JOYTYPE_NULL
libQFjs_la_SOURCES=      joy.c joy_null.c
endif
libQFjs_la_CFLAGS=       $(JOY_CFLAGS)
libQFjs.la: $(libQFjs_la_OBJECTS) $(libQFjs_la_DEPENDENCIES)
	$(LINK) -rpath $(libdir) $(libQFjs_la_LDFLAGS) $(libQFjs_la_OBJECTS) $(libQFjs_la_LIBADD) $(LIBS)

# Linux FBdev
YFLAGS = -d
YACCLEX_CLEANFILES=	fbset_modes_y.c fbset_modes_y.h \
			fbset_modes_y.tab.h fbset_modes_l.c
libQFfbdev_la_LDFLAGS=	-version-info 1:0:0
libQFfbdev_la_SOURCES=	fbset.c fbset_modes_y.y fbset_modes_l.l \
			in_common.c in_fbdev.c vid.c vid_common_sw.c vid_fbdev.c
EXTRA_libQFfbdev_la_SOURCES=fbset_modes_y.h
libQFfbdev.la: $(libQFfbdev_la_OBJECTS) $(libQFfbdev_la_DEPENDENCIES)
	$(LINK) -rpath $(libdir) $(libQFfbdev_la_LDFLAGS) $(libQFfbdev_la_OBJECTS) $(libQFfbdev_la_LIBADD) $(LIBS)
fbset_modes_y.lo: $(srcdir)/fbset_modes_y.c
	$(LTCOMPILE) -Wno-error -c $<
fbset_modes_l.lo: $(srcdir)/fbset_modes_l.c
	$(LTCOMPILE) -Wno-error -c $<

libQFglx_la_LDFLAGS=	-version-info 1:0:0 $(GLX_LIBS) $(X_PRE_LIBS) $(VIDMODE_LIBS) $(DGA_LIBS) $(X_LIBS) -lX11 $(X_EXTRA_LIBS)
libQFglx_la_SOURCES=	in_common.c in_x11.c vid.c context_x11.c dga_check.c \
			qfgl_ext.c vid_common_gl.c vid_glx.c
libQFglx.la: $(libQFglx_la_OBJECTS) $(libQFglx_la_DEPENDENCIES)
	$(LINK) -rpath $(libdir) $(libQFglx_la_LDFLAGS) $(libQFglx_la_OBJECTS) $(libQFglx_la_LIBADD) $(LIBS)


libQFmgl_la_LDFLAGS=	-version-info 1:0:0
libQFmgl_la_SOURCES=	in_common.c in_win.c vid.c vid_common_sw.c vid_mgl.c
libQFmgl.la: $(libQFmgl_la_OBJECTS) $(libQFmgl_la_DEPENDENCIES)
	$(LINK) -rpath $(libdir) $(libQFmgl_la_LDFLAGS) $(libQFmgl_la_OBJECTS) $(libQFmgl_la_LIBADD) $(LIBS)


libQFsdl_la_LDFLAGS=	-version-info 1:0:0
libQFsdl_la_SOURCES=	in_common.c in_sdl.c vid.c vid_common_sw.c vid_sdl.c
libQFsdl.la: $(libQFsdl_la_OBJECTS) $(libQFsdl_la_DEPENDENCIES)
	$(LINK) -rpath $(libdir) $(libQFsdl_la_LDFLAGS) $(libQFsdl_la_OBJECTS) $(libQFsdl_la_LIBADD) $(LIBS)


libQFsgl_la_LDFLAGS=	-version-info 1:0:0
libQFsgl_la_SOURCES=	in_common.c in_sdl.c qfgl_ext.c vid.c vid_common_gl.c vid_sgl.c
libQFsgl.la: $(libQFsgl_la_OBJECTS) $(libQFsgl_la_DEPENDENCIES)
	$(LINK) -rpath $(libdir) $(libQFsgl_la_LDFLAGS) $(libQFsgl_la_OBJECTS) $(libQFsgl_la_LIBADD) $(LIBS)


libQFsvga_la_LDFLAGS=	-version-info 1:0:0
libQFsvga_la_SOURCES=	d_copy.S in_common.c in_svgalib.c vid.c vid_common_sw.c \
			vid_svgalib.c
libQFsvga.la: $(libQFsvga_la_OBJECTS) $(libQFsvga_la_DEPENDENCIES)
	$(LINK) -rpath $(libdir) $(libQFsvga_la_LDFLAGS) $(libQFsvga_la_OBJECTS) $(libQFsvga_la_LIBADD) $(LIBS)


libQFtdfx_la_LDFLAGS=	-version-info 1:0:0
libQFtdfx_la_SOURCES=	in_common.c in_svgalib.c qfgl_ext.c vid.c vid_common_gl.c vid_3dfxsvga.c
libQFtdfx.la: $(libQFtdfx_la_OBJECTS) $(libQFtdfx_la_DEPENDENCIES)
	$(LINK) -rpath $(libdir) $(libQFtdfx_la_LDFLAGS) $(libQFtdfx_la_OBJECTS) $(libQFtdfx_la_LIBADD) $(LIBS)


libQFwgl_la_LDFLAGS=	-version-info 1:0:0
libQFwgl_la_SOURCES=	in_common.c in_win.c qfgl_ext.c vid.c vid_common_gl.c vid_wgl.c
libQFwgl.la: $(libQFwgl_la_OBJECTS) $(libQFwgl_la_DEPENDENCIES)
	$(LINK) -rpath $(libdir) $(libQFwgl_la_LDFLAGS) $(libQFwgl_la_OBJECTS) $(libQFwgl_la_LIBADD) $(LIBS)


libQFx11_la_LDFLAGS=	-version-info 1:0:0 $(X_PRE_LIBS) $(VIDMODE_LIBS) $(DGA_LIBS) $(X_LIBS) -lX11 $(X_EXTRA_LIBS) $(X_SHM_LIB)
libQFx11_la_SOURCES=	in_common.c in_x11.c vid.c context_x11.c dga_check.c \
			vid_common_sw.c vid_x11.c
libQFx11.la: $(libQFx11_la_OBJECTS) $(libQFx11_la_DEPENDENCIES)
	$(LINK) -rpath $(libdir) $(libQFx11_la_LDFLAGS) $(libQFx11_la_OBJECTS) $(libQFx11_la_LIBADD) $(LIBS)


LIBLIST =	$(lib_LTLIBRARIES) @LIBRARY_SEARCH_PATH@

# Kill the temp files, hopefully.
CLEANFILES  = *.i *.s $(YACCLEX_CLEANFILES)
