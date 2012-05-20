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
INCLUDES=	-I$(top_srcdir)/include -I$(top_srcdir)/qw/include @LIBCURL_CFLAGS@
SDL_LIBS = @SDL_LIBS@
AM_CFLAGS= @PTHREAD_CFLAGS@

bin_PROGRAMS=	@QW_TARGETS@

EXTRA_PROGRAMS= \
	qw-client-fbdev qw-client-sdl qw-client-svga qw-client-wgl qw-client-x11 \
	qw-server qw-master

noinst_LIBRARIES= @qw_libs@
EXTRA_LIBRARIES=libqw_client.a libqw_common.a libqw_sdl.a libqw_server.a


libqw_common_a_SOURCES=\
	game.c map_cfg.c pmove.c pmovetst.c net_packetlog.c
libqw_sdl_a_SOURCES=cl_sys_sdl.c
libqw_sdl_a_CFLAGS=$(SDL_CFLAGS)

common_ldflags= -export-dynamic @PTHREAD_LDFLAGS@

# Server builds
#
# ... System type
if SYSTYPE_WIN32
syssv_SRC=	sv_sys_win.c
else
syssv_SRC=	sv_sys_unix.c
endif

EXTRA_DIST=sv_sys_win.c sv_sys_unix.c

libqw_server_a_SOURCES= \
	crudefile.c sv_ccmds.c sv_demo.c sv_ents.c sv_gib.c sv_init.c sv_main.c \
	sv_move.c sv_phys.c sv_pr_cmds.c sv_pr_cpqw.c sv_pr_qwe.c sv_progs.c \
	sv_qtv.c sv_recorder.c sv_sbar.c sv_send.c sv_user.c world.c

qw_server_LIBS= \
		@server_static_plugin_libs@ \
		$(top_builddir)/libs/qw/libqw.a \
		$(top_builddir)/libs/net/libnet_chan.la \
		$(top_builddir)/libs/models/libQFmodels.la \
		$(top_builddir)/libs/image/libQFimage.la \
		$(top_builddir)/libs/gib/libQFgib.la \
		$(top_builddir)/libs/ruamoko/libQFruamoko.la \
		$(top_builddir)/libs/console/libQFconsole.la \
		$(top_builddir)/libs/util/libQFutil.la

qw_server_deps=libqw_server.a libqw_common.a $(qw_server_LIBS)
qw_server_SOURCES=		$(syssv_SRC)
qw_server_LDADD=		$(qw_server_deps) $(NET_LIBS) $(DL_LIBS) $(CURSES_LIBS)
qw_server_LDFLAGS=		$(common_ldflags)
qw_server_DEPENDENCIES=	$(qw_server_deps)

qw_master_deps=$(top_builddir)/libs/util/libQFutil.la
qw_master_SOURCES=		master.c
qw_master_LDADD=		$(qw_master_deps) $(NET_LIBS)
qw_master_DEPENDENCIES=	$(qw_master_deps)
qw_master_LDFLAGS=		$(common_ldflags)

cl_plugin_LIBS= \
		@client_static_plugin_libs@

qw_client_LIBS= \
		$(top_builddir)/libs/qw/libqw.a \
		$(top_builddir)/libs/net/libnet_chan.la \
		$(top_builddir)/libs/console/libQFconsole.la \
		$(top_builddir)/libs/video/targets/libQFjs.la \
		$(top_builddir)/libs/audio/libQFcd.la \
		$(top_builddir)/libs/audio/libQFsound.la \
		$(top_builddir)/libs/image/libQFimage.la \
		$(top_builddir)/libs/gib/libQFgib.la \
		$(top_builddir)/libs/ruamoko/libQFruamoko.la \
		$(top_builddir)/libs/util/libQFutil.la

client_LIBS= $(qw_client_LIBS)
client_libs= libqw_client.a libqw_common.a

libqw_client_a_SOURCES= \
		cl_cam.c cl_chase.c cl_chat.c cl_cmd.c cl_cvar.c cl_demo.c \
		cl_entparse.c \
		cl_ents.c cl_http.c cl_input.c cl_main.c cl_ngraph.c cl_parse.c \
		cl_pred.c cl_screen.c cl_skin.c cl_slist.c cl_tent.c cl_view.c \
		locs.c sbar.c teamplay.c

