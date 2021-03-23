/*
	sw_rpart.c

	Software renderer particle effects code.

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

#include <stdlib.h>
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/cvar.h"
#include "QF/entity.h"
#include "QF/mersenne.h"
#include "QF/qargs.h"
#include "QF/quakefs.h"
#include "QF/render.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "compat.h"
#include "r_internal.h"

static int		ramp1[8] = { 0x6f, 0x6d, 0x6b, 0x69, 0x67, 0x65, 0x63, 0x61 };
//static int	ramp2[8] = { 0x6f, 0x6e, 0x6d, 0x6c, 0x6b, 0x6a, 0x68, 0x66 };
static int		ramp3[8] = { 0x6d, 0x6b, 6, 5, 4, 3 };

static mtstate_t mt;	// private PRNG state

void
R_InitParticles (void)
{
	mtwist_seed (&mt, 0xdeadbeef);
}

void
R_ClearParticles (void)
{
	unsigned int i;

	free_particles = &particles[0];
	active_particles = NULL;

	for (i = 0; i < r_maxparticles; i++)
		particles[i].next = &particles[i + 1];
	if (r_maxparticles)
		particles[r_maxparticles - 1].next = NULL;
}

void
R_ReadPointFile_f (void)
{
	QFile      *f;
	vec3_t      org;
	int         c, r;
	particle_t *p;
	const char *name;
	char       *mapname;

	mapname = strdup (r_worldentity.renderer.model->path);
	if (!mapname)
		Sys_Error ("Can't duplicate mapname!");
	QFS_StripExtension (mapname, mapname);

	name = va (0, "maps/%s.pts", mapname);
	free (mapname);

	f = QFS_FOpenFile (name);
	if (!f) {
		Sys_Printf ("couldn't open %s\n", name);
		return;
	}

	Sys_Printf ("Reading %s...\n", name);
	c = 0;
	for (;;) {
		char        buf[64];

		Qgets (f, buf, sizeof (buf));
		r = sscanf (buf, "%f %f %f\n", &org[0], &org[1], &org[2]);
		if (r != 3)
			break;
		c++;

		if (!free_particles) {
			Sys_Printf ("Not enough free particles\n");
			break;
		}
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->die = 99999;
		p->color = (-c) & 15;
		p->type = pt_static;
		p->phys = R_ParticlePhysics (p->type);
		VectorZero (p->vel);
		VectorCopy (org, p->org);
	}

	Qclose (f);
	Sys_Printf ("%i points read\n", c);
}

static void
R_ParticleExplosion_QF (const vec3_t org)
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

		p->die = vr_data.realtime + 5;
		p->color = ramp1[0];
		p->ramp = mtwist_rand (&mt) & 3;
		if (i & 1)
			p->type = pt_explode;
		else
			p->type = pt_explode2;
		p->phys = R_ParticlePhysics (p->type);
		for (j = 0; j < 3; j++) {
			p->org[j] = org[j] + ((mtwist_rand (&mt) % 32) - 16);
			p->vel[j] = (mtwist_rand (&mt) % 512) - 256;
		}
	}
}

static void
R_ParticleExplosion2_QF (const vec3_t org, int colorStart, int colorLength)
{
	int              i, j;
	int              colorMod = 0;
	particle_t      *p;

	for (i=0; i<512; i++) {
		if (!free_particles)
			return;
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->die = vr_data.realtime + 0.3;
		p->color = colorStart + (colorMod % colorLength);
		colorMod++;

		p->type = pt_blob;
		p->phys = R_ParticlePhysics (p->type);
		for (j=0 ; j<3 ; j++) {
			p->org[j] = org[j] + ((mtwist_rand (&mt)%32)-16);
			p->vel[j] = (mtwist_rand (&mt)%512)-256;
		}
	}
}

static void
R_BlobExplosion_QF (const vec3_t org)
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

		p->die = vr_data.realtime + 1 + (mtwist_rand (&mt) & 8) * 0.05;

		if (i & 1) {
			p->type = pt_blob;
			p->color = 66 + mtwist_rand (&mt) % 6;
		} else {
			p->type = pt_blob2;
			p->color = 150 + mtwist_rand (&mt) % 6;
		}
		p->phys = R_ParticlePhysics (p->type);
		for (j = 0; j < 3; j++) {
			p->org[j] = org[j] + ((mtwist_rand (&mt) % 32) - 16);
			p->vel[j] = (mtwist_rand (&mt) % 512) - 256;
		}
	}
}

static void
R_RunParticleEffect_QF (const vec3_t org, const vec3_t dir, int color,
						int count)
{
	int         i, j;
	particle_t *p;

	if (!r_particles->int_val)
		return;

	for (i = 0; i < count; i++) {
		if (!free_particles)
			return;
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->die = vr_data.realtime + 0.1 * (mtwist_rand (&mt) % 5);
		p->color = (color & ~7) + (mtwist_rand (&mt) & 7);
		p->type = pt_grav;
		p->phys = R_ParticlePhysics (p->type);
		for (j = 0; j < 3; j++) {
			p->org[j] = org[j] + ((mtwist_rand (&mt) & 15) - 8);
			p->vel[j] = dir[j];	// + (mtwist_rand (&mt)%300)-150;
		}
	}
}

static void
R_SpikeEffect_QF (const vec3_t org)
{
	R_RunParticleEffect_QF (org, vec3_origin, 0, 10);
}

static void
R_SuperSpikeEffect_QF (const vec3_t org)
{
	R_RunParticleEffect_QF (org, vec3_origin, 0, 20);
}

static void
R_KnightSpikeEffect_QF (const vec3_t org)
{
	R_RunParticleEffect_QF (org, vec3_origin, 226, 20);
}

static void
R_WizSpikeEffect_QF (const vec3_t org)
{
	R_RunParticleEffect_QF (org, vec3_origin, 20, 30);
}

static void
R_BloodPuffEffect_QF (const vec3_t org, int count)
{
	R_RunParticleEffect_QF (org, vec3_origin, 73, count);
}

static void
R_GunshotEffect_QF (const vec3_t org, int count)
{
	R_RunParticleEffect_QF (org, vec3_origin, 0, count);
}

static void
R_LightningBloodEffect_QF (const vec3_t org)
{
	R_RunParticleEffect_QF (org, vec3_origin, 225, 50);
}

static void
R_LavaSplash_QF (const vec3_t org)
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

				p->die = vr_data.realtime + 2 + (mtwist_rand (&mt) & 31) * 0.02;
				p->color = 224 + (mtwist_rand (&mt) & 7);
				p->type = pt_grav;
				p->phys = R_ParticlePhysics (p->type);

				dir[0] = j * 8 + (mtwist_rand (&mt) & 7);
				dir[1] = i * 8 + (mtwist_rand (&mt) & 7);
				dir[2] = 256;

				p->org[0] = org[0] + dir[0];
				p->org[1] = org[1] + dir[1];
				p->org[2] = org[2] + (mtwist_rand (&mt) & 63);

				VectorNormalize (dir);
				vel = 50 + (mtwist_rand (&mt) & 63);
				VectorScale (dir, vel, p->vel);
			}
}

static void
R_TeleportSplash_QF (const vec3_t org)
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

				p->die = vr_data.realtime + 0.2 + (mtwist_rand (&mt) & 7) * 0.02;
				p->color = 7 + (mtwist_rand (&mt) & 7);
				p->type = pt_grav;
				p->phys = R_ParticlePhysics (p->type);

				dir[0] = j * 8;
				dir[1] = i * 8;
				dir[2] = k * 8;

				p->org[0] = org[0] + i + (mtwist_rand (&mt) & 3);
				p->org[1] = org[1] + j + (mtwist_rand (&mt) & 3);
				p->org[2] = org[2] + k + (mtwist_rand (&mt) & 3);

				VectorNormalize (dir);
				vel = 50 + (mtwist_rand (&mt) & 63);
				VectorScale (dir, vel, p->vel);
			}
}

static void
R_DarkFieldParticles_ID (const entity_t *ent)
{
	int				i, j, k;
	unsigned int	rnd;
	float			vel;
	particle_t	   *p;
	vec3_t			dir, org;

	if (!r_particles->int_val)
		return;

	VectorCopy (Transform_GetWorldPosition (ent->transform), org);
	for (i = -16; i < 16; i += 8) {
		for (j = -16; j < 16; j += 8) {
			for (k = 0; k < 32; k += 8) {
				if (!free_particles)
					return;
				p = free_particles;
				free_particles = p->next;
                p->next = active_particles;
				active_particles = p;

				rnd = mtwist_rand (&mt);

				p->die = vr_data.realtime + 0.2 + (rnd & 7) * 0.02;
				p->color = 150 + mtwist_rand (&mt) % 6;
				p->type = pt_slowgrav;
				p->phys = R_ParticlePhysics (p->type);
				dir[0] = j * 8;
				dir[1] = i * 8;
				dir[2] = k * 8;

				p->org[0] = org[0] + i + ((rnd >> 3) & 3);
				p->org[1] = org[1] + j + ((rnd >> 5) & 3);
				p->org[2] = org[2] + k + ((rnd >> 7) & 3);

				VectorNormalize (dir);
				vel = 50 + ((rnd >> 9) & 63);
				VectorScale (dir, vel, p->vel);
			}
		}
	}
}

static vec3_t		avelocities[NUMVERTEXNORMALS];

static void
R_EntityParticles_ID (const entity_t *ent)
{
	int			i;
	float		angle, sp, sy, cp, cy; // cr, sr
	float		beamlength = 16.0, dist = 64.0;
	particle_t *p;
	vec3_t		forward;
	vec3_t      org;

	if (!r_particles->int_val)
		return;

	VectorCopy (Transform_GetWorldPosition (ent->transform), org);

	for (i = 0; i < NUMVERTEXNORMALS; i++) {
		int         k;
		for (k = 0; k < 3; k++) {
			avelocities[i][k] = (mtwist_rand (&mt) & 255) * 0.01;
		}
	}

	for (i = 0; i < NUMVERTEXNORMALS; i++) {
		angle = vr_data.realtime * avelocities[i][0];
		cy = cos (angle);
		sy = sin (angle);
		angle = vr_data.realtime * avelocities[i][1];
		cp = cos (angle);
		sp = sin (angle);
// Next 3 lines results aren't currently used, may be in future. --Despair
//		angle = vr_data.realtime * avelocities[i][2];
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

		p->die = vr_data.realtime + 0.01;
		p->color = 0x6f;
		p->type = pt_explode;
		p->phys = R_ParticlePhysics (p->type);

		p->org[0] = org[0] + r_avertexnormals[i][0] * dist +
			forward[0] * beamlength;
		p->org[1] = org[1] + r_avertexnormals[i][1] * dist +
			forward[1] * beamlength;
		p->org[2] = org[2] + r_avertexnormals[i][2] * dist +
			forward[2] * beamlength;
	}
}

static void
R_RocketTrail_QF (const entity_t *ent)
{
	float		len;
	int			j;
	particle_t *p;
	vec3_t		old_origin, vec;
	vec3_t		org;

	if (!r_particles->int_val)
		return;

	VectorCopy (Transform_GetWorldPosition (ent->transform), org);
	VectorCopy (ent->old_origin, old_origin);
	VectorSubtract (org, old_origin, vec);
	len = VectorNormalize (vec);

	while (len > 0) {
		len -= 3;

		if (!free_particles)
			return;
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		VectorZero (p->vel);

		p->die = vr_data.realtime + 2;
		p->ramp = (mtwist_rand (&mt) & 3);
		p->color = ramp3[(int) p->ramp];
		p->type = pt_fire;
		p->phys = R_ParticlePhysics (p->type);
		for (j = 0; j < 3; j++)
			p->org[j] = old_origin[j] + ((mtwist_rand (&mt) % 6) - 3);

		VectorAdd (old_origin, vec, old_origin);
	}
}

static void
R_GrenadeTrail_QF (const entity_t *ent)
{
	float		len;
	int			j;
	particle_t *p;
	vec3_t		old_origin, vec;
	vec3_t		org;

	if (!r_particles->int_val)
		return;

	VectorCopy (ent->old_origin, old_origin);
	VectorCopy (Transform_GetWorldPosition (ent->transform), org);
	VectorSubtract (org, old_origin, vec);
	len = VectorNormalize (vec);

	while (len > 0) {
		len -= 3;

		if (!free_particles)
			return;
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		VectorZero (p->vel);

		p->die = vr_data.realtime + 2;
		p->ramp = (mtwist_rand (&mt) & 3) + 2;
		p->color = ramp3[(int) p->ramp];
		p->type = pt_fire;
		p->phys = R_ParticlePhysics (p->type);
		for (j = 0; j < 3; j++)
			p->org[j] = old_origin[j] + ((mtwist_rand (&mt) % 6) - 3);

		VectorAdd (old_origin, vec, old_origin);
	}
}

static void
R_BloodTrail_QF (const entity_t *ent)
{
	float		len;
	int			j;
	particle_t *p;
	vec3_t		old_origin, vec;
	vec3_t      org;

	if (!r_particles->int_val)
		return;

	VectorCopy (ent->old_origin, old_origin);
	VectorCopy (Transform_GetWorldPosition (ent->transform), org);
	VectorSubtract (org, old_origin, vec);
	len = VectorNormalize (vec);

	while (len > 0) {
		len -= 3;

		if (!free_particles)
			return;
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		VectorZero (p->vel);

		p->die = vr_data.realtime + 2;
		p->type = pt_slowgrav;
		p->phys = R_ParticlePhysics (p->type);
		p->color = 67 + (mtwist_rand (&mt) & 3);
		for (j = 0; j < 3; j++)
			p->org[j] = old_origin[j] + ((mtwist_rand (&mt) % 6) - 3);

		VectorAdd (old_origin, vec, old_origin);
	}
}

static void
R_SlightBloodTrail_QF (const entity_t *ent)
{
	float		len;
	int			j;
	particle_t *p;
	vec3_t		old_origin, vec;
	vec3_t		org;

	if (!r_particles->int_val)
		return;

	VectorCopy (ent->old_origin, old_origin);
	VectorCopy (Transform_GetWorldPosition (ent->transform), org);
	VectorSubtract (org, old_origin, vec);
	len = VectorNormalize (vec);

	while (len > 0) {
		len -= 6;

		if (!free_particles)
			return;
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		VectorZero (p->vel);

		p->die = vr_data.realtime + 2;
		p->type = pt_slowgrav;
		p->phys = R_ParticlePhysics (p->type);
		p->color = 67 + (mtwist_rand (&mt) & 3);
		for (j = 0; j < 3; j++)
			p->org[j] = old_origin[j] + ((mtwist_rand (&mt) % 6) - 3);

		VectorAdd (old_origin, vec, old_origin);
	}
}

static void
R_WizTrail_QF (const entity_t *ent)
{
	float		len;
	particle_t *p;
	vec3_t		old_origin, vec;
	vec3_t		org;

	if (!r_particles->int_val)
		return;

	VectorCopy (ent->old_origin, old_origin);
	VectorCopy (Transform_GetWorldPosition (ent->transform), org);
	VectorSubtract (org, old_origin, vec);
	len = VectorNormalize (vec);

	while (len > 0) {
		static int  tracercount;

		len -= 3;

		if (!free_particles)
			return;
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->die = vr_data.realtime + 0.5;
		p->type = pt_static;
		p->phys = R_ParticlePhysics (p->type);
		p->color = 52 + ((tracercount & 4) << 1);

		tracercount++;

		VectorCopy (old_origin, p->org);
		if (tracercount & 1) {
			p->vel[0] = 30.0 * vec[1];
			p->vel[1] = 30.0 * -vec[0];
		} else {
			p->vel[0] = 30.0 * -vec[1];
			p->vel[1] = 30.0 * vec[0];
		}
		p->vel[2] = 0.0;

		VectorAdd (old_origin, vec, old_origin);
	}
}

static void
R_FlameTrail_QF (const entity_t *ent)
{
	float		len;
	particle_t *p;
	vec3_t		old_origin, vec;
	vec3_t		org;

	if (!r_particles->int_val)
		return;

	VectorCopy (ent->old_origin, old_origin);
	VectorCopy (Transform_GetWorldPosition (ent->transform), org);
	VectorSubtract (org, old_origin, vec);
	len = VectorNormalize (vec);

	while (len > 0) {
		static int tracercount;

		len -= 3;

		if (!free_particles)
			return;
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->die = vr_data.realtime + 0.5;
		p->type = pt_static;
		p->phys = R_ParticlePhysics (p->type);
		p->color = 230 + ((tracercount & 4) << 1);

		tracercount++;

		VectorCopy (old_origin, p->org);
		if (tracercount & 1) {
			p->vel[0] = 30 * vec[1];
			p->vel[1] = 30 * -vec[0];
		} else {
			p->vel[0] = 30 * -vec[1];
			p->vel[1] = 30 * vec[0];
		}
		p->vel[2] = 0.0;

		VectorAdd (old_origin, vec, old_origin);
	}
}

static void
R_VoorTrail_QF (const entity_t *ent)
{
	float		len;
	int			j;
	particle_t *p;
	vec3_t		old_origin, vec;
	vec3_t      org;

	if (!r_particles->int_val)
		return;

	VectorCopy (ent->old_origin, old_origin);
	VectorCopy (Transform_GetWorldPosition (ent->transform), org);
	VectorSubtract (org, old_origin, vec);
	len = VectorNormalize (vec);

	while (len > 0) {
		len -= 3;

		if (!free_particles)
			return;
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		VectorZero (p->vel);

		p->die = vr_data.realtime + 0.3;
		p->type = pt_static;
		p->phys = R_ParticlePhysics (p->type);
		p->color = 9 * 16 + 8 + (mtwist_rand (&mt) & 3);
		for (j = 0; j < 3; j++)
			p->org[j] = old_origin[j] + ((mtwist_rand (&mt) & 15) - 8);

		VectorAdd (old_origin, vec, old_origin);
	}
}

void
R_DrawParticles (void)
{
	particle_t *p, **particle;

	VectorScale (vright, xscaleshrink, r_pright);
	VectorScale (vup, yscaleshrink, r_pup);
	VectorCopy (vpn, r_ppn);

	for (particle = &active_particles; *particle;) {
		if ((*particle)->die < vr_data.realtime) {
			p = (*particle)->next;
			(*particle)->next = free_particles;
			free_particles = (*particle);
			(*particle) = p;
		} else {
			p = *particle;
			particle = &(*particle)->next;

			D_DrawParticle (p);

			p->phys (p);
		}
	}
}

void
r_easter_eggs_f (cvar_t *var)
{
}

void
r_particles_style_f (cvar_t *var)
{
}

static void
R_Particle_New (ptype_t type, int texnum, const vec3_t org, float scale,
			    const vec3_t vel, float die, int color, float alpha, float ramp)
{
	particle_t *p;

	if (!free_particles)
		return;
	p = free_particles;
	free_particles = p->next;
	p->next = active_particles;
	active_particles = p;

	VectorCopy (org, p->org);
	p->color = color;
	p->tex = texnum;
	p->scale = scale;
	p->alpha = alpha;
	VectorCopy (vel, p->vel);
	p->type = type;
	p->phys = R_ParticlePhysics (p->type);
	p->die = die;
	p->ramp = ramp;
}

static void
R_Particle_NewRandom (ptype_t type, int texnum, const vec3_t org, int org_fuzz,
					  float scale, int vel_fuzz, float die, int color,
					  float alpha, float ramp)
{
	float       o_fuzz = org_fuzz, v_fuzz = vel_fuzz;
	int         rnd;
	vec3_t      porg, pvel;

	rnd = mtwist_rand (&mt);
	porg[0] = o_fuzz * ((rnd & 63) - 31.5) / 63.0 + org[0];
	porg[1] = o_fuzz * (((rnd >> 6) & 63) - 31.5) / 63.0 + org[1];
	porg[2] = o_fuzz * (((rnd >> 10) & 63) - 31.5) / 63.0 + org[2];
	rnd = mtwist_rand (&mt);
	pvel[0] = v_fuzz * ((rnd & 63) - 31.5) / 63.0;
	pvel[1] = v_fuzz * (((rnd >> 6) & 63) - 31.5) / 63.0;
	pvel[2] = v_fuzz * (((rnd >> 10) & 63) - 31.5) / 63.0;

	R_Particle_New (type, texnum, porg, scale, pvel, die, color, alpha, ramp);
}

static vid_particle_funcs_t particles_QF = {
	R_RocketTrail_QF,
	R_GrenadeTrail_QF,
	R_BloodTrail_QF,
	R_SlightBloodTrail_QF,
	R_WizTrail_QF,
	R_FlameTrail_QF,
	R_VoorTrail_QF,
	0,//R_GlowTrail_QF,
	R_RunParticleEffect_QF,
	R_BloodPuffEffect_QF,
	R_GunshotEffect_QF,
	R_LightningBloodEffect_QF,
	R_SpikeEffect_QF,
	R_KnightSpikeEffect_QF,
	R_SuperSpikeEffect_QF,
	R_WizSpikeEffect_QF,
	R_BlobExplosion_QF,
	R_ParticleExplosion_QF,
	R_ParticleExplosion2_QF,
	R_LavaSplash_QF,
	R_TeleportSplash_QF,
	R_DarkFieldParticles_ID,
	R_EntityParticles_ID,
	R_Particle_New,
	R_Particle_NewRandom,
};

static void
R_ParticleFunctionInit (void)
{
	sw_vid_render_funcs.particles = &particles_QF;
}

static void
r_particles_nearclip_f (cvar_t *var)
{
	Cvar_SetValue (r_particles_nearclip, bound (r_nearclip->value, var->value,
												r_farclip->value));
}

static void
r_particles_f (cvar_t *var)
{
	R_MaxParticlesCheck (var, r_particles_max);
}

static void
r_particles_max_f (cvar_t *var)
{
	R_MaxParticlesCheck (r_particles, var);
}

void
R_Particles_Init_Cvars (void)
{
	easter_eggs = Cvar_Get ("easter_eggs", "0", CVAR_NONE, r_easter_eggs_f,
							"Enables easter eggs.");
	r_particles = Cvar_Get ("r_particles", "1", CVAR_ARCHIVE, r_particles_f,
							"Toggles drawing of particles.");
	r_particles_max = Cvar_Get ("r_particles_max", "2048", CVAR_ARCHIVE,
								r_particles_max_f, "Maximum amount of "
								"particles to display. No maximum, minimum "
								"is 0.");
	r_particles_nearclip = Cvar_Get ("r_particles_nearclip", "32",
									 CVAR_ARCHIVE, r_particles_nearclip_f,
									 "Distance of the particle near clipping "
									 "plane from the player.");
	r_particles_style = Cvar_Get ("r_particles_style", "1", CVAR_ARCHIVE,
								  r_particles_style_f, "Sets particle style. "
								  "0 for Id, 1 for QF.");
	R_ParticleFunctionInit ();
}
