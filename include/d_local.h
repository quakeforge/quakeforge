/*
	d_local.h

	Private rasterization driver defs

	Copyright (C) 1996-1997  Id Software, Inc.

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

*/

#ifndef _D_LOCAL_H
#define _D_LOCAL_H

#include "QF/model.h"
#include "QF/qtypes.h"

//
// TODO: fine-tune this; it's based on providing some overage even if there
// is a 2k-wide scan, with subdivision every 8, for 256 spans of 12 bytes each
//
#define SCANBUFFERPAD		0x1000

#define R_SKY_SMASK	0x007F0000
#define R_SKY_TMASK	0x007F0000

#define DS_SPAN_LIST_END	-128

#define SURFCACHE_SIZE_AT_320X200	600*1024

typedef struct surfcache_s
{
	struct surfcache_s	*next;
	struct surfcache_s 	**owner;		// NULL is an empty chunk of memory
	int					lightadj[MAXLIGHTMAPS]; // checked for strobe flush
	int					dlight;
	int					size;		// including header
	unsigned int		width;
	unsigned int		height;		// DEBUG needed only for debug
	float				mipscale;
	struct texture_s	*texture;	// checked for animating textures
	byte				data[4];	// width*height elements
} surfcache_t;

// !!! if this is changed, it must be changed in asm_draw.h too !!!
typedef struct sspan_s {
	int				u, v, count;
} sspan_t;

extern struct cvar_s	*d_subdiv16;

extern float	scale_for_mip;

extern qboolean		d_roverwrapped;
extern surfcache_t	*sc_rover;
extern surfcache_t	*d_initial_rover;

extern float	d_sdivzstepu, d_tdivzstepu, d_zistepu;
extern float	d_sdivzstepv, d_tdivzstepv, d_zistepv;
extern float	d_sdivzorigin, d_tdivzorigin, d_ziorigin;

extern float d_skyoffs;

extern fixed16_t       sadjust, tadjust;
extern fixed16_t       bbextents, bbextentt;

// FIXME: Better way of handling D_DrawSpans depths?
struct espan_s;
void D_DrawSpans (struct espan_s *pspans);
void D_DrawSpans8 (struct espan_s *pspans);
void D_DrawSpans16 (struct espan_s *pspans);
void D_DrawZSpans (struct espan_s *pspans);
void Turbulent (struct espan_s *pspan);
void D_SpriteDrawSpans (sspan_t *pspan);

void D_DrawSkyScans (struct espan_s *pspan);

void R_ShowSubDiv (void);
extern void (*prealspandrawer)(void);
struct entity_s;
surfcache_t	*D_CacheSurface (struct entity_s *ent,
							 msurface_t *surface, int miplevel);

int D_MipLevelForScale (float scale) __attribute__((pure));

#ifdef USE_INTEL_ASM
void D_PolysetAff8Start (void);
void D_PolysetAff8End (void);
#endif

extern byte	*d_viewbuffer;
extern int   d_rowbytes;
extern unsigned d_height;

extern short *d_zbuffer;
extern int	 d_zrowbytes;
extern unsigned d_zwidth;

extern int	*d_pscantable;
extern int	 d_scantable[];

extern int	 d_vrectx, d_vrecty, d_vrectright_particle, d_vrectbottom_particle;

extern int	 d_y_aspect_shift, d_pix_min, d_pix_max, d_pix_shift;

extern short *zspantable[];

extern int	 d_minmip;
extern float d_scalemip[3];

extern void (*d_drawspans) (struct espan_s *pspan);

void D_RasterizeAliasPolySmooth (void);
void D_PolysetCalcGradients (int skinwidth);
void D_PolysetScanLeftEdge (int height);
struct spanpackage_s;
void D_PolysetDrawSpans8 (struct spanpackage_s * pspanpackage);
void D_DrawTurbulent8Span (void);

#endif	// _D_LOCAL_H
