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

#
# Stuff that is common to both client and server
#
INCLUDES=	-I$(top_srcdir)/include -I$(top_srcdir)/qw/include $(GGI_CFLAGS) $(MGL_CFLAGS) $(SDL_CFLAGS) $(SVGA_CFLAGS) $(X_CFLAGS) $(GLX_CFLAGS) $(TDFXGL_CFLAGS)

bin_PROGRAMS =	@QW_TARGETS@

EXTRA_PROGRAMS= cougar-qw-cl-3dfx cougar-qw-cl-fbdev cougar-qw-cl-ggi \
		cougar-qw-cl-glx cougar-qw-cl-mgl cougar-qw-cl-sdl \
		cougar-qw-cl-sgl cougar-qw-cl-svga cougar-qw-cl-wgl \
		cougar-qw-cl-x11 cougar-qw-sv

noinst_LIBRARIES= libqfcd.a libqfjs.a libqfnet.a libqfsnd.a libqfsys_cl.a libqfsys_sv.a

if PACKETLOG
packetlogger = net_packetlog.c
endif

if ASM_ARCH
math_ASM = math.S sys_x86.S
endif
common_SOURCES= buildnum.c checksum.c cmd.c com.c crc.c cvar.c hash.c \
	info.c link.c mathlib.c mdfour.c model.c model_brush.c \
	msg.c pmove.c pmovetst.c qargs.c qendian.c quakefs.c quakeio.c \
	sizebuf.c va.c zone.c $(math_ASM) $(packetlogger)

#
# ... Network type
#
if NETTYPE_IPV6
libqfnet_a_SOURCES=	net_chan.c net_com.c net_udp6.c
else
libqfnet_a_SOURCES=	net_chan.c net_com.c net_udp.c
endif
EXTRA_libqfnet_a_SOURCES= net_chan.c net_com.c net_udp.c net_udp6.c

#
# Server builds
#
# ... System type
#
if SYSTYPE_WIN32
libqfsys_sv_a_SOURCES=	fnmatch.c dirent.c sv_sys_win.c sys_win.c
else
libqfsys_sv_a_SOURCES=	sv_sys_unix.c sys_unix.c
endif
EXTRA_libqfsys_sv_a_SOURCES= fnmatch.c dirent.c sv_sys_unix.c sv_sys_win.c sys_unix.c sys_win.c

if ASM_ARCH
world_ASM=	worlda.S
endif

server_SOURCES=	pr_edict.c pr_exec.c pr_offs.c sv_ccmds.c sv_cvar.c \
		sv_ents.c sv_init.c sv_main.c sv_misc.c sv_model.c \
		sv_move.c sv_nchan.c sv_phys.c sv_pr_cmds.c sv_progs.c sv_send.c \
		sv_user.c ver_check.c world.c $(world_ASM)

cougar_qw_sv_SOURCES=	$(common_SOURCES) $(server_SOURCES)
cougar_qw_sv_LDADD=	-L. -lqfnet -lqfsys_sv $(NET_LIBS) $(Z_LIBS) $(DL_LIBS)
cougar_qw_sv_DEPENDENCIES= libqfnet.a libqfsys_sv.a

#
# Client builds
#
# ... System type
#
if SYSTYPE_WIN32
libqfsys_cl_a_SOURCES=	cl_sys_win.c fnmatch.c dirent.c sys_win.c
else
libqfsys_cl_a_SOURCES=	cl_sys_unix.c sys_unix.c
endif
EXTRA_libqfsys_cl_a_SOURCES= cl_sys_sdl.c cl_sys_unix.c cl_sys_win.c sys_win.c sys_unix.c fnmatch.c dirent.c

