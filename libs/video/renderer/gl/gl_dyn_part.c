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
static const char rcsid[] = 
	"$Id$";

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
#include "QF/console.h"
#include "QF/cvar.h"
#include "QF/qargs.h"
#include "QF/render.h"
#include "QF/sys.h"
#include "QF/vfs.h"
#include "QF/GL/defines.h"
#include "QF/GL/funcs.h"
#include "QF/GL/qf_explosions.h"
#include "QF/GL/qf_textures.h"

#include "compat.h"
#include "r_cvar.h"
#include "r_dynamic.h"
#include "r_shared.h"
#include "varrays.h"

int			ramp[8] = { 0x6f, 0x6d, 0x6b, 0x69, 0x67, 0x65, 0x63, 0x61 };

int part_tex_dot = 0;
int part_tex_spark = 1;
int part_tex_smoke = 2;

int						pVAsize;
int					   *pVAindices;
varray_t2f_c4ub_v3f_t  *particleVertexArray;


inline static void
particle_new (ptype_t type, int texnum, const vec3_t org, float scale,
			  const vec3_t vel, float die, byte color, byte alpha)
{
	particle_t *part;

/*
	if (numparticles >= r_maxparticles) {
		Sys_Error  ("FAILED PARTICLE ALLOC!\n");
		return NULL;
	}
*/

	part = &particles[numparticles++];

	part->type = type;
	VectorCopy (org, part->org);
	VectorCopy (vel, part->vel);
	part->die = die;
	part->color = color;
	part->alpha = alpha;
	part->tex = texnum;
	part->scale = scale;
}

inline static void
particle_new_random (ptype_t type, int texnum, const vec3_t org, int org_fuzz,
					 float scale, int vel_fuzz, float die, byte color,
					 byte alpha)
{
	int         j;
	vec3_t      porg, pvel;

	for (j = 0; j < 3; j++) {
		if (org_fuzz)
			porg[j] = qfrandom (org_fuzz * 2) - org_fuzz + org[j];
		if (vel_fuzz)
			pvel[j] = qfrandom (vel_fuzz * 2) - vel_fuzz;
	}
	particle_new (type, texnum, porg, scale, pvel, die, color, alpha);
}

inline void
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
			pVAsize = min (vaelements - (vaelements % 4), r_maxparticles * 4);
		else
			pVAsize = r_maxparticles * 4;
		Con_Printf ("%i maximum vertex elements.\n", pVAsize);

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
	VFile      *f;

	mapname = strdup (r_worldentity.model->name);
	if (!mapname)
		Sys_Error ("Can't duplicate mapname!");
	t1 = strrchr (mapname, '.');
	if (!t1)
		Sys_Error ("Can't find .!");
	t1[0] = '\0';

	snprintf (name, sizeof (name), "%s.pts", mapname);
	free (mapname);

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

		if (numparticles >= r_maxparticles) {
			Con_Printf ("Not enough free particles\n");
			break;
		} else {
			particle_new (pt_static, part_tex_dot, org, 1.5, vec3_origin,
						  99999, (-c) & 15, 255);
		}
	}
	Qclose (f);
	Con_Printf ("%i points read\n", c);
}

void
R_ParticleExplosion_QF (const vec3_t org)
{
/*
	R_NewExplosion (org);
*/
	if (numparticles >= r_maxparticles)
		return;
	particle_new_random (pt_smokecloud, part_tex_smoke, org, 4, 30, 8,
						 r_realtime + 5.0, (rand () & 7) + 8,
						 128 + (rand () & 63));
}

void
R_ParticleExplosion2_QF (const vec3_t org, int colorStart, int colorLength)
{
	int         i;
	int         colorMod = 0, j = 512;

	if (numparticles >= r_maxparticles)
		return;
	else if (numparticles + j >= r_maxparticles)
		j = r_maxparticles - numparticles;

	for (i = 0; i < j; i++) {
		particle_new_random (pt_blob, part_tex_dot, org, 16, 2, 256, 
							 r_realtime + 0.3,
							 colorStart + (colorMod % colorLength), 255);
		colorMod++;
	}
}

