INCLUDES= -I$(top_srcdir)/include $(GGI_CFLAGS) $(MGL_CFLAGS) $(SDL_CFLAGS) $(SVGA_CFLAGS) $(X_CFLAGS) $(GLIDE_CFLAGS)

lib_LTLIBRARIES= @VID_TARGETS@

EXTRA_LTLIBRARIES= \
	libQFjs.la libQFfbdev.la libQFglx.la libQFsvga.la libQFx11.la \
	libQFsdl.la libQFsdl32.la libQFsgl.la libQFwgl.la

noinst_LTLIBRARIES=	libasm.la

if ASM_ARCH
libasm_la_SOURCES= d_copy.S
else
libasm_la_SOURCES=
endif
EXTRA_libasm_la_SOURCES= d_copy.S

libQFjs_la_LDFLAGS=	-version-info 1:0:0 -rpath $(libdir)
libQFjs_la_CFLAGS=      $(JOY_CFLAGS)
if JOYTYPE_LINUX
libQFjs_la_SOURCES=      joy.c joy_linux.c
else
if JOYTYPE_WIN32
libQFjs_la_SOURCES=      joy.c joy_win.c
else
libQFjs_la_SOURCES=      joy.c joy_null.c
endif
endif
EXTRA_libQFjs_la_SOURCES= joy.c joy_linux.c joy_null.c joy_win.c

in_common_SOURCE=	in_common.c in_event.c keys.c old_keys.c

#
# Linux FBdev
#
YFLAGS = -d
YACCLEX_CLEANFILES=	fbset_modes_y.c fbset_modes_y.h \
			fbset_modes_y.tab.h fbset_modes_l.c

libQFfbdev_la_LDFLAGS=	-version-info 1:0:0 -rpath $(libdir)
libQFfbdev_la_SOURCES=	fbset.c fbset_modes_y.y fbset_modes_l.l \
			in_fbdev.c vid.c vid_common_sw.c vid_fbdev.c \
			$(in_common_SOURCE)

fbset_modes_y.lo: $(srcdir)/fbset_modes_y.c
	$(LTCOMPILE) -Wno-error -c $<
fbset_modes_l.lo: $(srcdir)/fbset_modes_l.c
	$(LTCOMPILE) -Wno-error -c $<

#
# OpenGL in X Window
#
libQFglx_la_LDFLAGS=	-version-info 1:0:0 -rpath $(libdir)
libQFglx_la_LIBADD=	$(X_PRE_LIBS) $(VIDMODE_LIBS) $(DGA_LIBS) $(X_LIBS) -lX11 $(X_EXTRA_LIBS)
libQFglx_la_SOURCES=	in_x11.c vid.c context_x11.c dga_check.c qfgl_ext.c \
			vid_common_gl.c vid_glx.c \
			$(in_common_SOURCE)

#
# Simple DirectMedia Library
#
libQFsdl_la_LDFLAGS=	-version-info 1:0:0 -rpath $(libdir)
libQFsdl_la_SOURCES=	in_sdl.c vid.c vid_common_sw.c vid_sdl.c \
			$(in_common_SOURCE)

libQFsdl32_la_LDFLAGS=	-version-info 1:0:0 -rpath $(libdir)
libQFsdl32_la_SOURCES=	in_sdl.c vid.c vid_common_sw32.c vid_sdl32.c \
			$(in_common_SOURCE)

libQFsgl_la_LDFLAGS=	-version-info 1:0:0 -rpath $(libdir)
libQFsgl_la_SOURCES=	$(in_common_SOURCE) in_sdl.c qfgl_ext.c vid.c vid_common_gl.c vid_sgl.c

#
# SVGAlib
#
libQFsvga_la_LIBADD=	libasm.la
libQFsvga_la_DEPENDENCIES=libasm.la
libQFsvga_la_SOURCES=	in_svgalib.c vid.c vid_common_sw.c vid_svgalib.c \
			$(in_common_SOURCE)

#
# OpenGL in Win32
#
libQFwgl_la_LDFLAGS=	-version-info 1:0:0 -rpath $(libdir)
libQFwgl_la_SOURCES=	$(in_common_SOURCE) in_win.c qfgl_ext.c vid.c vid_common_gl.c vid_wgl.c

#
# X11 software rendering
#
libQFx11_la_LDFLAGS=	-version-info 1:0:0 -rpath $(libdir)
libQFx11_la_LIBADD=	$(X_PRE_LIBS) $(VIDMODE_LIBS) $(DGA_LIBS) $(X_LIBS) -lX11 $(X_EXTRA_LIBS) $(X_SHM_LIB)
libQFx11_la_SOURCES=	$(in_common_SOURCE) in_x11.c vid.c context_x11.c dga_check.c vid_common_sw.c vid_x11.c

# Kill the temp files, hopefully.
CLEANFILES  = *.i *.s $(YACCLEX_CLEANFILES)
