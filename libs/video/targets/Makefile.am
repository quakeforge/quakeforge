INCLUDES= -I$(top_srcdir)/include

lib_LTLIBRARIES =	libQFjs.la $(VID_TARGETS)

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

# Linux FBdev
if $(HAVE_FBDEV)
	YFLAGS = -d
	libQFfbdev_la_LDFLAGS=	-version-info 1:0:0
	libQFfbdev_la_SOURCES=	fbset.c fbset_modes_y.y fbset_modes_l.l \
				in_fbdev.c vid.c vid_fbdev.c
	YACCLEX_CLEANFILES=	fbset_modes_y.c fbset_modes_y.h \
				fbset_modes_y.tab.h fbset_modes_l.c
	EXTRA_libQFfbdev_la_SOURCES=fbset_modes_y.h

	fbset_modes_y.lo: $(srcdir)/fbset_modes_y.c
		$(COMPILE) -Wno-error -c $<
	fbset_modes_l.lo: $(srcdir)/fbset_modes_l.c
		$(COMPILE) -Wno-error -c $<
endif

if $(HAVE_MGL)
	libQFmgl_la_LDFLAGS=	-version-info 1:0:0
	libQFmgl_la_SOURCES=	in_win.c vid.c vid_common_sw.c vid_mgl.c
endif

if $(HAVE_SDL)
	libQFsdl_la_LDFLAGS=	-version-info 1:0:0
	libQFsdl_la_SOURCES=	in_sdl.c vid.c vid_common_sw.c vid_sdl.c
endif

if $(HAVE_SVGA)
	libQFsvga_la_LDFLAGS=	-version-info 1:0:0
	libQFsvga_la_SOURCES=	d_copy.S in_svgalib.c vid.c vid_common_sw.c \
				vid_svgalib.c
endif

if $(HAVE_X11)
	libQFx11_la_LDFLAGS=	-version-info 1:0:0
	libQFx11_la_SOURCES=	in_x11.c vid.c context_x11.c dga_check.c \
				vid_common_sw.c vid_x11.c

if $(HAVE_GLX)
	libQFglx_la_LDFLAGS=	-version-info 1:0:0
	libQFglx_la_SOURCES=	in_x11.c vid.c context_x11.c dga_check.c \
				vid_common_gl.c vid_glx.c
endif

if $(HAVE_SGL)
	libQFsgl_la_LDFLAGS=	-version-info 1:0:0
	libQFsgl_la_SOURCES=	in_sdl.c vid.c vid_common_gl.c vid_sgl.c
endif

if $(HAVE_TDFX)
	libQFtdfx_la_LDFLAGS=	-version-info 1:0:0
	libQFtdfx_la_SOURCES=	in_svgalib.c vid.c vid_common_gl.c vid_3dfxsvga.c
endif

if $(HAVE_WGL)
	libQFwgl_la_LDFLAGS=	-version-info 1:0:0
	libQFwgl_la_SOURCES=	in_win.c vid.c vid_wgl.c
endif

LIBLIST =	$(lib_LTLIBRARIES) @LIBRARY_SEARCH_PATH@

# Kill the temp files, hopefully.
CLEANFILES  = *.i *.s $(YACCLEX_CLEANFILES)
