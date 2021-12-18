/*
	sw32_rpart.c

	24 bit color software renderer particle effects.

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

#define NH_DEFINE
#include "namehack.h"

#include <stdlib.h>
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/cvar.h"
#include "QF/mersenne.h"
#include "QF/qargs.h"
#include "QF/quakefs.h"
#include "QF/render.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "QF/scene/entity.h"

#include "compat.h"
#include "r_internal.h"

static mtstate_t mt;	// private PRNG state

static vec4f_t
roffs (int mod)
{
	vec4f_t     offs = {
		(mtwist_rand (&mt) & mod) - 0.5 * (mod - 1),
		(mtwist_rand (&mt) & mod) - 0.5 * (mod - 1),
		(mtwist_rand (&mt) & mod) - 0.5 * (mod - 1),
		0
	};
	return offs;
}

static vec4f_t
tracer_vel (int tracercount, vec4f_t vec)
{
	if (tracercount & 1) {
		return (vec4f_t) { vec[1], -vec[0], 0, 0 };
	} else {
		return (vec4f_t) { -vec[1], vec[0], 0, 0 };
	}
}

static void
add_particle (ptype_t type, vec4f_t pos, vec4f_t vel, float live, int color,
			  float ramp)
{
	if (numparticles >= r_maxparticles)
		return;
	particle_t *p = &particles[numparticles];
	partparm_t *parm = &partparams[numparticles];
	const int **rampptr = &partramps[numparticles];
	numparticles += 1;

	p->pos = pos;
	p->vel = vel;
	p->icolor = color;
	p->alpha = 1;
	p->tex = 0;
	p->ramp = ramp;
	p->scale = 1;
	p->live = live;

	*parm = R_ParticlePhysics (type);
	*rampptr = R_ParticleRamp (type);
	if (*rampptr) {
		p->icolor = (*rampptr) [(int) p->ramp];
	}
}

void
sw32_R_InitParticles (void)
{
	mtwist_seed (&mt, 0xdeadbeef);
}

void
sw32_R_ClearParticles (void)
{
	if (r_maxparticles) {
		memset (particles, 0, r_maxparticles * sizeof (*particles));
	}
}

void
sw32_R_ReadPointFile_f (void)
{
	QFile      *f;
	int         c, r;
	const char *name;
	char       *mapname;
	vec4f_t     zero = {};

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
		vec4f_t     org = { 0, 0, 0, 1 };

		Qgets (f, buf, sizeof (buf));
		r = sscanf (buf, "%f %f %f\n", &org[0], &org[1], &org[2]);
		if (r != 3)
			break;
		c++;

		//if (!free_particles) {
		//	Sys_Printf ("Not enough free particles\n");
		//	break;
		//}
		add_particle (pt_static, org, zero, INFINITY, (-c) & 15, 0);
	}

	Qclose (f);
	Sys_Printf ("%i points read\n", c);
}

static void
R_ParticleExplosion_QF (vec4f_t org)
{
	if (!r_particles->int_val)
		return;

	for (int i = 0; i < 1024; i++) {
		ptype_t     type = i & 1 ? pt_explode2 : pt_explode;
		add_particle (type, org + roffs (32), roffs (512), 5,
					  0, mtwist_rand (&mt) & 3);
	}
}

static void
R_ParticleExplosion2_QF (vec4f_t org, int colorStart, int colorLength)
{
	int              colorMod = 0;

	for (int i=0; i<512; i++) {
		add_particle (pt_blob, org + roffs (32), roffs (512), 0.3,
					  colorStart + (colorMod % colorLength), 0);
	}
}

static void
R_BlobExplosion_QF (vec4f_t org)
{
	if (!r_particles->int_val)
		return;

	for (int i = 0; i < 1024; i++) {
		ptype_t     type = i & 1 ? pt_blob : pt_blob2;
		int         color = i & 1 ? 66 : 150;
		add_particle (type, org + roffs (32), roffs (512),
					  color + mtwist_rand (&mt) % 6,
					  (color & ~7) + (mtwist_rand (&mt) & 7), 0);
	}
}

static void
R_RunParticleEffect_QF (vec4f_t org, vec4f_t dir, int color, int count)
{
	if (!r_particles->int_val)
		return;

	for (int i = 0; i < count; i++) {
		add_particle (pt_slowgrav, org + roffs (16),
					  dir/* + roffs (300)*/,
					  0.1 * (mtwist_rand (&mt) % 5),
					  (color & ~7) + (mtwist_rand (&mt) & 7), 0);
	}
}

