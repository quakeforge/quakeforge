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
#
INCLUDES=	-I$(top_srcdir)/include -I$(top_srcdir)/nq/include

bin_PROGRAMS =	@NQ_TARGETS@

EXTRA_PROGRAMS= nq-3dfx nq-fbdev nq-glx nq-mgl nq-sdl \
		nq-sgl nq-svga nq-wgl nq-x11 nq-server

if ASM_ARCH
ASM = libasm.la
else
ASM =
endif

noinst_LTLIBRARIES= libqfnet.la $(ASM)

libasm_la_SOURCES = sys_ia32.S worlda.S

common_SOURCES=	game.c host_skin.c skin.c world.c com.c

common_ldflags= -export-dynamic

# ... System type
if SYSTYPE_WIN32
qfsys_SRC=	sys_win.c
else
qfsys_SRC=	sys_unix.c
endif

# ... Networking
libqfnet_la_SOURCES=	net_bsd.c net_dgrm.c net_loop.c \
			net_main.c net_udp.c net_vcr.c

EXTRA_libqfnet_la_SOURCES= net_dos.c net_bw.c net_ipx.c net_mp.c net_ser.c \
			net_win.c net_wins.c net_wipx.c

qf_client_LIBS= $(top_builddir)/libs/video/targets/libQFjs.la \
		$(top_builddir)/libs/gamecode/libQFgamecode.la \
		$(top_builddir)/libs/gib/libQFgib.la \
		$(top_builddir)/libs/audio/libQFcd.la \
		$(top_builddir)/libs/audio/libQFsound.la \
		$(top_builddir)/libs/util/libQFutil.la

client_LIBS=	-L. libqfnet.la $(ASM) $(qf_client_LIBS) $(NET_LIBS)
client_LIB_DEPS=	libqfnet.la $(ASM) $(qf_client_LIBS)

client_SOURCES=	cl_cam.c cl_cmd.c cl_demo.c cl_input.c cl_main.c cl_screen.c \
		cl_parse.c cl_tent.c \
		console.c keys.c sbar.c \
		r_view.c locs.c

server_SOURCES= host.c host_cmd.c pr_cmds.c sv_cvar.c sv_main.c \
		sv_move.c sv_phys.c sv_progs.c sv_user.c

combined_SOURCES=	$(common_SOURCES) $(client_SOURCES) $(server_SOURCES) \
			$(qfsys_SRC)

# Software-rendering targets
#
# ... Common stuff
soft_SOURCES=	sw_view.c

# ... Linux FBDev
nq_fbdev_SOURCES= $(combined_SOURCES) $(soft_SOURCES)
nq_fbdev_LDADD= ../../libs/video/renderer/libQFrenderer_sw.la ../../libs/models/libQFmodels_sw.la ../../libs/video/targets/libQFfbdev.la $(client_LIBS)
nq_fbdev_LDFLAGS= $(common_ldflags)
nq_fbdev_DEPENDENCIES=../../libs/video/renderer/libQFrenderer_sw.la ../../libs/models/libQFmodels_sw.la ../../libs/video/targets/libQFfbdev.la $(client_LIB_DEPS)

# ... SciTech MGL
nq_mgl_SOURCES=	$(combined_SOURCES) $(soft_SOURCES)
nq_mgl_LDADD=	../../libs/video/renderer/libQFrenderer_sw.la ../../libs/models/libQFmodels_sw.la ../../libs/video/targets/libQFmgl.la $(client_LIBS) $(MGL_LIBS)
nq_mgl_LDFLAGS= $(common_ldflags)
nq_mgl_DEPENDENCIES=../../libs/video/renderer/libQFrenderer_sw.la ../../libs/models/libQFmodels_sw.la ../../libs/video/targets/libQFmgl.la $(client_LIB_DEPS)

# ... Sam Lantinga's Simple DirectMedia Layer, version 1.0 and higher
nq_sdl_SOURCES=	$(combined_SOURCES) $(soft_SOURCES)
nq_sdl_LDADD=	../../libs/video/renderer/libQFrenderer_sw.la ../../libs/models/libQFmodels_sw.la ../../libs/video/targets/libQFsdl.la $(client_LIBS) $(SDL_LIBS)
nq_sgl_LDFLAGS= $(common_ldflags)
nq_sdl_DEPENDENCIES=../../libs/video/renderer/libQFrenderer_sw.la ../../libs/models/libQFmodels_sw.la ../../libs/video/targets/libQFsdl.la $(client_LIB_DEPS)

# ... Linux SVGAlib
nq_svga_SOURCES=	$(combined_SOURCES) $(soft_SOURCES)
nq_svga_LDADD=	../../libs/video/renderer/libQFrenderer_sw.la ../../libs/models/libQFmodels_sw.la ../../libs/video/targets/libQFsvga.la $(client_LIBS) $(SVGA_LIBS)
nq_svga_LDFLAGS=    $(common_ldflags)
nq_svga_DEPENDENCIES=../../libs/video/renderer/libQFrenderer_sw.la ../../libs/models/libQFmodels_sw.la ../../libs/video/targets/libQFsvga.la $(client_LIB_DEPS)

