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

static __attribute__ ((used)) const char rcsid[] =
	"$Id$";

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include <stdlib.h>

#include "QF/cmd.h"
#include "QF/console.h"
#include "QF/cvar.h"
#include "QF/qargs.h"
#include "QF/quakefs.h"
#include "QF/render.h"
#include "QF/sys.h"
#include "QF/GL/defines.h"
#include "QF/GL/funcs.h"
#include "QF/GL/qf_explosions.h"
#include "QF/GL/qf_textures.h"
#include "QF/GL/qf_vid.h"

#include "compat.h"
#include "r_cvar.h"
#include "r_dynamic.h"
#include "r_local.h"
#include "r_shared.h"
#include "varrays.h"

int		ramp1[8] = { 0x6f, 0x6d, 0x6b, 0x69, 0x67, 0x65, 0x63, 0x61 };
int		ramp2[8] = { 0x6f, 0x6e, 0x6d, 0x6c, 0x6b, 0x6a, 0x68, 0x66 };
int		ramp3[8] = { 0x6d, 0x6b, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01 };

int		part_tex_dot = 0;
int		part_tex_spark = 1;
int		part_tex_smoke = 2;

int						pVAsize;
int					   *pVAindices;
varray_t2f_c4ub_v3f_t  *particleVertexArray;


inline static void
particle_new (ptype_t type, int texnum, const vec3_t org, float scale,
			  const vec3_t vel, float die, int color, float alpha, float ramp)
{
	particle_t *part;

/*
	// Uncomment this for particle debugging!
	if (numparticles >= r_maxparticles) {
		Sys_Error  ("FAILED PARTICLE ALLOC!");
		return NULL;
	}
*/

	part = &particles[numparticles++];

	VectorCopy (org, part->org);
	part->color = color;
	part->tex = texnum;
	part->scale = scale;
	part->alpha = alpha;
	VectorCopy (vel, part->vel);
	part->type = type;
	part->die = die;
	part->ramp = ramp;
}

/*
	particle_new_random

	note that org_fuzz & vel_fuzz should be ints greater than 0 if you are
	going to bother using this function.
*/
inline static void
particle_new_random (ptype_t type, int texnum, const vec3_t org, int org_fuzz,
					 float scale, int vel_fuzz, float die, int color,
					 float alpha, float ramp)
{
	float		o_fuzz = org_fuzz, v_fuzz = vel_fuzz;
	int         rnd;
	vec3_t      porg, pvel;

	rnd = rand ();
	porg[0] = o_fuzz * ((rnd & 63) - 31.5) / 63.0 + org[0];
	porg[1] = o_fuzz * (((rnd >> 5) & 63) - 31.5) / 63.0 + org[1];
	porg[2] = o_fuzz * (((rnd >> 10) & 63) - 31.5) / 63.0 + org[2];
	rnd = rand ();
	pvel[0] = v_fuzz * ((rnd & 63) - 31.5) / 63.0;
	pvel[1] = v_fuzz * (((rnd >> 5) & 63) - 31.5) / 63.0;
	pvel[2] = v_fuzz * (((rnd >> 10) & 63) - 31.5) / 63.0;

	particle_new (type, texnum, porg, scale, pvel, die, color, alpha, ramp);
}

/*
inline static void
particle_new_veryrandom (ptype_t type, int texnum, const vec3_t org,
						 int org_fuzz, float scale, int vel_fuzz, float die,
						 int color, float alpha, float ramp)
{
	vec3_t		porg, pvel;

	porg[0] = qfrandom (org_fuzz * 2) - org_fuzz + org[0];
	porg[1] = qfrandom (org_fuzz * 2) - org_fuzz + org[1];
	porg[2] = qfrandom (org_fuzz * 2) - org_fuzz + org[2];
	pvel[0] = qfrandom (vel_fuzz * 2) - vel_fuzz;
	pvel[1] = qfrandom (vel_fuzz * 2) - vel_fuzz;
	pvel[2] = qfrandom (vel_fuzz * 2) - vel_fuzz;
	particle_new (type, texnum, porg, scale, pvel, die, color, alpha, ramp);
}
*/

VISIBLE void
R_ClearParticles (void)
{
	numparticles = 0;
}

