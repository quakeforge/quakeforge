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
#include "QF/qdefs.h" // FIXME for MAX_STYLESTRING
#include "QF/vid.h"
#include "QF/simd/types.h"
#include "QF/ui/vrect.h"

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
	vec4f_t     gravity;
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

typedef struct dlight_s {
	vec4f_t origin;
	vec4f_t color;
	float   radius;
	float   die;                // stop lighting after this time
	float   decay;              // drop this each second
	float   minlight;           // don't add when contributing less
} dlight_t;

typedef struct {
	int		length;
	char	map[MAX_STYLESTRING];
	char    average;
	char    peak;
} lightstyle_t;

//===============

typedef union refframe_s {
	mat4f_t     mat;
	struct      {
		vec4f_t     right;
		vec4f_t     forward;
		vec4f_t     up;
		vec4f_t     position;
	};
} refframe_t;

/** Generic frame buffer object.
 *
 * For attaching scene cameras to render targets.
 */
typedef struct framebuffer_s {
	unsigned    width;
	unsigned    height;
	void       *buffer;	///< renderer-specific frame buffer data
} framebuffer_t;

// !!! if this is changed, it must be changed in asm_draw.h too !!!
typedef struct {
	float		fvrectx_adj, fvrecty_adj; // left and top edges, for clamping
	float		fvrectright_adj, fvrectbottom_adj;
	int			aliasvrectleft, aliasvrecttop;	// scaled Alias versions
	int			aliasvrectright, aliasvrectbottom;	// scaled Alias versions
	int			vrectright, vrectbottom;	// right & bottom screen coords
	int			vrectx_adj_shift20;	// (vrect.x + 0.5 - epsilon) << 20
	int			vrectright_adj_shift20;	// (vrectright + 0.5 - epsilon) << 20
	float		fvrectx, fvrecty;		// for floating-point compares
										// right and bottom edges, for clamping
	float		fvrectright;			// rightmost edge, for Alias clamping
	float		fvrectbottom;			// bottommost edge, for Alias clamping
	// end of asm refs

	//FIXME move all of below elsewhere
	vrect_t		vrect;				// subwindow in video for refresh

	refframe_t  frame;
	plane_t     frustum[4];
	mat4f_t     camera;
	mat4f_t     camera_inverse;

	int			ambientlight;
	int			drawflat;

	struct ecs_registry_s *registry;
	struct model_s *worldmodel;
} refdef_t;

// REFRESH ====================================================================

extern	struct texture_s	*r_notexture_mip;

void R_Init (void);
struct vid_internal_s;
void R_LoadModule (struct vid_internal_s *vid_internal);
struct progs_s;
void R_Progs_Init (struct progs_s *pr);

void Fog_Update (float density, float red, float green, float blue,
				 float time);
struct plitem_s;
void Fog_ParseWorldspawn (struct plitem_s *worldspawn);

void Fog_GetColor (quat_t fogcolor);
float Fog_GetDensity (void) __attribute__((pure));
void Fog_SetupFrame (void);
void Fog_StartAdditive (void);
void Fog_StopAdditive (void);
void Fog_Init (void);

#endif//__QF_render_h
