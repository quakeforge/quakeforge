/*
	cl_particles.c

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

#include "QF/plugin/vid_render.h"	//FIXME
#include "QF/scene/entity.h"

#include "compat.h"

#include "client/particles.h"
#include "client/world.h"

cl_particle_funcs_t *clp_funcs;

static mtstate_t mt;	// private PRNG state
static psystem_t *cl_psystem;

static int easter_eggs;
static cvar_t easter_eggs_cvar = {
	.name = "easter_eggs",
	.description =
		"Enables easter eggs.",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &easter_eggs },
};
static int particles_style;
static cvar_t particles_style_cvar = {
	.name = "particles_style",
	.description =
		"Sets particle style. 0 for Id, 1 for QF.",
	.default_value = "1",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_int, .value = &particles_style },
};

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

static partparm_t __attribute__((pure))
particle_params (ptype_t type)
{
	if (type > pt_flame) {
		Sys_Error ("particle_params: invalid particle type");
	}
	return part_params[type];
}

static const int * __attribute__((pure))
particle_ramp (ptype_t type)
{
	if (type > pt_flame) {
		Sys_Error ("particle_ramp: invalid particle type");
	}
	return part_ramps[type];
}

inline static int
particle_new (ptype_t type, int texnum, vec4f_t pos, float scale,
			  vec4f_t vel, float live, int color, float alpha, float ramp)
{
	if (cl_psystem->numparticles >= cl_psystem->maxparticles) {
		return 0;
	}

	__auto_type ps = cl_psystem;
	particle_t *p = &ps->particles[ps->numparticles];
	partparm_t *parm = &ps->partparams[ps->numparticles];
	const int **rampptr = &ps->partramps[ps->numparticles];
	ps->numparticles += 1;

	p->pos = pos;
	p->vel = vel;
	p->icolor = color;
	p->alpha = alpha;
	p->tex = texnum;
	p->ramp = ramp;
	p->scale = scale;
	p->live = live;

	*parm = particle_params (type);
	*rampptr = particle_ramp (type);
	if (*rampptr) {
		p->icolor = (*rampptr) [(int) p->ramp];
	}
	return 1;
}

/*
	particle_new_random

	note that org_fuzz & vel_fuzz should be ints greater than 0 if you are
	going to bother using this function.
*/
inline static int
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

	return particle_new (type, texnum, porg, scale, pvel, live, color, alpha,
						 ramp);
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
CL_LoadPointFile (const model_t *model)
{
	const char *name;
	char       *mapname;
	int         c;
	QFile      *f;

	mapname = strdup (model->path);
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
		union {
			vec4f_t     org;
			vec3_t      org3;
		} o = { .org = { 0, 0, 0, 1 }};

		Qgets (f, buf, sizeof (buf));
		int         r = sscanf (buf, "%f %f %f\n",
								&o.org3[0], &o.org3[1], &o.org3[2]);
		if (r != 3)
			break;
		c++;

		if (!particle_new (pt_static, part_tex_dot, o.org, 1.5, zero,
						   99999, (-c) & 15, 1.0, 0.0)) {
			Sys_MaskPrintf (SYS_dev, "Not enough free particles\n");
			break;
		}
	}
	Qclose (f);
	Sys_MaskPrintf (SYS_dev, "%i points read\n", c);
}

static void
CL_ParticleExplosion_QF (vec4f_t org)
{
//	CL_NewExplosion (org);
	particle_new_random (pt_smokecloud, part_tex_smoke, org, 4, 30, 8,
						 5.0, (mtwist_rand (&mt) & 7) + 8,
						 0.5 + qfrandom (0.25), 0.0);
}

static void
CL_ParticleExplosion2_QF (vec4f_t org, int colorStart, int colorLength)
{
	unsigned int	i, j = 512;

	for (i = 0; i < j; i++) {
		particle_new_random (pt_blob, part_tex_dot, org, 16, 2, 256,
							 0.3,
							 colorStart + (i % colorLength), 1.0, 0.0);
	}
}