void
R_BlobExplosion_QF (const vec3_t org)
{
	int         i;
	int			j = 1024;

	if (numparticles >= r_maxparticles)
		return;
	else if (numparticles + j >= r_maxparticles)
		j = r_maxparticles - numparticles;

	for (i = 0; i < j / 2; i++) {
		particle_new_random (pt_blob, part_tex_dot, org, 12, 2, 256,
							 r_realtime + 1.0 + (rand () & 7) * 0.05,
							 66 + rand () % 6, 255);
	}
	for (i = 0; i < j / 2; i++) {
		particle_new_random (pt_blob2, part_tex_dot, org, 12, 2, 256,
							 r_realtime + 1.0 + (rand () & 7) * 0.05,
							 150 + rand () % 6, 255);
	}
}

static inline void
R_RunSparkEffect_QF (const vec3_t org, int count, int ofuzz)
{
	if (numparticles >= r_maxparticles)
		return;
	particle_new (pt_smokecloud, part_tex_smoke, org,
				  ofuzz * 0.08, vec3_origin, r_realtime + 9,
				  12 + (rand () & 3), 64 + (rand () & 31));

	if (numparticles + count >= r_maxparticles)
		count = r_maxparticles - numparticles;
	while (count--)
		particle_new_random (pt_fallfadespark, part_tex_dot, org,
							 ofuzz * 0.75, 0.7, 96, r_realtime + 5,
							 ramp[rand () & 7], 255);
}

static inline void
R_BloodPuff_QF (const vec3_t org, int count)
{
	if (numparticles >= r_maxparticles)
		return;

	particle_new (pt_bloodcloud, part_tex_smoke, org, count / 5,
				  vec3_origin, r_realtime + 99, 70 + (rand () & 3), 128);
}

void
R_BloodPuffEffect_QF (const vec3_t org, int count)
{
	R_BloodPuff_QF (org, count);
}

void
R_GunshotEffect_QF (const vec3_t org, int count)
{
	int scale = 16;

	if (count > 120)
		scale = 24;
	R_RunSparkEffect_QF (org, count >> 1, scale);
}

void
R_LightningBloodEffect_QF (const vec3_t org)
{
	int		count = 7;

	R_BloodPuff_QF (org, 50);

	if (numparticles >= r_maxparticles)
		return;
	particle_new (pt_smokecloud, part_tex_smoke, org, 3, vec3_origin,
				  r_realtime + 9, 12 + (rand () & 3), 64 + (rand () & 31));

	if (numparticles + count >= r_maxparticles)
		count = r_maxparticles - numparticles;
	while (count--)
		particle_new_random (pt_fallfadespark, part_tex_spark, org, 12, 2,
							 128, r_realtime + 5, 244 + (rand () % 3), 255);
}

void
R_RunParticleEffect_QF (const vec3_t _org, const vec3_t dir, int color,
						int count)
{
	int         i, j;
	vec3_t      org;

	VectorCopy (_org, org);

	if (numparticles >= r_maxparticles)
		return;
	if (numparticles + count >= r_maxparticles)
		count = r_maxparticles - numparticles;

	for (i = 0; i < count; i++) {
		for (j = 0; j < 3; j++) {
			org[j] += ((rand () & 15) - 8);
		}
		particle_new (pt_slowgrav, part_tex_dot, org, 1.5, dir,
					  r_realtime + 0.1 * (rand () % 5),
					  (color & ~7) + (rand () & 7), 255);
	}
}

void
R_SpikeEffect_QF (const vec3_t org)
{
	R_RunSparkEffect_QF (org, 5, 8);
}

void
R_SuperSpikeEffect_QF (const vec3_t org)
{
	R_RunSparkEffect_QF (org, 10, 8);
}