# ... X11
nq_x11_SOURCES=	$(combined_SOURCES) $(soft_SOURCES)
nq_x11_LDADD=	../../libs/video/renderer/libQFrenderer_sw.la ../../libs/models/libQFmodels_sw.la ../../libs/video/targets/libQFx11.la $(client_LIBS) $(X_PRE_LIBS) $(VIDMODE_LIBS) $(DGA_LIBS) $(X_LIBS) -lX11 $(X_EXTRA_LIBS) $(X_SHM_LIB)
nq_x11_LDFLAGS= $(common_ldflags)
nq_x11_DEPENDENCIES=../../libs/video/renderer/libQFrenderer_sw.la ../../libs/models/libQFmodels_sw.la ../../libs/video/targets/libQFx11.la $(client_LIB_DEPS)


# OpenGL-using targets
# ... Common stuff
ogl_SOURCES=	gl_view.c

# ... 3Dfx Voodoo 1 and 2 SVGAlib-based console GL
nq_3dfx_SOURCES=	$(combined_SOURCES) $(ogl_SOURCES)
nq_3dfx_LDADD=	../../libs/video/renderer/libQFrenderer_gl.la ../../libs/models/libQFmodels_gl.la ../../libs/video/targets/libQFtdfx.la $(client_LIBS) $(TDFXGL_LIBS) $(SVGA_LIBS) $(DL_LIBS)
nq_3dfx_LDFLAGS=    $(common_ldflags)
nq_3dfx_DEPENDENCIES=../../libs/video/renderer/libQFrenderer_gl.la ../../libs/models/libQFmodels_gl.la ../../libs/video/targets/libQFtdfx.la $(client_LIB_DEPS)

# ... OpenGL in X Window
nq_glx_SOURCES=	$(combined_SOURCES) $(ogl_SOURCES)
nq_glx_LDADD=	../../libs/video/renderer/libQFrenderer_gl.la ../../libs/models/libQFmodels_gl.la ../../libs/video/targets/libQFglx.la $(client_LIBS) $(X_PRE_LIBS) $(VIDMODE_LIBS) $(DGA_LIBS) $(X_LIBS) -lX11 -lXext $(X_EXTRA_LIBS) $(DL_LIBS)
nq_glx_LDFLAGS= $(common_ldflags)
nq_glx_DEPENDENCIES=../../libs/video/renderer/libQFrenderer_gl.la ../../libs/models/libQFmodels_gl.la ../../libs/video/targets/libQFglx.la $(client_LIB_DEPS)

# ... Simple Directmedia Layer, version 1.1 and higher, in GL mode
nq_sgl_SOURCES=	$(combined_SOURCES) $(ogl_SOURCES)
nq_sgl_LDADD=	../../libs/video/renderer/libQFrenderer_gl.la ../../libs/models/libQFmodels_gl.la ../../libs/video/targets/libQFsgl.la $(client_LIBS) $(X_LIBS) $(SDL_LIBS) $(DL_LIBS)
nq_sgl_LDFLAGS= $(common_ldflags)
nq_sgl_DEPENDENCIES=../../libs/video/renderer/libQFrenderer_gl.la ../../libs/models/libQFmodels_gl.la ../../libs/video/targets/libQFsgl.la $(client_LIB_DEPS)

# ... SGI/Microsoft WGL (Windows OpenGL)
nq_wgl_SOURCES=	$(combined_SOURCES) $(ogl_SOURCES) conproc.c
nq_wgl_LDADD=	../../libs/video/renderer/libQFrenderer_gl.la ../../libs/models/libQFmodels_gl.la ../../libs/video/targets/libQFwgl.la $(client_LIBS) -lgdi32 -lcomctl32 -lwinmm
nq_wgl_LDFLAGS= $(common_ldflags)
nq_wgl_DEPENDENCIES=../../libs/video/renderer/libQFrenderer_gl.la ../../libs/models/libQFmodels_gl.la ../../libs/video/targets/libQFwgl.la $(client_LIB_DEPS)

# Dedicated Server
ded_SOURCES=	sys_unixd.c sv_ded.c

nq_server_SOURCES=	$(common_SOURCES) $(server_SOURCES) $(ded_SOURCES)
nq_server_LDADD=	libqfnet.la $(ASM)
nq_server_DEPENDENCIES=	libqfnet.la $(ASM)

# Stuff that doesn't get linked into an executable NEEDS to be mentioned here,
# or it won't be distributed with 'make dist'
EXTRA_DIST=	sys_win.c sys_unix.c #nq.dsp

# Kill the temp files, hopefully.
CLEANFILES  = *.i *.s $(YACCLEX_CLEANFILES)
