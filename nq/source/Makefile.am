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
AM_CFLAGS= @PTHREAD_CFLAGS@

bin_PROGRAMS=	@NQ_TARGETS@

EXTRA_PROGRAMS= nq-fbdev nq-sdl nq-svga nq-wgl nq-x11 nq-server

noinst_LIBRARIES= @nq_libs@
EXTRA_LIBRARIES=libnq_client.a libnq_common.a libnq_sdl.a libnq_server.a

libnq_common_a_SOURCES=game.c world.c
libnq_sdl_a_SOURCES=	sys_sdl.c
libnq_sdl_a_CFLAGS=	$(SDL_CFLAGS)

common_ldflags= -export-dynamic @PTHREAD_LDFLAGS@

cl_plugin_LIBS= \
		@server_static_plugin_libs@ \
		@client_static_plugin_libs@

client_LIBFILES= \
		$(top_builddir)/libs/video/targets/libQFjs.la \
		$(top_builddir)/libs/audio/libQFcd.la \
		$(top_builddir)/libs/audio/libQFsound.la

server_LIBFILES= \
		@server_static_plugin_libs@ \
		$(top_builddir)/libs/models/libQFmodels.la

common_LIBFILES= \
		$(top_builddir)/libs/net/libnet_main.la \
		$(top_builddir)/libs/console/libQFconsole.la \
		$(top_builddir)/libs/image/libQFimage.la \
		$(top_builddir)/libs/gib/libQFgib.la \
		$(top_builddir)/libs/ruamoko/libQFruamoko.la \
		$(top_builddir)/libs/util/libQFutil.la

client_LIBS=	$(client_LIBFILES) $(common_LIBFILES)

server_LIBS=	$(server_LIBFILES) $(common_LIBFILES) $(NET_LIBS)
server_LIB_DEPS=$(server_LIBFILES) $(common_LIBFILES)

libnq_client_a_SOURCES= \
		cl_cam.c cl_cmd.c cl_demo.c cl_ents.c cl_input.c cl_main.c \
		cl_screen.c cl_parse.c cl_tent.c cl_view.c sbar.c

libnq_server_a_SOURCES= \
		host.c host_cmd.c sv_cl_phys.c sv_cvar.c sv_main.c \
		sv_move.c sv_phys.c sv_pr_cmds.c sv_progs.c sv_user.c

client_libs= libnq_server.a libnq_client.a libnq_common.a
server_libs = libnq_server.a libnq_common.a

# Software-rendering targets

# ... Linux FBDev
nq_fbdev_libs=  \
			$(client_libs) \
			$(cl_plugin_LIBS) \
			$(top_builddir)/libs/video/renderer/libQFrenderer.la \
			$(top_builddir)/libs/models/libQFmodels.la \
			$(top_builddir)/libs/video/targets/libQFfbdev.la \
			$(client_LIBS)
nq_fbdev_SOURCES=	sys_unix.c
nq_fbdev_LDADD= $(nq_fbdev_libs) $(NET_LIBS)
nq_fbdev_LDFLAGS=	$(common_ldflags)
nq_fbdev_DEPENDENCIES=	$(nq_fbdev_libs)

# ... SDL, version 1.2 and higher
nq_sdl_libs=  \
			libnq_sdl.a \
			$(client_libs) \
			$(cl_plugin_LIBS) \
			$(top_builddir)/libs/video/renderer/libQFrenderer.la \
			$(top_builddir)/libs/models/libQFmodels.la \
			$(top_builddir)/libs/video/targets/libQFsdl.la \
			$(client_LIBS)
nq_sdl_SOURCES=sdl_link.c
nq_sdl_LDADD=		$(nq_sdl_libs) $(SDL_LIBS) $(NET_LIBS)
nq_sdl_LDFLAGS= 	$(common_ldflags)
nq_sdl_DEPENDENCIES=	$(nq_sdl_libs)

# ... Linux SVGAlib
nq_svga_libs=  \
			$(client_libs) \
			$(cl_plugin_LIBS) \
			$(top_builddir)/libs/video/renderer/libQFrenderer.la \
			$(top_builddir)/libs/models/libQFmodels.la \
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
			$(top_builddir)/libs/video/renderer/libQFrenderer.la \
			$(top_builddir)/libs/models/libQFmodels.la \
			$(top_builddir)/libs/video/targets/libQFx11.la \
			$(client_LIBS)
nq_x11_SOURCES= 	sys_unix.c
nq_x11_LDADD=		$(nq_x11_libs) \
			$(VIDMODE_LIBS) $(DGA_LIBS) $(X_LIBS) -lX11 \
			$(X_EXTRA_LIBS) $(X_SHM_LIB) $(NET_LIBS)
nq_x11_LDFLAGS= 	$(common_ldflags)
nq_x11_DEPENDENCIES=	$(nq_x11_libs)

# OpenGL-using targets
							
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
