
/*
	r_part.c

	@description@

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
static const char rcsid[] = 
	"$Id$";

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "QF/console.h"
#include "QF/cvar.h"
#include "QF/qargs.h"
#include "QF/render.h"

#include "compat.h"
#include "r_local.h"

short			r_maxparticles, numparticles;
particle_t	   *active_particles, *free_particles, *particles, **freeparticles;
vec3_t			r_pright, r_pup, r_ppn;

extern cvar_t  *cl_max_particles;
extern cvar_t  *r_particles;


/*
  R_MaxParticlesCheck

  Misty-chan: Dynamically change the maximum amount of particles on the fly.
  Thanks to a LOT of help from Taniwha, Deek, Mercury, Lordhavoc, and lots of
  others.
*/
void
R_MaxParticlesCheck (cvar_t *r_particles, cvar_t *cl_max_particles)
{
/*
	Catchall. If the user changed the setting to a number less than zero *or*
	if we had a wacky cfg get past the init code check, this will make sure we
	don't have problems. Also note that grabbing the var->int_val is IMPORTANT:
	Prevents a segfault since if we grabbed the int_val of cl_max_particles
	we'd sig11 right here at startup.
*/
	if (r_particles && r_particles->int_val)
		r_maxparticles = cl_max_particles ? cl_max_particles->int_val : 0;
	else
		r_maxparticles = 0;

/*
	Be very careful the next time we do something like this. calloc/free are
	IMPORTANT and the compiler doesn't know when we do bad things with them.
*/
	if (particles)
		free (particles);
	if (freeparticles)
		free (freeparticles);

	particles = 0;
	freeparticles = 0;

	if (r_maxparticles) {
		particles = (particle_t *) calloc (r_maxparticles, sizeof (particle_t));
		freeparticles = (particle_t **) calloc (r_maxparticles,
												sizeof (particle_t *));
	}

	R_ClearParticles();
}

void
R_DarkFieldParticles (entity_t *ent)
{
	int         i, j, k;
	float       vel;
	particle_t *p;
	vec3_t      dir, org;

	org[0] = ent->origin[0];
	org[1] = ent->origin[1];
	org[2] = ent->origin[2];
	for (i = -16; i < 16; i += 8)
		for (j = -16; j < 16; j += 8)
			for (k = 0; k < 32; k += 8) {
				if (!free_particles)
					return;
				p = free_particles;
				free_particles = p->next;
				p->next = active_particles;
				active_particles = p;

				p->die = r_realtime + 0.2 + (rand () & 7) * 0.02;
				p->color = 150 + rand () % 6;
				p->type = pt_slowgrav;

				dir[0] = j * 8;
				dir[1] = i * 8;
				dir[2] = k * 8;

				p->org[0] = org[0] + i + (rand () & 3);
				p->org[1] = org[1] + j + (rand () & 3);
				p->org[2] = org[2] + k + (rand () & 3);

				VectorNormalize (dir);
				vel = 50 + (rand () & 63);
				VectorScale (dir, vel, p->vel);
			}
}

#define NUMVERTEXNORMALS	162
static float		beamlength = 16;
static vec3_t		avelocities[NUMVERTEXNORMALS];

extern float		r_avertexnormals[NUMVERTEXNORMALS][3];

void
R_EntityParticles (entity_t *ent)
{
	int         count, i;
	float       angle, dist, sp, sy, cp, cy; // cr, sr
	particle_t *p;
	vec3_t      forward;

	dist = 64;
	count = 50;

	if (!avelocities[0][0]) {
		for (i = 0; i < NUMVERTEXNORMALS * 3; i++)
			avelocities[0][i] = (rand () & 255) * 0.01;
	}

	for (i = 0; i < NUMVERTEXNORMALS; i++) {
		angle = r_realtime * avelocities[i][0];
		sy = sin (angle);
		cy = cos (angle);
		angle = r_realtime * avelocities[i][1];
		sp = sin (angle);
		cp = cos (angle);
// Next 3 lines results aren't currently used, may be in future. --Despair
//		angle = r_realtime * avelocities[i][2];
//		sr = sin (angle);
//		cr = cos (angle);

		forward[0] = cp * cy;
		forward[1] = cp * sy;
		forward[2] = -sp;

		if (!free_particles)
			return;
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->die = r_realtime + 0.01;
		p->color = 0x6f;
		p->type = pt_explode;

		p->org[0] = ent->origin[0] + r_avertexnormals[i][0] * dist +
			forward[0] * beamlength;
		p->org[1] = ent->origin[1] + r_avertexnormals[i][1] * dist +
			forward[1] * beamlength;
		p->org[2] = ent->origin[2] + r_avertexnormals[i][2] * dist +
			forward[2] * beamlength;
	}
}
