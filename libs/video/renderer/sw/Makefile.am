AUTOMAKE_OPTIONS= foreign

AM_CFLAGS= @PREFER_PIC@
CCASFLAGS+= @PREFER_PIC@
INCLUDES= -I$(top_srcdir)/include

asm = @swrend_libs@

noinst_LTLIBRARIES=	libsw.la $(asm)
EXTRA_LTLIBRARIES=libswrend_asm.la

asm_src= \
	d_draw.S d_parta.S d_polysa.S d_scana.S d_spr8.S d_varsa.S fpua.S \
	surf8.S sw_raclipa.S sw_raliasa.S sw_rdrawa.S sw_redgea.S \
	sw_rvarsa.S transform.S

sw_src= \
	d_edge.c d_fill.c d_init.c d_modech.c d_part.c d_polyse.c d_scan.c \
	d_sky.c d_sprite.c d_surf.c d_vars.c d_zpoint.c draw.c fpu.c nonintel.c \
	screen.c sw_graph.c sw_raclip.c sw_ralias.c sw_rbsp.c sw_rdraw.c \
	sw_redge.c sw_rmain.c sw_rmisc.c sw_rpart.c sw_rsky.c sw_rsprite.c \
	sw_rsurf.c

libswrend_asm_la_LDFLAGS= @STATIC@
libswrend_asm_la_SOURCES= $(asm_src)

libsw_la_LDFLAGS= @STATIC@
libsw_la_SOURCES= $(sw_src)
libsw_la_LIBADD= $(asm)
libsw_la_DEPENDENCIES= $(asm)

EXTRA_DIST= $(sw_src) $(asm_src)