void
R_KnightSpikeEffect_QF (const vec3_t org)
{
	int		count = 10;

	if (numparticles >= r_maxparticles)
		return;
	particle_new (pt_smokecloud, part_tex_smoke, org, 1, vec3_origin,
				  r_realtime + 9, 234, 64 + (rand () & 31));

	if (numparticles + count >= r_maxparticles)
		count = r_maxparticles - numparticles;
	while (count--)
		particle_new_random (pt_fallfadespark, part_tex_dot, org, 6, 0.7, 96,
							 r_realtime + 5, 234, 255);
}

void
R_WizSpikeEffect_QF (const vec3_t org)
{
	int		count = 15;

	if (numparticles >= r_maxparticles)
		return;
	particle_new (pt_smokecloud, part_tex_smoke, org, 2, vec3_origin,
				  r_realtime + 9, 52 + (rand () & 3), 64 + (rand () & 31));

	if (numparticles + count >= r_maxparticles)
		count = r_maxparticles - numparticles;
	while (count--)
		particle_new_random (pt_fallfadespark, part_tex_dot, org, 12, 0.7, 96,
							 r_realtime + 5, 52 + (rand () & 3), 255);
}

void
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
			vel = 50 + (rnd & 63);
			VectorScale (dir, vel, pvel);
			particle_new (pt_grav, part_tex_dot, porg, 3, pvel,
						  r_realtime + 2 + ((rnd >> 7) & 31) * 0.02,
						  224 + ((rnd >> 12) & 7), 193);
		}
	}
}

void
R_TeleportSplash_QF (const vec3_t org)
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

	for (i = -16; i < 16; i += 4) {
		dir[1] = i * 8;
		for (j = -16; j < 16; j += 4) {
			dir[0] = j * 8;
			for (k = -24; k < 32; k += 4) {
				dir[2] = k * 8;
				
				rnd = rand ();
				porg[0] = org[0] + i + (rnd & 3);
				porg[1] = org[1] + j + ((rnd >> 2) & 3);
				porg[2] = org[2] + k + ((rnd >> 4) & 3);

				VectorNormalize (dir);
				vel = 50 + ((rnd >> 6) & 63);
				VectorScale (dir, vel, pvel);
				particle_new (pt_grav, part_tex_spark, porg, 0.6, pvel,
							  (r_realtime + 0.2 + (rand () & 15) * 0.01),
							  (7 + ((rnd >> 12) & 7)), 255);
			}
		}
	}
}

void
R_RocketTrail_QF (entity_t *ent)
{
	float		dist, maxlen, origlen, percent, pscale, pscalenext;
	float		len = 0.0;
	vec3_t		subtract, vec;

	if (numparticles >= r_maxparticles)
		return;

	VectorSubtract (ent->origin, ent->old_origin, vec);
	maxlen = VectorNormalize (vec);
	origlen = r_frametime / maxlen;

	pscale = 1.5 + qfrandom (1.5);

	while (len < maxlen) {
		pscalenext = 1.5 + qfrandom (1.5);
		dist = (pscale + pscalenext) * 3.0;

		VectorScale (vec, min(dist, len), subtract);
		VectorAdd (ent->old_origin, subtract, ent->old_origin);
		percent = len * origlen;

		particle_new (pt_smoke, part_tex_smoke, ent->old_origin,
					  pscale + percent * 4.0, vec3_origin,
					  r_realtime + 2.0 - percent * 2.0, 
					  12 + (rand () & 3),
					  128 + (rand () & 31) - percent * 100.0);
		if (numparticles >= r_maxparticles)
			break;
		len += dist;
		pscale = pscalenext;
	}
}

