/*
	sw_rpart.c

	(description)

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

#include <stdlib.h>

#include "QF/compat.h"
#include "QF/console.h"
#include "QF/qargs.h"
#include "QF/quakefs.h"
#include "QF/render.h"

#include "r_cvar.h"
#include "r_dynamic.h"
#include "r_local.h"

int         ramp1[8] = { 0x6f, 0x6d, 0x6b, 0x69, 0x67, 0x65, 0x63, 0x61 };
int         ramp2[8] = { 0x6f, 0x6e, 0x6d, 0x6c, 0x6b, 0x6a, 0x68, 0x66 };
int         ramp3[8] = { 0x6d, 0x6b, 6, 5, 4, 3 };

particle_t *active_particles, *free_particles;

particle_t *particles;
int         r_numparticles;

vec3_t      r_pright, r_pup, r_ppn;

/*
	R_MaxParticlesCheck

	Misty-chan: EXTREME heavy lifting and bugfixing thanks goes out to taniwha
				- I built this, and he got it working :)
*/
void
R_MaxParticlesCheck (cvar_t *var)
{
	// Do not use 0 in this! sw doesn't grok 0 and it'll segfault if we do!
	r_numparticles = max(var->int_val, 1);
        
	/*
	Debugging code. will print what the above was set to, and is also useful
	for checking if this is accidentally being run all the time.
	Con_Printf ("%d", r_numparticles);
	*/
	
	if (particles)
		free (particles);
                
	particles = (particle_t *) calloc (r_numparticles, sizeof (particle_t));
        
	R_ClearParticles ();
}


void
R_Particles_Init_Cvars (void)
{
	// Does a callback to R_MaxParticleCheck when the cvar changes. Neat trick.
	Cvar_Get ("cl_max_particles", "2048", CVAR_ARCHIVE, R_MaxParticlesCheck,
			  "Maximum amount of particles to display. No maximum, minimum is 1.");
}


void
R_ClearParticles (void)
{
	int         i;

	free_particles = &particles[0];
	active_particles = NULL;

	for (i = 0; i < r_numparticles; i++)
		particles[i].next = &particles[i + 1];
	particles[r_numparticles - 1].next = NULL;
}


void
R_ReadPointFile_f (void)
{
	VFile      *f;
	vec3_t      org;
	int         r;
	int         c;
	particle_t *p;
	char        name[MAX_OSPATH];

// FIXME    snprintf (name, sizeof (name), "maps/%s.pts", sv.name);

	COM_FOpenFile (name, &f);
	if (!f) {
		Con_Printf ("couldn't open %s\n", name);
		return;
	}

	Con_Printf ("Reading %s...\n", name);
	c = 0;
	for (;;) {
		char        buf[64];

		Qgets (f, buf, sizeof (buf));
		r = sscanf (buf, "%f %f %f\n", &org[0], &org[1], &org[2]);
		if (r != 3)
			break;
		c++;

		if (!free_particles) {
			Con_Printf ("Not enough free particles\n");
			break;
		}
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->die = 99999;
		p->color = (-c) & 15;
		p->type = pt_static;
		VectorCopy (vec3_origin, p->vel);
		VectorCopy (org, p->org);
	}

	Qclose (f);
	Con_Printf ("%i points read\n", c);
}


void
R_RunSpikeEffect (vec3_t pos, particle_effect_t type)
{
	switch (type) {
		case PE_WIZSPIKE:
			R_RunParticleEffect (pos, 20, 30);
			break;
		case PE_KNIGHTSPIKE:
			R_RunParticleEffect (pos, 226, 20);
			break;
		case PE_SPIKE:
			R_RunParticleEffect (pos, 0, 10);
			break;
		case PE_SUPERSPIKE:
			R_RunParticleEffect (pos, 0, 20);
			break;
		default:
			break;
	}
}


void
R_RunPuffEffect (vec3_t pos, particle_effect_t type, byte cnt)
{
	if (!r_particles->int_val)
		return;

	switch (type) {
		case PE_GUNSHOT:
			R_RunParticleEffect (pos, 0, 20 * cnt);
			break;
		case PE_BLOOD:
			R_RunParticleEffect (pos, 73, 20 * cnt);
			break;
		case PE_LIGHTNINGBLOOD:
			R_RunParticleEffect (pos, 225, 50);
			break;
		default:
			break;
	}
}


