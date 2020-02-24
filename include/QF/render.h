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

*/

#ifndef __QF_render_h
#define __QF_render_h

#include "QF/mathlib.h"
#include "QF/model.h"
#include "QF/qdefs.h" // FIXME
#include "QF/vid.h"

typedef enum {
	pt_static,
	pt_grav,
	pt_slowgrav,
	pt_fire,
	pt_explode,
	pt_explode2,
	pt_blob,
	pt_blob2,
	pt_smoke,
	pt_smokecloud,
	pt_bloodcloud,
	pt_fadespark,
	pt_fadespark2,
	pt_fallfade,
	pt_fallfadespark,
	pt_flame
} ptype_t;

extern struct vid_render_funcs_s *r_funcs;
extern struct vid_render_data_s *r_data;

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

typedef struct
{
	int		length;
	char	map[MAX_STYLESTRING];
	char    average;
	char    peak;
} lightstyle_t;

//===============

typedef struct entity_s {
	struct entity_s *next;
	struct entity_s *unext;	//FIXME this shouldn't be here. for qw demos

	vec3_t					origin;
	vec3_t					old_origin;
	vec3_t					angles;
	vec_t					transform[4 * 4];
	vec_t					full_transform[4 * 4];
	struct model_s			*model;			// NULL = no model
	int						frame;
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
	struct model_s			*pose_model;	// no lerp if not the same as model
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

// color shifts =============================================================

typedef struct {
	int     destcolor[3];
	int     percent;        // 0-255
	double  time;
	int     initialpct;
} cshift_t;

#define CSHIFT_CONTENTS 0
#define CSHIFT_DAMAGE   1
#define CSHIFT_BONUS    2
#define CSHIFT_POWERUP  3
#define NUM_CSHIFTS     4

// REFRESH ====================================================================

extern	struct texture_s	*r_notexture_mip;

extern entity_t r_worldentity;

void R_Init (void);
void R_LoadModule (void (*load_gl)(void),
				   void (*set_palette) (const byte *palette));
struct progs_s;
void R_Progs_Init (struct progs_s *pr);

#endif//__QF_render_h
