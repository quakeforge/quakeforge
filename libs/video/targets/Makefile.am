INCLUDES= -I$(top_srcdir)/include $(GGI_CFLAGS) $(MGL_CFLAGS) $(SDL_CFLAGS) $(SVGA_CFLAGS) $(X_CFLAGS) $(GLIDE_CFLAGS)

if ASM_ARCH
asm= libasm.la
else
asm=
endif

noinst_LTLIBRARIES= @VID_TARGETS@

EXTRA_LTLIBRARIES= \
	libQFjs.la libQFfbdev.la libQFglx.la libQFsvga.la libQFx11.la \
	libQFsdl.la libQFsdl32.la libQFsgl.la libQFwgl.la libasm.la

libasm_la_SOURCES= d_copy.S

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

libQFjs_la_LDFLAGS=	-version-info 1:0:0 -rpath $(libdir)
libQFjs_la_CFLAGS=      $(JOY_CFLAGS)
libQFjs_la_SOURCES=      joy.c $(joy_src)
EXTRA_libQFjs_la_SOURCES= $(joy_linux_src) $(joy_win_src) $(joy_null_src)

in_common_SOURCE=	in_common.c in_event.c keys.c old_keys.c

#
# Linux FBdev
#
fbdev_c= fbset_modes_y.c fbset_modes_l.c
fbdev_h= fbset_modes_y.h
YFLAGS = -d
YACCLEX_CLEANFILES=	$(fbdev_c) $(fbdev_h)

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
libQFglx_la_SOURCES=	in_x11.c vid.c context_x11.c dga_check.c qfgl_ext.c \
			vid_common_gl.c vid_glx.c \
			$(in_common_SOURCE)

#
# Simple DirectMedia Library
#
libQFsdl_la_SOURCES=	in_sdl.c vid.c vid_common_sw.c vid_sdl.c \
			$(in_common_SOURCE)

libQFsdl32_la_SOURCES=	in_sdl.c vid.c vid_common_sw32.c vid_sdl32.c \
			$(in_common_SOURCE)

libQFsgl_la_SOURCES=	in_sdl.c qfgl_ext.c vid.c vid_common_gl.c vid_sgl.c \
			$(in_common_SOURCE)

#
# SVGAlib
#
if ASM_ARCH
libQFsvga_la_LIBADD=	$(asm)
libQFsvga_la_DEPENDENCIES=$(asm)
endif
libQFsvga_la_SOURCES=	in_svgalib.c vid.c vid_common_sw.c vid_svgalib.c \
			$(in_common_SOURCE)

#
# OpenGL in Win32
#
libQFwgl_la_SOURCES=	in_win.c qfgl_ext.c vid.c vid_common_gl.c vid_wgl.c \
			$(in_common_SOURCE)

#
# X11 software rendering
#
libQFx11_la_SOURCES=	in_x11.c vid.c context_x11.c dga_check.c \
			vid_common_sw.c vid_x11.c \
			$(in_common_SOURCE)

# Kill the temp files, hopefully.
CLEANFILES  = *.i *.s $(YACCLEX_CLEANFILES)

EXTRA_DIST= $(fbdev_c) $(fbdev_h)
