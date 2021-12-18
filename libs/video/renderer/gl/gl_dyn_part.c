/*
	gl_dyn_part.c

	OpenGL particle system.

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

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include <stdlib.h>

#include "QF/cmd.h"
#include "QF/cvar.h"
#include "QF/mersenne.h"
#include "QF/qargs.h"
#include "QF/quakefs.h"
#include "QF/render.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "QF/scene/entity.h"

#include "QF/GL/defines.h"
#include "QF/GL/funcs.h"
#include "QF/GL/qf_explosions.h"
#include "QF/GL/qf_textures.h"
#include "QF/GL/qf_vid.h"

#include "compat.h"
#include "r_internal.h"
#include "varrays.h"

static int						partUseVA;
static int						pVAsize;
static int					   *pVAindices;
static varray_t2f_c4ub_v3f_t   *particleVertexArray;

static mtstate_t mt;	// private PRNG state

inline static void
particle_new (ptype_t type, int texnum, vec4f_t pos, float scale,
			  vec4f_t vel, float live, int color, float alpha, float ramp)
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
	p->alpha = alpha;
	p->tex = texnum;
	p->ramp = ramp;
	p->scale = scale;
	p->live = live;

	*parm = R_ParticlePhysics (type);
	*rampptr = R_ParticleRamp (type);
	if (*rampptr) {
		p->icolor = (*rampptr) [(int) p->ramp];
	}
}

/*
	particle_new_random

	note that org_fuzz & vel_fuzz should be ints greater than 0 if you are
	going to bother using this function.
*/
inline static void
particle_new_random (ptype_t type, int texnum, vec4f_t org, int org_fuzz,
					 float scale, int vel_fuzz, float live, int color,
					 float alpha, float ramp)
{
	float		o_fuzz = org_fuzz, v_fuzz = vel_fuzz;
	int         rnd;
	vec4f_t     porg, pvel;

	rnd = mtwist_rand (&mt);
	porg[0] = o_fuzz * ((rnd & 63) - 31.5) / 63.0 + org[0];
	porg[1] = o_fuzz * (((rnd >> 6) & 63) - 31.5) / 63.0 + org[1];
	porg[2] = o_fuzz * (((rnd >> 10) & 63) - 31.5) / 63.0 + org[2];
	porg[3] = 1;
	rnd = mtwist_rand (&mt);
	pvel[0] = v_fuzz * ((rnd & 63) - 31.5) / 63.0;
	pvel[1] = v_fuzz * (((rnd >> 6) & 63) - 31.5) / 63.0;
	pvel[2] = v_fuzz * (((rnd >> 10) & 63) - 31.5) / 63.0;
	pvel[3] = 0;

	particle_new (type, texnum, porg, scale, pvel, live, color, alpha, ramp);
}

/*
inline static void
particle_new_veryrandom (ptype_t type, int texnum, vec4f_t org,
						 int org_fuzz, float scale, int vel_fuzz, float live,
						 int color, float alpha, float ramp)
{
	vec3_t		porg, pvel;

	porg[0] = qfrandom (org_fuzz * 2) - org_fuzz + org[0];
	porg[1] = qfrandom (org_fuzz * 2) - org_fuzz + org[1];
	porg[2] = qfrandom (org_fuzz * 2) - org_fuzz + org[2];
	pvel[0] = qfrandom (vel_fuzz * 2) - vel_fuzz;
	pvel[1] = qfrandom (vel_fuzz * 2) - vel_fuzz;
	pvel[2] = qfrandom (vel_fuzz * 2) - vel_fuzz;
	particle_new (type, texnum, porg, scale, pvel, live, color, alpha, ramp);
}
*/

