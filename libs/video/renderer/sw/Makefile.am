INCLUDES= -I$(top_srcdir)/include

if BUILD_SW
SW = libsw.la
else
SW =
endif

noinst_LTLIBRARIES = $(SW)

libsw_la_LDFLAGS = -version-info 1:0:0
libsw_la_SOURCES = d_edge.c d_fill.c d_init.c d_modech.c d_part.c d_polyse.c \
    d_scan.c d_sky.c d_sprite.c d_surf.c d_vars.c d_zpoint.c draw.c \
	nonintel.c screen.c \
    sw_graph.c sw_raclip.c sw_ralias.c sw_rbsp.c sw_rdraw.c sw_redge.c \
    sw_rlight.c sw_rmain.c sw_rmisc.c sw_rpart.c sw_rsky.c sw_rsprite.c \
	sw_rsurf.c sw_skin.c \
    d_draw.S d_draw16.S d_parta.S d_polysa.S d_scana.S d_spr8.S \
    d_varsa.S surf16.S surf8.S sw_raclipa.S sw_raliasa.S sw_rdrawa.S \
    sw_redgea.S sw_rvarsa.S \
	transform.S
