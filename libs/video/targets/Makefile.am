AUTOMAKE_OPTIONS= foreign

INCLUDES= -I$(top_srcdir)/include

if ASM_ARCH
asm= libasm.la
else
asm=
endif

lib_LTLIBRARIES= @JOY_TARGETS@

noinst_LTLIBRARIES= @VID_TARGETS@ 

EXTRA_LTLIBRARIES= \
	libQFjs.la libQFfbdev.la libQFglx.la libQFsvga.la libQFtdfx.la \
	libQFx11.la libQFsdl.la libQFsdl32.la libQFsgl.la libQFwgl.la libasm.la \
	libcommon.la libgl.la libsdl.la libsw.la libsw32.la libsvga.la libx11.la

libasm_la_SOURCES=		d_copy.S
libasmn_la_CFLAGS=		@PREFER_NON_PIC@

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

libQFjs_la_LDFLAGS=		-version-info 1:0:0 -rpath $(libdir)
libQFjs_la_CFLAGS=		@PREFER_PIC@ $(JOY_CFLAGS)
libQFjs_la_SOURCES=		joy.c $(joy_src)
EXTRA_libQFjs_la_SOURCES= $(joy_linux_src) $(joy_win_src) $(joy_null_src)

libcommon_la_SOURCES=	in_common.c in_event.c keys.c old_keys.c vid.c
libcommon_la_CFLAGS=	@PREFER_NON_PIC@
libcommon_la_LDFLAGS=	-static

libsw_la_SOURCES=		vid_common_sw.c
libsw_la_CFLAGS=		@PREFER_NON_PIC@
libsw_la_LDFLAGS=		-static

libsw32_la_SOURCES=		vid_common_sw32.c
libsw32_la_CFLAGS=		@PREFER_NON_PIC@
libsw32_la_LDFLAGS=		-static

libgl_la_SOURCES=		vid_common_gl.c qfgl_ext.c
libgl_la_CFLAGS=		@PREFER_NON_PIC@
libgl_la_LDFLAGS=		-static

libx11_la_SOURCES=		in_x11.c context_x11.c dga_check.c
libx11_la_CFLAGS=		@PREFER_NON_PIC@ $(X_CFLAGS)
libx11_la_LDFLAGS=		-static

libsvga_la_SOURCES=		in_svgalib.c
libsvga_la_CFLAGS=		@PREFER_NON_PIC@ $(SVGA_CFLAGS)
libsvga_la_LDFLAGS=		-static

libsdl_la_SOURCES=		in_sdl.c
libsdl_la_CFLAGS=		@PREFER_NON_PIC@ $(SDL_CFLAGS)
libsdl_la_LDFLAGS=		-static

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
libQFfbdev_la_LDFLAGS=		-static
libQFfbdev_la_LIBADD=		libcommon.la libsw.la
libQFfbdev_la_DEPENDENCIES=	libcommon.la libsw.la

#
# OpenGL in X Window
#
libQFglx_la_CFLAGS=			@PREFER_NON_PIC@ $(X_CFLAGS)
libQFglx_la_SOURCES=		vid_glx.c
libQFglx_la_LDFLAGS=		-static
libQFglx_la_LIBADD=			libcommon.la libgl.la libx11.la
libQFglx_la_DEPENDENCIES=	libcommon.la libgl.la libx11.la

#
# Simple DirectMedia Library
#
libQFsdl_la_CFLAGS=			@PREFER_NON_PIC@ $(SDL_CFLAGS)
libQFsdl_la_SOURCES=		vid_sdl.c
libQFsdl_la_LDFLAGS=		-static
libQFsdl_la_LIBADD=			libcommon.la libsw.la libsdl.la
libQFsdl_la_DEPENDENCIES=	libcommon.la libsw.la libsdl.la

libQFsdl32_la_CFLAGS=		@PREFER_NON_PIC@ $(SDL_CFLAGS)
libQFsdl32_la_SOURCES=		vid_sdl32.c
libQFsdl32_la_LDFLAGS=		-static
libQFsdl32_la_LIBADD=		libcommon.la libsw32.la libsdl.la
libQFsdl32_la_DEPENDENCIES=	libcommon.la libsw32.la libsdl.la

libQFsgl_la_CFLAGS=			@PREFER_NON_PIC@ $(SDL_CFLAGS)
libQFsgl_la_SOURCES=		vid_sgl.c
libQFsgl_la_LDFLAGS=		-static
libQFsgl_la_LIBADD=			libcommon.la libgl.la libsdl.la
libQFsgl_la_DEPENDENCIES=	libcommon.la libgl.la libsdl.la

#
# SVGAlib
#
libQFsvga_la_CFLAGS=		@PREFER_NON_PIC@ $(SVGA_CFLAGS)
libQFsvga_la_SOURCES=		vid_svgalib.c
libQFsvga_la_LDFLAGS=		-static
libQFsvga_la_LIBADD=		$(asm) libcommon.la libsw.la libsvga.la
libQFsvga_la_DEPENDENCIES=	$(asm) libcommon.la libsw.la libsvga.la

#
# 3dfx
#
libQFtdfx_la_CFLAGS=		@PREFER_NON_PIC@ $(SVGA_CFLAGS)
libQFtdfx_la_SOURCES=		vid_3dfxsvga.c
libQFtdfx_la_LDFLAGS=		-static
libQFtdfx_la_LIBADD=		$(asm) libcommon.la libgl.la libsvga.la
libQFtdfx_la_DEPENDENCIES=	$(asm) libcommon.la libgl.la libsvga.la

#
# OpenGL in Win32
#
libQFwgl_la_CFLAGS=			@PREFER_NON_PIC@ $(WGL_CFLAGS)
libQFwgl_la_SOURCES=		in_win.c vid_wgl.c
libQFwgl_la_LDFLAGS=		-static
libQFwgl_la_LIBADD=			libcommon.la libgl.la
libQFwgl_la_DEPENDENCIES=	libcommon.la libgl.la

#
# X11 software rendering
#
libQFx11_la_CFLAGS=			@PREFER_NON_PIC@ $(X_CFLAGS)
libQFx11_la_SOURCES=		vid_x11.c
libQFx11_la_LDFLAGS=		-static
libQFx11_la_LIBADD=			libcommon.la libsw.la libx11.la
libQFx11_la_DEPENDENCIES=	libcommon.la libsw.la libx11.la

# Kill the temp files, hopefully.
CLEANFILES  = *.i *.s $(YACCLEX_CLEANFILES)

EXTRA_DIST= $(fbdev_c) $(fbdev_h)
