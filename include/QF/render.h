/*
	render.h

	public interface to refresh functions

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

#ifndef __render_h
#define __render_h

#include "QF/mathlib.h"
#include "QF/model.h"
#include "QF/qdefs.h" // FIXME
#include "QF/vid.h"

// dynamic lights ===========================================================

typedef struct dlight_s
{
	int     key;                // so entities can reuse same entry
	vec3_t  origin;
	float   radius;
	float   die;                // stop lighting after this time
	float   decay;              // drop this each second
	float   minlight;           // don't add when contributing less
	float   color[4];
} dlight_t;

extern  dlight_t       *r_dlights;
extern	unsigned int	r_maxdlights;

// FIXME: client_state_t should hold all pieces of the client state
typedef struct
{
	int		length;
	char	map[MAX_STYLESTRING];
} lightstyle_t;

extern  lightstyle_t    r_lightstyle[MAX_LIGHTSTYLES];

// FIXME: lightstyle_t and r_lightstyle were in client.h, is this the best place for them?

//===============

#define	TOP_RANGE		16			// soldier uniform colors
#define	BOTTOM_RANGE	96

typedef struct entity_s {
	struct entity_s *next;
	struct entity_s *unext;	//FIXME this shouldn't be here. for qw demos

	vec3_t					origin;
	vec3_t					old_origin;
	vec3_t					angles;
	struct model_s			*model;			// NULL = no model
	int						frame;
	byte					*colormap;
	int						skinnum;		// for Alias models
	struct skin_s			*skin;

	float					syncbase;		// for client-side animations

	struct efrag_s			*efrag;			// linked list of efrags
	int						visframe;		// last frame this entity was
											// found in an active leaf
	float					colormod[4];	// color tint and alpha for model
	float					scale;			// size scaler of the model

	int						fullbright;
	float					min_light;
	
	// FIXME: could turn these into a union
	int						trivial_accept;
	struct mnode_s			*topnode; // for bmodels, first world node that
									  // splits bmodel, or NULL if not split

	// Animation interpolation
	float                   frame_start_time;
	float                   frame_interval;
	int						pose1;
	int						pose2;
} entity_t;

// !!! if this is changed, it must be changed in asm_draw.h too !!!
typedef struct
{
	vrect_t		vrect;				// subwindow in video for refresh
									// FIXME: not need vrect next field here?
	vrect_t		aliasvrect;			// scaled Alias version
	int			vrectright, vrectbottom;	// right & bottom screen coords
	int			aliasvrectright, aliasvrectbottom;	// scaled Alias versions
	float		vrectrightedge;			// rightmost right edge we care about,
										//  for use in edge list
	float		fvrectx, fvrecty;		// for floating-point compares
	float		fvrectx_adj, fvrecty_adj; // left and top edges, for clamping
	int			vrect_x_adj_shift20;	// (vrect.x + 0.5 - epsilon) << 20
	int			vrectright_adj_shift20;	// (vrectright + 0.5 - epsilon) << 20
	float		fvrectright_adj, fvrectbottom_adj;
										// right and bottom edges, for clamping
	float		fvrectright;			// rightmost edge, for Alias clamping
	float		fvrectbottom;			// bottommost edge, for Alias clamping
	float		horizontalFieldOfView;	// at Z = 1.0, this many X is visible 
										// 2.0 = 90 degrees
	float		xOrigin;			// should probably always be 0.5
	float		yOrigin;			// between be around 0.3 to 0.5

	vec3_t		vieworg;
	vec3_t		viewangles;

	float		fov_x, fov_y;

	int			ambientlight;
} refdef_t;

// REFRESH ====================================================================

extern	int		reinit_surfcache;

extern	refdef_t	r_refdef;
extern vec3_t	r_origin, vpn, vright, vup;

extern	struct texture_s	*r_notexture_mip;

extern entity_t r_worldentity;

extern void (*r_viewsize_callback)(struct cvar_s *var);
extern int r_viewsize;

void R_Init (void);
void R_Init_Cvars (void);
void R_InitEfrags (void);
void R_InitSky (struct texture_s *mt);	// called at level load
void R_Textures_Init (void);
void R_RenderView (void);			// must set r_refdef first
void R_ViewChanged (float aspect);	// must set r_refdef first
								// called whenever r_refdef or vid change

void R_AddEfrags (entity_t *ent);
void R_RemoveEfrags (entity_t *ent);

void R_NewMap (model_t *worldmodel, struct model_s **models, int num_models);

// LordHavoc: relative bmodel lighting
void R_PushDlights (const vec3_t entorigin);
void R_DrawWaterSurfaces (void);

void Fog_Update (float density, float red, float green, float blue,
				 float time);
struct plitem_s;
void Fog_ParseWorldspawn (struct plitem_s *worldspawn);
float *Fog_GetColor (void);
float Fog_GetDensity (void);
void Fog_SetupFrame (void);
void Fog_EnableGFog (void);
void Fog_DisableGFog (void);
void Fog_StartAdditive (void);
void Fog_StopAdditive (void);
void Fog_Init (void);

//  Surface cache related ==========
extern	int		reinit_surfcache;	// if 1, surface cache is currently empty
extern qboolean	r_cache_thrash;	// set if thrashing the surface cache
extern qboolean	r_inhibit_viewmodel;
extern qboolean	r_force_fullscreen;
extern qboolean	r_paused;
extern entity_t *r_view_model;
extern entity_t *r_player_entity;
extern int		r_lineadj;
extern qboolean r_active;

void *D_SurfaceCacheAddress (void);
int D_SurfaceCacheForRes (int width, int height);
void D_FlushCaches (void);
void D_DeleteSurfaceCache (void);
void D_InitCaches (void *buffer, int size);
void R_SetVrect (vrect_t *pvrect, vrect_t *pvrectin, int lineadj);

void R_LoadSkys (const char *);

void R_ClearEfrags (void);
void R_ClearEnts (void);
void R_EnqueueEntity (struct entity_s *ent);
struct entity_s *R_AllocEntity (void);
void R_FreeAllEntities (void);

dlight_t *R_AllocDlight (int key);
void R_DecayLights (double frametime);
void R_ClearDlights (void);

int R_InitGraphTextures (int base);
void R_LineGraph (int x, int y, int *h_vals, int count);
struct progs_s;
void R_Progs_Init (struct progs_s *pr);

void R_DrawAliasModel (entity_t *e);

void R_MarkLeaves (void);

#endif // __render_h
