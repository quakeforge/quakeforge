## Process this file with automake to produce Makefile.in
#
#	Makefile.am
#
#	Automake-using build system for QuakeForge
#
#	Copyright (C) 2000 Jeff Teunissen <deek@quakeforge.net>
#
#	This Makefile is free software; you can redistribute it and/or
#	modify it under the terms of the GNU General Public License
#	as published by the Free Software Foundation; either version 2
#	of the License, or (at your option) any later version.
#
#	This program is distributed in the hope that it will be useful,
#	but WITHOUT ANY WARRANTY; without even the implied warranty of
#	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
#
#	See the GNU General Public License for more details.
#
#	You should have received a copy of the GNU General Public License
#	along with this program; if not, write to:
#
#		Free Software Foundation, Inc.
#		59 Temple Place - Suite 330
#		Boston, MA  02111-1307, USA
#
#	$Id$
#
AUTOMAKE_OPTIONS= foreign

# Stuff that is common to both client and server
#
INCLUDES=	-I$(top_srcdir)/include -I$(top_srcdir)/nq/include
SDL_LIBS = @SDL_LIBS@

bin_PROGRAMS=	@NQ_TARGETS@

EXTRA_PROGRAMS= nq-fbdev nq-glx nq-mgl nq-sdl nq-sdl32 nq-sgl nq-svga nq-3dfx \
		nq-wgl nq-x11 nq-server

if ASM_ARCH
asm= libasm.a
else
asm= 
endif

noinst_LIBRARIES= @nq_libs@ $(asm)
EXTRA_LIBRARIES=libasm.a libnq_client.a libnq_common.a libnq_sdl.a libnq_server.a

libasm_a_SOURCES=	worlda.S
libasm_la_CCASFLAGS=@PREFER_NON_PIC@
libnq_common_a_SOURCES=game.c world.c com.c
libnq_sdl_a_SOURCES=	sys_sdl.c
libnq_sdl_a_CFLAGS=	$(SDL_CFLAGS)

common_ldflags= -export-dynamic

cl_plugin_LIBS= \
		$(SERVER_PLUGIN_STATIC_LIBS) \
		$(CLIENT_PLUGIN_STATIC_LIBS)

client_LIBFILES= \
		$(top_builddir)/libs/video/targets/libQFjs.la \
		$(top_builddir)/libs/audio/libQFcd.la \
		$(top_builddir)/libs/audio/libQFsound.la

server_LIBFILES= \
		$(SERVER_PLUGIN_STATIC_LIBS) \
		$(top_builddir)/libs/models/libQFmodels.la

common_LIBFILES= \
		$(asm) \
		$(top_builddir)/libs/net/libnet_main.la \
		$(top_builddir)/libs/console/libQFconsole.la \
		$(top_builddir)/libs/gamecode/builtins/libQFgamecode_builtins.la \
		$(top_builddir)/libs/image/libQFimage.la \
		$(top_builddir)/libs/gib/libQFgib.la \
		$(top_builddir)/libs/ruamoko/libQFruamoko.la \
		$(top_builddir)/libs/gamecode/engine/libQFgamecode.la \
		$(top_builddir)/libs/util/libQFutil.la

client_LIBS=	$(client_LIBFILES) $(common_LIBFILES)

server_LIBS=	$(server_LIBFILES) $(common_LIBFILES) $(NET_LIBS)
server_LIB_DEPS=$(server_LIBFILES) $(common_LIBFILES)

libnq_client_a_SOURCES= \
		cl_cam.c cl_cmd.c cl_demo.c cl_input.c cl_main.c cl_screen.c \
		cl_parse.c cl_tent.c cl_view.c host_skin.c skin.c \
		sbar.c

libnq_server_a_SOURCES= \
		host.c host_cmd.c sv_cl_phys.c sv_cvar.c sv_main.c \
		sv_move.c sv_phys.c sv_pr_cmds.c sv_progs.c sv_user.c

