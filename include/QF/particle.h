/*
	particle.h

	Particle system

	Copyright (C) 1996-1997  Id Software, Inc.
	Copyright (C) 2026  Bill Currie <bill@taniwha.org>

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

#ifndef __QF_particle_h
#define __QF_particle_h

#include "QF/math/vector.h"
#include "QF/simd/types.h"

typedef enum {
	part_tex_dot,
	part_tex_spark,
	part_tex_smoke,
} ptextype_t;

// !!! if this is changed, it must be changed in d_ifacea.h too !!!
typedef struct particle_s {
	vec4f_t     pos;
	vec4f_t     vel;

	uint32_t    color;
	int         ramp_base; // < 0 -> don't apply ramp to color
	int         pad;
	float       alpha;

	ptextype_t  tex;
	float       ramp;
	float       scale;
	float       live;
} particle_t;

static_assert (sizeof (particle_t) == 4 * sizeof(vec4f_t),
			   "particle_t wrong size");

typedef struct trailpnt_s {
	vec4f_t     pos;
	vec4f_t     vel;
	vec3_t      bary;
	float       pathoffset;
	byte        colora[4];
	byte        colorb[4];
	uint32_t    trailid;
	float       live;
} trailpnt_t;

static_assert (sizeof (trailpnt_t) == 4 * sizeof(vec4f_t),
			   "trailprt_t wrong size");

typedef struct partparm_s {
	vec4f_t     drag;	// drag[3] is grav scale
	float       ramp_rate;
	float       ramp_max;
	float       scale_rate;
	float       alpha_rate;
} partparm_t;

static_assert (sizeof (partparm_t) == 2 * sizeof(vec4f_t),
			   "partparm_t wrong size");

typedef struct psystem_s {
	vec4f_t     center;
	float       gravity;
	float       min_dist;
	uint32_t    maxparticles;
	uint32_t    numparticles;
	particle_t *particles;
	partparm_t *partparams;
	const int  *partramps;
	int         partramps_count;// number of ints total

	bool        points_only;
} psystem_t;

// base for particle emitters
typedef struct pemitter_s {
	uint32_t    psystem;	// FIXME currently always 0
	int16_t     ramp_base;	// index into psystem_t.partramps
	int16_t     ramp_range;	// number of elements on ramp
	float       rate;		// emission rate (particles/s).
	float       timer;		// state for emission rate
} pemitter_t;

// common particle settings for emitters
typedef struct peparticle_s {
	uint32_t    color;
	float       scale;
	float       alpha;
	float       live;
	ptextype_t  tex;
	float       ramp_rate;
	float       ramp_max;
	float       scale_rate;
	float       alpha_rate;
	vec3_t      drag;		// additive, so use -ve for actual drag
	float       grav_scale;
} peparticle_t;

typedef struct pe_plane_s {
	pemitter_t  base;
	peparticle_t particle;
	vec3_t      u;
	vec3_t      v;
	vec3_t      vel;
	bool        square;		// false = circular
	int8_t      dist;		// exponent for pdf
} pe_plane_t;

enum {
	pemitter_plane,

	pemitter_comp_count,
};

typedef struct ecs_registry_s ecs_registry_t;
typedef struct scene_s scene_t;

bool R_Trail_Valid (psystem_t *system, uint32_t trailid) __attribute__((pure));
uint32_t R_Trail_Create (psystem_t *system, int num_points, vec4f_t start);
void R_Trail_Update (psystem_t *system, uint32_t trailid, vec4f_t pos);
void R_Trail_Destroy (psystem_t *system, uint32_t trailid);
void R_Trails_Init (void);
void R_Trails_Init_Cvars (void);
void R_ClearTrails (void);
void R_RunTrails (float dT);

void R_Particles_NewScene (scene_t *scene);
void R_RunParticles (float dT);
void R_Particles_RunEmitters (float dT);
void R_Particles_Init (ecs_registry_t *reg);
void R_Particles_Init_Cvars (void);
void R_ClearParticles (void);

#endif//__QF_particle_h
