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
	int         ambientlight;
	int         shadelight;
	vec3_t      lightvec;
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

extern int r_speeds;
extern int r_timegraph;
extern int r_graphheight;
extern int r_clearcolor;
extern int r_waterwarp;
extern int r_drawentities;
extern int r_aliasstats;
extern int r_dspeeds;
extern int r_drawflat;
extern int r_ambient;
extern int r_reportsurfout;
extern int r_maxsurfs;
extern int r_numsurfs;
extern int r_reportedgeout;
extern int r_maxedges;
extern int r_numedges;

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
struct entqueue_s;
void R_DrawEntitiesOnList (struct entqueue_s *queue);

//=============================================================================

extern	plane_t	screenedge[4];

extern	vec4f_t r_entorigin;

extern	int		r_visframecount;

//=============================================================================

extern int	vstartscan;


void R_ClearPolyList (void);
void R_DrawPolyList (void);

//  Surface cache related ==========
extern bool	r_cache_thrash;	// set if thrashing the surface cache

// current entity info
extern bool     insubmodel;
extern vec3_t   r_worldmodelorg;

extern mat4f_t  glsl_projection;
extern mat4f_t  glsl_view;

union refframe_s;
void R_SetFrustum (plane_t *frustum, const union refframe_s *frame,
				   float fov_x, float fov_y);

struct entity_s;
struct animation_s;

void R_SpriteBegin (void);
void R_SpriteEnd (void);
void R_DrawSprite (struct entity_s ent);
void R_RenderFace (uint32_t render_id, msurface_t *fa, int clipflags);
void R_RenderPoly (uint32_t render_id, msurface_t *fa, int clipflags);
void R_RenderBmodelFace (uint32_t render_id, bedge_t *pedges, msurface_t *psurf);
void R_TransformFrustum (void);
void R_SetSkyFrame (void);
void R_DrawSurfaceBlock (void);
struct texture_s *R_TextureAnimation (int frame, msurface_t *surf) __attribute__((pure));

void R_GenSkyTile (void *pdest);
void R_SurfPatch (void);
void R_DrawSubmodelPolygons (uint32_t render_id, mod_brush_t *brush, int clipflags, struct mleaf_s *topleaf);
void R_DrawSolidClippedSubmodelPolygons (uint32_t render_id, mod_brush_t *brush, struct mnode_s *topnode);

void R_AddPolygonEdges (emitpoint_t *pverts, int numverts, int miplevel);
surf_t *R_GetSurf (void);
void R_AliasClipAndProjectFinalVert (finalvert_t *fv, auxvert_t *av);
void R_AliasDrawModel (struct entity_s ent, alight_t *plighting);
void R_IQMDrawModel (struct entity_s ent, alight_t *plighting);
struct animation_s;
maliasskindesc_t *R_AliasGetSkindesc (struct animation_s *animation, int skinnum, aliashdr_t *hdr);
maliasframedesc_t *R_AliasGetFramedesc (struct animation_s *animation, aliashdr_t *hdr);
float R_AliasGetLerpedFrames (struct animation_s *animation, aliashdr_t *hdr);
float R_IQMGetLerpedFrames (struct animation_s *animation, iqm_t *hdr);
iqmframe_t *R_IQMBlendFrames (const iqm_t *iqm, int frame1, int frame2,
							  float blend, int extra);
iqmframe_t *R_IQMBlendPalette (const iqm_t *iqm, int frame1, int frame2,
							   float blend, int extra,
							   iqmblend_t *blend_palette, int palette_size);
float R_EntityBlend (struct animation_s *animation, int pose, float interval);
void R_BeginEdgeFrame (void);
void R_ScanEdges (void);
void D_DrawSurfaces (void);
void R_InsertNewEdges (edge_t *edgestoadd, edge_t *edgelist);
void R_StepActiveU (edge_t *pedge);
void R_RemoveEdges (edge_t *pedge);
void R_AddTexture (struct texture_s *tex);
struct vulkan_ctx_s;
void R_ClearTextures (void);
void R_InitSurfaceChains (mod_brush_t *brush);

extern const byte *r_colormap;
void R_SetColormap (const byte *cmap);
extern void R_Surf8Start (void);
extern void R_Surf8End (void);
extern void R_EdgeCodeStart (void);
extern void R_EdgeCodeEnd (void);

struct transform_s;
extern void R_RotateBmodel (vec4f_t *mat);

extern int	c_faceclip;
extern int	r_polycount;

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
void	R_ZDrawSubmodelPolys (uint32_t render_id, mod_brush_t *brush);

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

bool R_AliasCheckBBox (struct entity_s ent);

// turbulence stuff =======================================

#define	AMP		8*0x10000
#define	AMP2	3
#define	SPEED	20

// particle stuff =========================================

struct psystem_s;
void R_DrawParticles (struct psystem_s *psystem);
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

extern int		r_bmodelactive;
extern vrect_t	*pconupdate;

extern float		aliasxscale, aliasyscale, aliasxcenter, aliasycenter;
extern float		r_aliastransition, r_resfudge;

extern int		r_outofsurfaces;
extern int		r_outofedges;

extern mvertex_t	*r_pcurrentvertbase;
extern int			r_maxvalidedgeoffset;

void R_AliasClipTriangle (mtriangle_t *ptri);

extern double	r_time1;
extern int		r_frustum_indexes[4*6];
extern int		r_maxsurfsseen, r_maxedgesseen;
extern bool		r_dowarpold, r_viewchanged;

extern int		r_clipflags;

extern struct entqueue_s *r_ent_queue;
struct dlight_s;

extern vec3_t lightspot;

struct efrag_s;
void R_StoreEfrags (const struct efrag_s *efrag);
void R_TimeRefresh_f (void);
void R_PrintAliasStats (void);
void R_PrintTimes (void);
void R_AnimateLight (void);
int R_LightPoint (mod_brush_t *brush, vec4f_t p);
void R_SetupFrame (void);
void R_cshift_f (void);
void R_EmitEdge (mvertex_t *pv0, mvertex_t *pv1);
void R_ClipEdge (mvertex_t *pv0, mvertex_t *pv1, clipplane_t *clip);
void R_RecursiveMarkLights (mod_brush_t *brush, vec4f_t lightorigin,
							struct dlight_s *light, int bit, int node_id);

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
