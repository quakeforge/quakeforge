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

	$Id$
*/

#ifndef _R_LOCAL_H
#define _R_LOCAL_H

#include "QF/mathlib.h"
#include "QF/vid.h"
#include "QF/model.h"
#include "r_shared.h"

#define ALIAS_BASE_SIZE_RATIO		(1.0 / 11.0)
					// normalizing factor so player model works out to about
					//  1 pixel per triangle

#define BMODEL_FULLY_CLIPPED	0x10 // value returned by R_BmodelCheckBBox ()
									 //  if bbox is trivially rejected

// color shifts =============================================================

typedef struct
{
	int     destcolor[3];
	int     percent;        // 0-255
} cshift_t;

#define CSHIFT_CONTENTS 0
#define CSHIFT_DAMAGE   1
#define CSHIFT_BONUS    2
#define CSHIFT_POWERUP  3
#define NUM_CSHIFTS     4

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

extern struct cvar_s	*r_draworder;
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
typedef struct clipplane_s
{
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

extern	mplane_t	screenedge[4];

extern	vec3_t	r_origin;

extern	vec3_t	r_entorigin;

extern	float	screenAspect;
extern	float	verticalFieldOfView;
extern	float	xOrigin, yOrigin;

extern	int		r_visframecount;

//=============================================================================

extern int	vstartscan;


void R_ClearPolyList (void);
void R_DrawPolyList (void);


// current entity info
extern	qboolean		insubmodel;
extern	vec3_t			r_worldmodelorg;


void R_DrawSprite (void);
void R_RenderFace (msurface_t *fa, int clipflags);
void R_RenderPoly (msurface_t *fa, int clipflags);
void R_RenderBmodelFace (bedge_t *pedges, msurface_t *psurf);
void R_TransformPlane (mplane_t *p, float *normal, float *dist);
void R_TransformFrustum (void);
void R_SetSkyFrame (void);
void R_DrawSurfaceBlock (void);
texture_t *R_TextureAnimation (msurface_t *surf);

void R_GenSkyTile (void *pdest);
void R_SurfPatch (void);
void R_DrawSubmodelPolygons (model_t *pmodel, int clipflags);
void R_DrawSolidClippedSubmodelPolygons (model_t *pmodel);

void R_AddPolygonEdges (emitpoint_t *pverts, int numverts, int miplevel);
surf_t *R_GetSurf (void);
void R_AliasDrawModel (alight_t *plighting);
void R_BeginEdgeFrame (void);
void R_ScanEdges (void);
void D_DrawSurfaces (void);
void R_InsertNewEdges (edge_t *edgestoadd, edge_t *edgelist);
void R_StepActiveU (edge_t *pedge);
void R_RemoveEdges (edge_t *pedge);

extern void R_Surf8Start (void);
extern void R_Surf8End (void);
extern void R_EdgeCodeStart (void);
extern void R_EdgeCodeEnd (void);

extern void R_RotateBmodel (void);

extern int	c_faceclip;
extern int	r_polycount;
extern int	r_wholepolycount;

extern	model_t		*cl_worldmodel;

extern int		*pfrustum_indexes[4];

// !!! if this is changed, it must be changed in asm_draw.h too !!!
#define	NEAR_CLIP	0.01

extern int			ubasestep, errorterm, erroradjustup, erroradjustdown;
extern int			vstartscan;

extern fixed16_t	sadjust, tadjust;
extern fixed16_t	bbextents, bbextentt;

#define MAXBVERTINDEXES	1000	// new clipped vertices when clipping bmodels
								//  to the world BSP
extern mvertex_t	*r_ptverts, *r_ptvertsmax;

extern vec3_t			sbaseaxis[3], tbaseaxis[3];
extern float			entity_rotation[3][3];

extern int		reinit_surfcache;

extern int		r_currentkey;
extern int		r_currentbkey;

typedef struct btofpoly_s {
	int			clipflags;
	msurface_t	*psurf;
} btofpoly_t;

#define MAX_BTOFPOLYS	5000	// FIXME: tune this

extern int			numbtofpolys;
extern btofpoly_t	*pbtofpolys;

void	R_InitTurb (void);
void	R_ZDrawSubmodelPolys (model_t *clmodel);

// Alias models ===========================================

#define ALIAS_Z_CLIP_PLANE	5

extern int				numverts;
extern int				a_skinwidth;
extern mtriangle_t		*ptriangles;
extern int				numtriangles;
extern aliashdr_t		*paliashdr;
extern mdl_t			*pmdl;
extern float			leftclip, topclip, rightclip, bottomclip;
extern int				r_acliptype;
extern finalvert_t		*pfinalverts;
extern auxvert_t		*pauxverts;

qboolean R_AliasCheckBBox (void);

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

extern float r_gravity;

// renderer stuff again ===================================

extern int		r_amodels_drawn;
extern edge_t	*auxedges;
extern int		r_numallocatededges;
extern edge_t	*r_edges, *edge_p, *edge_max;

extern	edge_t	*newedges[MAXHEIGHT];
extern	edge_t	*removeedges[MAXHEIGHT];

extern	int	screenwidth;

// FIXME: make stack vars when debugging done
extern	edge_t	edge_head;
extern	edge_t	edge_tail;
extern	edge_t	edge_aftertail;
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
extern float	dp_time1, dp_time2, db_time1, db_time2, rw_time1, rw_time2;
extern float	se_time1, se_time2, de_time1, de_time2, dv_time1, dv_time2;
extern int		r_frustum_indexes[4*6];
extern int		r_maxsurfsseen, r_maxedgesseen, r_cnumsurfs;
extern qboolean	r_surfsonstack;
extern qboolean	r_dowarpold, r_viewchanged;

extern mleaf_t	*r_viewleaf, *r_oldviewleaf;

extern vec3_t	r_emins, r_emaxs;
extern mnode_t	*r_pefragtopnode;
extern int		r_clipflags;
extern int		r_dlightframecount;
extern qboolean	r_fov_greater_than_90;

extern int r_numvisedicts;
extern struct entity_s *r_visedicts[];
struct dlight_s;

void R_StoreEfrags (efrag_t **ppefrag);
void R_TimeRefresh_f (void);
void R_TimeGraph (void);
void R_ZGraph (void);
void R_PrintAliasStats (void);
void R_PrintTimes (void);
void R_PrintDSpeeds (void);
void R_AnimateLight (void);
int R_LightPoint (const vec3_t p);
void R_SetupFrame (void);
void R_cshift_f (void);
void R_EmitEdge (mvertex_t *pv0, mvertex_t *pv1);
void R_ClipEdge (mvertex_t *pv0, mvertex_t *pv1, clipplane_t *clip);
void R_SplitEntityOnNode2 (mnode_t *node);
void R_RecursiveMarkLights (const vec3_t lightorigin, struct dlight_s *light,
							int bit, mnode_t *node);
void R_MarkLights (const vec3_t lightorigin, struct dlight_s *light, int bit,
				   model_t *model);

void R_LoadSkys (const char *);

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

void R_AliasSetUpTransform (int trivial_accept);
void R_AliasTransformVector (vec3_t in, vec3_t out);
void R_AliasTransformFinalVert (finalvert_t *fv, auxvert_t *av,
		                        trivertx_t *pverts, stvert_t *pstverts);
void R_AliasTransformAndProjectFinalVerts (finalvert_t *fv, stvert_t *pstverts);

void R_GenerateSpans (void);

void R_InitVars (void);

void R_LoadSky_f (void);

void R_DrawSurfaceBlock_mip0 (void);
void R_DrawSurfaceBlock_mip1 (void);
void R_DrawSurfaceBlock_mip2 (void);
void R_DrawSurfaceBlock_mip3 (void);
void R_DrawSurfaceBlock8_mip0 (void);
void R_DrawSurfaceBlock8_mip1 (void);
void R_DrawSurfaceBlock8_mip2 (void);
void R_DrawSurfaceBlock8_mip3 (void);
void R_DrawSurfaceBlock16_mip0 (void);
void R_DrawSurfaceBlock16_mip1 (void);
void R_DrawSurfaceBlock16_mip2 (void);
void R_DrawSurfaceBlock16_mip3 (void);
void R_DrawSurfaceBlock32_mip0 (void);
void R_DrawSurfaceBlock32_mip1 (void);
void R_DrawSurfaceBlock32_mip2 (void);
void R_DrawSurfaceBlock32_mip3 (void);

#endif // _R_LOCAL_H