#
# ... Digital audio
#
if SNDTYPE_ALSA_0_5
libqfsnd_a_SOURCES=	snd_dma.c snd_mem.c snd_mix.c snd_alsa_0_5.c
endif
if SNDTYPE_ALSA_0_6
libqfsnd_a_SOURCES=	snd_dma.c snd_mem.c snd_mix.c snd_alsa_0_6.c
endif
if SNDTYPE_MME
libqfsnd_a_SOURCES=	snd_dma.c snd_mem.c snd_mix.c snd_mme.c
endif
if SNDTYPE_OSS
libqfsnd_a_SOURCES=	snd_dma.c snd_mem.c snd_mix.c snd_oss.c
endif
if SNDTYPE_SDL
libqfsnd_a_SOURCES=	snd_dma.c snd_mem.c snd_mix.c snd_sdl.c
endif
if SNDTYPE_SGI
libqfsnd_a_SOURCES=	snd_dma.c snd_mem.c snd_mix.c snd_sgi.c
endif
if SNDTYPE_SUN
libqfsnd_a_SOURCES=	snd_dma.c snd_mem.c snd_mix.c snd_sun.c
endif
if SNDTYPE_WIN32
libqfsnd_a_SOURCES=	snd_dma.c snd_mem.c snd_mix.c snd_win.c
endif
if SNDTYPE_NULL
libqfsnd_a_SOURCES=	snd_null.c
endif
EXTRA_libqfsnd_a_SOURCES= snd_dma.c snd_mem.c snd_mix.c snd_alsa_0_5.c \
			snd_alsa_0_6.c snd_oss.c snd_sdl.c snd_sgi.c \
			snd_sun.c snd_win.c snd_null.c

#
# ... CD audio
#
if CDTYPE_LINUX
libqfcd_a_SOURCES=	cd_linux.c
endif
if CDTYPE_SDL
libqfcd_a_SOURCES=	cd_sdl.c
endif
if CDTYPE_SGI
libqfcd_a_SOURCES=	cd_sgi.c
endif
if CDTYPE_WIN32
libqfcd_a_SOURCES=	cd_win.c
endif
if CDTYPE_NULL
libqfcd_a_SOURCES=	cd_null.c
endif
EXTRA_libqfcd_a_SOURCES= cd_dos.c cd_linux.c cd_sdl.c cd_sgi.c cd_win.c cd_null.c

#
# ... Joystick
#
if JOYTYPE_LINUX
libqfjs_a_SOURCES=	joy_linux.c
endif
if JOYTYPE_WIN32
libqfjs_a_SOURCES=	joy_win.c
endif
if JOYTYPE_NULL
libqfjs_a_SOURCES=	joy_null.c
endif
libqfjs_a_CFLAGS=	$(JOY_CFLAGS)
EXTRA_libqfjs_a_SOURCES= joy_linux.c joy_win.c joy_null.c

CLIENT_LIBS=	-L. -lqfnet -lqfsys_cl -lqfsnd -lqfcd -lqfjs $(SOUND_LIBS) $(NET_LIBS) $(CD_LIBS) $(JOY_LIBS) $(Z_LIBS)

if ASM_ARCH
client_ASM=	snd_mixa.S cl_math.S
endif

client_SOURCES= cl_cam.c cl_cmd.c cl_cvar.c cl_demo.c cl_ents.c cl_input.c \
	cl_main.c cl_misc.c cl_parse.c cl_pred.c cl_slist.c cl_tent.c \
	console.c keys.c locs.c menu.c model_alias.c model_sprite.c nonintel.c \
	pcx.c r_view.c sbar.c skin.c teamplay.c tga.c wad.c vid.c $(client_ASM)

#
# Software-rendering clients
#
# ... Common stuff
#
if ASM_ARCH
soft_ASM=	d_draw.S d_draw16.S d_parta.S d_polysa.S d_scana.S d_spr8.S \
		d_varsa.S r_aclipa.S r_aliasa.S r_drawa.S r_edgea.S r_varsa.S \
		surf16.S surf8.S
endif

soft_SOURCES=	d_edge.c d_fill.c d_init.c d_modech.c \
		d_part.c d_polyse.c d_scan.c d_sky.c d_sprite.c d_surf.c \
		d_vars.c d_zpoint.c draw.c r_aclip.c r_alias.c r_bsp.c \
		r_draw.c r_edge.c r_efrag.c r_light.c r_main.c r_misc.c \
		r_part.c r_sky.c r_sprite.c r_surf.c r_vars.c sw_skin.c sw_view.c \
		screen.c $(soft_ASM) sw_model_alias.c sw_model_brush.c \
		sw_model_sprite.c