void
R_InitParticles (void)
{
	int		i;

	if (r_maxparticles && r_init) {
		if (vaelements > 3)
			pVAsize = min ((unsigned int) (vaelements - (vaelements % 4)),
						   r_maxparticles * 4);
		else if (vaelements >= 0)
			pVAsize = r_maxparticles * 4;
		else
			pVAsize = 0;
		if (pVAsize) {
			Con_Printf ("Particles: %i maximum vertex elements.\n", pVAsize);

			if (particleVertexArray)
				free (particleVertexArray);
			particleVertexArray = (varray_t2f_c4ub_v3f_t *)
				calloc (pVAsize, sizeof (varray_t2f_c4ub_v3f_t));
			qfglInterleavedArrays (GL_T2F_C4UB_V3F, 0, particleVertexArray);

			if (pVAindices)
				free (pVAindices);
			pVAindices = (int *) calloc (pVAsize, sizeof (int));
			for (i = 0; i < pVAsize; i++)
				pVAindices[i] = i;
		} else {
			Con_Printf ("Particles: Vertex Array use disabled.\n");
		}
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
R_ReadPointFile_f (void)
{
	char        name[MAX_OSPATH], *mapname, *t1;
	int         c, r;
	vec3_t      org;
	QFile      *f;

	mapname = strdup (r_worldentity.model->name);
	if (!mapname)
		Sys_Error ("Can't duplicate mapname!");
	t1 = strrchr (mapname, '.');
	if (!t1)
		Sys_Error ("Can't find .!");
	t1[0] = '\0';

	snprintf (name, sizeof (name), "%s.pts", mapname);
	free (mapname);

	QFS_FOpenFile (name, &f);
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

		if (numparticles >= r_maxparticles) {
			Con_Printf ("Not enough free particles\n");
			break;
		} else {
			particle_new (pt_static, part_tex_dot, org, 1.5, vec3_origin,
						  99999, (-c) & 15, 1.0, 0.0);
		}
	}
	Qclose (f);
	Con_Printf ("%i points read\n", c);
}

static void
R_ParticleExplosion_QF (const vec3_t org)
{
//	R_NewExplosion (org);
	if (numparticles >= r_maxparticles)
		return;
	particle_new_random (pt_smokecloud, part_tex_smoke, org, 4, 30, 8,
						 r_realtime + 5.0, (rand () & 7) + 8,
						 0.5 + qfrandom (0.25), 0.0);
}

static void
R_ParticleExplosion2_QF (const vec3_t org, int colorStart, int colorLength)
{
	unsigned int	i, j = 512;

	if (numparticles >= r_maxparticles)
		return;
	else if (numparticles + j >= r_maxparticles)
		j = r_maxparticles - numparticles;

	for (i = 0; i < j; i++) {
		particle_new_random (pt_blob, part_tex_dot, org, 16, 2, 256, 
							 r_realtime + 0.3,
							 								 colorStart + (i % colorLength), 1.0, 0.0);
	}
}

static void
R_BlobExplosion_QF (const vec3_t org)
{
	unsigned int	i;
	unsigned int	j = 1024;

	if (numparticles >= r_maxparticles)
		return;
	else if (numparticles + j >= r_maxparticles)
		j = r_maxparticles - numparticles;

	for (i = 0; i < j >> 1; i++) {
		particle_new_random (pt_blob, part_tex_dot, org, 12, 2, 256,
							 r_realtime + 1.0 + (rand () & 7) * 0.05,
							 66 + i % 6, 1.0, 0.0);
	}
	for (i = 0; i < j / 2; i++) {
		particle_new_random (pt_blob2, part_tex_dot, org, 12, 2, 256,
							 r_realtime + 1.0 + (rand () & 7) * 0.05,
							 150 + i % 6, 1.0, 0.0);
	}
}

static inline void
R_RunSparkEffect_QF (const vec3_t org, int count, int ofuzz)
{
	if (numparticles >= r_maxparticles)
		return;
	particle_new (pt_smokecloud, part_tex_smoke, org, ofuzz * 0.08,
				  vec3_origin, r_realtime + 9, 12 + (rand () & 3),
				  0.25 + qfrandom (0.125), 0.0);

	if (numparticles + count >= r_maxparticles)
		count = r_maxparticles - numparticles;
	if (count) {
		int orgfuzz = ofuzz * 3 / 4;
		if (orgfuzz < 1)
			orgfuzz = 1;

		while (count--) {
			int		color = rand () & 7;

			particle_new_random (pt_fallfadespark, part_tex_dot, org, orgfuzz,
								 0.7, 96, r_realtime + 5.0, ramp1[color], 1.0,
								 color);
		}
	}
}

static inline void
R_BloodPuff_QF (const vec3_t org, int count)
{
	if (numparticles >= r_maxparticles)
		return;

	particle_new (pt_bloodcloud, part_tex_smoke, org, count / 5, vec3_origin,
				  r_realtime + 99.0, 70 + (rand () & 3), 0.5, 0.0);
}

static void
R_BloodPuffEffect_QF (const vec3_t org, int count)
{
	R_BloodPuff_QF (org, count);
}

static void
R_GunshotEffect_QF (const vec3_t org, int count)
{
	int scale = 16;

	scale += count / 15;
	R_RunSparkEffect_QF (org, count >> 1, scale);
}

static void
R_LightningBloodEffect_QF (const vec3_t org)
{
	int		count = 7;

	R_BloodPuff_QF (org, 50);

	if (numparticles >= r_maxparticles)
		return;
	particle_new (pt_smokecloud, part_tex_smoke, org, 3.0, vec3_origin,
				  r_realtime + 9.0, 12 + (rand () & 3),
				  0.25 + qfrandom (0.125), 0.0);

	if (numparticles + count >= r_maxparticles)
		count = r_maxparticles - numparticles;
	while (count--)
		particle_new_random (pt_fallfade, part_tex_spark, org, 12, 2.0, 128,
							 r_realtime + 5.0, 244 + (count % 3), 1.0, 0.0);
}

static void
R_RunParticleEffect_QF (const vec3_t org, const vec3_t dir, int color,
						int count)
{
	float       scale;
	int         i;
	vec3_t      porg;

	if (numparticles >= r_maxparticles)
		return;

	scale = pow (count, 0.23); // calculate scale before clipping to part. max

	if (numparticles + count >= r_maxparticles)
		count = r_maxparticles - numparticles;

	for (i = 0; i < count; i++) {
		int 	rnd = rand ();

		porg[0] = org[0] + scale * (((rnd >> 3) & 15) - 7.5);
		porg[1] = org[1] + scale * (((rnd >> 7) & 15) - 7.5);
		porg[2] = org[2] + scale * (((rnd >> 11) & 15) - 7.5);
		// Note that ParseParticleEffect handles (dir * 15)
		particle_new (pt_grav, part_tex_dot, porg, 1.5, dir,
					  r_realtime + 0.1 * (i % 5),
					  (color & ~7) + (rnd & 7), 1.0, 0.0);
	}
}

static void
R_SpikeEffect_QF (const vec3_t org)
{
	R_RunSparkEffect_QF (org, 5, 8);
}

static void
R_SuperSpikeEffect_QF (const vec3_t org)
{
	R_RunSparkEffect_QF (org, 10, 8);
}

static void
R_KnightSpikeEffect_QF (const vec3_t org)
{
	unsigned int	count = 10;

	if (numparticles >= r_maxparticles)
		return;
	particle_new (pt_smokecloud, part_tex_smoke, org, 1.0, vec3_origin,
				  r_realtime + 9.0, 234, 0.25 + qfrandom (0.125), 0.0);

	if (numparticles + count >= r_maxparticles)
		count = r_maxparticles - numparticles;
	while (count--)
		particle_new_random (pt_fallfade, part_tex_dot, org, 6, 0.7, 96,
							 r_realtime + 5.0, 234, 1.0, 0.0);
}

static void
R_WizSpikeEffect_QF (const vec3_t org)
{
	unsigned int	count = 15;

	if (numparticles >= r_maxparticles)
		return;
	particle_new (pt_smokecloud, part_tex_smoke, org, 2.0, vec3_origin,
				  r_realtime + 9.0, 63, 0.25 + qfrandom (0.125), 0.0);

	if (numparticles + count >= r_maxparticles)
		count = r_maxparticles - numparticles;
	while (count--)
		particle_new_random (pt_fallfade, part_tex_dot, org, 12, 0.7, 96,
							 r_realtime + 5.0, 63, 1.0, 0.0);
}

static void
R_LavaSplash_QF (const vec3_t org)
{
	float       vel;
	int         rnd, i, j;
	int			k = 256;
	vec3_t      dir, porg, pvel;

	if (numparticles + k >= r_maxparticles) {
		return;
	} // else if (numparticles + k >= r_maxparticles) {
//		k = r_maxparticles - numparticles;
//	}

	dir[2] = 256;
	for (i = -128; i < 128; i += 16) {
		for (j = -128; j < 128; j += 16) {
			rnd = rand ();
			dir[0] = j + (rnd & 7);
			dir[1] = i + ((rnd >> 6) & 7);

			porg[0] = org[0] + dir[0];
			porg[1] = org[1] + dir[1];
			porg[2] = org[2] + ((rnd >> 9) & 63);

			VectorNormalize (dir);
			rnd = rand ();
			vel = 50.0 + 0.5 * (float) (rnd & 127);
			VectorScale (dir, vel, pvel);
			particle_new (pt_grav, part_tex_dot, porg, 3, pvel,
						  r_realtime + 2.0 + ((rnd >> 7) & 31) * 0.02,
						  224 + ((rnd >> 12) & 7), 0.75, 0.0);
		}
	}
}

static void
R_TeleportSplash_QF (const vec3_t org)
{
	float       vel;
	int         rnd, i, j, k;
	int			l = 896;
	vec3_t      dir, pdir, porg, pvel;

	if (numparticles + l >= r_maxparticles) {
		return;
	} // else if (numparticles + l >= r_maxparticles) {
//		l = r_maxparticles - numparticles;
//	}

	for (k = -24; k < 32; k += 4) {
		dir[2] = k * 8;
		for (i = -16; i < 16; i += 4) {
			dir[1] = i * 8;
			for (j = -16; j < 16; j += 4) {
				dir[0] = j * 8;

				VectorCopy (dir, pdir);
				VectorNormalize (pdir);

				rnd = rand ();
				porg[0] = org[0] + i + (rnd & 3);
				porg[1] = org[1] + j + ((rnd >> 2) & 3);
				porg[2] = org[2] + k + ((rnd >> 4) & 3);

				vel = 50 + ((rnd >> 6) & 63);
				VectorScale (pdir, vel, pvel);
				particle_new (pt_grav, part_tex_spark, porg, 0.6, pvel,
							  (r_realtime + 0.2 + (rand () & 15) * 0.01),
							  (7 + ((rnd >> 12) & 7)), 1.0, 0.0);
			}
		}
	}
}

static void
R_RocketTrail_QF (const entity_t *ent)
{
	float		dist, maxlen, origlen, percent, pscale, pscalenext;
	float		len = 0.0;
	vec3_t		old_origin, vec;

	if (numparticles >= r_maxparticles)
		return;

	VectorCopy (ent->old_origin, old_origin);
	VectorSubtract (ent->origin, old_origin, vec);
	maxlen = VectorNormalize (vec);
	origlen = r_frametime / maxlen;
	pscale = 1.5 + qfrandom (1.5);

	while (len < maxlen) {
		pscalenext = 1.5 + qfrandom (1.5);
		dist = (pscale + pscalenext) * 3.0;
		percent = len * origlen;

		particle_new (pt_smoke, part_tex_smoke, old_origin,
					  pscale + percent * 4.0, vec3_origin,
					  r_realtime + 2.0 - percent * 2.0, 
					  12 + (rand () & 3),
					  0.5 + qfrandom (0.125) - percent * 0.40, 0.0);
		if (numparticles >= r_maxparticles)
			break;
		len += dist;
		VectorMultAdd (old_origin, len, vec, old_origin);
		pscale = pscalenext;
	}
}

static void
R_GrenadeTrail_QF (const entity_t *ent)
{
	float		dist, maxlen, origlen, percent, pscale, pscalenext;
	float		len = 0.0;
	vec3_t		old_origin, vec;

	if (numparticles >= r_maxparticles)
		return;

	VectorCopy (ent->old_origin, old_origin);
	VectorSubtract (ent->origin, old_origin, vec);
	maxlen = VectorNormalize (vec);
	origlen = r_frametime / maxlen;
	pscale = 6.0 + qfrandom (7.0);

	while (len < maxlen) {
		pscalenext = 6.0 + qfrandom (7.0);
		dist = (pscale + pscalenext) * 2.0;
		percent = len * origlen;

		particle_new (pt_smoke, part_tex_smoke, old_origin,
					  pscale + percent * 4.0, vec3_origin,
					  r_realtime + 2.0 - percent * 2.0,
					  1 + (rand () & 3),
					  0.625 + qfrandom (0.125) - percent * 0.40, 0.0);
		if (numparticles >= r_maxparticles)
			break;
		len += dist;
		VectorMultAdd (old_origin, len, vec, old_origin);
		pscale = pscalenext;
	}
}

static void
R_BloodTrail_QF (const entity_t *ent)
{
	float		dist, maxlen, origlen, percent, pscale, pscalenext;
	float		len = 0.0;
	int			j;
	vec3_t		old_origin, porg, pvel, vec;

	if (numparticles >= r_maxparticles)
		return;

	VectorCopy (ent->old_origin, old_origin);
	VectorSubtract (ent->origin, old_origin, vec);
	maxlen = VectorNormalize (vec);
	origlen = r_frametime / maxlen;
	pscale = 5.0 + qfrandom (10.0);

	while (len < maxlen) {
		pscalenext = 5.0 + qfrandom (10.0);
		dist = (pscale + pscalenext) * 1.5;

		for (j = 0; j < 3; j++) {
			pvel[j] = qfrandom (24.0) - 12.0;
			porg[j] = old_origin[j] + qfrandom (3.0) - 1.5;
		}

		percent = len * origlen;
		pvel[2] -= percent * 40.0;

		particle_new (pt_grav, part_tex_smoke, porg, pscale, pvel,
					  r_realtime + 2.0 - percent * 2.0, 68 + (rand () & 3),
					  1.0, 0.0);
		if (numparticles >= r_maxparticles)
			break;
		len += dist;
		VectorMultAdd (old_origin, len, vec, old_origin);
		pscale = pscalenext;
	}
}

static void
R_SlightBloodTrail_QF (const entity_t *ent)
{
	float		dist, maxlen, origlen, percent, pscale, pscalenext;
	float		len = 0.0;
	int			j;
	vec3_t      old_origin, porg, pvel, vec;

	if (numparticles >= r_maxparticles)
		return;

	VectorCopy (ent->old_origin, old_origin);
	VectorSubtract (ent->origin, old_origin, vec);
	maxlen = VectorNormalize (vec);
	origlen = r_frametime / maxlen;
	pscale = 1.5 + qfrandom (7.5);

	while (len < maxlen) {
		pscalenext = 1.5 + qfrandom (7.5);
		dist = (pscale + pscalenext) * 1.5;

		for (j = 0; j < 3; j++) {
			pvel[j] = (qfrandom (12.0) - 6.0);
			porg[j] = old_origin[j] + qfrandom (3.0) - 1.5;
		}

		percent = len * origlen;
		pvel[2] -= percent * 40;

		particle_new (pt_grav, part_tex_smoke, porg, pscale, pvel,
					  r_realtime + 1.5 - percent * 1.5, 68 + (rand () & 3),
					  0.75, 0.0);
		if (numparticles >= r_maxparticles)
			break;
		len += dist;
		VectorMultAdd (old_origin, len, vec, old_origin);
		pscale = pscalenext;
	}
}

static void
R_WizTrail_QF (const entity_t *ent)
{
	float		maxlen, origlen, percent;
	float		dist = 3.0, len = 0.0;
	static int	tracercount;
	vec3_t		old_origin, pvel, subtract, vec;

	if (numparticles >= r_maxparticles)
		return;

	VectorCopy (ent->old_origin, old_origin);
	VectorSubtract (ent->origin, old_origin, vec);
	maxlen = VectorNormalize (vec);
	origlen = r_frametime / maxlen;
	VectorScale (vec, maxlen - dist, subtract);

	while (len < maxlen) {
		percent = len * origlen;

		tracercount++;
		if (tracercount & 1) {
			pvel[0] = 30.0 * vec[1];
			pvel[1] = 30.0 * -vec[0];
		} else {
			pvel[0] = 30.0 * -vec[1];
			pvel[1] = 30.0 * vec[0];
		}
		pvel[2] = 0.0;

		particle_new (pt_flame, part_tex_smoke, old_origin,
					  2.0 + qfrandom (1.0) - percent * 2.0, pvel,
					  r_realtime + 0.5 - percent * 0.5, 52 + (rand () & 4),
					  1.0 - percent * 0.125, 0.0);
		if (numparticles >= r_maxparticles)
			break;
		len += dist;
		VectorAdd (old_origin, subtract, old_origin);
	}
}

static void
R_FlameTrail_QF (const entity_t *ent)
{
	float		maxlen, origlen, percent;
	float		dist = 3.0, len = 0.0;
	static int	tracercount;
	vec3_t		old_origin, pvel, subtract, vec;

	if (numparticles >= r_maxparticles)
		return;

	VectorCopy (ent->old_origin, old_origin);
	VectorSubtract (ent->origin, old_origin, vec);
	maxlen = VectorNormalize (vec);
	origlen = r_frametime / maxlen;
	VectorScale (vec, maxlen - dist, subtract);

	while (len < maxlen) {
		percent = len * origlen;

		tracercount++;
		if (tracercount & 1) {
			pvel[0] = 30.0 * vec[1];
			pvel[1] = 30.0 * -vec[0];
		} else {
			pvel[0] = 30.0 * -vec[1];
			pvel[1] = 30.0 * vec[0];
		}
		pvel[2] = 0.0;

		particle_new (pt_flame, part_tex_smoke, old_origin,
					  2.0 + qfrandom (1.0) - percent * 2.0, pvel,
					  r_realtime + 0.5 - percent * 0.5, 234,
					  1.0 - percent * 0.125, 0.0);
		if (numparticles >= r_maxparticles)
			break;
		len += dist;
		VectorAdd (old_origin, subtract, old_origin);
	}
}

static void
R_VoorTrail_QF (const entity_t *ent)
{
	float		maxlen, origlen, percent;
	float		dist = 3.0, len = 0.0;
	int			j;
	vec3_t		subtract, old_origin, porg, vec;

	if (numparticles >= r_maxparticles)
		return;

	VectorCopy (ent->old_origin, old_origin);
	VectorSubtract (ent->origin, old_origin, vec);
	maxlen = VectorNormalize (vec);
	origlen = r_frametime / maxlen;
	VectorScale (vec, maxlen - dist, subtract);

	while (len < maxlen) {
		percent = len * origlen;

		for (j = 0; j < 3; j++)
			porg[j] = old_origin[j] + qfrandom (16.0) - 8.0;

		particle_new (pt_static, part_tex_dot, porg, 1.0 + qfrandom (1.0),
					  vec3_origin, r_realtime + 0.3 - percent * 0.3,
					  9 * 16 + 8 + (rand () & 3), 1.0, 0.0);
		if (numparticles >= r_maxparticles)
			break;
		len += dist;
		VectorAdd (old_origin, subtract, old_origin);
	}
}

static void
R_GlowTrail_QF (const entity_t *ent, int glow_color)
{
	float		maxlen, origlen, percent;
	float		dist = 3.0, len = 0.0;
	int			rnd;
	vec3_t		old_origin, org, subtract, vec;

	if (numparticles >= r_maxparticles)
		return;

	VectorCopy (ent->old_origin, old_origin);
	VectorSubtract (ent->origin, old_origin, vec);
	maxlen = VectorNormalize (vec);
	origlen = r_frametime / maxlen;
	VectorScale (vec, (maxlen - dist), subtract);

	while (len < maxlen) {
		percent = len * origlen;

		rnd = rand ();
		org[0] = old_origin[0] + ((rnd >> 12) & 7) * (5.0/7.0) - 2.5;
		org[1] = old_origin[1] + ((rnd >> 9) & 7) * (5.0/7.0) - 2.5;
		org[2] = old_origin[2] + ((rnd >> 6) & 7) * (5.0/7.0) - 2.5;

		particle_new (pt_smoke, part_tex_dot, org, 1.0, vec3_origin,
					  r_realtime + 2.0 - percent * 0.2, glow_color, 1.0, 0.0);
		if (numparticles >= r_maxparticles)
			break;
		len += dist;
		VectorAdd (old_origin, subtract, old_origin);
	}
}

static void
R_ParticleExplosion_EE (const vec3_t org)
{
/*
	R_NewExplosion (org);
*/
	if (numparticles >= r_maxparticles)
		return;
	particle_new_random (pt_smokecloud, part_tex_smoke, org, 4, 30, 8,
						 r_realtime + 5.0, rand () & 255,
						 0.5 + qfrandom (0.25), 0.0);
}

static void
R_TeleportSplash_EE (const vec3_t org)
{
	float       vel;
	int         rnd, i, j, k;
	int			l = 896;
	vec3_t      dir, porg, pvel;

	if (numparticles + l >= r_maxparticles) {
		return;
	} // else if (numparticles + l >= r_maxparticles) {
//		l = r_maxparticles - numparticles;
//	}

	for (k = -24; k < 32; k += 4) {
		dir[2] = k * 8;
		for (i = -16; i < 16; i += 4) {
			dir[1] = i * 8;
			for (j = -16; j < 16; j += 4) {
				dir[0] = j * 8;
				
				rnd = rand ();
				porg[0] = org[0] + i + (rnd & 3);
				porg[1] = org[1] + j + ((rnd >> 2) & 3);
				porg[2] = org[2] + k + ((rnd >> 4) & 3);

				VectorNormalize (dir);
				vel = 50 + ((rnd >> 6) & 63);
				VectorScale (dir, vel, pvel);
				particle_new (pt_grav, part_tex_spark, porg, 0.6, pvel,
							  (r_realtime + 0.2 + (rand () & 15) * 0.01),
							  qfrandom (1.0), 1.0, 0.0);
			}
		}
	}
}

static void
R_RocketTrail_EE (const entity_t *ent)
{
	float		dist, maxlen, origlen, percent, pscale, pscalenext;
	float		len = 0.0;
	vec3_t		old_origin, subtract, vec;

	if (numparticles >= r_maxparticles)
		return;

	VectorCopy (ent->old_origin, old_origin);
	VectorSubtract (ent->origin, old_origin, vec);
	maxlen = VectorNormalize (vec);
	origlen = r_frametime / maxlen;
	pscale = 1.5 + qfrandom (1.5);

	while (len < maxlen) {
		pscalenext = 1.5 + qfrandom (1.5);
		dist = (pscale + pscalenext) * 3.0;
		percent = len * origlen;

		particle_new (pt_smoke, part_tex_smoke, old_origin,
					  pscale + percent * 4.0, vec3_origin,
					  r_realtime + 2.0 - percent * 2.0, 
					  rand () & 255,
					  0.5 + qfrandom (0.125) - percent * 0.40, 0.0);
		if (numparticles >= r_maxparticles)
			break;
		len += dist;
		VectorScale (vec, len, subtract);
		VectorAdd (old_origin, subtract, old_origin);
		pscale = pscalenext;
	}
}

static void
R_GrenadeTrail_EE (const entity_t *ent)
{
	float		dist, maxlen, origlen, percent, pscale, pscalenext;
	float		len = 0.0;
	vec3_t		old_origin, subtract, vec;

	if (numparticles >= r_maxparticles)
		return;

	VectorCopy (ent->old_origin, old_origin);
	VectorSubtract (ent->origin, ent->old_origin, vec);
	maxlen = VectorNormalize (vec);
	origlen = r_frametime / maxlen;
	pscale = 6.0 + qfrandom (7.0);

	while (len < maxlen) {
		pscalenext = 6.0 + qfrandom (7.0);
		dist = (pscale + pscalenext) * 2.0;
		percent = len * origlen;

		particle_new (pt_smoke, part_tex_smoke, old_origin,
					  pscale + percent * 4.0, vec3_origin,
					  r_realtime + 2.0 - percent * 2.0,
					  rand () & 255,
					  0.625 + qfrandom (0.125) - percent * 0.40, 0.0);
		if (numparticles >= r_maxparticles)
			break;
		len += dist;
		VectorScale (vec, len, subtract);
		VectorAdd (old_origin, subtract, old_origin);
		pscale = pscalenext;
	}
}

static void
R_ParticleExplosion_ID (const vec3_t org)
{
	unsigned int	i;
	unsigned int	j = 1024;

	if (numparticles >= r_maxparticles)
		return;
	else if (numparticles + j >= r_maxparticles)
		j = r_maxparticles - numparticles;

	for (i = 0; i < j >> 1; i++) {
		particle_new_random (pt_explode, part_tex_dot, org, 16, 1.0, 256,
							 r_realtime + 5.0, ramp1[0], 1.0, i & 3);
	}
	for (i = 0; i < j / 2; i++) {
		particle_new_random (pt_explode2, part_tex_dot, org, 16, 1.0, 256,
                             r_realtime + 5.0, ramp1[0], 1.0, i & 3);
	}
}

static void
R_BlobExplosion_ID (const vec3_t org)
{
	unsigned int	i;
	unsigned int	j = 1024;

	if (numparticles >= r_maxparticles)
		return;
	else if (numparticles + j >= r_maxparticles)
		j = r_maxparticles - numparticles;

	for (i = 0; i < j >> 1; i++) {
		particle_new_random (pt_blob, part_tex_dot, org, 12, 1.0, 256,
							 r_realtime + 1.0 + (rand () & 8) * 0.05,
							 66 + i % 6, 1.0, 0.0);
	}
	for (i = 0; i < j / 2; i++) {
		particle_new_random (pt_blob2, part_tex_dot, org, 12, 1.0, 256,
							 r_realtime + 1.0 + (rand () & 8) * 0.05,
							 150 + i % 6, 1.0, 0.0);
	}
}

static inline void		// FIXME: inline?
R_RunParticleEffect_ID (const vec3_t org, const vec3_t dir, int color,
						int count)
{
	float			scale;
	int             i;
	vec3_t			porg;

	if (numparticles >= r_maxparticles)
		return;

	if (count > 130) // calculate scale before clipping to particle max
		scale = 3.0;
	else if (count > 20)
		scale = 2.0;
	else
		scale = 1.0;

	if (numparticles + count >= r_maxparticles)
		count = r_maxparticles - numparticles;

	for (i = 0; i < count; i++) {
		int rnd = rand ();

		porg[0] = org[0] + scale * (((rnd >> 3) & 15) - 8);
		porg[1] = org[1] + scale * (((rnd >> 7) & 15) - 8);
		porg[2] = org[2] + scale * (((rnd >> 11) & 15) - 8);

		// Note that ParseParticleEffect handles (dir * 15)
		particle_new (pt_grav, part_tex_dot, porg, 1.0, dir,
					  r_realtime + 0.1 * (i % 5),
					  (color & ~7) + (rnd & 7), 1.0, 0.0);
	}
}

static void
R_BloodPuffEffect_ID (const vec3_t org, int count)
{
	R_RunParticleEffect_ID (org, vec3_origin, 73, count);
}

static void
R_GunshotEffect_ID (const vec3_t org, int count)
{
	R_RunParticleEffect_ID (org, vec3_origin, 0, count);
}

static void
R_LightningBloodEffect_ID (const vec3_t org)
{
	R_RunParticleEffect_ID (org, vec3_origin, 225, 50);
}

static void
R_SpikeEffect_ID (const vec3_t org)
{
	R_RunParticleEffect_ID (org, vec3_origin, 0, 10);
}

static void
R_SuperSpikeEffect_ID (const vec3_t org)
{
	R_RunParticleEffect_ID (org, vec3_origin, 0, 20);
}

static void
R_KnightSpikeEffect_ID (const vec3_t org)
{
	R_RunParticleEffect_ID (org, vec3_origin, 226, 20);
}

static void
R_WizSpikeEffect_ID (const vec3_t org)
{
	R_RunParticleEffect_ID (org, vec3_origin, 20, 30);
}

static void
R_LavaSplash_ID (const vec3_t org)
{
	float       vel;
	int         rnd, i, j;
	int			k = 256;
	vec3_t      dir, porg, pvel;

	if (numparticles + k >= r_maxparticles) {
		return;
	} // else if (numparticles + k >= r_maxparticles) {
//		k = r_maxparticles - numparticles;
//	}

	dir[2] = 256;
	for (i = -128; i < 128; i += 16) {
		for (j = -128; j < 128; j += 16) {
			rnd = rand ();
			dir[0] = j + (rnd & 7);
			dir[1] = i + ((rnd >> 6) & 7);

			porg[0] = org[0] + dir[0];
			porg[1] = org[1] + dir[1];
			porg[2] = org[2] + ((rnd >> 9) & 63);

			VectorNormalize (dir);
			rnd = rand ();
			vel = 50 + (rnd & 63);
			VectorScale (dir, vel, pvel);
			particle_new (pt_grav, part_tex_dot, porg, 1.0, pvel,
						  r_realtime + 2 + ((rnd >> 7) & 31) * 0.02,
						  224 + ((rnd >> 12) & 7), 1.0, 0.0);
		}
	}
}

static void
R_TeleportSplash_ID (const vec3_t org)
{
	float       vel;
	int         rnd, i, j, k;
	int			l = 896;
	vec3_t      dir, pdir, porg, pvel;

	if (numparticles + l >= r_maxparticles) {
		return;
	} // else if (numparticles + l >= r_maxparticles) {
//		l = r_maxparticles - numparticles;
//	}

	for (k = -24; k < 32; k += 4) {
		dir[2] = k * 8;
		for (i = -16; i < 16; i += 4) {
			dir[1] = i * 8;
			for (j = -16; j < 16; j += 4) {
				dir[0] = j * 8;

				VectorCopy (dir, pdir);
				VectorNormalize (pdir);

				rnd = rand ();
				porg[0] = org[0] + i + (rnd & 3);
				porg[1] = org[1] + j + ((rnd >> 2) & 3);
				porg[2] = org[2] + k + ((rnd >> 4) & 3);

				vel = 50 + ((rnd >> 6) & 63);
				VectorScale (pdir, vel, pvel);
				particle_new (pt_grav, part_tex_dot, porg, 1.0, pvel,
							  (r_realtime + 0.2 + (rand () & 7) * 0.02),
							  (7 + ((rnd >> 12) & 7)), 1.0, 0.0);
			}
		}
	}
}

static void
R_DarkFieldParticles_ID (const entity_t *ent)
{
	int				i, j, k, l = 64;
	unsigned int	rnd;
	float			vel;
	vec3_t			dir, org, porg, pvel;

	if (numparticles + l >= r_maxparticles) {
		return;
	} // else if (numparticles + l >= r_maxparticles) {
//		l = r_maxparticles - numparticles;
//	}

	VectorCopy (ent->origin, org);

	for (i = -16; i < 16; i += 8) {
		dir [1] = i * 8;
		for (j = -16; j < 16; j += 8) {
			dir [0] = j * 8;
			for (k = 0; k < 32; k += 8) {
				dir [2] = k * 8;
				rnd = rand ();

				porg[0] = org[0] + i + ((rnd >> 3) & 3);
				porg[1] = org[1] + j + ((rnd >> 5) & 3);
				porg[2] = org[2] + k + ((rnd >> 7) & 3);

				VectorNormalize (dir);
				vel = 50 + ((rnd >> 9) & 63);
				VectorScale (dir, vel, pvel);
				particle_new (pt_slowgrav, part_tex_dot, porg, 1.5, pvel,
							  (r_realtime + 0.2 + (rnd & 7) * 0.02),
							  (150 + rand () % 6), 1.0, 0.0);
            }
		}
	}
}

static vec3_t		avelocities[NUMVERTEXNORMALS];

static void
R_EntityParticles_ID (const entity_t *ent)
{
	int         i, j = NUMVERTEXNORMALS;
	float       angle, sp, sy, cp, cy; // cr, sr
	float       beamlength = 16.0, dist = 64.0;
	vec3_t      forward, porg;

	if (numparticles + j >= r_maxparticles) {
		return;
	} else if (numparticles + j >= r_maxparticles) {
		j = r_maxparticles - numparticles;
	}

	if (!avelocities[0][0]) {
		for (i = 0; i < NUMVERTEXNORMALS * 3; i++)
			avelocities[0][i] = (rand () & 255) * 0.01;
	}

	for (i = 0; i < j; i++) {
		angle = r_realtime * avelocities[i][0];
		cy = cos (angle);
		sy = sin (angle);
		angle = r_realtime * avelocities[i][1];
		cp = cos (angle);
		sp = sin (angle);
// Next 3 lines results aren't currently used, may be in future. --Despair
//		angle = r_realtime * avelocities[i][2];
//		sr = sin (angle);
//		cr = cos (angle);

		forward[0] = cp * cy;
		forward[1] = cp * sy;
		forward[2] = -sp;

		porg[0] = ent->origin[0] + r_avertexnormals[i][0] * dist +
			forward[0] * beamlength;
		porg[1] = ent->origin[1] + r_avertexnormals[i][1] * dist +
			forward[1] * beamlength;
		porg[2] = ent->origin[2] + r_avertexnormals[i][2] * dist +
			forward[2] * beamlength;
		particle_new (pt_explode, part_tex_dot, porg, 1.0, vec3_origin,
					  r_realtime + 0.01, 0x6f, 1.0, 0);
	}
}

static void
R_RocketTrail_ID (const entity_t *ent)
{
	float		maxlen;
	float		dist = 3.0, len = 0.0;
	int			ramp, rnd;
	vec3_t		old_origin, org, subtract, vec;

	if (numparticles >= r_maxparticles)
		return;

	VectorCopy (ent->old_origin, old_origin);
	VectorSubtract (ent->origin, ent->old_origin, vec);
	maxlen = VectorNormalize (vec);
	VectorScale (vec, (maxlen - dist), subtract);

	while (len < maxlen) {
		rnd = rand ();
		org[0] = old_origin[0] + ((rnd >> 12) & 7) * (5.0/7.0) - 2.5;
		org[1] = old_origin[1] + ((rnd >> 9) & 7) * (5.0/7.0) - 2.5;
		org[2] = old_origin[2] + ((rnd >> 6) & 7) * (5.0/7.0) - 2.5;
		ramp = rnd & 3;

		particle_new (pt_fire, part_tex_dot, org, 1.0, vec3_origin,
					  r_realtime + 2.0, ramp3[ramp], 1.0, ramp);
		if (numparticles >= r_maxparticles)
			break;
		len += dist;
		VectorAdd (old_origin, subtract, old_origin);
	}
}

static void
R_GrenadeTrail_ID (const entity_t *ent)
{
	float			maxlen;
	float			dist = 3.0, len = 0.0;
	unsigned int	ramp, rnd;
	vec3_t			old_origin, org, subtract, vec;

	if (numparticles >= r_maxparticles)
		return;

	VectorCopy (ent->old_origin, old_origin);
	VectorSubtract (ent->origin, ent->old_origin, vec);
	maxlen = VectorNormalize (vec);
	VectorScale (vec, maxlen - dist, subtract);

	while (len < maxlen) {
		rnd = rand ();
		org[0] = old_origin[0] + ((rnd >> 12) & 7) * (5.0/7.0) - 2.5;
		org[1] = old_origin[1] + ((rnd >> 9) & 7) * (5.0/7.0) - 2.5;
		org[2] = old_origin[2] + ((rnd >> 6) & 7) * (5.0/7.0) - 2.5;
		ramp = (rnd & 3) + 2;

		particle_new (pt_fire, part_tex_dot, org, 1.0, vec3_origin,
					  r_realtime + 2.0, ramp3[ramp], 1.0, ramp);
		if (numparticles >= r_maxparticles)
			break;
		len += dist;
		VectorAdd (old_origin, subtract, old_origin);
	}
}

static void
R_BloodTrail_ID (const entity_t *ent)
{
	float			maxlen;
	float			dist = 3.0, len = 0.0;
	unsigned int	rnd;
	vec3_t			old_origin, subtract, vec, porg;

	if (numparticles >= r_maxparticles)
		return;

	VectorCopy (ent->old_origin, old_origin);
	VectorSubtract (ent->origin, old_origin, vec);
	maxlen = VectorNormalize (vec);
	VectorScale (vec, maxlen - dist, subtract);

	while (len < maxlen) {
		rnd = rand ();
		porg[0] = old_origin[0] + ((rnd >> 12) & 7) * (5.0/7.0) - 2.5;
		porg[1] = old_origin[1] + ((rnd >> 9) & 7) * (5.0/7.0) - 2.5;
		porg[2] = old_origin[2] + ((rnd >> 6) & 7) * (5.0/7.0) - 2.5;

		particle_new (pt_grav, part_tex_dot, porg, 1.0, vec3_origin,
					  r_realtime + 2.0, 67 + (rnd & 3), 1.0, 0.0);
		if (numparticles >= r_maxparticles)
			break;
		len += dist;
		VectorAdd (old_origin, subtract, old_origin);
	}
}

static void
R_SlightBloodTrail_ID (const entity_t *ent)
{
	float			maxlen;
	float			dist = 6.0, len = 0.0;
	unsigned int	rnd;
	vec3_t			old_origin, porg, subtract, vec;

	if (numparticles >= r_maxparticles)
		return;

	VectorCopy (ent->old_origin, old_origin);
	VectorSubtract (ent->origin, old_origin, vec);
	maxlen = VectorNormalize (vec);
	VectorScale (vec, maxlen - dist, subtract);

	while (len < maxlen) {
		rnd = rand ();
		porg[0] = old_origin[0] + ((rnd >> 12) & 7) * (5.0/7.0) - 2.5;
		porg[1] = old_origin[1] + ((rnd >> 9) & 7) * (5.0/7.0) - 2.5;
		porg[2] = old_origin[2] + ((rnd >> 6) & 7) * (5.0/7.0) - 2.5;

		particle_new (pt_grav, part_tex_dot, porg, 1.0, vec3_origin,
					  r_realtime + 1.5, 67 + (rnd & 3), 1.0, 0.0);
		if (numparticles >= r_maxparticles)
			break;
		len += dist;
		VectorAdd (old_origin, subtract, old_origin);
	}
}

static void
R_WizTrail_ID (const entity_t *ent)
{
	float		maxlen;
	float		dist = 3.0, len = 0.0;
	static int	tracercount;
	vec3_t		old_origin, pvel, subtract, vec;

	if (numparticles >= r_maxparticles)
		return;

	VectorCopy (ent->old_origin, old_origin);
	VectorSubtract (ent->origin, old_origin, vec);
	maxlen = VectorNormalize (vec);
	VectorScale (vec, maxlen - dist, subtract);

	while (len < maxlen) {
		tracercount++;
		if (tracercount & 1) {
			pvel[0] = 30.0 * vec[1];
			pvel[1] = 30.0 * -vec[0];
		} else {
			pvel[0] = 30.0 * -vec[1];
			pvel[1] = 30.0 * vec[0];
		}
		pvel[2] = 0.0;

		particle_new (pt_static, part_tex_dot, old_origin, 1.0, pvel,
					  r_realtime + 0.5, 52 + ((tracercount & 4) << 1), 1.0,
					  0.0);
		if (numparticles >= r_maxparticles)
			break;
		len += dist;
		VectorAdd (old_origin, subtract, old_origin);
	}
}

static void
R_FlameTrail_ID (const entity_t *ent)
{
	float		maxlen;
	float		dist = 3.0, len = 0.0;
	static int	tracercount;
	vec3_t		old_origin, pvel, subtract, vec;

	if (numparticles >= r_maxparticles)
		return;

	VectorCopy (ent->old_origin, old_origin);
	VectorSubtract (ent->origin, old_origin, vec);
	maxlen = VectorNormalize (vec);
	VectorScale (vec, maxlen - dist, subtract);

	while (len < maxlen) {
		tracercount++;
		if (tracercount & 1) {
			pvel[0] = 30.0 * vec[1];
			pvel[1] = 30.0 * -vec[0];
		} else {
			pvel[0] = 30.0 * -vec[1];
			pvel[1] = 30.0 * vec[0];
		}
		pvel[2] = 0.0;

		particle_new (pt_static, part_tex_dot, old_origin, 1.0, pvel,
					  r_realtime + 0.5, 230 + ((tracercount & 4) << 1), 1.0,
					  0.0);
		if (numparticles >= r_maxparticles)
			break;
		len += dist;
		VectorAdd (old_origin, subtract, old_origin);
	}
}

static void
R_VoorTrail_ID (const entity_t *ent)
{
	float			maxlen;
	float			dist = 3.0, len = 0.0;
	unsigned int	rnd;
	vec3_t			old_origin, porg, subtract, vec;

	if (numparticles >= r_maxparticles)
		return;

	VectorCopy (ent->old_origin, old_origin);
	VectorSubtract (ent->origin, old_origin, vec);
	maxlen = VectorNormalize (vec);
	VectorScale (vec, maxlen - dist, subtract);

	while (len < maxlen) {
		rnd = rand ();
		porg[0] = old_origin[0] + ((rnd >> 3) & 15) - 7.5;
		porg[1] = old_origin[1] + ((rnd >> 7) & 15) - 7.5;
		porg[2] = old_origin[2] + ((rnd >> 11) & 15) - 7.5;

		particle_new (pt_static, part_tex_dot, porg, 1.0, vec3_origin,
					  r_realtime + 0.3, 9 * 16 + 8 + (rnd & 3), 1.0, 0.0);
		if (numparticles >= r_maxparticles)
			break;
		len += dist;
		VectorAdd (old_origin, subtract, old_origin);
	}
}

static void
R_ParticlePhysics (particle_t *part)
{
	float			grav, fast_grav,
					time_125, time_25, time_40, time_55, time2, time4, time5,
					time10, time15, time30, time50;
	grav = (fast_grav = r_frametime * r_gravity) * 0.05;
	time_125 = r_frametime * 0.125;
	time_25= r_frametime * 0.25;
	time_40 = r_frametime * 0.40;
	time_55 = r_frametime * 0.55;
	time2 = r_frametime * 2.0;
	time4 = r_frametime * 4.0;
	time5 = r_frametime * 5.0;
	time10 = r_frametime * 10.0;
	time15 = r_frametime * 15.0;
	time30 = r_frametime * 30.0;
	time50 = r_frametime * 50.0;

	VectorMultAdd (part->org, r_frametime, part->vel, part->org);
	switch (part->type) {
		case pt_static:
			break;
		case pt_grav:
			part->vel[2] -= grav;
			break;
		case pt_fire:
			part->ramp += time5;
			if (part->ramp >= 6) {
				part->die = -1;
				break;
			}
			part->color = ramp3[(int) part->ramp];
			part->alpha = (6.0 - part->ramp) / 6.0;
			part->vel[2] += grav;
			break;
		case pt_explode:
			part->ramp += time10;
			if (part->ramp >= 8) {
				part->die = -1;
				break;
			}
			part->color = ramp1[(int) part->ramp];
			VectorMultAdd (part->vel, time4, part->vel, part->vel);
			part->vel[2] -= grav;
			break;
		case pt_explode2:
			part->ramp += time15;
			if (part->ramp >= 8) {
				part->die = -1;
				break;
			}
			part->color = ramp2[(int) part->ramp];
			VectorMultSub (part->vel, r_frametime, part->vel, part->vel);
			part->vel[2] -= grav;
			break;
		case pt_blob:
			VectorMultAdd (part->vel, time4, part->vel, part->vel);
			part->vel[2] -= grav;
			break;
		case pt_blob2:
			part->vel[0] -= part->vel[0] * time4;
			part->vel[1] -= part->vel[1] * time4;
			part->vel[2] -= grav;
			break;
		case pt_smoke:
			if ((part->alpha -= time_40) <= 0.0)
				part->die = -1;
			part->scale += time4;
//			part->org[2] += time30;
			break;
		case pt_smokecloud:
			if ((part->alpha -= time_55) <= 0.0) {
				part->die = -1;
				break;
			}
			part->scale += time50;
			part->org[2] += time30;
			break;
		case pt_bloodcloud:
			if ((part->alpha -= time_25) <= 0.0) {
				part->die = -1;
				break;
			}
			part->scale += time4;
			part->vel[2] -= grav;
			break;
		case pt_fallfade:
			if ((part->alpha -= r_frametime) <= 0.0)
				part->die = -1;
			part->vel[2] -= fast_grav;
			break;
		case pt_fallfadespark:
			part->ramp += time15;
			if (part->ramp >= 8) {
				part->die = -1;
				break;
			}
			part->color = ramp1[(int) part->ramp];
			if ((part->alpha -= r_frametime) <= 0.0)
				part->die = -1;
			part->vel[2] -= fast_grav;
			break;
		case pt_flame:
			if ((part->alpha -= time_125) <= 0.0)
				part->die = -1;
			part->scale -= time2;
			break;
		default:
			Con_DPrintf ("unhandled particle type %d\n", part->type);
			break;
	}
}

void
R_DrawParticles (void)
{
	unsigned char  *at;
	int				activeparticles, maxparticle, j, vacount;
	unsigned int	k;
	float		minparticledist, scale;
	particle_t	   *part;
	vec3_t			up_scale, right_scale, up_right_scale, down_right_scale;
	varray_t2f_c4ub_v3f_t		*VA;

	if (!r_particles->int_val)
		return;

	qfglBindTexture (GL_TEXTURE_2D, part_tex);
	// LordHavoc: particles should not affect zbuffer
	qfglDepthMask (GL_FALSE);
	qfglInterleavedArrays (GL_T2F_C4UB_V3F, 0, particleVertexArray);

	minparticledist = DotProduct (r_refdef.vieworg, vpn) +
		r_particles_nearclip->value;

	activeparticles = 0;
	vacount = 0;
	VA = particleVertexArray;
	maxparticle = -1;
	j = 0;

	for (k = 0, part = particles; k < numparticles; k++, part++) {
		// Don't render particles too close to us.
		// Note, we must still do physics and such on them.
		if (!(DotProduct (part->org, vpn) < minparticledist)) {
			at = (byte *) &d_8to24table[(byte) part->color];
			VA[0].color[0] = at[0];
			VA[0].color[1] = at[1];
			VA[0].color[2] = at[2];
			VA[0].color[3] = part->alpha * 255;
			memcpy (VA[1].color, VA[0].color, sizeof (VA[0].color));
			memcpy (VA[2].color, VA[0].color, sizeof (VA[0].color));
			memcpy (VA[3].color, VA[0].color, sizeof (VA[0].color));

			switch (part->tex) {
				case 0:
					VA[0].texcoord[0] = 0.0;
					VA[0].texcoord[1] = 0.0;
					VA[1].texcoord[0] = 0.5;
					VA[1].texcoord[1] = 0.0;
					VA[2].texcoord[0] = 0.5;
					VA[2].texcoord[1] = 0.5;
					VA[3].texcoord[0] = 0.0;
					VA[3].texcoord[1] = 0.5;
					break;
				case 1:
					VA[0].texcoord[0] = 0.5;
					VA[0].texcoord[1] = 0.0;
					VA[1].texcoord[0] = 1.0;
					VA[1].texcoord[1] = 0.0;
					VA[2].texcoord[0] = 1.0;
					VA[2].texcoord[1] = 0.5;
					VA[3].texcoord[0] = 0.5;
					VA[3].texcoord[1] = 0.5;
					break;
				case 2:
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

			scale = part->scale;

			VectorScale (vup, scale, up_scale);
			VectorScale (vright, scale, right_scale);

			VectorAdd (right_scale, up_scale, up_right_scale);
			VectorSubtract (right_scale, up_scale, down_right_scale);

			VectorAdd (part->org, down_right_scale, VA[0].vertex);
			VectorSubtract (part->org, up_right_scale, VA[1].vertex);
			VectorSubtract (part->org, down_right_scale, VA[2].vertex);
			VectorAdd (part->org, up_right_scale, VA[3].vertex);

			VA += 4;
			vacount += 4;
			if (vacount + 4 > pVAsize) {
				qfglDrawElements (GL_QUADS, vacount, GL_UNSIGNED_INT,
								  pVAindices);
				vacount = 0;
				VA = particleVertexArray;
			}
		}

		R_ParticlePhysics (part);

		// LordHavoc: immediate removal of unnecessary particles (must be done
		// to ensure compactor below operates properly in all cases)
		if (part->die < r_realtime) {
			freeparticles[j++] = part;
		} else {
			maxparticle = k;
			activeparticles++;
		}
	}
	if (vacount)
		qfglDrawElements (GL_QUADS, vacount, GL_UNSIGNED_INT, pVAindices);

	k = 0;
	while (maxparticle >= activeparticles) {
		*freeparticles[k++] = particles[maxparticle--];
		while (maxparticle >= activeparticles &&
			   particles[maxparticle].die <= r_realtime)
			maxparticle--;
	}
	numparticles = activeparticles;

	qfglColor3ubv (color_white);
	qfglDepthMask (GL_TRUE);
}

void
r_easter_eggs_f (cvar_t *var)
{
	if (easter_eggs && !gl_feature_mach64) {
		if (easter_eggs->int_val) {
			R_ParticleExplosion = R_ParticleExplosion_EE;
			R_TeleportSplash = R_TeleportSplash_EE;
			R_RocketTrail = R_RocketTrail_EE;
			R_GrenadeTrail = R_GrenadeTrail_EE;
		} else if (r_particles_style) {
			if (r_particles_style->int_val) {
				R_ParticleExplosion = R_ParticleExplosion_QF;
				R_TeleportSplash = R_TeleportSplash_QF;
				R_RocketTrail = R_RocketTrail_QF;
				R_GrenadeTrail = R_GrenadeTrail_QF;
			} else {
				R_ParticleExplosion = R_ParticleExplosion_ID;
				R_TeleportSplash = R_TeleportSplash_ID;
				R_RocketTrail = R_RocketTrail_ID;
				R_GrenadeTrail = R_GrenadeTrail_ID;
			}
		}
	}
}

void
r_particles_style_f (cvar_t *var)
{
	if (r_particles_style) {
		if (r_particles_style->int_val && !gl_feature_mach64) {
			R_BlobExplosion = R_BlobExplosion_QF;
			R_LavaSplash = R_LavaSplash_QF;

			R_BloodPuffEffect = R_BloodPuffEffect_QF;
			R_GunshotEffect = R_GunshotEffect_QF;
			R_LightningBloodEffect = R_LightningBloodEffect_QF;

			R_RunParticleEffect = R_RunParticleEffect_QF;
			R_SpikeEffect = R_SpikeEffect_QF;
			R_SuperSpikeEffect = R_SuperSpikeEffect_QF;
			R_KnightSpikeEffect = R_KnightSpikeEffect_QF;
			R_WizSpikeEffect = R_WizSpikeEffect_QF;

			R_BloodTrail = R_BloodTrail_QF;
			R_SlightBloodTrail = R_SlightBloodTrail_QF;
			R_WizTrail = R_WizTrail_QF;
			R_FlameTrail = R_FlameTrail_QF;
			R_VoorTrail = R_VoorTrail_QF;
		} else {
			R_BlobExplosion = R_BlobExplosion_ID;
			R_LavaSplash = R_LavaSplash_ID;

			R_BloodPuffEffect = R_BloodPuffEffect_ID;
			R_GunshotEffect = R_GunshotEffect_ID;
			R_LightningBloodEffect = R_LightningBloodEffect_ID;

			R_RunParticleEffect = R_RunParticleEffect_ID;
			R_SpikeEffect = R_SpikeEffect_ID;
			R_SuperSpikeEffect = R_SuperSpikeEffect_ID;
			R_KnightSpikeEffect = R_KnightSpikeEffect_ID;
			R_WizSpikeEffect = R_WizSpikeEffect_ID;

			R_BloodTrail = R_BloodTrail_ID;
			R_SlightBloodTrail = R_SlightBloodTrail_ID;
			R_WizTrail = R_WizTrail_ID;
			R_FlameTrail = R_FlameTrail_ID;
			R_VoorTrail = R_VoorTrail_ID;
		}
	}
	R_DarkFieldParticles = R_DarkFieldParticles_ID;
	R_EntityParticles = R_EntityParticles_ID;
	R_ParticleExplosion2 = R_ParticleExplosion2_QF;
	R_GlowTrail = R_GlowTrail_QF;
	// Handle R_GrenadeTrail, R_RocketTrail, R_ParticleExplosion,
	// R_TeleportSplash
	r_easter_eggs_f (easter_eggs);
}

static void
R_ParticleFunctionInit (void)
{
	r_particles_style_f (r_particles_style);
	r_easter_eggs_f (easter_eggs);
}

VISIBLE void
R_Particles_Init_Cvars (void)
{
	R_ParticleFunctionInit ();
}
