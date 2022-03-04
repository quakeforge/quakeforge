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
#include "QF/simd/types.h"
#include "QF/ui/vrect.h"

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

typedef enum {
	part_tex_dot,
	part_tex_spark,
	part_tex_smoke,
} ptextype_t;

typedef struct particle_s particle_t;

// !!! if this is changed, it must be changed in d_ifacea.h too !!!
struct particle_s {
	vec4f_t     pos;
	vec4f_t     vel;

	union {
		struct {
			int			icolor;
			int         pad[2];
			float		alpha;
		};
		vec4f_t     color;
	};

	ptextype_t	tex;
	float		ramp;
	float		scale;
	float		live;
};

typedef struct partparm_s {
	vec4f_t     drag;	// drag[3] is grav scale
	float       ramp;
	float       ramp_max;
	float       scale_rate;
	float       alpha_rate;
} partparm_t;

typedef struct psystem_s {
	uint32_t    maxparticles;
	uint32_t    numparticles;
	particle_t *particles;
	partparm_t *partparams;
	const int **partramps;

	int         points_only;
} psystem_t;

extern struct vid_render_funcs_s *r_funcs;
extern struct vid_render_data_s *r_data;

typedef struct subpic_s {
	const struct subpic_s *const next;
	const struct scrap_s *const scrap;	///< renderer specific type
	const struct vrect_s *const rect;
	const int width;					///< requested width
	const int height;					///< requested height
	const float size;					///< size factor for tex coords (mult)
} subpic_t;

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

typedef struct animation_s {
	int         frame;
	float       syncbase;	// randomize time base for local animations
	float       frame_start_time;
	float       frame_interval;
	int         pose1;
	int         pose2;
	float       blend;
	int         nolerp;		// don't lerp this frame (pose data invalid)
} animation_t;

typedef struct visibility_s {
	struct entity_s *entity;	// owning entity
	struct efrag_s *efrag;		// linked list of efrags
	struct mnode_s *topnode;	// bmodels, first world node that
								// splits bmodel, or NULL if not split
								// applies to other models, too
								// found in an active leaf
	int         trivial_accept;	// view clipping (frustum and depth)
} visibility_t;

typedef struct renderer_s {
	struct model_s *model;			// NULL = no model
	struct skin_s *skin;
	float       colormod[4];	// color tint and alpha for model
	int         skinnum;		// for Alias models
	int         fullbright;
	float       min_light;
	mat4_t      full_transform;
} renderer_t;

typedef struct entity_s {
	struct entity_s *next;
	struct transform_s *transform;
	int         id;		///< scene id
	animation_t animation;
	visibility_t visibility;
	renderer_t  renderer;
	int         active;
	//XXX FIXME XXX should not be here
	vec4f_t     old_origin;
} entity_t;

// !!! if this is changed, it must be changed in asm_draw.h too !!!
typedef struct {
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

	//FIXME was vec3_t, need to deal with asm (maybe? is it worth it?)
	vec4f_t     viewposition;
	vec4f_t     viewrotation;

	int			ambientlight;

	float		fov_x, fov_y;
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
struct vid_internal_s;
void R_LoadModule (struct vid_internal_s *vid_internal);
struct progs_s;
void R_Progs_Init (struct progs_s *pr);

#endif//__QF_render_h