static vec4f_t
roffs (int mod)
{
	vec4f_t     offs = {
		(mtwist_rand (&mt) % mod) - 0.5 * (mod - 1),
		(mtwist_rand (&mt) % mod) - 0.5 * (mod - 1),
		(mtwist_rand (&mt) % mod) - 0.5 * (mod - 1),
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
	particle_new (type, part_tex_dot, pos, 1, vel, live, color, 1, ramp);
}

void
gl_R_ClearParticles (void)
{
	numparticles = 0;
}

void
gl_R_InitParticles (void)
{
	int		i;

	mtwist_seed (&mt, 0xdeadbeef);

	if (r_maxparticles && r_init) {
		if (vaelements) {
			partUseVA = 0;
			pVAsize = r_maxparticles * 4;
			Sys_MaskPrintf (SYS_dev,
							"Particles: Vertex Array use disabled.\n");
		} else {
			if (vaelements > 3)
				pVAsize = min ((unsigned int) (vaelements - (vaelements % 4)),
							   r_maxparticles * 4);
			else if (vaelements >= 0)
				pVAsize = r_maxparticles * 4;
			Sys_MaskPrintf (SYS_dev,
							"Particles: %i maximum vertex elements.\n",
							pVAsize);
		}
		if (particleVertexArray)
			free (particleVertexArray);
		particleVertexArray = (varray_t2f_c4ub_v3f_t *)
			calloc (pVAsize, sizeof (varray_t2f_c4ub_v3f_t));

		if (partUseVA)
			qfglInterleavedArrays (GL_T2F_C4UB_V3F, 0, particleVertexArray);

		if (pVAindices)
			free (pVAindices);
		pVAindices = (int *) calloc (pVAsize, sizeof (int));
		for (i = 0; i < pVAsize; i++)
			pVAindices[i] = i;
	} else {
		if (particleVertexArray) {
			free (particleVertexArray);
			particleVertexArray = 0;
		}
		if (pVAindices) {
			free (pVAindices);
			pVAindices = 0;
		}
	}
}


void
gl_R_ReadPointFile_f (void)
{
	const char *name;
	char       *mapname;
	int         c;
	QFile      *f;

	mapname = strdup (r_worldentity.renderer.model->path);
	if (!mapname)
		Sys_Error ("Can't duplicate mapname!");
	QFS_StripExtension (mapname, mapname);

	name = va (0, "%s.pts", mapname);
	free (mapname);

	f = QFS_FOpenFile (name);
	if (!f) {
		Sys_Printf ("couldn't open %s\n", name);
		return;
	}

	Sys_MaskPrintf (SYS_dev, "Reading %s...\n", name);
	c = 0;
	vec4f_t     zero = {};
	for (;;) {
		char        buf[64];
		vec4f_t     org = { 0, 0, 0, 1 };

		Qgets (f, buf, sizeof (buf));
		int         r = sscanf (buf, "%f %f %f\n", &org[0], &org[1], &org[2]);
		if (r != 3)
			break;
		c++;

		if (numparticles >= r_maxparticles) {
			Sys_MaskPrintf (SYS_dev, "Not enough free particles\n");
			break;
		} else {
			particle_new (pt_static, part_tex_dot, org, 1.5, zero,
						  99999, (-c) & 15, 1.0, 0.0);
		}
	}
	Qclose (f);
	Sys_MaskPrintf (SYS_dev, "%i points read\n", c);
}

static void
R_ParticleExplosion_QF (vec4f_t org)
{
//	R_NewExplosion (org);
	if (numparticles >= r_maxparticles)
		return;
	particle_new_random (pt_smokecloud, part_tex_smoke, org, 4, 30, 8,
						 5.0, (mtwist_rand (&mt) & 7) + 8,
						 0.5 + qfrandom (0.25), 0.0);
}

static void
R_ParticleExplosion2_QF (vec4f_t org, int colorStart, int colorLength)
{
	unsigned int	i, j = 512;

	if (numparticles >= r_maxparticles)
		return;
	else if (numparticles + j >= r_maxparticles)
		j = r_maxparticles - numparticles;

	for (i = 0; i < j; i++) {
		particle_new_random (pt_blob, part_tex_dot, org, 16, 2, 256,
							 0.3,
							 colorStart + (i % colorLength), 1.0, 0.0);
	}
}

static void
R_BlobExplosion_QF (vec4f_t org)
{
	unsigned int	i;
	unsigned int	j = 1024;

	if (numparticles >= r_maxparticles)
		return;
	else if (numparticles + j >= r_maxparticles)
		j = r_maxparticles - numparticles;

	for (i = 0; i < j >> 1; i++) {
		particle_new_random (pt_blob, part_tex_dot, org, 12, 2, 256,
							 1.0 + (mtwist_rand (&mt) & 7) * 0.05,
							 66 + i % 6, 1.0, 0.0);
	}
	for (i = 0; i < j / 2; i++) {
		particle_new_random (pt_blob2, part_tex_dot, org, 12, 2, 256,
							 1.0 + (mtwist_rand (&mt) & 7) * 0.05,
							 150 + i % 6, 1.0, 0.0);
	}
}

static inline void
R_RunSparkEffect_QF (vec4f_t org, int count, int ofuzz)
{
	if (!r_particles->int_val)
		return;

	vec4f_t     zero = {};
	particle_new (pt_smokecloud, part_tex_smoke, org, ofuzz * 0.08,
				  zero, 9, 12 + (mtwist_rand (&mt) & 3),
				  0.25 + qfrandom (0.125), 0.0);

	if (count > 0) {
		int orgfuzz = ofuzz * 3 / 4;
		if (orgfuzz < 1)
			orgfuzz = 1;

		while (count--) {
			int		color = mtwist_rand (&mt) & 7;

			particle_new_random (pt_fallfadespark, part_tex_dot, org, orgfuzz,
								 0.7, 96, 5.0, 0, 1.0, color);
		}
	}
}

static inline void
R_BloodPuff_QF (vec4f_t org, int count)
{
	if (!r_particles->int_val)
		return;

	vec4f_t     zero = {};
	particle_new (pt_bloodcloud, part_tex_smoke, org, count / 5, zero,
				  99.0, 70 + (mtwist_rand (&mt) & 3), 0.5, 0.0);
}

static void
R_BloodPuffEffect_QF (vec4f_t org, int count)
{
	R_BloodPuff_QF (org, count);
}

static void
R_GunshotEffect_QF (vec4f_t org, int count)
{
	int scale = 16;

	scale += count / 15;
	R_RunSparkEffect_QF (org, count >> 1, scale);
}

static void
R_LightningBloodEffect_QF (vec4f_t org)
{
	if (!r_particles->int_val)
		return;

	R_BloodPuff_QF (org, 50);

	vec4f_t     zero = {};
	particle_new (pt_smokecloud, part_tex_smoke, org, 3.0, zero,
				  9.0, 12 + (mtwist_rand (&mt) & 3),
				  0.25 + qfrandom (0.125), 0.0);

	for (int count = 7; count-- > 0; ) {
		particle_new_random (pt_fallfade, part_tex_spark, org, 12, 2.0, 128,
							 5.0, 244 + (count % 3), 1.0, 0.0);
	}
}

static void
R_RunParticleEffect_QF (vec4f_t org, vec4f_t dir, int color,
						int count)
{
	if (!r_particles->int_val)
		return;

	float       scale = pow (count, 0.23);

	for (int i = 0; i < count; i++) {
		int 	rnd = mtwist_rand (&mt);

		// Note that ParseParticleEffect handles (dir * 15)
		particle_new (pt_grav, part_tex_dot, org + scale * roffs (16), 1.5,
					  dir, 0.1 * (i % 5),
					  (color & ~7) + (rnd & 7), 1.0, 0.0);
	}
}

static void
R_SpikeEffect_QF (vec4f_t org)
{
	R_RunSparkEffect_QF (org, 5, 8);
}

static void
R_SuperSpikeEffect_QF (vec4f_t org)
{
	R_RunSparkEffect_QF (org, 10, 8);
}

static void
R_KnightSpikeEffect_QF (vec4f_t org)
{
	if (!r_particles->int_val)
		return;

	vec4f_t     zero = {};
	particle_new (pt_smokecloud, part_tex_smoke, org, 1.0, zero,
				  9.0, 234, 0.25 + qfrandom (0.125), 0.0);

	for (int count = 10; count-- > 0; ) {
		particle_new_random (pt_fallfade, part_tex_dot, org, 6, 0.7, 96,
							 5.0, 234, 1.0, 0.0);
	}
}

static void
R_WizSpikeEffect_QF (vec4f_t org)
{
	if (!r_particles->int_val)
		return;

	vec4f_t     zero = {};
	particle_new (pt_smokecloud, part_tex_smoke, org, 2.0, zero,
				  9.0, 63, 0.25 + qfrandom (0.125), 0.0);

	for (int count = 15; count-- > 0; ) {
		particle_new_random (pt_fallfade, part_tex_dot, org, 12, 0.7, 96,
							 5.0, 63, 1.0, 0.0);
	}
}

static void
R_LavaSplash_QF (vec4f_t org)
{
	if (!r_particles->int_val)
		return;

	for (int i = -16; i < 16; i++) {
		for (int j = -16; j < 16; j++) {
			uint32_t    rnd = mtwist_rand (&mt);
			float       vel = 50.0 + 0.5 * (mtwist_rand (&mt) & 127);
			vec4f_t     dir = {
				j * 8 + (rnd & 7),
				i * 8 + ((rnd >> 6) & 7),
				256,
				0
			};
			vec4f_t     offs = { dir[0], dir[1], ((rnd >> 9) & 63), 0 };
			dir = normalf (dir);
			particle_new (pt_grav, part_tex_dot, org + offs, 3, vel * dir,
						  2.0 + ((rnd >> 7) & 31) * 0.02,
						  224 + ((rnd >> 12) & 7), 0.75, 0.0);
		}
	}
}

static void
R_TeleportSplash_QF (vec4f_t org)
{
	if (!r_particles->int_val)
		return;

	for (int k = -24; k < 32; k += 4) {
		for (int i = -16; i < 16; i += 4) {
			for (int j = -16; j < 16; j += 4) {
				uint32_t    rnd = mtwist_rand (&mt);
				float       vel = 50 + ((rnd >> 6) & 63);
				vec4f_t     dir = normalf ((vec4f_t) { j, i, k, 0 } * 8);
				vec4f_t     offs = {
					i + (rnd & 3),
					j + ((rnd >> 2) & 3),
					k + ((rnd >> 4) & 3),
					0
				};
				particle_new (pt_grav, part_tex_spark, org + offs, 0.6,
							  vel * dir,
							  (0.2 + (mtwist_rand (&mt) & 15) * 0.01),
							  (7 + ((rnd >> 12) & 7)), 1.0, 0.0);
			}
		}
	}
}

static void
R_RocketTrail_QF (vec4f_t start, vec4f_t end)
{
	if (numparticles >= r_maxparticles)
		return;

	vec4f_t     vec = end - start;
	float       maxlen = magnitudef (vec)[0];
	vec = normalf (vec);

	float       origlen = vr_data.frametime / maxlen;
	vec4f_t     step = (maxlen - 3) * vec;

	float       len = 0;
	vec4f_t     zero = {};
	vec4f_t     pos = start;
	float       pscale = 1.5 + qfrandom (1.5);

	while (len < maxlen) {
		float       pscalenext = 1.5 + qfrandom (1.5);
		float       dist = (pscale + pscalenext) * 3.0;
		float       percent = len * origlen;

		particle_new (pt_smoke, part_tex_smoke, pos,
					  pscale + percent * 4.0, zero,
					  2.0 - percent * 2.0,
					  12 + (mtwist_rand (&mt) & 3),
					  0.5 + qfrandom (0.125) - percent * 0.40, 0.0);
		if (numparticles >= r_maxparticles)
			break;
		len += dist;
		pos += step;
		pscale = pscalenext;
	}
}

static void
R_GrenadeTrail_QF (vec4f_t start, vec4f_t end)
{
	if (numparticles >= r_maxparticles)
		return;

	vec4f_t     vec = end - start;
	float       maxlen = magnitudef (vec)[0];
	vec = normalf (vec);

	float       origlen = vr_data.frametime / maxlen;
	vec4f_t     step = (maxlen - 3) * vec;

	float       len = 0;
	vec4f_t     zero = {};
	vec4f_t     pos = start;
	float       pscale = 6.0 + qfrandom (7.0);

	while (len < maxlen) {
		float       pscalenext = 6.0 + qfrandom (7.0);
		float       dist = (pscale + pscalenext) * 2.0;
		float       percent = len * origlen;

		particle_new (pt_smoke, part_tex_smoke, pos,
					  pscale + percent * 4.0, zero,
					  2.0 - percent * 2.0,
					  1 + (mtwist_rand (&mt) & 3),
					  0.625 + qfrandom (0.125) - percent * 0.40, 0.0);
		if (numparticles >= r_maxparticles)
			break;
		len += dist;
		pos += step;
		pscale = pscalenext;
	}
}

static void
R_BloodTrail_QF (vec4f_t start, vec4f_t end)
{
	if (numparticles >= r_maxparticles)
		return;

	vec4f_t     vec = end - start;
	float       maxlen = magnitudef (vec)[0];
	vec = normalf (vec);

	float       origlen = vr_data.frametime / maxlen;
	vec4f_t     step = (maxlen - 3) * vec;

	float       len = 0;
	vec4f_t     pos = start;
	float       pscale = 5.0 + qfrandom (10.0);

	while (len < maxlen) {
		float       pscalenext = 5.0 + qfrandom (10.0);
		float       dist = (pscale + pscalenext) * 1.5;
		float       percent = len * origlen;
		vec4f_t     vel = roffs (24);
		vel[2] -= percent * 40;

		particle_new (pt_grav, part_tex_smoke, pos + roffs (4), pscale, vel,
					  2.0 - percent * 2.0,
					  68 + (mtwist_rand (&mt) & 3), 1.0, 0.0);
		if (numparticles >= r_maxparticles)
			break;
		len += dist;
		pos += step;
		pscale = pscalenext;
	}
}

static void
R_SlightBloodTrail_QF (vec4f_t start, vec4f_t end)
{
	if (numparticles >= r_maxparticles)
		return;

	vec4f_t     vec = end - start;
	float       maxlen = magnitudef (vec)[0];
	vec = normalf (vec);

	float       origlen = vr_data.frametime / maxlen;
	vec4f_t     step = (maxlen - 3) * vec;

	float       len = 0;
	vec4f_t     pos = start;
	float       pscale = 1.5 + qfrandom (7.5);
	while (len < maxlen) {
		float       pscalenext = 1.5 + qfrandom (7.5);
		float       dist = (pscale + pscalenext) * 1.5;
		float       percent = len * origlen;
		vec4f_t     vel = roffs (12);
		vel[2] -= percent * 40;

		particle_new (pt_grav, part_tex_smoke, pos + roffs (4), pscale, vel,
					  1.5 - percent * 1.5,
					  68 + (mtwist_rand (&mt) & 3), 0.75, 0.0);
		if (numparticles >= r_maxparticles)
			break;
		len += dist;
		pos += step;
		pscale = pscalenext;
	}
}

static void
R_WizTrail_QF (vec4f_t start, vec4f_t end)
{
	float		dist = 3.0;

	if (numparticles >= r_maxparticles)
		return;

	vec4f_t     vec = end - start;
	float       maxlen = magnitudef (vec)[0];
	vec = normalf (vec);

	float       origlen = vr_data.frametime / maxlen;
	vec4f_t     step = (maxlen - dist) * vec;

	float       len = 0;
	vec4f_t     pos = start;
	while (len < maxlen) {
		static int  tracercount;
		float       percent = len * origlen;

		particle_new (pt_flame, part_tex_smoke, pos,
					  2.0 + qfrandom (1.0) - percent * 2.0,
					  30 * tracer_vel (tracercount++, vec),
					  0.5 - percent * 0.5,
					  52 + (mtwist_rand (&mt) & 4), 1.0 - percent * 0.125, 0.0);
		if (numparticles >= r_maxparticles)
			break;
		len += dist;
		pos += step;
	}
}

static void
R_FlameTrail_QF (vec4f_t start, vec4f_t end)
{
	float		dist = 3.0;

	if (numparticles >= r_maxparticles)
		return;

	vec4f_t     vec = end - start;
	float       maxlen = magnitudef (vec)[0];
	vec = normalf (vec);

	float       origlen = vr_data.frametime / maxlen;
	vec4f_t     step = (maxlen - dist) * vec;

	float       len = 0;
	vec4f_t     pos = start;
	while (len < maxlen) {
		static int  tracercount;
		float       percent = len * origlen;

		particle_new (pt_flame, part_tex_smoke, pos,
					  2.0 + qfrandom (1.0) - percent * 2.0,
					  30 * tracer_vel (tracercount++, vec),
					  0.5 - percent * 0.5, 234,
					  1.0 - percent * 0.125, 0.0);
		len += dist;
		pos += step;
	}
}

static void
R_VoorTrail_QF (vec4f_t start, vec4f_t end)
{
	float		dist = 3.0;

	if (numparticles >= r_maxparticles)
		return;

	vec4f_t     vec = end - start;
	float       maxlen = magnitudef (vec)[0];
	vec = normalf (vec);

	float       origlen = vr_data.frametime / maxlen;
	vec4f_t     step = (maxlen - dist) * vec;

	float       len = 0;
	vec4f_t     zero = {};
	vec4f_t     pos = start;
	while (len < maxlen) {
		float       percent = len * origlen;

		particle_new (pt_static, part_tex_dot, pos + roffs (16),
					  1.0 + qfrandom (1.0),
					  zero, 0.3 - percent * 0.3,
					  9 * 16 + 8 + (mtwist_rand (&mt) & 3), 1.0, 0.0);
		len += dist;
		pos += step;
	}
}

static void
R_GlowTrail_QF (vec4f_t start, vec4f_t end, int glow_color)
{
	float		dist = 3.0;

	if (numparticles >= r_maxparticles)
		return;

	vec4f_t     vec = end - start;
	float       maxlen = magnitudef (vec)[0];
	vec = normalf (vec);

	float       origlen = vr_data.frametime / maxlen;
	vec4f_t     step = (maxlen - dist) * vec;

	float       len = 0;
	vec4f_t     zero = {};
	vec4f_t     pos = start;
	while (len < maxlen) {
		float       percent = len * origlen;

		particle_new (pt_smoke, part_tex_dot, pos + roffs (5), 1.0, zero,
					  2.0 - percent * 0.2, glow_color, 1.0, 0.0);
		if (numparticles >= r_maxparticles)
			break;
		len += dist;
		pos += step;
	}
}

static void
R_ParticleExplosion_EE (vec4f_t org)
{
/*
	R_NewExplosion (org);
*/
	if (numparticles >= r_maxparticles)
		return;
	particle_new_random (pt_smokecloud, part_tex_smoke, org, 4, 30, 8,
						 5.0, mtwist_rand (&mt) & 255,
						 0.5 + qfrandom (0.25), 0.0);
}

static void
R_TeleportSplash_EE (vec4f_t org)
{
	if (!r_particles->int_val)
		return;

	for (int k = -24; k < 32; k += 4) {
		for (int i = -16; i < 16; i += 4) {
			for (int j = -16; j < 16; j += 4) {
				uint32_t    rnd = mtwist_rand (&mt);
				float       vel = 50 + ((rnd >> 6) & 63);
				vec4f_t     dir = normalf ((vec4f_t) { j, i, k, 0 } * 8);
				vec4f_t     offs = {
					i + (rnd & 3),
					j + ((rnd >> 2) & 3),
					k + ((rnd >> 4) & 3),
					0
				};
				particle_new (pt_grav, part_tex_spark, org + offs, 0.6,
							  vel * dir,
							  (0.2 + (mtwist_rand (&mt) & 15) * 0.01),
							  qfrandom (1.0), 1.0, 0.0);
			}
		}
	}
}

static void
R_RocketTrail_EE (vec4f_t start, vec4f_t end)
{
	if (numparticles >= r_maxparticles)
		return;

	vec4f_t     vec = end - start;
	float       maxlen = magnitudef (vec)[0];
	vec = normalf (vec);

	float       origlen = vr_data.frametime / maxlen;
	float       pscale = 1.5 + qfrandom (1.5);

	float       len = 0;
	vec4f_t     zero = {};
	vec4f_t     pos = start;
	while (len < maxlen) {
		float       pscalenext = 1.5 + qfrandom (1.5);
		float       dist = (pscale + pscalenext) * 3.0;
		float       percent = len * origlen;

		particle_new (pt_smoke, part_tex_smoke, pos,
					  pscale + percent * 4.0, zero,
					  2.0 - percent * 2.0,
					  mtwist_rand (&mt) & 255,
					  0.5 + qfrandom (0.125) - percent * 0.40, 0.0);
		if (numparticles >= r_maxparticles)
			break;
		len += dist;
		pos += len * vec;
		pscale = pscalenext;
	}
}

static void
R_GrenadeTrail_EE (vec4f_t start, vec4f_t end)
{
	if (!r_particles->int_val)
		return;

	vec4f_t     vec = end - start;
	float       maxlen = magnitudef (vec)[0];
	vec = normalf (vec);

	float       origlen = vr_data.frametime / maxlen;
	float       pscale = 6.0 + qfrandom (7.0);

	float       len = 0;
	vec4f_t     zero = {};
	vec4f_t     pos = start;
	while (len < maxlen) {
		float       pscalenext = 6.0 + qfrandom (7.0);
		float       dist = (pscale + pscalenext) * 2.0;
		float       percent = len * origlen;

		particle_new (pt_smoke, part_tex_smoke, pos,
					  pscale + percent * 4.0, zero,
					  2.0 - percent * 2.0,
					  mtwist_rand (&mt) & 255,
					  0.625 + qfrandom (0.125) - percent * 0.40, 0.0);
		len += dist;
		pos += len * vec;
		pscale = pscalenext;
	}
}

static void
R_ParticleExplosion_ID (vec4f_t org)
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
R_BlobExplosion_ID (vec4f_t org)
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

static inline void		// FIXME: inline?
R_RunParticleEffect_ID (vec4f_t org, vec4f_t dir, int color,
						int count)
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
R_BloodPuffEffect_ID (vec4f_t org, int count)
{
	vec4f_t     zero = {};
	R_RunParticleEffect_ID (org, zero, 73, count);
}

static void
R_GunshotEffect_ID (vec4f_t org, int count)
{
	vec4f_t     zero = {};
	R_RunParticleEffect_ID (org, zero, 0, count);
}

static void
R_LightningBloodEffect_ID (vec4f_t org)
{
	vec4f_t     zero = {};
	R_RunParticleEffect_ID (org, zero, 225, 50);
}

static void
R_SpikeEffect_ID (vec4f_t org)
{
	vec4f_t     zero = {};
	R_RunParticleEffect_ID (org, zero, 0, 10);
}

static void
R_SuperSpikeEffect_ID (vec4f_t org)
{
	vec4f_t     zero = {};
	R_RunParticleEffect_ID (org, zero, 0, 20);
}

static void
R_KnightSpikeEffect_ID (vec4f_t org)
{
	vec4f_t     zero = {};
	R_RunParticleEffect_ID (org, zero, 226, 20);
}

static void
R_WizSpikeEffect_ID (vec4f_t org)
{
	vec4f_t     zero = {};
	R_RunParticleEffect_ID (org, zero, 20, 30);
}

static void
R_LavaSplash_ID (vec4f_t org)
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
R_TeleportSplash_ID (vec4f_t org)
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
R_RocketTrail_ID (vec4f_t start, vec4f_t end)
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
R_GrenadeTrail_ID (vec4f_t start, vec4f_t end)
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
R_BloodTrail_ID (vec4f_t start, vec4f_t end)
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
R_SlightBloodTrail_ID (vec4f_t start, vec4f_t end)
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
R_WizTrail_ID (vec4f_t start, vec4f_t end)
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
R_FlameTrail_ID (vec4f_t start, vec4f_t end)
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
R_VoorTrail_ID (vec4f_t start, vec4f_t end)
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
gl_R_DrawParticles (void)
{
	unsigned char  *at;
	int				vacount;
	float		minparticledist, scale;
	vec3_t			up_scale, right_scale, up_right_scale, down_right_scale;
	varray_t2f_c4ub_v3f_t		*VA;

	if (!r_particles->int_val)
		return;

	qfglBindTexture (GL_TEXTURE_2D, gl_part_tex);
	// LordHavoc: particles should not affect zbuffer
	qfglDepthMask (GL_FALSE);
	qfglInterleavedArrays (GL_T2F_C4UB_V3F, 0, particleVertexArray);

	minparticledist = DotProduct (r_refdef.viewposition, vpn) +
		r_particles_nearclip->value;

	vacount = 0;
	VA = particleVertexArray;

	vec4f_t     gravity = {0, 0, -vr_data.gravity, 0};

	unsigned    j = 0;
	for (unsigned i = 0; i < numparticles; i++) {
		particle_t *p = &particles[i];
		partparm_t *parm = &partparams[i];

		if (p->live <= 0 || p->ramp >= parm->ramp_max
			|| p->alpha <= 0 || p->scale <= 0) {
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

		// Don't render particles too close to us.
		// Note, we must still do physics and such on them.
		if (!(DotProduct (p->pos, vpn) < minparticledist)) {
			at = (byte *) &d_8to24table[(byte) p->icolor];
			VA[0].color[0] = at[0];
			VA[0].color[1] = at[1];
			VA[0].color[2] = at[2];
			VA[0].color[3] = p->alpha * 255;
			memcpy (VA[1].color, VA[0].color, sizeof (VA[0].color));
			memcpy (VA[2].color, VA[0].color, sizeof (VA[0].color));
			memcpy (VA[3].color, VA[0].color, sizeof (VA[0].color));

			switch (p->tex) {
				case part_tex_dot:
					VA[0].texcoord[0] = 0.0;
					VA[0].texcoord[1] = 0.0;
					VA[1].texcoord[0] = 0.5;
					VA[1].texcoord[1] = 0.0;
					VA[2].texcoord[0] = 0.5;
					VA[2].texcoord[1] = 0.5;
					VA[3].texcoord[0] = 0.0;
					VA[3].texcoord[1] = 0.5;
					break;
				case part_tex_spark:
					VA[0].texcoord[0] = 0.5;
					VA[0].texcoord[1] = 0.0;
					VA[1].texcoord[0] = 1.0;
					VA[1].texcoord[1] = 0.0;
					VA[2].texcoord[0] = 1.0;
					VA[2].texcoord[1] = 0.5;
					VA[3].texcoord[0] = 0.5;
					VA[3].texcoord[1] = 0.5;
					break;
				case part_tex_smoke:
					VA[0].texcoord[0] = 0.0;
					VA[0].texcoord[1] = 0.5;
					VA[1].texcoord[0] = 0.5;
					VA[1].texcoord[1] = 0.5;
					VA[2].texcoord[0] = 0.5;
					VA[2].texcoord[1] = 1.0;
					VA[3].texcoord[0] = 0.0;
					VA[3].texcoord[1] = 1.0;
					break;
			}

			scale = p->scale;

			VectorScale (vup, scale, up_scale);
			VectorScale (vright, scale, right_scale);

			VectorAdd (right_scale, up_scale, up_right_scale);
			VectorSubtract (right_scale, up_scale, down_right_scale);

			VectorAdd (p->pos, down_right_scale, VA[0].vertex);
			VectorSubtract (p->pos, up_right_scale, VA[1].vertex);
			VectorSubtract (p->pos, down_right_scale, VA[2].vertex);
			VectorAdd (p->pos, up_right_scale, VA[3].vertex);

			VA += 4;
			vacount += 4;
			if (vacount + 4 > pVAsize) {
				// never reached if partUseVA is false
				qfglDrawElements (GL_QUADS, vacount, GL_UNSIGNED_INT,
								  pVAindices);
				vacount = 0;
				VA = particleVertexArray;
			}
		}

		float       dT = vr_data.frametime;
		p->pos += dT * p->vel;
		p->vel += dT * (p->vel * parm->drag + gravity * parm->drag[3]);
		p->ramp += dT * parm->ramp;
		p->live -= dT;
		p->alpha -= dT * parm->alpha_rate;
		p->scale += dT * parm->scale_rate;
		if (ramp) {
			p->icolor = ramp[(int)p->ramp];
		}
	}
	numparticles = j;

	if (vacount) {
		if (partUseVA) {
			qfglDrawElements (GL_QUADS, vacount, GL_UNSIGNED_INT, pVAindices);
		} else {
			varray_t2f_c4ub_v3f_t *va = particleVertexArray;
			int         i;

			qfglBegin (GL_QUADS);
			for (i = 0; i < vacount; i++, va++) {
				qfglTexCoord2fv (va->texcoord);
				qfglColor4ubv (va->color);
				qfglVertex3fv (va->vertex);
			}
			qfglEnd ();
		}
	}

	qfglColor3ubv (color_white);
	qfglDepthMask (GL_TRUE);
}

static void
gl_R_Particle_New (ptype_t type, int texnum, vec4f_t org, float scale,
				   vec4f_t vel, float live, int color, float alpha,
				   float ramp)
{
	if (numparticles >= r_maxparticles)
		return;
	particle_new (type, texnum, org, scale, vel, live, color, alpha, ramp);
}

static void
gl_R_Particle_NewRandom (ptype_t type, int texnum, vec4f_t org,
						 int org_fuzz, float scale, int vel_fuzz, float live,
						 int color, float alpha, float ramp)
{
	if (numparticles >= r_maxparticles)
		return;
	particle_new_random (type, texnum, org, org_fuzz, scale, vel_fuzz, live,
						 color, alpha, ramp);
}

static vid_particle_funcs_t particles_QF = {
	R_RocketTrail_QF,
	R_GrenadeTrail_QF,
	R_BloodTrail_QF,
	R_SlightBloodTrail_QF,
	R_WizTrail_QF,
	R_FlameTrail_QF,
	R_VoorTrail_QF,
	R_GlowTrail_QF,
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

static vid_particle_funcs_t particles_ID = {
	R_RocketTrail_ID,
	R_GrenadeTrail_ID,
	R_BloodTrail_ID,
	R_SlightBloodTrail_ID,
	R_WizTrail_ID,
	R_FlameTrail_ID,
	R_VoorTrail_ID,
	R_GlowTrail_QF,
	R_RunParticleEffect_ID,
	R_BloodPuffEffect_ID,
	R_GunshotEffect_ID,
	R_LightningBloodEffect_ID,
	R_SpikeEffect_ID,
	R_KnightSpikeEffect_ID,
	R_SuperSpikeEffect_ID,
	R_WizSpikeEffect_ID,
	R_BlobExplosion_ID,
	R_ParticleExplosion_ID,
	R_ParticleExplosion2_QF,
	R_LavaSplash_ID,
	R_TeleportSplash_ID,
	R_DarkFieldParticles_ID,
	R_EntityParticles_ID,
	R_Particle_New,
	R_Particle_NewRandom,
};

static vid_particle_funcs_t particles_QF_egg = {
	R_RocketTrail_EE,
	R_GrenadeTrail_EE,
	R_BloodTrail_QF,
	R_SlightBloodTrail_QF,
	R_WizTrail_QF,
	R_FlameTrail_QF,
	R_VoorTrail_QF,
	R_GlowTrail_QF,
	R_RunParticleEffect_QF,
	R_BloodPuffEffect_QF,
	R_GunshotEffect_QF,
	R_LightningBloodEffect_QF,
	R_SpikeEffect_QF,
	R_KnightSpikeEffect_QF,
	R_SuperSpikeEffect_QF,
	R_WizSpikeEffect_QF,
	R_BlobExplosion_QF,
	R_ParticleExplosion_EE,
	R_ParticleExplosion2_QF,
	R_LavaSplash_QF,
	R_TeleportSplash_EE,
	R_DarkFieldParticles_ID,
	R_EntityParticles_ID,
	R_Particle_New,
	R_Particle_NewRandom,
};

static vid_particle_funcs_t particles_ID_egg = {
	R_RocketTrail_EE,
	R_GrenadeTrail_EE,
	R_BloodTrail_ID,
	R_SlightBloodTrail_ID,
	R_WizTrail_ID,
	R_FlameTrail_ID,
	R_VoorTrail_ID,
	R_GlowTrail_QF,
	R_RunParticleEffect_ID,
	R_BloodPuffEffect_ID,
	R_GunshotEffect_ID,
	R_LightningBloodEffect_ID,
	R_SpikeEffect_ID,
	R_KnightSpikeEffect_ID,
	R_SuperSpikeEffect_ID,
	R_WizSpikeEffect_ID,
	R_BlobExplosion_ID,
	R_ParticleExplosion_EE,
	R_ParticleExplosion2_QF,
	R_LavaSplash_ID,
	R_TeleportSplash_EE,
	R_DarkFieldParticles_ID,
	R_EntityParticles_ID,
	R_Particle_New,
	R_Particle_NewRandom,
};

void
gl_r_easter_eggs_f (cvar_t *var)
{
	if (easter_eggs && !gl_feature_mach64) {
		if (easter_eggs->int_val) {
			if (r_particles_style->int_val) {
				gl_vid_render_funcs.particles = &particles_QF_egg;
			} else {
				gl_vid_render_funcs.particles = &particles_ID_egg;
			}
		} else if (r_particles_style) {
			if (r_particles_style->int_val) {
				gl_vid_render_funcs.particles = &particles_QF;
			} else {
				gl_vid_render_funcs.particles = &particles_ID;
			}
		}
	}
}

void
gl_r_particles_style_f (cvar_t *var)
{
	gl_r_easter_eggs_f (easter_eggs);
}

static void
R_ParticleFunctionInit (void)
{
	gl_r_particles_style_f (r_particles_style);
	gl_r_easter_eggs_f (easter_eggs);
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
gl_R_Particles_Init_Cvars (void)
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