#
# ... Linux FBDev
#
YFLAGS = -d
fbdev_SOURCES=		fbset.c fbset_modes_y.y fbset_modes_l.l vid_fbdev.c in_fbdev.c
YACCLEX_CLEANFILES=	fbset_modes_y.c fbset_modes_y.h fbset_modes_y.tab.h fbset_modes_l.c
EXTRA_cougar_qw_cl_fbdev_SOURCES=fbset_modes_y.h

fbset_modes_y.o: fbset_modes_y.c
	$(CC) $(INCLUDES) $(CFLAGS) -Wno-error -c fbset_modes_y.c
fbset_modes_l.o: fbset_modes_l.c
	$(CC) $(INCLUDES) $(CFLAGS) -Wno-error -c fbset_modes_l.c

cougar_qw_cl_fbdev_SOURCES= $(common_SOURCES) $(client_SOURCES) $(soft_SOURCES) $(fbdev_SOURCES)
cougar_qw_cl_fbdev_LDADD= $(CLIENT_LIBS)
cougar_qw_cl_fbdev_DEPENDENCIES=libqfnet.a libqfsys_cl.a libqfsnd.a libqfcd.a libqfjs.a

#
# ... GGI
#
ggi_SOURCES=		vid_ggi.c in_ggi.c

cougar_qw_cl_ggi_SOURCES=	$(common_SOURCES) $(client_SOURCES) $(soft_SOURCES) $(ggi_SOURCES)
cougar_qw_cl_ggi_LDADD=	$(GGI_LIBS) $(CLIENT_LIBS)
cougar_qw_cl_ggi_DEPENDENCIES=libqfnet.a libqfsys_cl.a libqfsnd.a libqfcd.a libqfjs.a

#
# ... SciTech MGL
#
mgl_SOURCES=		vid_mgl.c in_win.c

cougar_qw_cl_mgl_SOURCES=	$(common_SOURCES) $(client_SOURCES) $(soft_SOURCES) $(mgl_SOURCES)
cougar_qw_cl_mgl_LDADD=	$(MGL_LIBS) $(CLIENT_LIBS)
cougar_qw_cl_mgl_DEPENDENCIES=libqfnet.a libqfsys_cl.a libqfsnd.a libqfcd.a libqfjs.a

#
# ... Sam Lantinga's Simple DirectMedia Layer, version 1.0 and higher
#
sdl_SOURCES=		vid_sdl.c in_sdl.c

cougar_qw_cl_sdl_SOURCES=	$(common_SOURCES) $(client_SOURCES) $(soft_SOURCES) $(sdl_SOURCES)
cougar_qw_cl_sdl_LDADD=	$(SDL_LIBS) $(CLIENT_LIBS)
cougar_qw_cl_sdl_DEPENDENCIES=libqfnet.a libqfsys_cl.a libqfsnd.a libqfcd.a libqfjs.a

#
# ... Linux SVGAlib
#
svga_SOURCES=		d_copy.S vid_svgalib.c in_svgalib.c

cougar_qw_cl_svga_SOURCES= $(common_SOURCES) $(client_SOURCES) $(soft_SOURCES) $(svga_SOURCES)
cougar_qw_cl_svga_LDADD=	$(SVGA_LIBS) $(CLIENT_LIBS)
cougar_qw_cl_svga_DEPENDENCIES=libqfnet.a libqfsys_cl.a libqfsnd.a libqfcd.a libqfjs.a

#
# ... X11
#
x11_SOURCES=		vid_x11.c in_x11.c context_x11.c dga_check.c

cougar_qw_cl_x11_SOURCES=	$(common_SOURCES) $(client_SOURCES) $(soft_SOURCES) $(x11_SOURCES)
cougar_qw_cl_x11_LDADD=	$(X_PRE_LIBS) $(VIDMODE_LIBS) $(DGA_LIBS) $(X_LIBS) -lX11 $(X_EXTRA_LIBS) $(X_SHM_LIB) $(CLIENT_LIBS)
cougar_qw_cl_x11_DEPENDENCIES=libqfnet.a libqfsys_cl.a libqfsnd.a libqfcd.a libqfjs.a


