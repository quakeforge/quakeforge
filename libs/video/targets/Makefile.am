AUTOMAKE_OPTIONS= foreign

INCLUDES= -I$(top_srcdir)/include

if ASM_ARCH
asm= libvid_asm.la
else
asm=
endif

lib_LTLIBRARIES= @JOY_TARGETS@

noinst_LTLIBRARIES= @VID_TARGETS@ @vid_libs@

EXTRA_LTLIBRARIES= \
	libQFjs.la libQFfbdev.la libQFglx.la libQFglslx.la libQFsvga.la \
	libQFtdfx.la libQFx11.la libQFsdl.la libQFsdl32.la libQFsgl.la \
	libQFwgl.la libvid_asm.la libvid_common.la libvid_gl.la libvid_glsl.la \
	libvid_sdl.la libvid_sw.la libvid_sw32.la libvid_svga.la libvid_x11.la

libvid_asm_la_SOURCES=		d_copy.S
libvid_asm_la_CFLAGS=		@PREFER_NON_PIC@

joy_linux_src= joy_linux.c
joy_win_src= joy_win.c
joy_null_src= joy_null.c
if JOYTYPE_LINUX
joy_src=      $(joy_linux_src)
else
if JOYTYPE_WIN32
joy_src=      $(joy_win_src)
else
joy_src=      $(joy_null_src)
endif
endif

libQFjs_la_LDFLAGS=		-version-info $(QUAKE_LIBRARY_VERSION_INFO) -rpath $(libdir)
libQFjs_la_CFLAGS=		@PREFER_PIC@ $(JOY_CFLAGS)
libQFjs_la_SOURCES=		joy.c $(joy_src)
EXTRA_libQFjs_la_SOURCES= $(joy_linux_src) $(joy_win_src) $(joy_null_src)

libvid_common_la_SOURCES=	\
		in_common.c in_event.c keys.c old_keys.c vid.c
libvid_common_la_CFLAGS=	@PREFER_NON_PIC@
libvid_common_la_LDFLAGS=	@STATIC@

libvid_sw_la_SOURCES=		vid_common_sw.c
libvid_sw_la_CFLAGS=		@PREFER_NON_PIC@
libvid_sw_la_LDFLAGS=		@STATIC@

libvid_sw32_la_SOURCES=		vid_common_sw32.c
libvid_sw32_la_CFLAGS=		@PREFER_NON_PIC@
libvid_sw32_la_LDFLAGS=		@STATIC@

libvid_gl_la_SOURCES=		vid_common_gl.c qfgl_ext.c vtxarray.c
libvid_gl_la_CFLAGS=		@PREFER_NON_PIC@
libvid_gl_la_LDFLAGS=		@STATIC@

libvid_glsl_la_SOURCES=		vid_common_glsl.c qfglsl.c vtxarray.c
libvid_glsl_la_CFLAGS=		@PREFER_NON_PIC@
libvid_glsl_la_LDFLAGS=		@STATIC@

libvid_x11_la_SOURCES=		in_x11.c context_x11.c dga_check.c
libvid_x11_la_CFLAGS=		@PREFER_NON_PIC@ $(X_CFLAGS)
libvid_x11_la_LDFLAGS=		@STATIC@

libvid_svga_la_SOURCES=		in_svgalib.c
libvid_svga_la_CFLAGS=		@PREFER_NON_PIC@ $(SVGA_CFLAGS)
libvid_svga_la_LDFLAGS=		@STATIC@

libvid_sdl_la_SOURCES=		in_sdl.c context_sdl.c
libvid_sdl_la_CFLAGS=		@PREFER_NON_PIC@ $(SDL_CFLAGS)
libvid_sdl_la_LDFLAGS=		@STATIC@

#
# Linux FBdev
#
fbdev_c= fbset_modes_y.c fbset_modes_l.c
fbdev_h= fbset_modes_y.h
YFLAGS = -d
YACCLEX_CLEANFILES=	$(fbdev_c) $(fbdev_h)

libQFfbdev_la_CFLAGS=		@PREFER_NON_PIC@
libQFfbdev_la_SOURCES=		fbset.c fbset_modes_y.y fbset_modes_l.l \
							in_fbdev.c vid_fbdev.c
libQFfbdev_la_LDFLAGS=		@STATIC@
libQFfbdev_la_LIBADD=		libvid_common.la libvid_sw.la
libQFfbdev_la_DEPENDENCIES=	libvid_common.la libvid_sw.la

