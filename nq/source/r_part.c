
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

	$Id$
*/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "QF/qargs.h"
#include "QF/compat.h"
#include "QF/console.h"
#include "QF/msg.h"

#include "r_local.h"
#include "server.h"

#define MAX_PARTICLES			2048	// default max # of particles at one
										// time
#define ABSOLUTE_MIN_PARTICLES	512		// no fewer than this no matter
										// what's
										// on the command line

particle_t *active_particles, *free_particles;

particle_t *particles;
int         r_numparticles;

vec3_t      r_pright, r_pup, r_ppn;



void
R_InitParticles (void)
{
	int         i;

	i = COM_CheckParm ("-particles");

	if (i) {
		r_numparticles = (int) (atoi (com_argv[i + 1]));
		if (r_numparticles < ABSOLUTE_MIN_PARTICLES)
			r_numparticles = ABSOLUTE_MIN_PARTICLES;
	} else {
		r_numparticles = MAX_PARTICLES;
	}

	particles = (particle_t *)
		Hunk_AllocName (r_numparticles * sizeof (particle_t), "particles");
}

#ifdef QUAKE2
void
R_DarkFieldParticles (entity_t *ent)
{
	int         i, j, k;
	particle_t *p;
	float       vel;
	vec3_t      dir;
	vec3_t      org;

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

				p->die = cl.time + 0.2 + (rand () & 7) * 0.02;
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
#endif


#define NUMVERTEXNORMALS	162
extern float r_avertexnormals[NUMVERTEXNORMALS][3];
vec3_t      avelocities[NUMVERTEXNORMALS];
float       beamlength = 16;
vec3_t      avelocity = { 23, 7, 3 };
float       partstep = 0.01;
float       timescale = 0.01;

void
R_EntityParticles (entity_t *ent)
{
	int         count;
	int         i;
	particle_t *p;
	float       angle;
	float       sr, sp, sy, cr, cp, cy;
	vec3_t      forward;
	float       dist;

	dist = 64;
	count = 50;

	if (!avelocities[0][0]) {
		for (i = 0; i < NUMVERTEXNORMALS * 3; i++)
			avelocities[0][i] = (rand () & 255) * 0.01;
	}


	for (i = 0; i < NUMVERTEXNORMALS; i++) {
		angle = cl.time * avelocities[i][0];
		sy = sin (angle);
		cy = cos (angle);
		angle = cl.time * avelocities[i][1];
		sp = sin (angle);
		cp = cos (angle);
		angle = cl.time * avelocities[i][2];
		sr = sin (angle);
		cr = cos (angle);

		forward[0] = cp * cy;
		forward[1] = cp * sy;
		forward[2] = -sp;

		if (!free_particles)
			return;
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->die = cl.time + 0.01;
		p->color = 0x6f;
		p->type = pt_explode;

		p->org[0] =
			ent->origin[0] + r_avertexnormals[i][0] * dist +
			forward[0] * beamlength;
		p->org[1] =
			ent->origin[1] + r_avertexnormals[i][1] * dist +
			forward[1] * beamlength;
		p->org[2] =
			ent->origin[2] + r_avertexnormals[i][2] * dist +
			forward[2] * beamlength;
	}
}


/*
===============
R_ParseParticleEffect

Parse an effect out of the server message
===============
*/
void
R_ParseParticleEffect (void)
{
	vec3_t      org, dir;
	int         i, count, msgcount, color;

	for (i = 0; i < 3; i++)
		org[i] = MSG_ReadCoord (net_message);
	for (i = 0; i < 3; i++)
		dir[i] = MSG_ReadChar (net_message) * (1.0 / 16);
	msgcount = MSG_ReadByte (net_message);
	color = MSG_ReadByte (net_message);

	if (msgcount == 255)
		count = 1024;
	else
		count = msgcount;

	R_RunParticleEffect (org, color, count);
}