# Software-rendering clients

# ... Linux FBDev
qw_client_fbdev_libs= \
			$(client_libs) \
			$(cl_plugin_LIBS) \
			$(top_builddir)/libs/video/renderer/libQFrenderer.la \
			$(top_builddir)/libs/models/libQFmodels.la \
			$(top_builddir)/libs/video/targets/libQFfbdev.la \
			$(client_LIBS)
qw_client_fbdev_SOURCES= cl_sys_unix.c
qw_client_fbdev_LDADD= $(qw_client_fbdev_libs) $(NET_LIBS) $(LIBCURL_LIBS)
qw_client_fbdev_LDFLAGS= $(common_ldflags)
qw_client_fbdev_DEPENDENCIES= $(qw_client_fbdev_libs)

# ... Simple DirectMedia Layer, version 1.2 and higher
qw_client_sdl_libs= \
			libqw_sdl.a \
			$(client_libs) \
			$(cl_plugin_LIBS) \
			$(top_builddir)/libs/video/renderer/libQFrenderer.la \
			$(top_builddir)/libs/models/libQFmodels.la \
			$(top_builddir)/libs/video/targets/libQFsdl.la \
			$(client_LIBS)
qw_client_sdl_SOURCES=sdl_link.c
qw_client_sdl_LDADD= libqw_sdl.a $(qw_client_sdl_libs) $(SDL_LIBS) $(NET_LIBS) $(LIBCURL_LIBS)
qw_client_sdl_LDFLAGS=  $(common_ldflags)
qw_client_sdl_DEPENDENCIES= libqw_sdl.a $(qw_client_sdl_libs)

# ... Linux SVGAlib
qw_client_svga_libs= \
			$(client_libs) \
			$(cl_plugin_LIBS) \
			$(top_builddir)/libs/video/renderer/libQFrenderer.la \
			$(top_builddir)/libs/models/libQFmodels.la \
			$(top_builddir)/libs/video/targets/libQFsvga.la \
			$(client_LIBS)
qw_client_svga_SOURCES= cl_sys_unix.c
qw_client_svga_LDADD= $(qw_client_svga_libs) $(SVGA_LIBS) $(NET_LIBS) $(LIBCURL_LIBS)
qw_client_svga_LDFLAGS=  $(common_ldflags)
qw_client_svga_DEPENDENCIES= $(qw_client_svga_libs)

# ... X11
qw_client_x11_libs= \
			$(client_libs) \
			$(cl_plugin_LIBS) \
			$(top_builddir)/libs/video/renderer/libQFrenderer.la \
			$(top_builddir)/libs/models/libQFmodels.la \
			$(top_builddir)/libs/video/targets/libQFx11.la \
			$(client_LIBS)
qw_client_x11_SOURCES=	cl_sys_unix.c
qw_client_x11_LDADD= $(qw_client_x11_libs) \
			$(VIDMODE_LIBS) $(DGA_LIBS) $(X_LIBS) -lX11 \
			$(X_EXTRA_LIBS) $(X_SHM_LIB) $(NET_LIBS) $(LIBCURL_LIBS)
qw_client_x11_LDFLAGS=	$(common_ldflags)
qw_client_x11_DEPENDENCIES= $(qw_client_x11_libs)

# ... SGI/Microsoft WGL (Windows OpenGL)
qw_client_wgl_libs= \
			$(client_libs) \
			$(cl_plugin_LIBS) \
			$(opengl_LIBS) \
			$(top_builddir)/libs/video/targets/libQFwgl.la \
			$(client_LIBS)
qw_client_wgl_SOURCES=	cl_sys_win.c
qw_client_wgl_LDADD= $(qw_client_wgl_libs) -lgdi32 -lwinmm $(NET_LIBS) $(LIBCURL_LIBS)
qw_client_wgl_LDFLAGS=  $(common_ldflags)
qw_client_wgl_DEPENDENCIES= $(qw_client_wgl_libs)

# Stuff that doesn't get linked into an executable NEEDS to be mentioned here,
# or it won't be distributed with 'make dist'

# Kill the temp files, hopefully.
CLEANFILES	= *.i *.s $(YACCLEX_CLEANFILES)
