AUTOMAKE_OPTIONS= foreign

AM_CPPFLAGS= -I$(top_srcdir)/include

lib_ldflags=-version-info $(QUAKE_LIBRARY_VERSION_INFO) \
			-rpath $(libdir) -no-undefined

lib_LTLIBRARIES= @JOY_TARGETS@

noinst_LTLIBRARIES= @VID_TARGETS@ @vid_libs@

EXTRA_LTLIBRARIES= \
	libQFjs.la libQFfbdev.la libQFsvga.la libQFx11.la libQFsdl.la libQFwgl.la \
	libvid_common.la libvid_sdl.la \
	libvid_svga.la libvid_x11.la

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

x11_gl_src=vid_x11_gl.c
x11_sw_src=vid_x11_sw.c
x11_vulkan_src=vid_x11_vulkan.c
if X11_VULKAN
x11_src=      $(x11_gl_src) $(x11_sw_src) $(x11_vulkan_src)
else
x11_src=      $(x11_gl_src) $(x11_sw_src)
endif

js_libs=$(top_builddir)/libs/util/libQFutil.la

libQFjs_la_LDFLAGS=		$(lib_ldflags)
libQFjs_la_LIBADD=		$(js_libs)
libQFjs_la_DEPENDENCIES=$(js_libs)
libQFjs_la_CFLAGS=		@PREFER_PIC@ $(JOY_CFLAGS)
libQFjs_la_SOURCES=		joy.c $(joy_src)
EXTRA_libQFjs_la_SOURCES= $(joy_linux_src) $(joy_win_src) $(joy_null_src)

libvid_common_la_SOURCES=	\
		in_common.c in_event.c keys.c old_keys.c pr_keys.c vid.c
libvid_common_la_CFLAGS=	@PREFER_NON_PIC@
libvid_common_la_LDFLAGS=	@STATIC@

libvid_x11_la_SOURCES=		in_x11.c context_x11.c dga_check.c $(x11_src)
libvid_x11_la_CFLAGS=		@PREFER_NON_PIC@ $(X_CFLAGS)
libvid_x11_la_LDFLAGS=		@STATIC@
EXTRA_libvid_x11_la_SOURCES= $(x11_vulkan_src)

libvid_svga_la_SOURCES=		in_svgalib.c
libvid_svga_la_CFLAGS=		@PREFER_NON_PIC@ $(SVGA_CFLAGS)
libvid_svga_la_LDFLAGS=		@STATIC@

libvid_sdl_la_SOURCES=		in_sdl.c context_sdl.c vid_sdl_gl.c vid_sdl_sw.c
libvid_sdl_la_CFLAGS=		@PREFER_NON_PIC@ $(SDL_CFLAGS)
libvid_sdl_la_LDFLAGS=		@STATIC@

#
# Linux FBdev
#
fbdev_c= fbset_modes_y.c fbset_modes_l.c
fbdev_h= fbset_modes_y.h
YFLAGS = -d
YACCLEX_CLEANFILES=	$(fbdev_c) $(fbdev_h)
BUILT_SOURCES= $(fbdev_c) $(fbdev_h)

fbdev_libs=libvid_common.la
libQFfbdev_la_CFLAGS=		@PREFER_NON_PIC@
libQFfbdev_la_SOURCES=		fbset.c fbset_modes_y.y fbset_modes_l.l \
							in_fbdev.c vid_fbdev.c
libQFfbdev_la_LDFLAGS=		@STATIC@
libQFfbdev_la_LIBADD=		$(fbdev_libs)
libQFfbdev_la_DEPENDENCIES=	$(fbdev_libs)


#
# Simple DirectMedia Library
#
sdl_libs=libvid_common.la libvid_sdl.la
libQFsdl_la_CFLAGS=			@PREFER_NON_PIC@ $(SDL_CFLAGS)
libQFsdl_la_SOURCES=		vid_sdl.c
libQFsdl_la_LDFLAGS=		@STATIC@
libQFsdl_la_LIBADD=			$(sdl_libs)
libQFsdl_la_DEPENDENCIES=	$(sdl_libs)

#
# SVGAlib
#
svga_libs=libvid_common.la libvid_svga.la
libQFsvga_la_CFLAGS=		@PREFER_NON_PIC@ $(SVGA_CFLAGS)
libQFsvga_la_SOURCES=		vid_svgalib.c
libQFsvga_la_LDFLAGS=		@STATIC@
libQFsvga_la_LIBADD=		$(svga_libs)
libQFsvga_la_DEPENDENCIES=	$(svga_libs)

#
# OpenGL in Win32
#
wgl_libs=libvid_common.la libvid_gl.la
libQFwgl_la_CFLAGS=			@PREFER_NON_PIC@ $(WGL_CFLAGS)
libQFwgl_la_SOURCES=		in_win.c vid_wgl.c
libQFwgl_la_LDFLAGS=		@STATIC@
libQFwgl_la_LIBADD=			$(wgl_libs)
libQFwgl_la_DEPENDENCIES=	$(wgl_libs)

#
# X11 software rendering
#
x11_libs=libvid_common.la libvid_x11.la
libQFx11_la_CFLAGS=			@PREFER_NON_PIC@ $(X_CFLAGS)
libQFx11_la_SOURCES=		vid_x11.c
libQFx11_la_LDFLAGS=		@STATIC@
libQFx11_la_LIBADD=			$(x11_libs)
libQFx11_la_DEPENDENCIES=	$(x11_libs)

# Kill the temp files, hopefully.
CLEANFILES  = *.i *.s $(YACCLEX_CLEANFILES)

EXTRA_DIST= $(fbdev_c) $(fbdev_h)
