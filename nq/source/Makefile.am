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

noinst_LIBRARIES= libqfnet.a

math_ASM= 	cl_math.S
soft_ASM=	d_draw.S d_draw16.S d_parta.S d_polysa.S d_scana.S d_spr8.S \
		d_varsa.S sw_raclipa.S sw_raliasa.S sw_rdrawa.S sw_redgea.S sw_rvarsa.S \
		surf16.S surf8.S
common_ASM=	sys_ia32.S worlda.S $(math_ASM)

common_SOURCES= game.c wad.c world.c model.c model_alias.c model_brush.c \
		model_sprite.c com.c $(common_ASM)

# ... System type
if SYSTYPE_WIN32
qfsys_SRC=	sys_win.c fnmatch.c
else
qfsys_SRC=	sys_unix.c
endif

# ... Networking
libqfnet_a_SOURCES=	net_bsd.c net_dgrm.c net_loop.c \
			net_main.c net_udp.c net_vcr.c

EXTRA_libqfnet_a_SOURCES=net_dos.c net_bw.c net_ipx.c net_mp.c net_ser.c \
			net_win.c net_wins.c net_wipx.c

client_LIBS=		$(top_builddir)/libs/gamecode/libQFgamecode.la \
			$(top_builddir)/libs/util/libQFutil.la \
			$(top_builddir)/libs/audio/cd/libQFcd.la \
			$(top_builddir)/libs/audio/targets/libQFsound.la \
			-L. -lqfnet $(SOUND_LIBS) $(NET_LIBS)
client_LIB_DEPS=	libqfnet.a
client_POST_LIBS=	$(top_builddir)/libs/video/targets/libQFjs.la

client_SOURCES=	cl_cam.c cl_cmd.c cl_demo.c cl_input.c cl_main.c cl_parse.c \
		cl_tent.c console.c keys.c sbar.c r_part.c r_view.c \
		nonintel.c gib.c gib_instructions.c gib_vars.c \
		gib_interpret.c gib_modules.c gib_parse.c gib_stack.c

server_SOURCES= 	host.c host_cmd.c pr_cmds.c sv_cvar.c sv_main.c \
			sv_move.c sv_phys.c sv_progs.c sv_user.c

combined_SOURCES=	$(common_SOURCES) $(client_SOURCES) $(server_SOURCES) \
			$(qfsys_SRC)

# Software-rendering targets
#
# ... Common stuff
soft_SOURCES=	d_edge.c d_fill.c d_init.c d_modech.c d_part.c d_polyse.c \
		d_scan.c d_sky.c d_sprite.c d_surf.c d_vars.c d_zpoint.c \
		draw.c sw_raclip.c sw_ralias.c sw_rbsp.c sw_rdraw.c sw_redge.c sw_refrag.c \
		sw_rlight.c sw_rmain.c sw_rmisc.c sw_rpart.c sw_rsky.c sw_rsprite.c \
		sw_rsurf.c sw_rvars.c screen.c sw_model_alias.c sw_model_brush.c \
		sw_model_sprite.c sw_view.c $(soft_ASM)

# ... Linux FBDev
nq_fbdev_SOURCES= $(combined_SOURCES) $(soft_SOURCES)
nq_fbdev_LDADD= $(client_LIBS) ../../libs/video/targets/libQFfbdev.la $(client_POST_LIBS)
nq_fbdev_DEPENDENCIES= $(client_LIB_DEPS)

# ... SciTech MGL
nq_mgl_SOURCES=	$(combined_SOURCES) $(soft_SOURCES)
nq_mgl_LDADD=	$(client_LIBS) $(MGL_LIBS) ../../libs/video/targets/libQFmgl.la $(client_POST_LIBS)
nq_mgl_DEPENDENCIES=$(client_LIB_DEPS)

# ... Sam Lantinga's Simple DirectMedia Layer, version 1.0 and higher
nq_sdl_SOURCES=	$(combined_SOURCES) $(soft_SOURCES)
nq_sdl_LDADD=	$(client_LIBS) $(SDL_LIBS) ../../libs/video/targets/libQFsdl.la $(client_POST_LIBS)
nq_sdl_DEPENDENCIES=$(client_LIB_DEPS)