void
R_GrenadeTrail_QF (entity_t *ent)
{
	float		dist, maxlen, origlen, percent, pscale, pscalenext;
	float		len = 0.0;
	vec3_t		subtract, vec;

	if (numparticles >= r_maxparticles)
		return;

	VectorSubtract (ent->origin, ent->old_origin, vec);
	maxlen = VectorNormalize (vec);
	origlen = r_frametime / maxlen;
	pscale = 6.0 + qfrandom (7.0);

	while (len < maxlen) {
		pscalenext = 6.0 + qfrandom (7.0);
		dist = (pscale + pscalenext) * 2.0;

		VectorScale (vec, min(dist, len), subtract);
		VectorAdd (ent->old_origin, subtract, ent->old_origin);
		percent = len * origlen;

		particle_new (pt_smoke, part_tex_smoke, ent->old_origin,
					  pscale + percent * 4.0, vec3_origin,
					  r_realtime + 2.0 - percent * 2.0,
					  1 + (rand () & 3),
					  160 + (rand () & 31) - percent * 100.0);
		if (numparticles >= r_maxparticles)
			break;
		len += dist;
		pscale = pscalenext;
	}
}

void
R_BloodTrail_QF (entity_t *ent)
{
	float		dist, maxlen, origlen, percent, pscale, pscalenext;
	float		len = 0.0;
	int			j;
	vec3_t		subtract, vec, porg, pvel;

	if (numparticles >= r_maxparticles)
		return;

	VectorSubtract (ent->origin, ent->old_origin, vec);
	maxlen = VectorNormalize (vec);
	origlen = r_frametime / maxlen;
	pscale = 5.0 + qfrandom (10.0);

	while (len < maxlen) {
		VectorCopy (vec3_origin, pvel);
		VectorCopy (ent->old_origin, porg);

		pscalenext = 5.0 + qfrandom (10.0);
		dist = (pscale + pscalenext) * 1.5;

		for (j = 0; j < 3; j++) {
			pvel[j] = qfrandom (24.0) - 12.0;
			porg[j] = ent->old_origin[j] + qfrandom (3.0) - 1.5;
		}

		VectorScale (vec, min(dist, len), subtract);
		VectorAdd (ent->old_origin, subtract, ent->old_origin);
		percent = len * origlen;
		pvel[2] -= percent * 40;

		particle_new (pt_grav, part_tex_smoke, porg, pscale, pvel,
					  r_realtime + 2.0 - percent * 2.0, 68 + (rand () & 3),
					  255);
		if (numparticles >= r_maxparticles)
			break;
		len += dist;
		pscale = pscalenext;
	}
}

void
R_SlightBloodTrail_QF (entity_t *ent)
{
	float		dist, maxlen, origlen, percent, pscale, pscalenext;
	float		len = 0;
	int			j;
	vec3_t      subtract, vec, porg, pvel;

	if (numparticles >= r_maxparticles)
		return;

	VectorSubtract (ent->origin, ent->old_origin, vec);
	maxlen = VectorNormalize (vec);
	origlen = r_frametime / maxlen;
	pscale = 1.5 + qfrandom (7.5);

	while (len < maxlen) {
		VectorCopy (vec3_origin, pvel);
		VectorCopy (ent->old_origin, porg);

		pscalenext = 1.5 + qfrandom (7.5);
		dist = (pscale + pscalenext) * 1.5;

		for (j = 0; j < 3; j++) {
			pvel[j] = (qfrandom (12.0) - 6.0);
			porg[j] = ent->old_origin[j] + qfrandom (3.0) - 1.5;
		}

		VectorScale (vec, min(dist, len), subtract);
		VectorAdd (ent->old_origin, subtract, ent->old_origin);
		percent = len * origlen;
		pvel[2] -= percent * 40;

		particle_new (pt_grav, part_tex_smoke, porg, pscale, pvel,
					  r_realtime + 1.5 - percent * 1.5, 68 + (rand () & 3),
					  192);
		if (numparticles >= r_maxparticles)
			break;
		len += dist;
		pscale = pscalenext;
	}
}