void
R_ParticleExplosion (vec3_t org)
{
	int         i, j;
	particle_t *p;

	if (!r_particles->int_val)
		return;

	for (i = 0; i < 1024; i++) {
		if (!free_particles)
			return;
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->die = r_realtime + 5;
		p->color = ramp1[0];
		p->ramp = rand () & 3;
		if (i & 1) {
			p->type = pt_explode;
			for (j = 0; j < 3; j++) {
				p->org[j] = org[j] + ((rand () % 32) - 16);
				p->vel[j] = (rand () % 512) - 256;
			}
		} else {
			p->type = pt_explode2;
			for (j = 0; j < 3; j++) {
				p->org[j] = org[j] + ((rand () % 32) - 16);
				p->vel[j] = (rand () % 512) - 256;
			}
		}
	}
}


void
R_ParticleExplosion2 (vec3_t org, int colorStart, int colorLength)
{
	int              i, j;
	particle_t      *p;
	int              colorMod = 0;

	for (i=0; i<512; i++)
	{
		if (!free_particles)
			return;
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->die = r_realtime + 0.3;
		p->color = colorStart + (colorMod % colorLength);
		colorMod++;

		p->type = pt_blob;
		for (j=0 ; j<3 ; j++)
		{
			p->org[j] = org[j] + ((rand()%32)-16);
			p->vel[j] = (rand()%512)-256;
		}
	}
}


void
R_BlobExplosion (vec3_t org)
{
	int         i, j;
	particle_t *p;

	if (!r_particles->int_val)
		return;

	for (i = 0; i < 1024; i++) {
		if (!free_particles)
			return;
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->die = r_realtime + 1 + (rand () & 8) * 0.05;

		if (i & 1) {
			p->type = pt_blob;
			p->color = 66 + rand () % 6;
			for (j = 0; j < 3; j++) {
				p->org[j] = org[j] + ((rand () % 32) - 16);
				p->vel[j] = (rand () % 512) - 256;
			}
		} else {
			p->type = pt_blob2;
			p->color = 150 + rand () % 6;
			for (j = 0; j < 3; j++) {
				p->org[j] = org[j] + ((rand () % 32) - 16);
				p->vel[j] = (rand () % 512) - 256;
			}
		}
	}
}


void
R_RunParticleEffect (vec3_t org, int color, int count)
{
	int         i, j;
	particle_t *p;
	int         scale;

	if (!r_particles->int_val)
		return;

	if (count > 130)
		scale = 3;
	else if (count > 20)
		scale = 2;
	else
		scale = 1;

	for (i = 0; i < count; i++) {
		if (!free_particles)
			return;
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->die = r_realtime + 0.1 * (rand () % 5);
		p->color = (color & ~7) + (rand () & 7);
		p->type = pt_grav;
		for (j = 0; j < 3; j++) {
			p->org[j] = org[j] + scale * ((rand () & 15) - 8);
			p->vel[j] = vec3_origin[j];	// + (rand()%300)-150;
		}
	}
}


void
R_LavaSplash (vec3_t org)
{
	int         i, j, k;
	particle_t *p;
	float       vel;
	vec3_t      dir;

	if (!r_particles->int_val)
		return;

	for (i = -16; i < 16; i++)
		for (j = -16; j < 16; j++)
			for (k = 0; k < 1; k++) {
				if (!free_particles)
					return;
				p = free_particles;
				free_particles = p->next;
				p->next = active_particles;
				active_particles = p;

				p->die = r_realtime + 2 + (rand () & 31) * 0.02;
				p->color = 224 + (rand () & 7);
				p->type = pt_grav;

				dir[0] = j * 8 + (rand () & 7);
				dir[1] = i * 8 + (rand () & 7);
				dir[2] = 256;

				p->org[0] = org[0] + dir[0];
				p->org[1] = org[1] + dir[1];
				p->org[2] = org[2] + (rand () & 63);

				VectorNormalize (dir);
				vel = 50 + (rand () & 63);
				VectorScale (dir, vel, p->vel);
			}
}