client_libs= libnq_server.a libnq_client.a libnq_common.a
server_libs = libnq_server.a libnq_common.a

# Software-rendering targets

# We need libQFrenderer_sw to always be static -- there's assembler in there
soft_QFLIBS=	$(top_builddir)/libs/video/renderer/libQFrenderer_sw.la \
		$(top_builddir)/libs/models/libQFmodels_sw.la

# ... Linux FBDev
nq_fbdev_libs=  \
			$(client_libs) \
			$(cl_plugin_LIBS) \
			$(soft_QFLIBS) \
			$(top_builddir)/libs/video/targets/libQFfbdev.la \
			$(client_LIBS)
nq_fbdev_SOURCES=	sys_unix.c
nq_fbdev_LDADD= $(nq_fbdev_libs) $(NET_LIBS)
nq_fbdev_LDFLAGS=	$(common_ldflags)
nq_fbdev_DEPENDENCIES=	$(nq_fbdev_libs)

# ... SciTech MGL
nq_mgl_libs=  \
			$(client_libs) \
			$(cl_plugin_LIBS) \
			$(soft_QFLIBS) \
			$(top_builddir)/libs/video/targets/libQFmgl.la \
			$(client_LIBS)
nq_mgl_SOURCES= 	sys_win.c
nq_mgl_LDADD=		$(nq_mgl_libs) $(MGL_LIBS) $(NET_LIBS)
nq_mgl_LDFLAGS= 	$(common_ldflags)
nq_mgl_DEPENDENCIES=	$(nq_mgl_libs)

# ... SDL, version 1.2 and higher
nq_sdl_libs=  \
			libnq_sdl.a \
			$(client_libs) \
			$(cl_plugin_LIBS) \
			$(soft_QFLIBS) \
			$(top_builddir)/libs/video/targets/libQFsdl.la \
			$(client_LIBS)
nq_sdl_SOURCES=sdl_link.c
nq_sdl_LDADD=		$(nq_sdl_libs) $(SDL_LIBS) $(NET_LIBS)
nq_sdl_LDFLAGS= 	$(common_ldflags)
nq_sdl_DEPENDENCIES=	$(nq_sdl_libs)

# ... 32-bit software, SDL
nq_sdl32_libs=  \
			libnq_sdl.a \
			$(client_libs) \
			$(cl_plugin_LIBS) \
			$(top_builddir)/libs/video/renderer/libQFrenderer_sw32.la \
			$(top_builddir)/libs/models/libQFmodels_sw.la \
			$(top_builddir)/libs/video/targets/libQFsdl32.la \
			$(client_LIBS)
nq_sdl32_SOURCES=sdl_link.c
nq_sdl32_LDADD= 	$(nq_sdl32_libs) $(SDL_LIBS) $(NET_LIBS)
nq_sdl32_LDFLAGS=	$(common_ldflags)
nq_sdl32_DEPENDENCIES=	$(nq_sdl32_libs)

# ... Linux SVGAlib
nq_svga_libs=  \
			$(client_libs) \
			$(cl_plugin_LIBS) \
			$(soft_QFLIBS) \
			$(top_builddir)/libs/video/targets/libQFsvga.la \
			$(client_LIBS)
nq_svga_SOURCES=	sys_unix.c
nq_svga_LDADD=		$(nq_svga_libs) $(SVGA_LIBS) $(NET_LIBS)
nq_svga_LDFLAGS=	$(common_ldflags)
nq_svga_DEPENDENCIES=	$(nq_svga_libs)

# ... X11
nq_x11_libs=  \
			$(client_libs) \
			$(cl_plugin_LIBS) \
			$(soft_QFLIBS) \
			$(top_builddir)/libs/video/targets/libQFx11.la \
			$(client_LIBS)
nq_x11_SOURCES= 	sys_unix.c
nq_x11_LDADD=		$(nq_x11_libs) \
			$(X_PRE_LIBS) $(VIDMODE_LIBS) $(DGA_LIBS) $(X_LIBS) -lX11 \
			$(X_EXTRA_LIBS) $(X_SHM_LIB) $(NET_LIBS)
