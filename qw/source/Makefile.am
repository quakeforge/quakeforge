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

AUTOMAKE_OPTIONS=	foreign

# Stuff that is common to both client and server
INCLUDES=	-I$(top_srcdir)/include -I$(top_srcdir)/qw/include

bin_PROGRAMS=	@QW_TARGETS@

EXTRA_PROGRAMS= qw-client-3dfx qw-client-fbdev \
		qw-client-glx qw-client-mgl qw-client-sdl \
		qw-client-sgl qw-client-svga qw-client-wgl \
		qw-client-x11 qw-server

noinst_LIBRARIES= libqfnet.a

common_SOURCES= buildnum.c com.c \
		game.c msg_ucmd.c \
		pmove.c pmovetst.c \
		sys_x86.S net_packetlog.c

# ... Network type
if NETTYPE_IPV6
libqfnet_a_SOURCES=	net_chan.c net_udp6.c
else
libqfnet_a_SOURCES=	net_chan.c net_udp.c
endif
EXTRA_libqfnet_a_SOURCES= net_chan.c net_udp.c net_udp6.c

# Server builds
#
# ... System type
if SYSTYPE_WIN32
syssv_SRC=	sv_sys_win.c
else
syssv_SRC=	sv_sys_unix.c
endif

server_SOURCES=	sv_ccmds.c sv_cvar.c sv_ents.c sv_init.c sv_main.c sv_misc.c \
		sv_model.c sv_move.c sv_nchan.c sv_phys.c sv_pr_cmds.c \
		sv_progs.c sv_send.c sv_user.c world.c worlda.S $(syssv_SRC)

qf_server_LIBS= $(top_builddir)/libs/models/libQFmodels.la \
		$(top_builddir)/libs/gamecode/libQFgamecode.la \
		$(top_builddir)/libs/util/libQFutil.la

qw_server_SOURCES=	$(common_SOURCES) $(server_SOURCES)
qw_server_LDADD=	-L. -lqfnet $(qf_server_LIBS) $(NET_LIBS) $(DL_LIBS)
qw_server_DEPENDENCIES=	libqfnet.a $(qf_server_LIBS)

# Client builds
#
# ... System type
if SYSTYPE_WIN32
syscl_SRC=	cl_sys_win.c
else
syscl_SRC=	cl_sys_unix.c
endif

qf_client_LIBS=	$(top_builddir)/libs/video/targets/libQFjs.la \
		$(top_builddir)/libs/gamecode/libQFgamecode.la \
		$(top_builddir)/libs/audio/cd/libQFcd.la \
		$(top_builddir)/libs/audio/libQFsound.la \
		$(top_builddir)/libs/util/libQFutil.la

client_LIBS= -L. -lqfnet $(qf_client_LIBS) $(NET_LIBS)
client_LIB_DEPS=	libqfnet.a $(qf_client_LIBS)
# libQFjs is seperate because it needs to be linked after when building statically

client_SOURCES= cl_cam.c cl_cmd.c cl_cvar.c cl_demo.c cl_ents.c cl_input.c \
		cl_main.c cl_misc.c cl_ngraph.c cl_parse.c cl_screen.c cl_pred.c \
		cl_skin.c cl_slist.c cl_tent.c \
		console.c keys.c locs.c pcx.c \
		r_view.c sbar.c skin.c teamplay.c tga.c \
		wad.c cl_math.S $(syscl_SRC)

# Software-rendering clients
#
# ... Common stuff

soft_SOURCES=	sw_rpart.c sw_view.c

# ... Linux FBDev
qw_client_fbdev_SOURCES= $(common_SOURCES) $(client_SOURCES) $(soft_SOURCES)
qw_client_fbdev_LDADD= ../../libs/video/renderer/libQFrenderer_sw.la ../../libs/models/libQFmodels_sw.la ../../libs/video/targets/libQFfbdev.la $(client_LIBS)
qw_client_fbdev_DEPENDENCIES=../../libs/video/renderer/libQFrenderer_sw.la ../../libs/models/libQFmodels_sw.la ../../libs/video/targets/libQFfbdev.la  $(client_LIB_DEPS)

# ... SciTech MGL
qw_client_mgl_SOURCES=	$(common_SOURCES) $(client_SOURCES) $(soft_SOURCES)
qw_client_mgl_LDADD=	../../libs/video/renderer/libQFrenderer_sw.la ../../libs/models/libQFmodels_sw.la ../../libs/video/targets/libQFmgl.la $(client_LIBS) $(MGL_LIBS)
qw_client_mgl_DEPENDENCIES=../../libs/video/renderer/libQFrenderer_sw.la ../../libs/models/libQFmodels_sw.la ../../libs/video/targets/libQFmgl.la $(client_LIB_DEPS)

# ... Simple DirectMedia Layer, version 1.0 and higher
qw_client_sdl_SOURCES=	$(common_SOURCES) $(client_SOURCES) $(soft_SOURCES)
qw_client_sdl_LDADD=	../../libs/video/renderer/libQFrenderer_sw.la ../../libs/models/libQFmodels_sw.la ../../libs/video/targets/libQFsdl.la $(client_LIBS) $(SDL_LIBS)
qw_client_sdl_DEPENDENCIES=../../libs/video/renderer/libQFrenderer_sw.la ../../libs/models/libQFmodels_sw.la ../../libs/video/targets/libQFsdl.la $(client_LIB_DEPS)

