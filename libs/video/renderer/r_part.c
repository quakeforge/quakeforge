/*
	r_part.c

	Interface for particles

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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "QF/cvar.h"
#include "QF/qargs.h"
#include "QF/render.h"
#include "QF/sys.h"

#include "compat.h"
#include "r_internal.h"

unsigned int    r_maxparticles, numparticles;
particle_t     *particles;
partparm_t     *partparams;
const int  	  **partramps;
vec3_t          r_pright, r_pup, r_ppn;

/*
  R_MaxParticlesCheck

  Misty-chan: Dynamically change the maximum amount of particles on the fly.
  Thanks to a LOT of help from Taniwha, Deek, Mercury, Lordhavoc, and lots of
  others.
*/
void
R_MaxParticlesCheck (cvar_t *r_particles, cvar_t *r_particles_max)
{
	unsigned    maxparticles = 0;
	if (r_particles && r_particles->int_val) {
		maxparticles = r_particles_max ? r_particles_max->int_val : 0;
	}

	if (r_maxparticles == maxparticles) {
		return;
	}

	size_t      size = sizeof (particle_t) + sizeof (partparm_t)
						+ sizeof (int *);

	if (particles) {
		Sys_Free (particles, r_maxparticles * size);
		particles = 0;
		partparams = 0;
		partramps = 0;
	}
	r_maxparticles = maxparticles;

	if (r_maxparticles) {
		particles = Sys_Alloc (r_maxparticles * size);
		partparams = (partparm_t *) &particles[r_maxparticles];
		partramps = (const int **) &partparams[r_maxparticles];
	}

	vr_funcs->R_ClearParticles ();

	if (r_init)
		vr_funcs->R_InitParticles ();
}

static int  ramp[] = {
	/*ramp1*/ 0x6f, 0x6d, 0x6b, 0x69, 0x67, 0x65, 0x63, 0x61,
	/*ramp2*/ 0x6f, 0x6e, 0x6d, 0x6c, 0x6b, 0x6a, 0x68, 0x66,
	/*ramp3*/ 0x6d, 0x6b, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01,
};

static partparm_t part_params[] = {
	[pt_static]         = {{0, 0, 0, 0},      0, 1,  0,     0},
	[pt_grav]           = {{0, 0, 0, 0.05},   0, 1,  0,     0},
	[pt_slowgrav]       = {{0, 0, 0, 0.05},   0, 1,  0,     0},
	[pt_fire]           = {{0, 0, 0, 0.05},   5, 6,  0,  5./6},
	[pt_explode]        = {{4, 4, 4, 0.05},  10, 8,  0,     0},
	[pt_explode2]       = {{1, 1, 1, 0.05},  15, 8,  0,     0},
	[pt_blob]           = {{4, 4, 4, 0.05},   0, 1,  0,     0},
	[pt_blob2]          = {{4, 4, 0, 0.05},   0, 1,  0,     0},
	[pt_smoke]          = {{0, 0, 0, 0},      0, 1,  4,   0.4},
	[pt_smokecloud]     = {{0, 0, 0, 0.0375}, 0, 1, 50,  0.55},
	[pt_bloodcloud]     = {{0, 0, 0, 0.05},   0, 1,  4,  0.25},
	[pt_fadespark]      = {{0, 0, 0, 0},      0, 1,  0,     0},
	[pt_fadespark2]     = {{0, 0, 0, 0},      0, 1,  0,     0},
	[pt_fallfade]       = {{0, 0, 0, 1},      0, 1,  0,     1},
	[pt_fallfadespark]  = {{0, 0, 0, 1},     15, 8,  0,     1},
	[pt_flame]          = {{0, 0, 0, 0},      0, 1, -2, 0.125},
};

static const int *part_ramps[] = {
	[pt_fire]           = ramp + 2 * 8, // ramp3
	[pt_explode]        = ramp + 0 * 8, // ramp1
	[pt_explode2]       = ramp + 1 * 8, // ramp2
	[pt_fallfadespark]  = ramp + 0 * 8, // ramp1
	[pt_flame]          = 0,
};

partparm_t
R_ParticlePhysics (ptype_t type)
{
	if (type > pt_flame) {
		Sys_Error ("R_ParticlePhysics: invalid particle type");
	}
	return part_params[type];
}

const int *
R_ParticleRamp (ptype_t type)
{
	if (type > pt_flame) {
		Sys_Error ("R_ParticleRamp: invalid particle type");
	}
	return part_ramps[type];
}