static void
R_SpikeEffect_QF (vec4f_t org)
{
	vec4f_t     zero = {};
	R_RunParticleEffect_QF (org, zero, 0, 10);
}

static void
R_SuperSpikeEffect_QF (vec4f_t org)
{
	vec4f_t     zero = {};
	R_RunParticleEffect_QF (org, zero, 0, 20);
}

static void
R_KnightSpikeEffect_QF (vec4f_t org)
{
	vec4f_t     zero = {};
	R_RunParticleEffect_QF (org, zero, 226, 20);
}

static void
R_WizSpikeEffect_QF (vec4f_t org)
{
	vec4f_t     zero = {};
	R_RunParticleEffect_QF (org, zero, 20, 30);
}

static void
R_BloodPuffEffect_QF (vec4f_t org, int count)
{
	vec4f_t     zero = {};
	R_RunParticleEffect_QF (org, zero, 73, count);
}

static void
R_GunshotEffect_QF (vec4f_t org, int count)
{
	vec4f_t     zero = {};
	R_RunParticleEffect_QF (org, zero, 0, count);
}

static void
R_LightningBloodEffect_QF (vec4f_t org)
{
	vec4f_t     zero = {};
	R_RunParticleEffect_QF (org, zero, 225, 50);
}

static void
R_LavaSplash_QF (vec4f_t org)
{
	if (!r_particles->int_val)
		return;

	for (int i = -16; i < 16; i++) {
		for (int j = -16; j < 16; j++) {
			for (int k = 0; k < 1; k++) {
				float       vel = 50 + (mtwist_rand (&mt) & 63);
				vec4f_t     dir = {
					j * 8 + (mtwist_rand (&mt) & 7),
					i * 8 + (mtwist_rand (&mt) & 7),
					256,
					0
				};
				vec4f_t     offs = {
					dir[0],
					dir[1],
					(mtwist_rand (&mt) & 63),
					0
				};
				dir = normalf (dir);
				add_particle (pt_grav, org + offs, vel * dir,
							  2 + (mtwist_rand (&mt) & 31) * 0.02,
							  224 + (mtwist_rand (&mt) & 7), 0);
			}
		}
	}
}

static void
R_TeleportSplash_QF (vec4f_t org)
{
	if (!r_particles->int_val)
		return;

	for (int i = -16; i < 16; i += 4) {
		for (int j = -16; j < 16; j += 4) {
			for (int k = -24; k < 32; k += 4) {
				float       vel = 50 + (mtwist_rand (&mt) & 63);
				vec4f_t     dir = normalf ((vec4f_t) { j, i, k, 0 } * 8);
				vec4f_t     offs = {
					i + (mtwist_rand (&mt) & 3),
					j + (mtwist_rand (&mt) & 3),
					k + (mtwist_rand (&mt) & 3),
					0
				};
				add_particle (pt_grav, org + offs, vel * dir,
							  0.2 + (mtwist_rand (&mt) & 7) * 0.02,
							  7 + (mtwist_rand (&mt) & 7), 0);
			}
		}
	}
}

static void
R_DarkFieldParticles_ID (vec4f_t org)
{
	if (!r_particles->int_val)
		return;

	for (int i = -16; i < 16; i += 8) {
		for (int j = -16; j < 16; j += 8) {
			for (int k = 0; k < 32; k += 8) {
				uint32_t    rnd = mtwist_rand (&mt);
				float       vel = 50 + ((rnd >> 9) & 63);
				vec4f_t     dir = normalf ((vec4f_t) { j, i, k, 0 } * 8);
				vec4f_t     offs = {
					i + ((rnd >> 3) & 3),
					j + ((rnd >> 5) & 3),
					k + ((rnd >> 7) & 3),
					0
				};

				add_particle (pt_slowgrav, org + offs, vel * dir,
							  0.2 + (rnd & 7) * 0.02,
							  150 + mtwist_rand (&mt) % 6, 0);
			}
		}
	}
}