# ... Linux SVGAlib
nq_svga_SOURCES=	$(combined_SOURCES) $(soft_SOURCES)
nq_svga_LDADD=	$(client_LIBS) $(SVGA_LIBS) ../../libs/video/targets/libQFsvga.la $(client_POST_LIBS)
nq_svga_DEPENDENCIES=$(client_LIB_DEPS)

# ... X11
nq_x11_SOURCES=	$(combined_SOURCES) $(soft_SOURCES)
nq_x11_LDADD=	$(client_LIBS) $(X_PRE_LIBS) $(VIDMODE_LIBS) $(DGA_LIBS) $(X_LIBS) -lX11 $(X_EXTRA_LIBS) $(X_SHM_LIB) ../../libs/video/targets/libQFx11.la $(client_POST_LIBS)
nq_x11_DEPENDENCIES=$(client_LIB_DEPS)


# OpenGL-using targets
# ... Common stuff
ogl_SOURCES=	noisetextures.c gl_textures.c gl_draw.c gl_dyn_fires.c	\
		gl_dyn_part.c gl_dyn_textures.c gl_mesh.c		\
		gl_refrag.c gl_rlight.c gl_rmain.c gl_rmisc.c gl_rsurf.c \
		gl_screen.c gl_view.c gl_warp.c gl_model_alias.c	\
		gl_model_brush.c gl_model_fullbright.c gl_model_sprite.c

# ... 3Dfx Voodoo 1 and 2 SVGAlib-based console GL
nq_3dfx_SOURCES=	$(combined_SOURCES) $(ogl_SOURCES)
nq_3dfx_LDADD=	$(client_LIBS) $(TDFXGL_LIBS) $(SVGA_LIBS) $(DL_LIBS) ../../libs/video/targets/libQFtdfx.la $(client_POST_LIBS)
nq_3dfx_DEPENDENCIES=$(client_LIB_DEPS)

# ... OpenGL in X Window
nq_glx_SOURCES=	$(combined_SOURCES) $(ogl_SOURCES)
nq_glx_LDADD=	$(client_LIBS) $(GLX_LIBS) $(X_PRE_LIBS) $(VIDMODE_LIBS) $(DGA_LIBS) $(X_LIBS) -lX11 $(X_EXTRA_LIBS) $(DL_LIBS) ../../libs/video/targets/libQFglx.la $(client_POST_LIBS)
nq_glx_DEPENDENCIES= $(client_LIB_DEPS)

# ... Simple Directmedia Layer, version 1.1 and higher, in GL mode
nq_sgl_SOURCES=	$(combined_SOURCES) $(ogl_SOURCES)
nq_sgl_LDADD=	$(client_LIBS) $(X_LIBS) $(SDL_LIBS) $(GLX_LIBS) $(DL_LIBS) ../../libs/video/targets/libQFsgl.la $(client_POST_LIBS)
nq_sgl_DEPENDENCIES= $(client_LIB_DEPS)


# ... SGI/Microsoft WGL (Windows OpenGL)
nq_wgl_SOURCES=	$(combined_SOURCES) $(ogl_SOURCES) conproc.c
nq_wgl_LDADD=	$(client_LIBS) ../../libs/video/targets/libQFwgl.la $(client_POST_LIBS) $(GLX_LIBS) -lgdi32 -lcomctl32 -lwinmm
nq_wgl_DEPENDENCIES= $(client_LIB_DEPS)

# Dedicated Server
ded_SOURCES=	sys_unixd.c sv_ded.c

nq_server_SOURCES=	$(common_SOURCES) $(server_SOURCES) $(ded_SOURCES)
nq_server_LDADD=	libqfnet.a
nq_server_DEPENDENCIES=	libqfnet.a

# Stuff that doesn't get linked into an executable NEEDS to be mentioned here,
# or it won't be distributed with 'make dist'
EXTRA_DIST=	#nq.dsp sys_unix.c sys_win.c

# Kill the temp files, hopefully.
CLEANFILES  = *.i *.s $(YACCLEX_CLEANFILES)
