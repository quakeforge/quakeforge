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

#
# Stuff that is common to both client and server
#
INCLUDES=	-I$(top_srcdir)/include -I$(top_srcdir)/nq/include $(GGI_CFLAGS) $(MGL_CFLAGS) $(SDL_CFLAGS) $(SVGA_CFLAGS) $(X_CFLAGS) $(GLX_CFLAGS) $(TDFXGL_CFLAGS)

bin_PROGRAMS =	@NQ_TARGETS@

EXTRA_PROGRAMS= nq-ggi nq-mgl nq-sdl nq-svga nq-x11 \
		nq-3dfx nq-glx nq-sgl nq-wgl nq-ded

noinst_LIBRARIES= libqfsys.a libqfsnd.a libqfcd.a libqfjs.a libqfnet.a

#if ASM_ARCH
math_ASM= 	math.S cl_math.S
soft_ASM=	d_draw.S d_draw16.S d_parta.S d_polysa.S d_scana.S d_spr8.S \
		d_varsa.S r_aclipa.S r_aliasa.S r_drawa.S r_edgea.S r_varsa.S \
		surf16.S surf8.S
sound_ASM=	snd_mixa.S
common_ASM=	sys_ia32.S worlda.S $(math_ASM)
#endif

common_SOURCES= crc.c cvar.c cmd.c mathlib.c wad.c world.c \
		model.c model_alias.c model_brush.c model_sprite.c \
		msg.c sizebuf.c qendian.c qargs.c quakefs.c \
		va.c quakeio.c link.c com.c \
		zone.c $(common_ASM)

#
# ... System type
#
if SYSTYPE_WIN32
libqfsys_a_SOURCES=	sys_win.c fnmatch.c
else
libqfsys_a_SOURCES=	sys_unix.c
endif
EXTRA_libqfsys_a_SOURCES= dirent.c fnmatch.c sys_unix.c sys_win.c

#
# ... Digital audio
#
if SNDTYPE_ALSA_0_5
libqfsnd_a_SOURCES=	snd_dma.c snd_mem.c snd_mix.c snd_alsa_0_5.c $(sound_ASM)
endif
if SNDTYPE_ALSA_0_6
libqfsnd_a_SOURCES=	snd_dma.c snd_mem.c snd_mix.c snd_alsa_0_6.c $(sound_ASM)
endif
if SNDTYPE_MME
libqfsnd_a_SOURCES=	snd_dma.c snd_mem.c snd_mix.c snd_mme.c $(sound_ASM)
endif
if SNDTYPE_OSS
libqfsnd_a_SOURCES=	snd_dma.c snd_mem.c snd_mix.c snd_oss.c $(sound_ASM)
endif
if SNDTYPE_SUN
libqfsnd_a_SOURCES=	snd_dma.c snd_mem.c snd_mix.c snd_sun.c $(sound_ASM)
endif
if SNDTYPE_WIN32
libqfsnd_a_SOURCES=	snd_dma.c snd_mem.c snd_mix.c snd_win.c $(sound_ASM)
endif
if SNDTYPE_NULL
libqfsnd_a_SOURCES=	snd_null.c $(sound_ASM)
endif
EXTRA_libqfsnd_a_SOURCES= snd_dma.c snd_mem.c snd_mix.c snd_alsa_0_5.c \
			snd_alsa_0_6.c snd_oss.c snd_sun.c snd_win.c snd_null.c \
			$(sound_ASM)

#
# ... CD audio
#
if CDTYPE_WIN32
libqfcd_a_SOURCES=	cd_win.c
endif
if CDTYPE_LINUX
libqfcd_a_SOURCES=	cd_linux.c
endif
if CDTYPE_NULL
libqfcd_a_SOURCES=	cd_null.c
endif
EXTRA_libqfcd_a_SOURCES= cd_dos.c cd_win.c cd_linux.c cd_null.c