void
R_WizTrail_QF (entity_t *ent)
{
	float		maxlen, origlen, percent;
	float		dist = 3.0, len = 0.0;
	static int	tracercount;
	vec3_t		subtract, vec, pvel;

	if (numparticles >= r_maxparticles)
		return;

	VectorSubtract (ent->origin, ent->old_origin, vec);
	maxlen = VectorNormalize (vec);
	origlen = r_frametime / maxlen;

	while (len < maxlen) {
		VectorCopy (vec3_origin, pvel);
		tracercount++;
		if (tracercount & 1) {
			pvel[0] = 30.0 * vec[1];
			pvel[1] = 30.0 * -vec[0];
		} else {
			pvel[0] = 30.0 * -vec[1];
			pvel[1] = 30.0 * vec[0];
		}

		VectorScale (vec, min(dist, len), subtract);
		VectorAdd (ent->old_origin, subtract, ent->old_origin);
		percent = len * origlen;

		particle_new (pt_fire, part_tex_smoke, ent->old_origin,
					  2.0 + qfrandom (1.0) - percent * 2.0, pvel,
					  r_realtime + 0.5 - percent * 0.5, 52 + (rand () & 4),
					  255 - percent * 32.0);
		if (numparticles >= r_maxparticles)
			break;
		len += dist;
	}
}

void
R_FlameTrail_QF (entity_t *ent)
{
	float		maxlen, origlen, percent;
	float		dist = 3.0, len = 0.0;
	static int	tracercount;
	vec3_t		subtract, vec, pvel;

	if (numparticles >= r_maxparticles)
		return;

	VectorSubtract (ent->origin, ent->old_origin, vec);
	maxlen = VectorNormalize (vec);
	origlen = r_frametime / maxlen;

	while (len < maxlen) {
		VectorCopy (vec3_origin, pvel);
		tracercount++;
		if (tracercount & 1) {
			pvel[0] = 30.0 * vec[1];
			pvel[1] = 30.0 * -vec[0];
		} else {
			pvel[0] = 30.0 * -vec[1];
			pvel[1] = 30.0 * vec[0];
		}

		VectorScale (vec, min(dist, len), subtract);
		VectorAdd (ent->old_origin, subtract, ent->old_origin);
		percent = len * origlen;

		particle_new (pt_fire, part_tex_smoke, ent->old_origin,
					  2.0 + qfrandom (1.0) - percent * 2.0, pvel,
					  r_realtime + 0.5 - percent * 0.5, 234,
					  255 - percent * 32.0);
		if (numparticles >= r_maxparticles)
			break;
		len += dist;
	}
}

void
R_VoorTrail_QF (entity_t *ent)
{
	float		maxlen, origlen, percent;
	float		dist = 3.0, len = 0.0;
	int			j;
	vec3_t		subtract, vec, porg;

	if (numparticles >= r_maxparticles)
		return;

	VectorSubtract (ent->origin, ent->old_origin, vec);
	maxlen = VectorNormalize (vec);
	origlen = r_frametime / maxlen;

	while (len < maxlen) {
		for (j = 0; j < 3; j++)
			porg[j] = ent->old_origin[j] + qfrandom (16.0) - 8.0;

		VectorScale (vec, min(dist, len), subtract);
		VectorAdd (ent->old_origin, subtract, ent->old_origin);
		percent = len * origlen;

		particle_new (pt_static, part_tex_dot, porg, 1.0 + qfrandom (1.0),
					  vec3_origin, r_realtime + 0.3 - percent * 0.3,
					  9 * 16 + 8 + (rand () & 3), 255);
		if (numparticles >= r_maxparticles)
			break;
		len += dist;
	}
}

void
R_ParticleExplosion_EE (const vec3_t org)
{
/*
	R_NewExplosion (org);
*/
	if (numparticles >= r_maxparticles)
		return;
	particle_new_random (pt_smokecloud, part_tex_smoke, org, 4, 30, 8,
						 r_realtime + 5.0, rand () & 255,
						 128 + (rand () & 63));
}