static vec4f_t  velocities[NUMVERTEXNORMALS];
static vec4f_t  normals[NUMVERTEXNORMALS] = {
#include "anorms.h"
};

static void
R_EntityParticles_ID (vec4f_t org)
{
	int			i;
	float		angle, sp, sy, cp, cy; // cr, sr
	float		beamlength = 16.0, dist = 64.0;

	if (!r_particles->int_val)
		return;

	for (i = 0; i < NUMVERTEXNORMALS; i++) {
		int         k;
		for (k = 0; k < 3; k++) {
			velocities[i][k] = (mtwist_rand (&mt) & 255) * 0.01;
		}
	}

	vec4f_t     zero = {};
	for (i = 0; i < NUMVERTEXNORMALS; i++) {
		angle = vr_data.realtime * velocities[i][0];
		cy = cos (angle);
		sy = sin (angle);
		angle = vr_data.realtime * velocities[i][1];
		cp = cos (angle);
		sp = sin (angle);
// Next 3 lines results aren't currently used, may be in future. --Despair
//		angle = vr_data.realtime * avelocities[i][2];
//		sr = sin (angle);
//		cr = cos (angle);

		vec4f_t     forward = { cp * cy, cp * sy, -sp, 0 };
		vec4f_t     pos = org + normals[i] * dist + forward * beamlength;
		//FIXME 0 velocity?
		add_particle (pt_explode, pos, zero, 0.01, 0x6f, 0);
	}
}

static void
R_RocketTrail_QF (vec4f_t start, vec4f_t end)
{
	if (!r_particles->int_val)
		return;

	vec4f_t     vec = end - start;
	float       len = magnitudef (vec)[0];
	vec = normalf (vec);

	vec4f_t     zero = {};
	vec4f_t     pos = start;
	while (len > 0) {
		len -= 3;
		add_particle (pt_fire, pos + roffs (6), zero, 2,
					  0, (mtwist_rand (&mt) & 3));
		pos += vec;
	}
}

static void
R_GrenadeTrail_QF (vec4f_t start, vec4f_t end)
{
	if (!r_particles->int_val)
		return;

	vec4f_t     vec = end - start;
	float       len = magnitudef (vec)[0];
	vec = normalf (vec);

	vec4f_t     zero = {};
	vec4f_t     pos = start;
	while (len > 0) {
		len -= 3;
		add_particle (pt_fire, pos + roffs (6), zero, 2,
					  0, (mtwist_rand (&mt) & 3) + 2);
		pos += vec;
	}
}

static void
R_BloodTrail_QF (vec4f_t start, vec4f_t end)
{
	if (!r_particles->int_val)
		return;

	vec4f_t     vec = end - start;
	float       len = magnitudef (vec)[0];
	vec = normalf (vec);

	vec4f_t     zero = {};
	vec4f_t     pos = start;
	while (len > 0) {
		len -= 3;
		add_particle (pt_slowgrav, pos + roffs (6), zero, 2,
					  67 + (mtwist_rand (&mt) & 3), 0);
		pos += vec;
	}
}

static void
R_SlightBloodTrail_QF (vec4f_t start, vec4f_t end)
{
	if (!r_particles->int_val)
		return;

	vec4f_t     vec = end - start;
	float       len = magnitudef (vec)[0];
	vec = normalf (vec);

	vec4f_t     zero = {};
	vec4f_t     pos = start;
	while (len > 0) {
		len -= 6;
		add_particle (pt_slowgrav, pos + roffs (6), zero, 2,
					  67 + (mtwist_rand (&mt) & 3), 0);
		pos += vec;
	}
}

static void
R_WizTrail_QF (vec4f_t start, vec4f_t end)
{
	if (!r_particles->int_val)
		return;

	vec4f_t     vec = end - start;
	float       len = magnitudef (vec)[0];
	vec = normalf (vec);

	vec4f_t     pos = start;
	while (len > 0) {
		static int  tracercount;
		len -= 3;
		add_particle (pt_static, pos, 30 * tracer_vel (tracercount, vec), 0.5,
					  52 + ((tracercount & 4) << 1), 0);
		tracercount++;
		pos += vec;
	}
}

