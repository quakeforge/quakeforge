AUTOMAKE_OPTIONS= foreign

AM_CFLAGS= @PREFER_PIC@
INCLUDES= -I$(top_srcdir)/include

noinst_LTLIBRARIES=	libsw32.la

sw32_src= \
	d_edge.c d_fill.c d_init.c d_modech.c d_part.c d_polyse.c d_scan.c \
	d_sky.c d_sprite.c d_surf.c d_vars.c d_zpoint.c draw.c screen.c \
	sw32_graph.c sw32_raclip.c sw32_ralias.c sw32_rbsp.c sw32_rdraw.c \
	sw32_redge.c sw32_riqm.c sw32_rmain.c sw32_rmisc.c sw32_rpart.c \
	sw32_rsky.c sw32_rsprite.c sw32_rsurf.c \
	vid_common_sw32.c

libsw32_la_SOURCES= $(sw32_src)

EXTRA_DIST= $(sw32_src)