void
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

	for (i = -16; i < 16; i += 4) {
		dir[1] = i * 8;
		for (j = -16; j < 16; j += 4) {
			dir[0] = j * 8;
			for (k = -24; k < 32; k += 4) {
				dir[2] = k * 8;
				
				rnd = rand ();
				porg[0] = org[0] + i + (rnd & 3);
				porg[1] = org[1] + j + ((rnd >> 2) & 3);
				porg[2] = org[2] + k + ((rnd >> 4) & 3);

				VectorNormalize (dir);
				vel = 50 + ((rnd >> 6) & 63);
				VectorScale (dir, vel, pvel);
				particle_new (pt_grav, part_tex_spark, porg, 0.6, pvel,
							  (r_realtime + 0.2 + (rand () & 15) * 0.01),
							  rand () & 255, 255);
			}
		}
	}
}

void
R_RocketTrail_EE (entity_t *ent)
{
	float		dist, maxlen, origlen, percent, pscale, pscalenext;
	float		len = 0.0;
	vec3_t		subtract, vec;

	if (numparticles >= r_maxparticles)
		return;

	VectorSubtract (ent->origin, ent->old_origin, vec);
	maxlen = VectorNormalize (vec);
	origlen = r_frametime / maxlen;

	pscale = 1.5 + qfrandom (1.5);

	while (len < maxlen) {
		pscalenext = 1.5 + qfrandom (1.5);
		dist = (pscale + pscalenext) * 3.0;

		VectorScale (vec, min(dist, len), subtract);
		VectorAdd (ent->old_origin, subtract, ent->old_origin);
		percent = len * origlen;

		particle_new (pt_smoke, part_tex_smoke, ent->old_origin,
					  pscale + percent * 4.0, vec3_origin,
					  r_realtime + 2.0 - percent * 2.0, 
					  rand () & 255,
					  128 + (rand () & 31) - percent * 100.0);
		if (numparticles >= r_maxparticles)
			break;
		len += dist;
		pscale = pscalenext;
	}
}

void
R_GrenadeTrail_EE (entity_t *ent)
{
	float		dist, maxlen, origlen, percent, pscale, pscalenext;
	float		len = 0.0;
	vec3_t		subtract, vec;

	if (numparticles >= r_maxparticles)
		return;

	VectorSubtract (ent->origin, ent->old_origin, vec);
	maxlen = VectorNormalize (vec);
	origlen = r_frametime / maxlen;
	pscale = 6.0 + qfrandom (7.0);

	while (len < maxlen) {
		pscalenext = 6.0 + qfrandom (7.0);
		dist = (pscale + pscalenext) * 2.0;

		VectorScale (vec, min(dist, len), subtract);
		VectorAdd (ent->old_origin, subtract, ent->old_origin);
		percent = len * origlen;

		particle_new (pt_smoke, part_tex_smoke, ent->old_origin,
					  pscale + percent * 4.0, vec3_origin,
					  r_realtime + 2.0 - percent * 2.0,
					  rand () & 255,
					  160 + (rand () & 31) - percent * 100.0);
		if (numparticles >= r_maxparticles)
			break;
		len += dist;
		pscale = pscalenext;
	}
}

void
R_ParticleExplosion_ID (const vec3_t org)
{
	int		i;
	int		j = 1024;
	ptype_t	ptype;

	if (numparticles >= r_maxparticles)
		return;
	else if (numparticles + j >= r_maxparticles)
		j = r_maxparticles - numparticles;

	for (i = 0; i < j; i++) {
		if (i & 1)
			ptype = pt_explode;
		else
			ptype = pt_explode2;
		particle_new_random (ptype, part_tex_dot, org, 16, 1.5, 256,
							 r_realtime + 5.0, (rand () & 7) + 8, 255);
	}
}