#
# ... Joystick
#
if JOYTYPE_LINUX
libqfjs_a_SOURCES=	joy_linux.c
endif
if JOYTYPE_NULL
libqfjs_a_SOURCES=	joy_null.c
endif
libqfjs_a_CFLAGS=      $(JOY_CFLAGS)
EXTRA_libqfjs_a_SOURCES= joy_linux.c joy_null.c

#
# ... Networking
#
libqfnet_a_SOURCES=	mdfour.c net_bsd.c checksum.c net_dgrm.c net_loop.c \
			net_main.c net_udp.c net_vcr.c

EXTRA_libqfcd_a_SOURCES=net_dos.c net_bw.c net_ipx.c net_mp.c net_ser.c \
			net_win.c net_wins.c net_wipx.c

client_LIBS=	-L. -lqfsys -lqfsnd -lqfcd -lqfjs -lqfnet $(SOUND_LIBS) $(NET_LIBS) $(Z_LIBS)

client_SOURCES= cl_cam.c cl_demo.c cl_input.c cl_main.c cl_parse.c \
		cl_tent.c console.c keys.c menu.c sbar.c r_part.c r_view.c \
		nonintel.c gib.c gib_instructions.c gib_vars.c \
		gib_interpret.c gib_modules.c gib_parse.c gib_stack.c vid.c

server_SOURCES= host.c host_cmd.c \
		pr_cmds.c pr_edict.c pr_exec.c \
		sv_main.c sv_move.c sv_phys.c sv_user.c

combined_SOURCES= $(common_SOURCES) $(client_SOURCES) $(server_SOURCES)

#
# Software-rendering targets
#
# ... Common stuff
#

soft_SOURCES=	d_edge.c d_fill.c d_init.c d_modech.c d_part.c d_polyse.c \
		d_scan.c d_sky.c d_sprite.c d_surf.c d_vars.c d_zpoint.c \
		draw.c r_aclip.c r_alias.c r_bsp.c r_draw.c r_edge.c r_efrag.c \
		r_light.c r_main.c r_misc.c sw_part.c r_sky.c r_sprite.c \
		r_surf.c r_vars.c screen.c sw_model_alias.c sw_model_brush.c \
		sw_model_sprite.c sw_view.c $(soft_ASM)

#
# ... GGI
#
ggi_SOURCES=		vid_ggi.c

nq_ggi_SOURCES=	$(combined_SOURCES) $(soft_SOURCES) $(ggi_SOURCES)
nq_ggi_LDADD=	$(client_LIBS) $(GGI_LIBS)
nq_ggi_DEPENDENCIES=libqfsys.a libqfsnd.a libqfcd.a libqfjs.a libqfnet.a

#
# ... SciTech MGL
#
mgl_SOURCES=		vid_mgl.c in_win.c

nq_mgl_SOURCES=	$(combined_SOURCES) $(soft_SOURCES) $(mgl_SOURCES)
nq_mgl_LDADD=	$(client_LIBS) $(MGL_LIBS)
nq_mgl_DEPENDENCIES=libqfsys.a libqfsnd.a libqfcd.a libqfjs.a libqfnet.a

#
# ... Sam Lantinga's Simple DirectMedia Layer, version 1.0 and higher
#
sdl_SOURCES=		vid_sdl.c in_sdl.c

nq_sdl_SOURCES=	$(combined_SOURCES) $(soft_SOURCES) $(sdl_SOURCES)
nq_sdl_LDADD=	$(client_LIBS) $(SDL_LIBS)
nq_sdl_DEPENDENCIES=libqfsys.a libqfsnd.a libqfcd.a libqfjs.a libqfnet.a

#
# ... Linux SVGAlib
#
svga_SOURCES=		d_copy.S vid_svgalib.c in_svgalib.c

nq_svga_SOURCES=	$(combined_SOURCES) $(soft_SOURCES) $(svga_SOURCES)
nq_svga_LDADD=	$(client_LIBS) $(SVGA_LIBS)
nq_svga_DEPENDENCIES=libqfsys.a libqfsnd.a libqfcd.a libqfjs.a libqfnet.a

