INCLUDES= -I$(top_srcdir)/include

lib_LTLIBRARIES =	libQFjs.la libQFfbdev.la libQFmgl.la libQFsdl.la \
			libQFsvga.la libQFx11.la libQFglx.la libQFsgl.la \
			libQFtdfx.la libQFwgl.la

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
YFLAGS = -d
libQFfbdev_la_LDFLAGS=	-version-info 1:0:0
libQFfbdev_la_SOURCES=	fbset.c fbset_modes_y.y fbset_modes_l.l vid_fbdev.c in_fbdev.c
YACCLEX_CLEANFILES=	fbset_modes_y.c fbset_modes_y.h fbset_modes_y.tab.h fbset_modes_l.c
EXTRA_libQFfbdev_la_SOURCES=fbset_modes_y.h

fbset_modes_y.o: fbset_modes_y.c
	$(COMPILE) -Wno-error -c fbset_modes_y.c
fbset_modes_l.o: fbset_modes_l.c
	$(COMPILE) -Wno-error -c fbset_modes_l.c

##libQFmgl_la_LDFLAGS=	-version-info 1:0:0
##libQFmgl_la_SOURCES=	in_win.c vid_common_sw.c vid_mgl.c
libQFsdl_la_LDFLAGS=	-version-info 1:0:0
libQFsdl_la_SOURCES=	in_sdl.c vid_common_sw.c vid_sdl.c
libQFsvga_la_LDFLAGS=	-version-info 1:0:0
libQFsvga_la_SOURCES=	d_copy.S in_svgalib.c vid_common_sw.c vid_svgalib.c
libQFx11_la_LDFLAGS=	-version-info 1:0:0
libQFx11_la_SOURCES=	in_x11.c context_x11.c dga_check.c vid_common_sw.c vid_x11.c

libQFglx_la_LDFLAGS=	-version-info 1:0:0
libQFglx_la_SOURCES=	in_x11.c context_x11.c dga_check.c vid_common_gl.c vid_glx.c
libQFsgl_la_LDFLAGS=	-version-info 1:0:0
libQFsgl_la_SOURCES=	in_sdl.c vid_common_gl.c vid_sgl.c
libQFtdfx_la_LDFLAGS=	-version-info 1:0:0
libQFtdfx_la_SOURCES=	in_svgalib.c vid_common_gl.c vid_3dfxsvga.c
##libQFwgl_la_LDFLAGS=	-version-info 1:0:0
##libQFwgl_la_SOURCES=	in_win.c vid_wgl.c

LIBLIST =	$(lib_LTLIBRARIES) @LIBRARY_SEARCH_PATH@