static void
R_FlameTrail_QF (vec4f_t start, vec4f_t end)
{
	if (!r_particles->int_val)
		return;

	vec4f_t     vec = end - start;
	float       len = magnitudef (vec)[0];
	vec = normalf (vec);

	vec4f_t     pos = start;
	while (len > 0) {
		static int tracercount;
		len -= 3;
		add_particle (pt_static, pos, 30 * tracer_vel (tracercount, vec), 0.5,
					  230 + ((tracercount & 4) << 1), 0);
		tracercount++;
		pos += vec;
	}
}

static void
R_VoorTrail_QF (vec4f_t start, vec4f_t end)
{
	if (!r_particles->int_val)
		return;

	vec4f_t     vec = end - start;
	float       len = magnitudef (vec)[0];
	vec = normalf (vec);

	vec4f_t     zero = {};
	vec4f_t     pos = start;
	while (len > 0) {
		len -= 3;
		add_particle (pt_static, pos + roffs (16), zero, 0.3,
					  9 * 16 + 8 + (mtwist_rand (&mt) & 3), 0);
		pos += vec;
	}
}

void
sw32_R_DrawParticles (void)
{
	VectorScale (vright, xscaleshrink, r_pright);
	VectorScale (vup, yscaleshrink, r_pup);
	VectorCopy (vpn, r_ppn);

	vec4f_t     gravity = {0, 0, -vr_data.gravity, 0};

	unsigned    j = 0;
	for (unsigned i = 0; i < numparticles; i++) {
		particle_t *p = &particles[i];
		partparm_t *parm = &partparams[i];

		if (p->live <= 0 || p->ramp >= parm->ramp_max) {
			continue;
		}
		const int  *ramp = partramps[j];
		if (i > j) {
			particles[j] = *p;
			partparams[j] = *parm;
			partramps[j] = ramp;
		}
		p = &particles[j];
		parm = &partparams[j];
		j += 1;

		sw32_D_DrawParticle (p);

		float       dT = vr_data.frametime;
		p->pos += dT * p->vel;
		p->vel += dT * (p->vel * parm->drag + gravity * parm->drag[3]);
		p->ramp += dT * parm->ramp;
		p->live -= dT;
		if (ramp) {
			p->icolor = ramp[(int)p->ramp];
		}
	}
	numparticles = j;
}

void
sw32_r_easter_eggs_f (cvar_t *var)
{
}

void
sw32_r_particles_style_f (cvar_t *var)
{
}

static void
sw32_R_Particle_New (ptype_t type, int texnum, vec4f_t pos, float scale,
					 vec4f_t vel, float live, int color, float alpha,
					 float ramp)
{
	add_particle (type, pos, vel, live, color, ramp);
}

static void
sw32_R_Particle_NewRandom (ptype_t type, int texnum, vec4f_t org,
						   int org_fuzz, float scale, int vel_fuzz, float live,
						   int color, float alpha, float ramp)
{
	float       o_fuzz = org_fuzz, v_fuzz = vel_fuzz;
	int         rnd;
	vec4f_t     pos, vel;

	rnd = mtwist_rand (&mt);
	pos[0] = o_fuzz * ((rnd & 63) - 31.5) / 63.0 + org[0];
	pos[1] = o_fuzz * (((rnd >> 6) & 63) - 31.5) / 63.0 + org[1];
	pos[2] = o_fuzz * (((rnd >> 12) & 63) - 31.5) / 63.0 + org[2];
	pos[3] = 1;
	rnd = mtwist_rand (&mt);
	vel[0] = v_fuzz * ((rnd & 63) - 31.5) / 63.0;
	vel[1] = v_fuzz * (((rnd >> 6) & 63) - 31.5) / 63.0;
	vel[2] = v_fuzz * (((rnd >> 12) & 63) - 31.5) / 63.0;
	vel[3] = 0;

	add_particle (type, pos, vel, live, color, ramp);
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
	sw32_vid_render_funcs.particles = &particles_QF;
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
sw32_R_Particles_Init_Cvars (void)
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