void
R_TeleportSplash (vec3_t org)
{
	int         i, j, k;
	particle_t *p;
	float       vel;
	vec3_t      dir;

	if (!r_particles->int_val)
		return;

	for (i = -16; i < 16; i += 4)
		for (j = -16; j < 16; j += 4)
			for (k = -24; k < 32; k += 4) {
				if (!free_particles)
					return;
				p = free_particles;
				free_particles = p->next;
				p->next = active_particles;
				active_particles = p;

				p->die = r_realtime + 0.2 + (rand () & 7) * 0.02;
				p->color = 7 + (rand () & 7);
				p->type = pt_grav;

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


void
R_RocketTrail (int type, entity_t *ent)
{
	vec3_t      vec;
	float       len;
	int         j;
	particle_t *p;

	if (!r_particles->int_val)
		return;

	VectorSubtract (ent->origin, ent->old_origin, vec);
	len = VectorNormalize (vec);
	while (len > 0) {
		len -= 3;

		if (!free_particles)
			return;
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		VectorCopy (vec3_origin, p->vel);
		p->die = r_realtime + 2;

		if (type == 4) {				// slight blood
			p->type = pt_slowgrav;
			p->color = 67 + (rand () & 3);
			for (j = 0; j < 3; j++)
				p->org[j] = ent->old_origin[j] + ((rand () % 6) - 3);
			len -= 3;
		} else if (type == 2) {			// blood
			p->type = pt_slowgrav;
			p->color = 67 + (rand () & 3);
			for (j = 0; j < 3; j++)
				p->org[j] = ent->old_origin[j] + ((rand () % 6) - 3);
		} else if (type == 6) {			// voor trail
			p->color = 9 * 16 + 8 + (rand () & 3);
			p->type = pt_static;
			p->die = r_realtime + 0.3;
			for (j = 0; j < 3; j++)
				p->org[j] = ent->old_origin[j] + ((rand () & 15) - 8);
		} else if (type == 1) {			// smoke smoke
			p->ramp = (rand () & 3) + 2;
			p->color = ramp3[(int) p->ramp];
			p->type = pt_fire;
			for (j = 0; j < 3; j++)
				p->org[j] = ent->old_origin[j] + ((rand () % 6) - 3);
		} else if (type == 0) {			// rocket trail
			p->ramp = (rand () & 3);
			p->color = ramp3[(int) p->ramp];
			p->type = pt_fire;
			for (j = 0; j < 3; j++)
				p->org[j] = ent->old_origin[j] + ((rand () % 6) - 3);
		} else if (type == 3 || type == 5) {	// tracer
			static int  tracercount;

			p->die = r_realtime + 0.5;
			p->type = pt_static;
			if (type == 3)
				p->color = 52 + ((tracercount & 4) << 1);
			else
				p->color = 230 + ((tracercount & 4) << 1);

			tracercount++;

			VectorCopy (ent->old_origin, p->org);
			if (tracercount & 1) {
				p->vel[0] = 30 * vec[1];
				p->vel[1] = 30 * -vec[0];
			} else {
				p->vel[0] = 30 * -vec[1];
				p->vel[1] = 30 * vec[0];
			}

		}

		VectorAdd (ent->old_origin, vec, ent->old_origin);
	}
}


void
R_DrawParticles (void)
{
	particle_t *p, **particle;
	float       grav;
	int         i;
	float       time2, time3;
	float       time1;
	float       dvel;
	float       frametime;
	
	D_StartParticles ();

	VectorScale (vright, xscaleshrink, r_pright);
	VectorScale (vup, yscaleshrink, r_pup);
	VectorCopy (vpn, r_ppn);

	frametime = r_frametime;
	time3 = frametime * 15;
	time2 = frametime * 10;				// 15;
	time1 = frametime * 5;
	grav = frametime * 800 * 0.05;
	dvel = 4 * frametime;

	for (particle = &active_particles; *particle;) {
		if ((*particle)->die < r_realtime) {
			p = (*particle)->next;
			(*particle)->next = free_particles;
			free_particles = (*particle);
			(*particle) = p;
		} else {
			p = *particle;
			particle = &(*particle)->next;

			D_DrawParticle (p);

			p->org[0] += p->vel[0] * frametime;
			p->org[1] += p->vel[1] * frametime;
			p->org[2] += p->vel[2] * frametime;

			switch (p->type) {
				case pt_static:
					break;
				case pt_fire:
					p->ramp += time1;
					if (p->ramp >= 6)
						p->die = -1;
					else
						p->color = ramp3[(int) p->ramp];
					p->vel[2] += grav;
					break;

				case pt_explode:
					p->ramp += time2;
					if (p->ramp >= 8)
						p->die = -1;
					else
						p->color = ramp1[(int) p->ramp];
					for (i = 0; i < 3; i++)
						p->vel[i] += p->vel[i] * dvel;
					p->vel[2] -= grav;
					break;

				case pt_explode2:
					p->ramp += time3;
					if (p->ramp >= 8)
						p->die = -1;
					else
						p->color = ramp2[(int) p->ramp];
					for (i = 0; i < 3; i++)
						p->vel[i] -= p->vel[i] * frametime;
					p->vel[2] -= grav;
					break;

				case pt_blob:
					for (i = 0; i < 3; i++)
						p->vel[i] += p->vel[i] * dvel;
					p->vel[2] -= grav;
					break;

				case pt_blob2:
					for (i = 0; i < 2; i++)
						p->vel[i] -= p->vel[i] * dvel;
					p->vel[2] -= grav;
					break;

				case pt_slowgrav:
				case pt_grav:
					p->vel[2] -= grav;
					break;
				default:
					Con_DPrintf ("unhandled particle type %d\n", p->type);
					break;
			}
		}
	}
	D_EndParticles ();
}