#
# OpenGL-using clients
#
# ... Common stuff
#
ogl_SOURCES=	fractalnoise.c gl_draw.c gl_dyn_fires.c gl_dyn_part.c \
		gl_dyn_textures.c gl_mesh.c gl_ngraph.c r_efrag.c \
		gl_rlight.c gl_rmain.c gl_rmisc.c gl_rsurf.c gl_screen.c \
		gl_skin.c gl_sky.c gl_sky_clip.c gl_view.c gl_warp.c \
		gl_model_alias.c gl_model_brush.c gl_model_fullbright.c \
		gl_model_sprite.c qfgl_ext.c

#
# ... 3Dfx Voodoo 1 and 2 SVGAlib-based console GL
#
tdfx_SOURCES=	vid_3dfxsvga.c vid_common_gl.c in_svgalib.c

cougar_qw_cl_3dfx_SOURCES=	$(common_SOURCES) $(client_SOURCES) $(ogl_SOURCES) $(tdfx_SOURCES)
cougar_qw_cl_3dfx_LDADD=	$(TDFXGL_LIBS) $(SVGA_LIBS) $(CLIENT_LIBS) $(DL_LIBS)
cougar_qw_cl_3dfx_DEPENDENCIES=libqfnet.a libqfsys_cl.a libqfsnd.a libqfcd.a libqfjs.a

#
# ... OpenGL in X Window
#
glx_SOURCES=	vid_glx.c vid_common_gl.c in_x11.c context_x11.c dga_check.c

cougar_qw_cl_glx_SOURCES=	$(common_SOURCES) $(client_SOURCES) $(ogl_SOURCES) $(glx_SOURCES)
cougar_qw_cl_glx_LDADD=	$(GLX_LIBS) $(X_PRE_LIBS) $(VIDMODE_LIBS) $(DGA_LIBS) $(X_LIBS) -lX11 $(X_EXTRA_LIBS) $(CLIENT_LIBS) $(DL_LIBS)
cougar_qw_cl_glx_DEPENDENCIES=libqfnet.a libqfsys_cl.a libqfsnd.a libqfcd.a libqfjs.a

#
# ... Sam Lantinga's Simple DirectMedia Layer, version 1.1 and higher, in GL mode
#
sgl_SOURCES=	vid_sgl.c vid_common_gl.c in_sdl.c

cougar_qw_cl_sgl_SOURCES=	$(common_SOURCES) $(client_SOURCES) $(ogl_SOURCES) $(sgl_SOURCES)
cougar_qw_cl_sgl_LDADD=	$(SDL_LIBS) $(X_LIBS) $(GLX_LIBS) $(CLIENT_LIBS) $(DL_LIBS)
cougar_qw_cl_sgl_DEPENDENCIES=libqfnet.a libqfsys_cl.a libqfsnd.a libqfcd.a libqfjs.a

#
# SGI/Microsoft WGL (Windows OpenGL)
#
wgl_SOURCES=	vid_wgl.c

cougar_qw_cl_wgl_SOURCES=	$(common_SOURCES) $(client_SOURCES) $(ogl_SOURCES) $(wgl_SOURCES)
cougar_qw_cl_wgl_LDADD=	$(CLIENT_LIBS)
cougar_qw_cl_wgl_DEPENDENCIES=libqfnet.a libqfsys_cl.a libqfsnd.a libqfcd.a libqfjs.a

#
# Stuff that doesn't get linked into an executable NEEDS to be mentioned here,
# or it won't be distributed with 'make dist'
#
EXTRA_DIST=	makefile.mgw makefile.win \
		qf-server.mak qw_server.dsp \
		qf-client-sdl.mak qf-client-win.mak qw_client.dsp \
		qf-client-sgl.mak qf-client-wgl.mak qf-client-win.mak

# Kill the temp files, hopefully.
CLEANFILES	= *.i *.s $(YACCLEX_CLEANFILES)