static void
CL_BlobExplosion_QF (vec4f_t org)
{
	unsigned int	i;
	unsigned int	j = 1024;

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
CL_RunSparkEffect_QF (vec4f_t org, int count, int ofuzz)
{
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
CL_BloodPuff_QF (vec4f_t org, int count)
{
	vec4f_t     zero = {};
	particle_new (pt_bloodcloud, part_tex_smoke, org, count / 5, zero,
				  99.0, 70 + (mtwist_rand (&mt) & 3), 0.5, 0.0);
}

static void
CL_BloodPuffEffect_QF (vec4f_t org, int count)
{
	CL_BloodPuff_QF (org, count);
}

static void
CL_GunshotEffect_QF (vec4f_t org, int count)
{
	int scale = 16;

	scale += count / 15;
	CL_RunSparkEffect_QF (org, count >> 1, scale);
}

static void
CL_LightningBloodEffect_QF (vec4f_t org)
{
	CL_BloodPuff_QF (org, 50);

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
CL_RunParticleEffect_QF (vec4f_t org, vec4f_t dir, int color, int count)
{
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
CL_SpikeEffect_QF (vec4f_t org)
{
	CL_RunSparkEffect_QF (org, 5, 8);
}

static void
CL_SuperSpikeEffect_QF (vec4f_t org)
{
	CL_RunSparkEffect_QF (org, 10, 8);
}

static void
CL_KnightSpikeEffect_QF (vec4f_t org)
{
	vec4f_t     zero = {};
	particle_new (pt_smokecloud, part_tex_smoke, org, 1.0, zero,
				  9.0, 234, 0.25 + qfrandom (0.125), 0.0);

	for (int count = 10; count-- > 0; ) {
		particle_new_random (pt_fallfade, part_tex_dot, org, 6, 0.7, 96,
							 5.0, 234, 1.0, 0.0);
	}
}

static void
CL_WizSpikeEffect_QF (vec4f_t org)
{
	vec4f_t     zero = {};
	particle_new (pt_smokecloud, part_tex_smoke, org, 2.0, zero,
				  9.0, 63, 0.25 + qfrandom (0.125), 0.0);

	for (int count = 15; count-- > 0; ) {
		particle_new_random (pt_fallfade, part_tex_dot, org, 12, 0.7, 96,
							 5.0, 63, 1.0, 0.0);
	}
}

static void
CL_LavaSplash_QF (vec4f_t org)
{
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
CL_TeleportSplash_QF (vec4f_t org)
{
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
CL_RocketTrail_QF (vec4f_t start, vec4f_t end)
{
	vec4f_t     vec = end - start;
	float       maxlen = magnitudef (vec)[0];
	vec = normalf (vec);

	float       origlen = cl_frametime / maxlen;
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
		len += dist;
		pos += step;
		pscale = pscalenext;
	}
}

static void
CL_GrenadeTrail_QF (vec4f_t start, vec4f_t end)
{
	vec4f_t     vec = end - start;
	float       maxlen = magnitudef (vec)[0];
	vec = normalf (vec);

	float       origlen = cl_frametime / maxlen;
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
		len += dist;
		pos += step;
		pscale = pscalenext;
	}
}

static void
CL_BloodTrail_QF (vec4f_t start, vec4f_t end)
{
	vec4f_t     vec = end - start;
	float       maxlen = magnitudef (vec)[0];
	vec = normalf (vec);

	float       origlen = cl_frametime / maxlen;
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
		len += dist;
		pos += step;
		pscale = pscalenext;
	}
}

static void
CL_SlightBloodTrail_QF (vec4f_t start, vec4f_t end)
{
	vec4f_t     vec = end - start;
	float       maxlen = magnitudef (vec)[0];
	vec = normalf (vec);

	float       origlen = cl_frametime / maxlen;
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
		len += dist;
		pos += step;
		pscale = pscalenext;
	}
}

static void
CL_WizTrail_QF (vec4f_t start, vec4f_t end)
{
	float		dist = 3.0;

	vec4f_t     vec = end - start;
	float       maxlen = magnitudef (vec)[0];
	vec = normalf (vec);

	float       origlen = cl_frametime / maxlen;
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
		len += dist;
		pos += step;
	}
}

static void
CL_FlameTrail_QF (vec4f_t start, vec4f_t end)
{
	float		dist = 3.0;

	vec4f_t     vec = end - start;
	float       maxlen = magnitudef (vec)[0];
	vec = normalf (vec);

	float       origlen = cl_frametime / maxlen;
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
CL_VoorTrail_QF (vec4f_t start, vec4f_t end)
{
	float		dist = 3.0;

	vec4f_t     vec = end - start;
	float       maxlen = magnitudef (vec)[0];
	vec = normalf (vec);

	float       origlen = cl_frametime / maxlen;
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
CL_GlowTrail_QF (vec4f_t start, vec4f_t end, int glow_color)
{
	float		dist = 3.0;

	vec4f_t     vec = end - start;
	float       maxlen = magnitudef (vec)[0];
	vec = normalf (vec);

	float       origlen = cl_frametime / maxlen;
	vec4f_t     step = (maxlen - dist) * vec;

	float       len = 0;
	vec4f_t     zero = {};
	vec4f_t     pos = start;
	while (len < maxlen) {
		float       percent = len * origlen;

		particle_new (pt_smoke, part_tex_dot, pos + roffs (5), 1.0, zero,
					  2.0 - percent * 0.2, glow_color, 1.0, 0.0);
		len += dist;
		pos += step;
	}
}

static void
CL_ParticleExplosion_EE (vec4f_t org)
{
/*
	CL_NewExplosion (org);
*/
	particle_new_random (pt_smokecloud, part_tex_smoke, org, 4, 30, 8,
						 5.0, mtwist_rand (&mt) & 255,
						 0.5 + qfrandom (0.25), 0.0);
}

static void
CL_TeleportSplash_EE (vec4f_t org)
{
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
CL_RocketTrail_EE (vec4f_t start, vec4f_t end)
{
	vec4f_t     vec = end - start;
	float       maxlen = magnitudef (vec)[0];
	vec = normalf (vec);

	float       origlen = cl_frametime / maxlen;
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
		len += dist;
		pos += len * vec;
		pscale = pscalenext;
	}
}

static void
CL_GrenadeTrail_EE (vec4f_t start, vec4f_t end)
{
	vec4f_t     vec = end - start;
	float       maxlen = magnitudef (vec)[0];
	vec = normalf (vec);

	float       origlen = cl_frametime / maxlen;
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
CL_ParticleExplosion_ID (vec4f_t org)
{
	for (int i = 0; i < 1024; i++) {
		ptype_t     type = i & 1 ? pt_explode2 : pt_explode;
		add_particle (type, org + roffs (32), roffs (512), 5,
					  0, mtwist_rand (&mt) & 3);
	}
}

static void
CL_BlobExplosion_ID (vec4f_t org)
{
	for (int i = 0; i < 1024; i++) {
		ptype_t     type = i & 1 ? pt_blob : pt_blob2;
		int         color = i & 1 ? 66 : 150;
		add_particle (type, org + roffs (32), roffs (512),
					  color + mtwist_rand (&mt) % 6,
					  (color & ~7) + (mtwist_rand (&mt) & 7), 0);
	}
}

static inline void		// FIXME: inline?
CL_RunParticleEffect_ID (vec4f_t org, vec4f_t dir, int color, int count)
{
	for (int i = 0; i < count; i++) {
		add_particle (pt_slowgrav, org + roffs (16),
					  dir/* + roffs (300)*/,
					  0.1 * (mtwist_rand (&mt) % 5),
					  (color & ~7) + (mtwist_rand (&mt) & 7), 0);
	}
}

static void
CL_BloodPuffEffect_ID (vec4f_t org, int count)
{
	vec4f_t     zero = {};
	CL_RunParticleEffect_ID (org, zero, 73, count);
}

static void
CL_GunshotEffect_ID (vec4f_t org, int count)
{
	vec4f_t     zero = {};
	CL_RunParticleEffect_ID (org, zero, 0, count);
}

static void
CL_LightningBloodEffect_ID (vec4f_t org)
{
	vec4f_t     zero = {};
	CL_RunParticleEffect_ID (org, zero, 225, 50);
}

static void
CL_SpikeEffect_ID (vec4f_t org)
{
	vec4f_t     zero = {};
	CL_RunParticleEffect_ID (org, zero, 0, 10);
}

static void
CL_SuperSpikeEffect_ID (vec4f_t org)
{
	vec4f_t     zero = {};
	CL_RunParticleEffect_ID (org, zero, 0, 20);
}

static void
CL_KnightSpikeEffect_ID (vec4f_t org)
{
	vec4f_t     zero = {};
	CL_RunParticleEffect_ID (org, zero, 226, 20);
}

static void
CL_WizSpikeEffect_ID (vec4f_t org)
{
	vec4f_t     zero = {};
	CL_RunParticleEffect_ID (org, zero, 20, 30);
}

static void
CL_LavaSplash_ID (vec4f_t org)
{
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
CL_TeleportSplash_ID (vec4f_t org)
{
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
CL_DarkFieldParticles_ID (vec4f_t org)
{
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

#define num_normals (int)(sizeof (normals) / sizeof (normals[0]))
static vec4f_t  normals[] = {
#include "anorms.h"
};
static vec4f_t  velocities[num_normals];

static void
CL_EntityParticles_ID (vec4f_t org)
{
	float		angle, sp, sy, cp, cy; // cr, sr
	float		beamlength = 16.0, dist = 64.0;

	for (int i = 0; i < num_normals; i++) {
		int         k;
		for (k = 0; k < 3; k++) {
			velocities[i][k] = (mtwist_rand (&mt) & 255) * 0.01;
		}
	}

	vec4f_t     zero = {};
	for (int i = 0; i < num_normals; i++) {
		angle = cl_realtime * velocities[i][0];
		cy = cos (angle);
		sy = sin (angle);
		angle = cl_realtime * velocities[i][1];
		cp = cos (angle);
		sp = sin (angle);
// Next 3 lines results aren't currently used, may be in future. --Despair
//		angle = cl_realtime * avelocities[i][2];
//		sr = sin (angle);
//		cr = cos (angle);

		vec4f_t     forward = { cp * cy, cp * sy, -sp, 0 };
		vec4f_t     pos = org + normals[i] * dist + forward * beamlength;
		//FIXME 0 velocity?
		add_particle (pt_explode, pos, zero, 0.01, 0x6f, 0);
	}
}

static void
CL_RocketTrail_ID (vec4f_t start, vec4f_t end)
{
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
CL_GrenadeTrail_ID (vec4f_t start, vec4f_t end)
{
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
CL_BloodTrail_ID (vec4f_t start, vec4f_t end)
{
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
CL_SlightBloodTrail_ID (vec4f_t start, vec4f_t end)
{
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
CL_WizTrail_ID (vec4f_t start, vec4f_t end)
{
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
CL_FlameTrail_ID (vec4f_t start, vec4f_t end)
{
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
CL_VoorTrail_ID (vec4f_t start, vec4f_t end)
{
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

static void
CL_Particle_New (ptype_t type, int texnum, vec4f_t org, float scale,
				 vec4f_t vel, float live, int color, float alpha,
				 float ramp)
{
	particle_new (type, texnum, org, scale, vel, live, color, alpha, ramp);
}

static void
CL_Particle_NewRandom (ptype_t type, int texnum, vec4f_t org,
					   int org_fuzz, float scale, int vel_fuzz, float live,
					   int color, float alpha, float ramp)
{
	particle_new_random (type, texnum, org, org_fuzz, scale, vel_fuzz, live,
						 color, alpha, ramp);
}

static cl_particle_funcs_t particles_QF = {
	CL_RocketTrail_QF,
	CL_GrenadeTrail_QF,
	CL_BloodTrail_QF,
	CL_SlightBloodTrail_QF,
	CL_WizTrail_QF,
	CL_FlameTrail_QF,
	CL_VoorTrail_QF,
	CL_GlowTrail_QF,
	CL_RunParticleEffect_QF,
	CL_BloodPuffEffect_QF,
	CL_GunshotEffect_QF,
	CL_LightningBloodEffect_QF,
	CL_SpikeEffect_QF,
	CL_KnightSpikeEffect_QF,
	CL_SuperSpikeEffect_QF,
	CL_WizSpikeEffect_QF,
	CL_BlobExplosion_QF,
	CL_ParticleExplosion_QF,
	CL_ParticleExplosion2_QF,
	CL_LavaSplash_QF,
	CL_TeleportSplash_QF,
	CL_DarkFieldParticles_ID,
	CL_EntityParticles_ID,
	CL_Particle_New,
	CL_Particle_NewRandom,
};

static cl_particle_funcs_t particles_ID = {
	CL_RocketTrail_ID,
	CL_GrenadeTrail_ID,
	CL_BloodTrail_ID,
	CL_SlightBloodTrail_ID,
	CL_WizTrail_ID,
	CL_FlameTrail_ID,
	CL_VoorTrail_ID,
	CL_GlowTrail_QF,
	CL_RunParticleEffect_ID,
	CL_BloodPuffEffect_ID,
	CL_GunshotEffect_ID,
	CL_LightningBloodEffect_ID,
	CL_SpikeEffect_ID,
	CL_KnightSpikeEffect_ID,
	CL_SuperSpikeEffect_ID,
	CL_WizSpikeEffect_ID,
	CL_BlobExplosion_ID,
	CL_ParticleExplosion_ID,
	CL_ParticleExplosion2_QF,
	CL_LavaSplash_ID,
	CL_TeleportSplash_ID,
	CL_DarkFieldParticles_ID,
	CL_EntityParticles_ID,
	CL_Particle_New,
	CL_Particle_NewRandom,
};

static cl_particle_funcs_t particles_QF_egg = {
	CL_RocketTrail_EE,
	CL_GrenadeTrail_EE,
	CL_BloodTrail_QF,
	CL_SlightBloodTrail_QF,
	CL_WizTrail_QF,
	CL_FlameTrail_QF,
	CL_VoorTrail_QF,
	CL_GlowTrail_QF,
	CL_RunParticleEffect_QF,
	CL_BloodPuffEffect_QF,
	CL_GunshotEffect_QF,
	CL_LightningBloodEffect_QF,
	CL_SpikeEffect_QF,
	CL_KnightSpikeEffect_QF,
	CL_SuperSpikeEffect_QF,
	CL_WizSpikeEffect_QF,
	CL_BlobExplosion_QF,
	CL_ParticleExplosion_EE,
	CL_ParticleExplosion2_QF,
	CL_LavaSplash_QF,
	CL_TeleportSplash_EE,
	CL_DarkFieldParticles_ID,
	CL_EntityParticles_ID,
	CL_Particle_New,
	CL_Particle_NewRandom,
};

static cl_particle_funcs_t particles_ID_egg = {
	CL_RocketTrail_EE,
	CL_GrenadeTrail_EE,
	CL_BloodTrail_ID,
	CL_SlightBloodTrail_ID,
	CL_WizTrail_ID,
	CL_FlameTrail_ID,
	CL_VoorTrail_ID,
	CL_GlowTrail_QF,
	CL_RunParticleEffect_ID,
	CL_BloodPuffEffect_ID,
	CL_GunshotEffect_ID,
	CL_LightningBloodEffect_ID,
	CL_SpikeEffect_ID,
	CL_KnightSpikeEffect_ID,
	CL_SuperSpikeEffect_ID,
	CL_WizSpikeEffect_ID,
	CL_BlobExplosion_ID,
	CL_ParticleExplosion_EE,
	CL_ParticleExplosion2_QF,
	CL_LavaSplash_ID,
	CL_TeleportSplash_EE,
	CL_DarkFieldParticles_ID,
	CL_EntityParticles_ID,
	CL_Particle_New,
	CL_Particle_NewRandom,
};

static void
set_particle_funcs (void)
{
	if (easter_eggs) {
		if (particles_style) {
			clp_funcs = &particles_QF_egg;
		} else {
			clp_funcs = &particles_ID_egg;
		}
	} else {
		if (particles_style) {
			clp_funcs = &particles_QF;
		} else {
			clp_funcs = &particles_ID;
		}
	}
}

static void
easter_eggs_f (void *data, const cvar_t *cvar)
{
	set_particle_funcs ();
}

static void
particles_style_f (void *data, const cvar_t *cvar)
{
	set_particle_funcs ();
	cl_psystem->points_only = !particles_style;
}

void
CL_Particles_Init (void)
{
	qfZoneScoped (true);
	mtwist_seed (&mt, 0xdeadbeef);
	cl_psystem = r_funcs->ParticleSystem ();
	Cvar_Register (&easter_eggs_cvar, easter_eggs_f, 0);
	Cvar_Register (&particles_style_cvar, particles_style_f, 0);
	set_particle_funcs ();
}

void
CL_ParticlesGravity (float gravity)
{
	cl_psystem->gravity = (vec4f_t) { 0, 0, -gravity, 0 };
}