#
# ... X11
#
x11_SOURCES=		in_x11.c context_x11.c dga_check.c

nq_x11_SOURCES=	$(combined_SOURCES) $(soft_SOURCES) $(x11_SOURCES) vid_x11.c
nq_x11_LDADD=	$(client_LIBS) $(X_PRE_LIBS) $(VIDMODE_LIBS) $(DGA_LIBS) $(X_LIBS) -lX11 $(X_EXTRA_LIBS) $(X_SHM_LIB)
nq_x11_DEPENDENCIES=libqfsys.a libqfsnd.a libqfcd.a libqfjs.a libqfnet.a


#
# OpenGL-using targets
#
# ... Common stuff
#
ogl_SOURCES=	gl_draw.c gl_mesh.c gl_part.c gl_refrag.c gl_rlight.c \
		gl_rmain.c gl_rmisc.c gl_rsurf.c gl_screen.c gl_view.c \
		gl_warp.c gl_model_alias.c gl_model_brush.c gl_model_fullbright.c \
		gl_model_sprite.c

#
# ... 3Dfx Voodoo 1 and 2 SVGAlib-based console GL
#
tdfx_SOURCES=	vid_3dfxsvga.c vid_common_gl.c in_svgalib.c

nq_3dfx_SOURCES=	$(combined_SOURCES) $(ogl_SOURCES) $(tdfx_SOURCES)
nq_3dfx_LDADD=	$(client_LIBS) $(TDFXGL_LIBS) $(SVGA_LIBS) $(DL_LIBS)
nq_3dfx_DEPENDENCIES=libqfsys.a libqfsnd.a libqfcd.a libqfjs.a libqfnet.a

#
# ... OpenGL in X Window
#
glx_SOURCES=	vid_glx.c vid_common_gl.c $(x11_SOURCES)

nq_glx_SOURCES=	$(combined_SOURCES) $(ogl_SOURCES) $(glx_SOURCES)
nq_glx_LDADD=	$(client_LIBS) $(GLX_LIBS) $(X_PRE_LIBS) $(VIDMODE_LIBS) $(DGA_LIBS) $(X_LIBS) -lX11 $(X_EXTRA_LIBS) $(DL_LIBS)
nq_glx_DEPENDENCIES=libqfsys.a libqfsnd.a libqfcd.a libqfjs.a libqfnet.a

#
# ... Sam Lantinga's Simple DirectMedia Layer, version 1.1 and higher, in GL mode
#
sgl_SOURCES=	vid_sgl.c vid_common_gl.c in_sdl.c

nq_sgl_SOURCES=	$(combined_SOURCES) $(ogl_SOURCES) $(sgl_SOURCES)
nq_sgl_LDADD=	$(client_LIBS) $(X_LIBS) $(SDL_LIBS) $(GLX_LIBS) $(DL_LIBS)
nq_sgl_DEPENDENCIES=libqfsys.a libqfsnd.a libqfcd.a libqfjs.a libqfnet.a

#
# SGI/Microsoft WGL (Windows OpenGL)
#
wgl_SOURCES=	vid_wgl.c

nq_wgl_SOURCES=	$(combined_SOURCES) $(ogl_SOURCES) $(wgl_SOURCES)
nq_wgl_LDADD=	$(client_LIBS)
nq_wgl_DEPENDENCIES=libqfsys.a libqfsnd.a libqfcd.a libqfjs.a libqfnet.a

#
# dedicated server
#
ded_SOURCES=sys_unixd.c sv_ded.c

nq_ded_SOURCES=$(common_SOURCES) $(server_SOURCES) $(ded_SOURCES)
nq_ded_LDADD= libqfnet.a $(Z_LIBS)
nq_ded_DEPENDENCIES=libqfnet.a

#
# Stuff that doesn't get linked into an executable NEEDS to be mentioned here,
# or it won't be distributed with 'make dist'
#
EXTRA_DIST=	#nq.dsp