#
# OpenGL in X Window
#
libQFglx_la_CFLAGS=			@PREFER_NON_PIC@ $(X_CFLAGS)
libQFglx_la_SOURCES=		vid_glx.c
libQFglx_la_LDFLAGS=		@STATIC@
libQFglx_la_LIBADD=			libvid_common.la libvid_gl.la libvid_x11.la
libQFglx_la_DEPENDENCIES=	libvid_common.la libvid_gl.la libvid_x11.la

#
# OpenGL in X Window
#
libQFglslx_la_CFLAGS=		@PREFER_NON_PIC@ $(X_CFLAGS)
libQFglslx_la_SOURCES=		vid_glx.c
libQFglslx_la_LDFLAGS=		@STATIC@
libQFglslx_la_LIBADD=		libvid_common.la libvid_glsl.la libvid_x11.la
libQFglslx_la_DEPENDENCIES=	libvid_common.la libvid_glsl.la libvid_x11.la

#
# Simple DirectMedia Library
#
libQFsdl_la_CFLAGS=			@PREFER_NON_PIC@ $(SDL_CFLAGS)
libQFsdl_la_SOURCES=		vid_sdl.c
libQFsdl_la_LDFLAGS=		@STATIC@
libQFsdl_la_LIBADD=			libvid_common.la libvid_sw.la libvid_sdl.la
libQFsdl_la_DEPENDENCIES=	libvid_common.la libvid_sw.la libvid_sdl.la

libQFsdl32_la_CFLAGS=		@PREFER_NON_PIC@ $(SDL_CFLAGS)
libQFsdl32_la_SOURCES=		vid_sdl32.c
libQFsdl32_la_LDFLAGS=		@STATIC@
libQFsdl32_la_LIBADD=		libvid_common.la libvid_sw32.la libvid_sdl.la
libQFsdl32_la_DEPENDENCIES=	libvid_common.la libvid_sw32.la libvid_sdl.la

libQFsgl_la_CFLAGS=			@PREFER_NON_PIC@ $(SDL_CFLAGS)
libQFsgl_la_SOURCES=		vid_sgl.c
libQFsgl_la_LDFLAGS=		@STATIC@
libQFsgl_la_LIBADD=			libvid_common.la libvid_gl.la libvid_sdl.la
libQFsgl_la_DEPENDENCIES=	libvid_common.la libvid_gl.la libvid_sdl.la

#
# SVGAlib
#
libQFsvga_la_CFLAGS=		@PREFER_NON_PIC@ $(SVGA_CFLAGS)
libQFsvga_la_SOURCES=		vid_svgalib.c
libQFsvga_la_LDFLAGS=		@STATIC@
libQFsvga_la_LIBADD=		$(asm) libvid_common.la libvid_sw.la libvid_svga.la
libQFsvga_la_DEPENDENCIES=	$(asm) libvid_common.la libvid_sw.la libvid_svga.la

#
# 3dfx
#
libQFtdfx_la_CFLAGS=		@PREFER_NON_PIC@ $(SVGA_CFLAGS)
libQFtdfx_la_SOURCES=		vid_3dfxsvga.c
libQFtdfx_la_LDFLAGS=		@STATIC@
libQFtdfx_la_LIBADD=		$(asm) libvid_common.la libvid_gl.la libvid_svga.la
libQFtdfx_la_DEPENDENCIES=	$(asm) libvid_common.la libvid_gl.la libvid_svga.la

#
# OpenGL in Win32
#
libQFwgl_la_CFLAGS=			@PREFER_NON_PIC@ $(WGL_CFLAGS)
libQFwgl_la_SOURCES=		in_win.c vid_wgl.c
libQFwgl_la_LDFLAGS=		@STATIC@
libQFwgl_la_LIBADD=			libvid_common.la libvid_gl.la
libQFwgl_la_DEPENDENCIES=	libvid_common.la libvid_gl.la

#
# X11 software rendering
#
libQFx11_la_CFLAGS=			@PREFER_NON_PIC@ $(X_CFLAGS)
libQFx11_la_SOURCES=		vid_x11.c
libQFx11_la_LDFLAGS=		@STATIC@
libQFx11_la_LIBADD=			libvid_common.la libvid_sw.la libvid_x11.la
libQFx11_la_DEPENDENCIES=	libvid_common.la libvid_sw.la libvid_x11.la

# Kill the temp files, hopefully.
CLEANFILES  = *.i *.s $(YACCLEX_CLEANFILES)

EXTRA_DIST= $(fbdev_c) $(fbdev_h)
