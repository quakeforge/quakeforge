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
	R_ClearParticles ();
}

void
R_ClearParticles (void)
{
	numparticles = 0;
}
