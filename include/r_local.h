/*
	r_local.h

	private refresh defs

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

#ifndef _R_LOCAL_H
#define _R_LOCAL_H

#include "QF/iqm.h"
#include "QF/mathlib.h"
#include "QF/model.h"
#include "QF/render.h"
#include "QF/vid.h"
#include "QF/simd/mat4f.h"
#include "QF/simd/vec4f.h"
#include "r_shared.h"

#define ALIAS_BASE_SIZE_RATIO		(1.0 / 11.0)
					// normalizing factor so player model works out to about
					//  1 pixel per triangle

#define BMODEL_FULLY_CLIPPED	0x10 // value returned by R_BmodelCheckBBox ()
									 //  if bbox is trivially rejected

// viewmodel lighting =======================================================

typedef struct {
	int			ambientlight;
	int			shadelight;
	float		*plightvec;
} alight_t;

// clipped bmodel edges =====================================================

typedef struct bedge_s
{
	mvertex_t		*v[2];
	struct bedge_s	*pnext;
} bedge_t;

typedef struct {
	float	fv[3];		// viewspace x, y
} auxvert_t;

//===========================================================================

extern struct cvar_s	*r_speeds;
extern struct cvar_s	*r_timegraph;
extern struct cvar_s	*r_graphheight;
extern struct cvar_s	*r_clearcolor;
extern struct cvar_s	*r_waterwarp;
extern struct cvar_s	*r_fullbright;
extern struct cvar_s	*r_drawentities;
extern struct cvar_s	*r_aliasstats;
extern struct cvar_s	*r_dspeeds;
extern struct cvar_s	*r_drawflat;
extern struct cvar_s	*r_ambient;
extern struct cvar_s	*r_reportsurfout;
extern struct cvar_s	*r_maxsurfs;
extern struct cvar_s	*r_numsurfs;
extern struct cvar_s	*r_reportedgeout;
extern struct cvar_s	*r_maxedges;
extern struct cvar_s	*r_numedges;

extern float	cl_wateralpha;

#define XCENTERING	(1.0 / 2.0)
#define YCENTERING	(1.0 / 2.0)

#define CLIP_EPSILON		0.001

#define BACKFACE_EPSILON	0.01

//===========================================================================

#define	DIST_NOT_SET	98765

// !!! if this is changed, it must be changed in asm_draw.h too !!!
typedef struct clipplane_s {
	vec3_t		normal;
	float		dist;
	struct		clipplane_s	*next;
	byte		leftedge;
	byte		rightedge;
	byte		reserved[2];
} clipplane_t;

extern	clipplane_t	view_clipplanes[4];

//=============================================================================

void R_RenderWorld (void);

//=============================================================================

extern	plane_t	screenedge[4];

extern	vec3_t	r_entorigin;

extern	int		r_visframecount;

//=============================================================================

extern int	vstartscan;


void R_ClearPolyList (void);
void R_DrawPolyList (void);

//  Surface cache related ==========
extern qboolean	r_cache_thrash;	// set if thrashing the surface cache

// current entity info
extern	qboolean		insubmodel;
extern	vec3_t			r_worldmodelorg;

extern mat4f_t   glsl_projection;
extern mat4f_t   glsl_view;

void R_SetFrustum (void);

void R_SpriteBegin (void);
void R_SpriteEnd (void);
void R_DrawSprite (entity_t *ent);
void R_RenderFace (msurface_t *fa, int clipflags);
void R_RenderPoly (msurface_t *fa, int clipflags);
void R_RenderBmodelFace (bedge_t *pedges, msurface_t *psurf);
void R_TransformFrustum (void);
void R_SetSkyFrame (void);
void R_DrawSurfaceBlock (void);
texture_t *R_TextureAnimation (const entity_t *entity, msurface_t *surf) __attribute__((pure));

void R_GenSkyTile (void *pdest);
void R_SurfPatch (void);
void R_DrawSubmodelPolygons (model_t *pmodel, int clipflags);
void R_DrawSolidClippedSubmodelPolygons (model_t *pmodel);

void R_AddPolygonEdges (emitpoint_t *pverts, int numverts, int miplevel);
surf_t *R_GetSurf (void);
void R_AliasClipAndProjectFinalVert (finalvert_t *fv, auxvert_t *av);
void R_AliasDrawModel (entity_t *ent, alight_t *plighting);
void R_IQMDrawModel (entity_t *ent, alight_t *plighting);
maliasskindesc_t *R_AliasGetSkindesc (animation_t *animation, int skinnum, aliashdr_t *hdr);
maliasframedesc_t *R_AliasGetFramedesc (animation_t *animation, aliashdr_t *hdr);
float R_AliasGetLerpedFrames (animation_t *animation, aliashdr_t *hdr);
float R_IQMGetLerpedFrames (entity_t *ent, iqm_t *hdr);
iqmframe_t *R_IQMBlendFrames (const iqm_t *iqm, int frame1, int frame2,
							  float blend, int extra);
iqmframe_t *R_IQMBlendPalette (const iqm_t *iqm, int frame1, int frame2,
							   float blend, int extra,
							   iqmblend_t *blend_palette, int palette_size);
float R_EntityBlend (animation_t *animation, int pose, float interval);
void R_BeginEdgeFrame (void);
void R_ScanEdges (void);
void D_DrawSurfaces (void);
void R_InsertNewEdges (edge_t *edgestoadd, edge_t *edgelist);
void R_StepActiveU (edge_t *pedge);
void R_RemoveEdges (edge_t *pedge);
void R_AddTexture (texture_t *tex);
struct vulkan_ctx_s;
void R_ClearTextures (void);
void R_InitSurfaceChains (mod_brush_t *brush);

extern void R_Surf8Start (void);
extern void R_Surf8End (void);
extern void R_EdgeCodeStart (void);
extern void R_EdgeCodeEnd (void);

extern void R_RotateBmodel (void);

extern int	c_faceclip;
extern int	r_polycount;

extern model_t     *cl_worldmodel;

extern int		*pfrustum_indexes[4];

// !!! if this is changed, it must be changed in asm_draw.h too !!!
#define	NEAR_CLIP	0.01

extern fixed16_t	sadjust, tadjust;
extern fixed16_t	bbextents, bbextentt;

#define MAXBVERTINDEXES	1000	// new clipped vertices when clipping bmodels
								//  to the world BSP
extern mvertex_t	*r_ptverts, *r_ptvertsmax;

extern vec3_t			sbaseaxis[3], tbaseaxis[3];

extern int		r_currentkey;
extern int		r_currentbkey;

typedef struct btofpoly_s {
	int			clipflags;
	msurface_t	*psurf;
} btofpoly_t;

#define MAX_BTOFPOLYS	5000	// FIXME: tune this

extern int			numbtofpolys;

void	R_InitTurb (void);
void	R_ZDrawSubmodelPolys (model_t *clmodel);

// Alias models ===========================================

#define ALIAS_Z_CLIP_PLANE	5

extern int				numverts;
extern int				numtriangles;
extern float			leftclip, topclip, rightclip, bottomclip;
extern int				r_acliptype;
extern finalvert_t		*pfinalverts;
extern auxvert_t		*pauxverts;
extern float            ziscale;
extern float            aliastransform[3][4];

qboolean R_AliasCheckBBox (entity_t *ent);

// turbulence stuff =======================================

#define	AMP		8*0x10000
#define	AMP2	3
#define	SPEED	20

// particle stuff =========================================

void R_DrawParticles (void);
void R_InitParticles (void);
void R_ClearParticles (void);
void R_ReadPointFile_f (void);
void R_InitSprites (void);
void R_SurfacePatch (void);

// renderer stuff again ===================================

extern int		r_amodels_drawn;
extern edge_t	*auxedges;
extern int		r_numallocatededges;
extern edge_t	*r_edges, *edge_p, *edge_max;

extern	edge_t	*newedges[MAXHEIGHT];
extern	edge_t	*removeedges[MAXHEIGHT];

extern	int	screenwidth;

extern int		r_bmodelactive;
extern vrect_t	*pconupdate;

extern float		aliasxscale, aliasyscale, aliasxcenter, aliasycenter;
extern float		r_aliastransition, r_resfudge;

extern int		r_outofsurfaces;
extern int		r_outofedges;

extern mvertex_t	*r_pcurrentvertbase;
extern int			r_maxvalidedgeoffset;

void R_AliasClipTriangle (mtriangle_t *ptri);

extern float	r_time1;
extern int		r_frustum_indexes[4*6];
extern int		r_maxsurfsseen, r_maxedgesseen;
extern qboolean	r_dowarpold, r_viewchanged;

extern mleaf_t	*r_viewleaf;

extern int		r_clipflags;
extern int		r_dlightframecount;

extern struct entqueue_s *r_ent_queue;
struct dlight_s;

extern vec3_t lightspot;

void R_StoreEfrags (const efrag_t *ppefrag);
void R_TimeRefresh_f (void);
void R_TimeGraph (void);
void R_ZGraph (void);
void R_PrintAliasStats (void);
void R_PrintTimes (void);
void R_AnimateLight (void);
int R_LightPoint (mod_brush_t *brush, const vec3_t p);
void R_SetupFrame (void);
void R_cshift_f (void);
void R_EmitEdge (mvertex_t *pv0, mvertex_t *pv1);
void R_ClipEdge (mvertex_t *pv0, mvertex_t *pv1, clipplane_t *clip);
void R_RecursiveMarkLights (mod_brush_t *brush, const vec3_t lightorigin,
							struct dlight_s *light, int bit, mnode_t *node);
void R_MarkLights (const vec3_t lightorigin, struct dlight_s *light, int bit,
				   model_t *model);

void R_LoadSkys (const char *);
//void Vulkan_R_LoadSkys (const char *, struct vulkan_ctx_s *ctx);

void R_LowFPPrecision (void);
void R_HighFPPrecision (void);
void R_SetFPCW (void);

void R_AliasProjectFinalVert (finalvert_t *fv, auxvert_t *av);
void R_Alias_clip_left (finalvert_t *pfv0, finalvert_t *pfv1,
						finalvert_t *out);
void R_Alias_clip_right (finalvert_t *pfv0, finalvert_t *pfv1,
						 finalvert_t *out);
void R_Alias_clip_bottom (finalvert_t *pfv0, finalvert_t *pfv1,
						  finalvert_t *out);
void R_Alias_clip_top (finalvert_t *pfv0, finalvert_t *pfv1, finalvert_t *out);

void R_AliasTransformVector (vec3_t in, vec3_t out);
void R_AliasTransformFinalVert (finalvert_t *fv, trivertx_t *pverts,
								stvert_t *pstverts);
void R_AliasTransformAndProjectFinalVerts (finalvert_t *fv, stvert_t *pstverts);

void R_GenerateSpans (void);

void R_InitVars (void);

void R_LoadSky_f (void);

extern byte crosshair_data[];
#define CROSSHAIR_WIDTH 8
#define CROSSHAIR_HEIGHT 8
#define CROSSHAIR_TILEX 2
#define CROSSHAIR_TILEY 2
#define CROSSHAIR_COUNT (CROSSHAIR_TILEX * CROSSHAIR_TILEY)

//NOTE: This is packed 8x8 bitmap data, one byte per scanline, 8 scanlines
////per character. Also, it is NOT the quake font, but the IBM charset.
extern byte font8x8_data[];

struct qpic_s *Draw_CrosshairPic (void);
struct qpic_s *Draw_Font8x8Pic (void);

struct tex_s *R_DotParticleTexture (void);
struct tex_s *R_SparkParticleTexture (void);
struct tex_s *R_SmokeParticleTexture (void);

#endif // _R_LOCAL_H
