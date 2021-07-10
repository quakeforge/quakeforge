/*
	d_iface.h

	Interface header file for rasterization driver modules

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

#ifndef _D_IFACE_H
#define _D_IFACE_H

#include "QF/iqm.h"
#include "QF/mathlib.h"
#include "QF/model.h"
#include "QF/render.h"
#include "QF/vid.h"

#define WARP_WIDTH		320
#define WARP_HEIGHT		200

#define MAX_LBM_HEIGHT	1024

typedef struct {
	struct tex_s **skins;
	iqmblend_t *blend_palette;	// includes base data from iqm
	int         palette_size;	// includes base data from iqm
	iqmvertexarray *position;
	iqmvertexarray *texcoord;
	iqmvertexarray *normal;
	iqmvertexarray *bindices;
} swiqm_t;

typedef struct
{
	float	u, v;
	float	s, t;
	float	zi;
} emitpoint_t;

typedef enum {
	part_tex_dot,
	part_tex_spark,
	part_tex_smoke,
} ptextype_t;

typedef struct particle_s particle_t;
typedef void (*pt_phys_func)(particle_t *);

pt_phys_func R_ParticlePhysics (ptype_t type) __attribute__((pure));

// !!! if this is changed, it must be changed in d_ifacea.h too !!!
struct particle_s
{
// driver-usable fields
	vec3_t		org;
	int			color;
	float		alpha;
	ptextype_t	tex;
	float		scale;
// drivers never touch the following fields
	vec3_t		vel;
	ptype_t		type;
	float		die;
	float		ramp;
	pt_phys_func phys;
	particle_t *next;
};

#define PARTICLE_Z_CLIP	8.0

typedef struct polyvert_s {
	float	u, v, zi, s, t;
} polyvert_t;

typedef struct polydesc_s {
	int			numverts;
	float		nearzi;
	msurface_t	*pcurrentface;
	polyvert_t	*pverts;
} polydesc_t;

// !!! if this is changed, it must be changed in d_ifacea.h too !!!
typedef struct finalvert_s {
	int		v[6];		// u, v, s, t, l, 1/z
	int		flags;
	float	reserved;
} finalvert_t;

// !!! if this is changed, it must be changed in d_ifacea.h too !!!
typedef struct
{
	void				*pskin;
	int					skinwidth;
	int					skinheight;
	mtriangle_t			*ptriangles;
	finalvert_t			*pfinalverts;
	int					numtriangles;
	int					drawtype;
	int					seamfixupX16;
} affinetridesc_t;

// !!! if this is changed, it must be changed in d_ifacea.h too !!!
typedef struct {
	float	u, v, zi, color;
} screenpart_t;

typedef struct
{
	int			nump;
	emitpoint_t	*pverts;	// there's room for an extra element at [nump],
							//  if the driver wants to duplicate element [0] at
							//  element [nump] to avoid dealing with wrapping
	mspriteframe_t	*pspriteframe;
	vec3_t			vup, vright, vpn;	// in worldspace
	float			nearzi;
} spritedesc_t;

typedef struct
{
	int		u, v;
	float	zi;
	int		color;
} zpointdesc_t;

extern struct cvar_s	*r_drawflat;
extern int		r_framecount;		// sequence # of current frame since Quake
									//  started
extern qboolean	r_drawpolys;		// 1 if driver wants clipped polygons
									//  rather than a span list
extern qboolean	r_drawculledpolys;	// 1 if driver wants clipped polygons that
									//  have been culled by the edge list
extern qboolean	r_worldpolysbacktofront;	// 1 if driver wants polygons
											//  delivered back to front rather
											//  than front to back
extern qboolean	r_recursiveaffinetriangles;	// true if a driver wants to use
											//  recursive triangular subdivison
											//  and vertex drawing via
											//  D_PolysetDrawFinalVerts() past
											//  a certain distance (normally
											//  used only by the software
											//  driver)
extern int		r_pixbytes;
extern qboolean	r_dowarp;

extern affinetridesc_t	r_affinetridesc;
extern spritedesc_t		r_spritedesc;
extern zpointdesc_t		r_zpointdesc;

extern int		d_con_indirect;	// if 0, Quake will draw console directly
								//  to vid.buffer; if 1, Quake will
								//  draw console via D_DrawRect. Must be
								//  defined by driver

extern vec3_t	r_pright, r_pup, r_ppn;


void D_Aff8Patch (void *pcolormap);
void D_BeginDirectRect (int x, int y, byte *pbitmap, int width, int height);
void D_DisableBackBufferAccess (void);
void D_EndDirectRect (int x, int y, int width, int height);
void D_PolysetDraw (void);
void D_PolysetDrawFinalVerts (finalvert_t *fv, int numverts);
void D_PolysetSetEdgeTable (void);
void D_DrawParticle (particle_t *pparticle);
void D_DrawPoly (void);
void D_DrawSprite (void);
void D_DrawSurfaces (void);
void D_DrawZPoint (void);
void D_EnableBackBufferAccess (void);
void D_Init (void);
void D_Init_Cvars (void);
void D_ViewChanged (void);
void D_SetupFrame (void);
void D_TurnZOn (void);
void D_WarpScreen (void);

void D_FillRect (vrect_t *vrect, int color);
void D_DrawRect (void);
void D_UpdateRects (vrect_t *prect);

// currently for internal use only, and should be a do-nothing function in
// hardware drivers
// FIXME: this should go away
void D_PolysetUpdateTables (void);

// these are currently for internal use only, and should not be used by drivers
extern byte				*r_skysource;

// transparency types for D_DrawRect ()
#define DR_SOLID		0
#define DR_TRANSPARENT	1

// !!! must be kept the same as in quakeasm.h !!!
#define TRANSPARENT_COLOR	0xFF

extern void *acolormap;	// FIXME: should go away

//=======================================================================//

// callbacks to Quake

typedef struct
{
	byte		*surfdat;	// destination for generated surface
	int			rowbytes;	// destination logical width in bytes
	msurface_t	*surf;		// description for surface to generate
	fixed8_t	lightadj[MAXLIGHTMAPS];
							// adjust for lightmap levels for dynamic lighting
	texture_t	*texture;	// corrected for animating textures
	int			surfmip;	// mipmapped ratio of surface texels / world pixels
	int			surfwidth;	// in mipmapped texels
	int			surfheight;	// in mipmapped texels
} drawsurf_t;

extern drawsurf_t	r_drawsurf;

void R_DrawSurface (void);
void R_GenTile (msurface_t *psurf, void *pdest);

// !!! if this is changed, it must be changed in d_iface.h too !!!
#define CACHE_SIZE		32		// used to align key data structures

// !!! if this is changed, it must be changed in d_ifacea.h too !!!
#define TURB_TEX_SIZE	64		// base turbulent texture size

// !!! if this is changed, it must be changed in d_ifacea.h too !!!
#define	CYCLE			128		// turbulent cycle size

#define TILE_SIZE		128		// size of textures generated by R_GenTiledSurf

#define SKYSHIFT		7
#define	SKYSIZE			(1 << SKYSHIFT)
#define SKYMASK			(SKYSIZE - 1)

extern float	d_zitable[65536];

extern float	r_skyspeed;
extern float	r_skytime;

extern int		c_surf;

extern byte		*r_warpbuffer;

#endif // _D_IFACE_H