nq_x11_LDFLAGS= 	$(common_ldflags)
nq_x11_DEPENDENCIES=	$(nq_x11_libs)

# OpenGL-using targets

opengl_QFLIBS= \
			$(top_builddir)/libs/video/renderer/libQFrenderer_gl.la \
			$(top_builddir)/libs/models/libQFmodels_gl.la

# ... Linux 3DFX
nq_3dfx_libs=  \
			$(client_libs) \
			$(cl_plugin_LIBS) \
			$(opengl_QFLIBS) \
			$(top_builddir)/libs/video/targets/libQFtdfx.la \
			$(client_LIBS)
nq_3dfx_SOURCES= 	sys_unix.c
nq_3dfx_LDADD=		$(nq_3dfx_libs) $(SVGA_LIBS) $(NET_LIBS)
nq_3dfx_LDFLAGS=		$(common_ldflags)
nq_3dfx_DEPENDENCIES= 	$(nq_3dfx_libs)
							
# ... OpenGL in X Window
nq_glx_libs=  \
			$(client_libs) \
			$(cl_plugin_LIBS) \
			$(opengl_QFLIBS) \
			$(top_builddir)/libs/video/targets/libQFglx.la \
			$(client_LIBS)
nq_glx_SOURCES= 	sys_unix.c
nq_glx_LDADD=		$(nq_glx_libs) \
			$(X_PRE_LIBS) $(VIDMODE_LIBS) $(DGA_LIBS) $(X_LIBS) -lX11 \
			-lXext $(X_EXTRA_LIBS) $(DL_LIBS) $(NET_LIBS)
nq_glx_LDFLAGS= 	$(common_ldflags)
nq_glx_DEPENDENCIES=	$(nq_glx_libs)

# ... Simple Directmedia Layer, version 1.2 and higher, in GL mode
nq_sgl_libs=  \
			libnq_sdl.a \
			$(client_libs) \
			$(cl_plugin_LIBS) \
			$(opengl_QFLIBS) \
			$(top_builddir)/libs/video/targets/libQFsgl.la \
			$(client_LIBS)
nq_sgl_SOURCES=sdl_link.c
nq_sgl_LDADD=		$(nq_sgl_libs) $(SDL_LIBS) $(DL_LIBS) $(NET_LIBS)
nq_sgl_LDFLAGS= 	$(common_ldflags)
nq_sgl_DEPENDENCIES=	$(nq_sgl_libs)

# ... SGI/Microsoft WGL (Windows OpenGL)
nq_wgl_libs=  \
			$(client_libs) \
			$(cl_plugin_LIBS) \
			$(opengl_QFLIBS) \
			$(top_builddir)/libs/video/targets/libQFwgl.la \
			$(client_LIBS)
nq_wgl_SOURCES= 	sys_win.c
nq_wgl_LDADD=		$(nq_wgl_libs) -lgdi32 -lcomctl32 -lwinmm $(NET_LIBS)
nq_wgl_LDFLAGS= 	$(common_ldflags)
nq_wgl_DEPENDENCIES=	$(nq_wgl_libs)

# Dedicated Server
if SYSTYPE_WIN32
ded_sources=	sys_wind.c sv_ded.c
else
ded_sources=	sys_unixd.c sv_ded.c
endif
EXTRA_DIST=sys_wind.c sys_unixd.c sv_ded.c

nq_server_LDFLAGS=	$(common_ldflags)
nq_server_SOURCES=	$(ded_sources)
nq_server_LDADD=	$(server_libs) $(server_LIBS)
nq_server_DEPENDENCIES=	$(server_libs) $(server_LIB_DEPS)

# Stuff that doesn't get linked into an executable NEEDS to be mentioned here,
# or it won't be distributed with 'make dist'

# Kill the temp files, hopefully.
CLEANFILES  = *.i *.s $(YACCLEX_CLEANFILES)