void
R_DrawParticles (void)
{
	unsigned char  *at;
	float			dvel, grav, fast_grav, minparticledist, scale,
					bloodcloud_alpha, bloodcloud_scale, fallfadespark_alpha,
					fire_alpha, fire_scale, smoke_alpha, smoke_scale,
					smokecloud_alpha, smokecloud_org, smokecloud_scale;
	int				activeparticles, maxparticle, j, k;
	unsigned int	vacount;
	varray_t2f_c4ub_v3f_t		*VA;
	particle_t	   *part;
	vec3_t			up_scale, right_scale, up_right_scale, down_right_scale;

	if (!r_particles->int_val)
		return;

	// LordHavoc: particles should not affect zbuffer
	qfglDepthMask (GL_FALSE);
	qfglBindTexture (GL_TEXTURE_2D, part_tex);

	grav = (fast_grav = r_frametime * 800.0) * 0.05;
	dvel = bloodcloud_scale = smoke_scale = r_frametime * 4.0;
	smoke_alpha = r_frametime * 100.0;
	smokecloud_alpha = r_frametime * 140.0;
	smokecloud_scale = r_frametime * 50.0;
	smokecloud_org = r_frametime * 30.0;
	bloodcloud_alpha = r_frametime * 65.0;
	fallfadespark_alpha = r_frametime * 256.0;
	fire_alpha = r_frametime * 32.0;
	fire_scale = r_frametime * 2.0;

	minparticledist = DotProduct (r_refdef.vieworg, vpn) + 32.0;

	activeparticles = 0;
	vacount = 0;
	VA = particleVertexArray;
	maxparticle = -1;
	j = 0;

	for (k = 0, part = particles; k < numparticles; k++, part++) {
		// Don't render particles too close to us.
		// Note, we must still do physics and such on them.
		if (!(DotProduct (part->org, vpn) < minparticledist)) {
			at = (byte *) & d_8to24table[(byte) part->color];
			VA[0].color[0] = at[0];
			VA[0].color[1] = at[1];
			VA[0].color[2] = at[2];
			VA[0].color[3] = part->alpha;
			memcpy (VA[1].color, VA[0].color, sizeof(VA[0].color));
			memcpy (VA[2].color, VA[0].color, sizeof(VA[0].color));
			memcpy (VA[3].color, VA[0].color, sizeof(VA[0].color));

			switch (part->tex) {
			case 0:
				VA[0].texcoord[0] = 0;
				VA[0].texcoord[1] = 0.5;
				VA[1].texcoord[0] = 0;
				VA[1].texcoord[1] = 0;
				VA[2].texcoord[0] = 0.5;
				VA[2].texcoord[1] = 0;
				VA[3].texcoord[0] = 0.5;
				VA[3].texcoord[1] = 0.5;
				break;
			case 1:
				VA[0].texcoord[0] = 0.5;
				VA[0].texcoord[1] = 0.5;
				VA[1].texcoord[0] = 0.5;
				VA[1].texcoord[1] = 0;
				VA[2].texcoord[0] = 1;
				VA[2].texcoord[1] = 0;
				VA[3].texcoord[0] = 1;
				VA[3].texcoord[1] = 0.5;
				break;
			case 2:
				VA[0].texcoord[0] = 0;
				VA[0].texcoord[1] = 1;
				VA[1].texcoord[0] = 0;
				VA[1].texcoord[1] = 0.5;
				VA[2].texcoord[0] = 0.5;
				VA[2].texcoord[1] = 0.5;
				VA[3].texcoord[0] = 0.5;
				VA[3].texcoord[1] = 1;
				break;
			}

			scale = part->scale;

			VectorScale (vup, scale, up_scale);
			VectorScale (vright, scale, right_scale);

			VectorAdd (right_scale, up_scale, up_right_scale);
			VectorSubtract (right_scale, up_scale, down_right_scale);

			VectorAdd (part->org, up_right_scale,   VA[0].vertex);
			VectorAdd (part->org, down_right_scale, VA[1].vertex);
			VectorSubtract (part->org, up_right_scale,   VA[2].vertex);
			VectorSubtract (part->org, down_right_scale, VA[3].vertex);

			VA += 4;
			vacount += 4;
			if (vacount + 4 > pVAsize) {
				qfglDrawElements (GL_QUADS, vacount, GL_UNSIGNED_INT,
								  pVAindices);
				vacount = 0;
				VA = particleVertexArray;
			}
		}

		VectorMA (part->org, r_frametime, part->vel, part->org);

		switch (part->type) {
			case pt_static:
				break;
			case pt_blob:
				VectorMA (part->vel, dvel, part->vel, part->vel);
				part->vel[2] -= grav;
				break;
			case pt_blob2:
				part->vel[0] -= part->vel[0] * dvel;
				part->vel[1] -= part->vel[1] * dvel;
				part->vel[2] -= grav;
				break;
			case pt_grav:
				part->vel[2] -= grav;
				break;
			case pt_smoke:
				if ((part->alpha -= smoke_alpha) < 1)
					part->die = -1;
				part->scale += smoke_scale;
//				part->org[2] += smokecloud_org;
				break;
			case pt_smokecloud:
				if ((part->alpha -= smokecloud_alpha) < 1) {
					part->die = -1;
					break;
				}
				part->scale += smokecloud_scale;
				part->org[2] += smokecloud_org;
				break;
			case pt_bloodcloud:
				if ((part->alpha -= bloodcloud_alpha) < 1) {
					part->die = -1;
					break;
				}
				part->scale += bloodcloud_scale;
				part->vel[2] -= grav;
				break;
			case pt_fallfadespark:
				if ((part->alpha -= fallfadespark_alpha) < 1)
					part->die = -1;
				part->vel[2] -= fast_grav;
				break;
			case pt_fire:
				if ((part->alpha -= fire_alpha) < 1)
					part->die = -1;
				part->scale -= fire_scale;
				break;
			default:
				Con_DPrintf ("unhandled particle type %d\n", part->type);
				break;
		}
		// LordHavoc: immediate removal of unnecessary particles (must be done
		// to ensure compactor below operates properly in all cases)
		if (part->die < r_realtime)
			freeparticles[j++] = part;
		else {
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
	if (easter_eggs) {
		if (easter_eggs->int_val) {
			R_ParticleExplosion = R_ParticleExplosion_EE;
			R_TeleportSplash = R_TeleportSplash_EE;
			R_RocketTrail = R_RocketTrail_EE;
			R_GrenadeTrail = R_GrenadeTrail_EE;
		} else {
			R_ParticleExplosion = R_ParticleExplosion_QF;
			R_TeleportSplash = R_TeleportSplash_QF;
			R_RocketTrail = R_RocketTrail_QF;
			R_GrenadeTrail = R_GrenadeTrail_QF;
		}
	}
}

void
R_ParticleFunctionInit (void)
{
	R_BlobExplosion = R_BlobExplosion_QF;
	R_ParticleExplosion = R_ParticleExplosion_QF;
	R_ParticleExplosion2 = R_ParticleExplosion2_QF;
	R_LavaSplash = R_LavaSplash_QF;
	R_TeleportSplash = R_TeleportSplash_QF;

	R_BloodPuffEffect = R_BloodPuffEffect_QF;
	R_GunshotEffect = R_GunshotEffect_QF;
	R_LightningBloodEffect = R_LightningBloodEffect_QF;

	R_RunParticleEffect = R_RunParticleEffect_QF;
	R_SpikeEffect = R_SpikeEffect_QF;
	R_SuperSpikeEffect = R_SuperSpikeEffect_QF;
	R_KnightSpikeEffect = R_KnightSpikeEffect_QF;
	R_WizSpikeEffect = R_WizSpikeEffect_QF;

	R_RocketTrail = R_RocketTrail_QF;
	R_GrenadeTrail = R_GrenadeTrail_QF;
	R_BloodTrail = R_BloodTrail_QF;
	R_SlightBloodTrail = R_SlightBloodTrail_QF;
	R_WizTrail = R_WizTrail_QF;
	R_FlameTrail = R_FlameTrail_QF;
	R_VoorTrail = R_VoorTrail_QF;
}

void
R_Particles_Init_Cvars (void)
{
	R_ParticleFunctionInit ();
}