# ... Linux SVGAlib
qw_client_svga_SOURCES= $(common_SOURCES) $(client_SOURCES) $(soft_SOURCES)
qw_client_svga_LDADD=	../../libs/video/renderer/libQFrenderer_sw.la ../../libs/models/libQFmodels_sw.la ../../libs/video/targets/libQFsvga.la $(client_LIBS) $(SVGA_LIBS)
qw_client_svga_DEPENDENCIES=../../libs/video/renderer/libQFrenderer_sw.la ../../libs/models/libQFmodels_sw.la ../../libs/video/targets/libQFsvga.la $(client_LIB_DEPS)

# ... X11
qw_client_x11_SOURCES=	$(common_SOURCES) $(client_SOURCES) $(soft_SOURCES)
qw_client_x11_LDADD=	../../libs/video/renderer/libQFrenderer_sw.la ../../libs/models/libQFmodels_sw.la ../../libs/video/targets/libQFx11.la $(client_LIBS) $(X_PRE_LIBS) $(VIDMODE_LIBS) $(DGA_LIBS) $(X_LIBS) -lX11 $(X_EXTRA_LIBS) $(X_SHM_LIB)
qw_client_x11_DEPENDENCIES=../../libs/video/renderer/libQFrenderer_sw.la ../../libs/models/libQFmodels_sw.la ../../libs/video/targets/libQFx11.la $(client_LIB_DEPS)


# OpenGL-using clients
#
# ... Common stuff
ogl_SOURCES=	gl_dyn_part.c gl_view.c

# ... 3Dfx Voodoo 1 and 2 SVGAlib-based console GL
qw_client_3dfx_SOURCES=	$(common_SOURCES) $(client_SOURCES) $(ogl_SOURCES)
qw_client_3dfx_LDADD=	../../libs/video/renderer/libQFrenderer_gl.la ../../libs/models/libQFmodels_gl.la ../../libs/video/targets/libQFtdfx.la $(client_LIBS) $(TDFXGL_LIBS) $(SVGA_LIBS) $(DL_LIBS)
qw_client_3dfx_DEPENDENCIES=../../libs/video/renderer/libQFrenderer_gl.la ../../libs/models/libQFmodels_gl.la ../../libs/video/targets/libQFtdfx.la $(client_LIB_DEPS)

# ... OpenGL in X Window
qw_client_glx_SOURCES=	$(common_SOURCES) $(client_SOURCES) $(ogl_SOURCES)
qw_client_glx_LDADD=	../../libs/video/renderer/libQFrenderer_gl.la ../../libs/models/libQFmodels_gl.la ../../libs/video/targets/libQFglx.la $(client_LIBS) $(GLX_LIBS) $(X_PRE_LIBS) $(VIDMODE_LIBS) $(DGA_LIBS) $(X_LIBS) -lX11 $(X_EXTRA_LIBS) $(DL_LIBS)
qw_client_glx_DEPENDENCIES=../../libs/video/renderer/libQFrenderer_gl.la ../../libs/models/libQFmodels_gl.la ../../libs/video/targets/libQFglx.la $(client_LIB_DEPS)

# ... Simple DirectMedia Layer, version 1.1 and higher, in GL mode
qw_client_sgl_SOURCES=	$(common_SOURCES) $(client_SOURCES) $(ogl_SOURCES)
qw_client_sgl_LDADD=	../../libs/video/renderer/libQFrenderer_gl.la ../../libs/models/libQFmodels_gl.la ../../libs/video/targets/libQFsgl.la $(client_LIBS) $(SDL_LIBS) $(X_LIBS) $(GLX_LIBS) $(DL_LIBS)
qw_client_sgl_DEPENDENCIES=../../libs/video/renderer/libQFrenderer_gl.la ../../libs/models/libQFmodels_gl.la ../../libs/video/targets/libQFsgl.la $(client_LIB_DEPS)

# ... SGI/Microsoft WGL (Windows OpenGL)
qw_client_wgl_SOURCES=	$(common_SOURCES) $(client_SOURCES) $(ogl_SOURCES)
qw_client_wgl_LDADD=	../../libs/video/renderer/libQFrenderer_gl.la ../../libs/models/libQFmodels_gl.la ../../libs/video/targets/libQFwgl.la $(client_LIBS) $(GLX_LIBS) -lgdi32 -lwinmm
qw_client_wgl_DEPENDENCIES=../../libs/video/renderer/libQFrenderer_gl.la ../../libs/models/libQFmodels_gl.la ../../libs/video/targets/libQFwgl.la $(client_LIB_DEPS)

# Stuff that doesn't get linked into an executable NEEDS to be mentioned here,
# or it won't be distributed with 'make dist'
EXTRA_DIST=	makefile.mgw makefile.win \
		qf-server.mak qw_server.dsp \
		qf-client-sdl.mak qf-client-win.mak qw_client.dsp \
		qf-client-sgl.mak qf-client-wgl.mak qf-client-win.mak

# Kill the temp files, hopefully.
CLEANFILES	= *.i *.s $(YACCLEX_CLEANFILES)
