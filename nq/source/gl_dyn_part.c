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

	$Id$
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
#include "QF/compat.h"
#include "QF/console.h"
#include "QF/qargs.h"
#include "QF/sys.h"
#include "QF/varrays.h"

#include "client.h"
#include "glquake.h"
#include "host.h"
#include "render.h"
#include "r_dynamic.h"

static particle_t *particles, **freeparticles;
static short r_numparticles, numparticles;

extern qboolean lighthalf;

extern cvar_t *cl_max_particles;

extern int  part_tex_dot;
extern int  part_tex_spark;
extern int  part_tex_smoke[8];
extern int  part_tex_smoke_ring[8];

int         ramp[8] = { 0x6d, 0x6b, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01 };


inline particle_t *
particle_new (ptype_t type, int texnum, vec3_t org, float scale, vec3_t vel,
			  float die, byte color, byte alpha, vec3_t up, vec3_t right)
{
	particle_t *part;

	if (numparticles >= r_numparticles) {
		// Con_Printf("FAILED PARTICLE ALLOC!\n");
		return NULL;
	}

	part = &particles[numparticles++];

	part->type = type;
	VectorCopy (org, part->org);
	VectorCopy (vel, part->vel);
	part->die = die;
	part->color = color;
	part->alpha = alpha;
	part->tex = texnum;
	part->scale = scale;

	VectorScale (up, 1.5, part->up);
	VectorScale (right, 1.5, part->right);

	return part;
}


inline particle_t *
particle_new_random (ptype_t type, int texnum, vec3_t org, int org_fuzz,
					 float scale, int vel_fuzz, float die, byte color,
					 byte alpha)
{
	vec3_t      porg, pvel;
	int         j;

	for (j = 0; j < 3; j++) {
		if (org_fuzz)
			porg[j] = lhrandom (-org_fuzz, org_fuzz) + org[j];
		if (vel_fuzz)
			pvel[j] = lhrandom (-vel_fuzz, vel_fuzz);
	}
	return particle_new (type, texnum, porg, scale, pvel, die, color, alpha, vec3_origin, vec3_origin);
}


/*
	R_MaxParticlesCheck

	Misty-chan: Dynamically change the maximum amount of particles on the fly.
	Thanks to a LOT of help from Taniwha, Deek, Mercury, Lordhavoc, and
	lots of others.
*/
void
R_MaxParticlesCheck (cvar_t *var)
{
	/*
	Catchall. If the user changed the setting to a number less than zero
	 *or* if we had a wacky cfg get past the init code check, this will
	 make sure we don't have problems. Also note that grabbing the
	 var->int_val is IMPORTANT:

	Prevents a segfault since if we grabbed the int_val of
	cl_max_particles we'd sig11 right here at startup.
	*/
	r_numparticles = max(var->int_val, 0);
	
	// Be very careful the next time we do something like this.
	// calloc/free are IMPORTANT and the compiler doesn't know when we
	// do bad things with them.
	free (particles);
	free (freeparticles);

	particles = (particle_t *)
		calloc (r_numparticles, sizeof (particle_t));
	freeparticles = (particle_t **)
		calloc (r_numparticles, sizeof (particle_t*));			

	R_ClearParticles();
}


void
R_Particles_Init_Cvars (void)
{
	// Misty-chan: This is a cvar that does callbacks. Whenever it
	// changes, it calls the function R_MaxParticlesCheck and therefore
	// is very nifty.
	Cvar_Get ("cl_max_particles", "2048", CVAR_ARCHIVE, R_MaxParticlesCheck,
					"Maximum amount of particles to display. No maximum, minimum is 0, although it's best to use r_particles 0 instead.");
}


void
R_ClearParticles (void)
{
	numparticles = 0;
}


void
R_ReadPointFile_f (void)
{
	QFile      *f;
	vec3_t      org;
	int         r;
	int         c;
	char        name[MAX_OSPATH], *mapname, *t1;

	mapname = strdup (cl.worldmodel->name);
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

		if (!particle_new (pt_static, part_tex_dot, org, 1.5, vec3_origin,
						   99999, (-c) & 15, 255, vec3_origin, vec3_origin)) {
			Con_Printf ("Not enough free particles\n");
			break;
		}
	}

	Qclose (f);
	Con_Printf ("%i points read\n", c);
}


void
R_ParticleExplosion (vec3_t org)
{
	if (!r_particles->int_val)
		return;

	particle_new_random (pt_smokecloud, part_tex_smoke[rand () & 7], org, 4, 30,
						 8, cl.time + 5, (rand () & 7) + 8,
						 128 + (rand () & 63));
}


void
R_ParticleExplosion2 (vec3_t org, int colorStart, int colorLength)
{
	int         i;
	int         colorMod = 0;

	if (!r_particles->int_val)
		return;
	
	for (i = 0; i < 512; i++) {
		particle_new_random (pt_blob, part_tex_dot, org, 16, 2, 256, (cl.time + 0.3), (colorStart + (colorMod % colorLength)), 255);
		colorMod++;
	}
}


void
R_BlobExplosion (vec3_t org)
{
	int         i;

	if (!r_particles->int_val)
		return;

	for (i = 0; i < 512; i++) {
		particle_new_random (pt_blob, part_tex_dot, org, 12, 2, 256,
							 (cl.time + 1 + (rand () & 8) * 0.05),
							 (66 + rand () % 6), 255);
	}
	for (i = 0; i < 512; i++) {
		particle_new_random (pt_blob2, part_tex_dot, org, 12, 2, 256,
							 (cl.time + 1 + (rand () & 8) * 0.05),
							 (150 + rand () % 6), 255);
	}
}


static void
R_RunSparkEffect (vec3_t org, int count, int ofuzz)
{
	if (!r_particles->int_val)
		return;

	particle_new (pt_smokecloud, part_tex_smoke[rand () & 7], org, 
				  (ofuzz / 8) * .75, vec3_origin, cl.time + 99, 
				  12 + (rand () & 3), 96, vec3_origin, vec3_origin);
	while (count--)
		particle_new_random (pt_fallfadespark, part_tex_spark, org, ofuzz * .75,
							 1, 96, cl.time + 5, ramp[rand () % 6], 
							 lhrandom (0, 255));
}


static void
R_RunGunshotEffect (vec3_t org, int count)
{
	int         scale;

	if (!r_particles->int_val)
		return;

	if (count > 6)
		scale = 3;
	else
		scale = 2;

	R_RunSparkEffect (org, count * 10, 8 * scale);
	return;
}


/*
static void
R_BloodPuff (vec3_t org, int count)
{
	if (!r_particles->int_val)
		return;

	particle_new (pt_bloodcloud, part_tex_smoke[rand () & 7], org, 9,
				  vec3_origin, cl.time + 99, 68 + (rand () & 3), 128,
				  vec3_origin, vec3_origin);
}
*/


void
R_RunPuffEffect (vec3_t org, byte type, byte count)
{
	if (!r_particles->int_val)
		return;

	switch (type) {
		case TE_GUNSHOT:
			R_RunGunshotEffect (org, count);
			break;
	}
}


void
R_RunParticleEffect (vec3_t org, int color, int count)
{
	int         i, j, scale;
	vec3_t      porg;

	if (!r_particles->int_val)
		return;

	if (count > 130)
		scale = 3;
	else if (count > 20)
		scale = 2;
	else
		scale = 1;

	for (i = 0; i < count; i++) {
		for (j = 0; j < 3; j++) {
			porg[j] = org[j] + scale * ((rand () & 15) - 8);
		}
		particle_new (pt_grav, part_tex_dot, porg, 1.5, vec3_origin,
					  (cl.time + 0.1 * (rand () % 5)),
					  (color & ~7) + (rand () & 7), 255, vec3_origin, vec3_origin);
	}
}


void
R_RunSpikeEffect (vec3_t org, byte type)
{
	switch (type) {
		case TE_SPIKE:
			R_RunSparkEffect (org, 5, 8);
			break;
		case TE_SUPERSPIKE:
			R_RunSparkEffect (org, 10, 8);
			break;
		case TE_KNIGHTSPIKE:
			R_RunSparkEffect (org, 10, 8);
			break;
		case TE_WIZSPIKE:
			R_RunSparkEffect (org, 15, 16);
			break;
	}
}


void
R_LavaSplash (vec3_t org)
{
	int         i, j;
	float       vel;
	vec3_t      dir, porg, pvel;

	if (!r_particles->int_val)
		return;

	for (i = -8; i < 8; i++) {
		for (j = -8; j < 8; j++) {
			dir[0] = j * 16 + (rand () & 7);
			dir[1] = i * 16 + (rand () & 7);
			dir[2] = 256;

			porg[0] = org[0] + dir[0];
			porg[1] = org[1] + dir[1];
			porg[2] = org[2] + (rand () & 63);

			VectorNormalize (dir);
			vel = 50 + (rand () & 63);
			VectorScale (dir, vel, pvel);
			particle_new (pt_grav, part_tex_dot, porg, 3, pvel,
						  (cl.time + 2 + (rand () & 31) * 0.02),
						  (224 + (rand () & 7)), 193, vec3_origin, vec3_origin);
		}
	}
}


void
R_TeleportSplash (vec3_t org)
{
	int         i, j, k;
	float       vel;
	vec3_t      dir, porg, pvel;

	if (!r_particles->int_val)
		return;

	for (i = -16; i < 16; i += 4)
		for (j = -16; j < 16; j += 4)
			for (k = -24; k < 32; k += 4) {
				dir[0] = j * 8;
				dir[1] = i * 8;
				dir[2] = k * 8;

				porg[0] = org[0] + i + (rand () & 3);
				porg[1] = org[1] + j + (rand () & 3);
				porg[2] = org[2] + k + (rand () & 3);

				VectorNormalize (dir);
				vel = 50 + (rand () & 63);
				VectorScale (dir, vel, pvel);
				particle_new (pt_grav, part_tex_spark, porg, 0.6, pvel,
							  (cl.time + 0.2 + (rand () & 7) * 0.02),
							  (7 + (rand () & 7)), 255, vec3_origin, vec3_origin);
			}
}


void
R_RocketTrail (int type, entity_t *ent)
{
	vec3_t      vec, subtract;
	float       len, dist;
	int         j, ptex;
	ptype_t     ptype;
	vec3_t      porg, pvel, up, right;
	float       pdie, pscale;
	byte        palpha, pcolor;

	if (type == 0)
		R_AddFire (ent->msg_origins[1], ent->origin, ent);

	if (!r_particles->int_val)
		return;

	VectorSubtract (ent->origin, ent->msg_origins[1], vec);
	len = VectorNormalize (vec);
	while (len > 0) {
		VectorCopy (vec3_origin, up);
		VectorCopy (vec3_origin, right);
		VectorCopy (vec3_origin, pvel);
		pdie = cl.time + 2;
		ptype = pt_static;
		ptex = part_tex_dot;
		palpha = 255;
		pcolor = 0;
		pscale = 6;
		dist = 40;

		switch (type) {
			case 0:					// rocket trail
				pdie = cl.time + 60;
//				ptype = pt_smokering; // Mercury's Rings
				ptype = pt_smoke;
				pscale = lhrandom (6, 8);
//				pcolor = (rand () & 255); // Misty-chan's Easter Egg
				pcolor = (rand () & 3) + 12;
				palpha = 128 + (rand () & 31);
//				VectorVectors(vec, right, up); // Mercury's Rings
				VectorCopy (ent->msg_origins[1], porg);
//				ptex = part_tex_smoke_ring[rand () & 7]; // Mercury's Rings
				ptex = part_tex_smoke[rand () & 7];
				break;
			case 1:					// grenade trail
				ptype = pt_smoke;
				pscale = lhrandom (7, 10);
//				pcolor = (rand () & 255); // Misty-chan's Easter Egg
				pcolor = (rand () & 3);
				palpha = 128 + (rand () & 31);
				VectorCopy (ent->msg_origins[1], porg);
				ptex = part_tex_smoke[rand () & 7];
				break;
			case 2:					// blood
				pscale = 5;
			case 4:					// slight blood
				pscale += lhrandom (1, 4);
				ptex = part_tex_smoke[rand () & 7];
				pcolor = 68 + (rand () & 3);
				for (j = 0; j < 3; j++) {
					pvel[j] = lhrandom (-3, 3) * type;
					porg[j] = ent->msg_origins[1][j] + lhrandom (-1.5, 1.5);
				}
				ptype = pt_grav;
				break;
			case 6:					// voor trail
// Use smoke ring effects here, once merged with nq? --Despair
				dist = 3;
				pcolor = 9 * 16 + 8 + (rand () & 3);
				ptype = pt_static;
				pscale = lhrandom (.75, 1.5);
				pdie = cl.time + 0.3;
				for (j = 0; j < 3; j++)
					porg[j] = ent->msg_origins[1][j] + lhrandom (-8, 8);
				break;
			case 3:
			case 5:					// tracer
				{
					static int  tracercount;

					dist = 3;
					pdie = cl.time + 0.5;
					ptype = pt_static;
					pscale = lhrandom (1.5, 3);
					if (type == 3)
						pcolor = 52 + ((tracercount & 4) << 1);
					else
						pcolor = 230 + ((tracercount & 4) << 1);

					tracercount++;

					VectorCopy (ent->msg_origins[1], porg);
					if (tracercount & 1) {
						pvel[0] = 30 * vec[1];
						pvel[1] = 30 * -vec[0];
					} else {
						pvel[0] = 30 * -vec[1];
						pvel[1] = 30 * vec[0];
					}
					break;
				}
		}

		VectorScale (vec, min(dist, len), subtract);
		VectorAdd (ent->msg_origins[1], subtract, ent->msg_origins[1]);
		len -= dist;

		particle_new (ptype, ptex, porg, pscale, pvel, pdie, pcolor, palpha, 
				up, right);
	}
}


void
R_DrawParticles (void)
{
	byte        i;
	float       grav, fast_grav, dvel;
	float       minparticledist;
	unsigned char *at;
	byte        alpha;
	float       scale;
	particle_t *part;
	vec3_t		up, right, o_up, o_right;
	vec3_t		up_scale, right_scale, up_right_scale;
	int         activeparticles, maxparticle, j, k;
	
	// LordHavoc: particles should not affect zbuffer
	glDepthMask (GL_FALSE);

	VectorScale (vup, 1.5, o_up);
	VectorScale (vright, 1.5, o_right);

	varray[0].texcoord[0] = 0; varray[0].texcoord[1] = 1;
	varray[1].texcoord[0] = 0; varray[1].texcoord[1] = 0;
	varray[2].texcoord[0] = 1; varray[2].texcoord[1] = 0;
	varray[3].texcoord[0] = 1; varray[3].texcoord[1] = 1;

	grav = (fast_grav = host_frametime * 800) * 0.05;
	dvel = 4 * host_frametime;

	minparticledist = DotProduct (r_refdef.vieworg, vpn) + 32.0f;

	activeparticles = 0;
	maxparticle = -1;
	j = 0;

	for (k = 0, part = particles; k < numparticles; k++, part++) {
		// LordHavoc: this is probably no longer necessary, as it is
		// checked at the end, but could still happen on weird particle
		// effects, left for safety...
		if (part->die <= cl.time) {
			freeparticles[j++] = part;
			continue;
		}
		maxparticle = k;
		activeparticles++;

		// Don't render particles too close to us.
		// Note, we must still do physics and such on them.
		if (!(DotProduct (part->org, vpn) < minparticledist) && 
				r_particles->int_val) {
			at = (byte *) & d_8to24table[(byte) part->color];
			alpha = part->alpha;

#define mVectorCompare(x, y) ((x[0] == y[0]) && (x[1] == y[1]) && (x[2] == y[2]))
			if (mVectorCompare(part->up, part->right)) {
				memcpy(up, o_up, sizeof(up));
				memcpy(right, o_right, sizeof(right));
			} else {
				memcpy(up, part->up, sizeof(up));
				memcpy(right, part->right, sizeof(right));
			}

			if (lighthalf) {
				varray[0].color[0] = (float) ((int) at[0] >> 1) / 255;
				varray[0].color[1] = (float) ((int) at[1] >> 1) / 255;
				varray[0].color[2] = (float) ((int) at[2] >> 1) / 255;
			} else {
				varray[0].color[0] = (float) at[0] / 255;
				varray[0].color[1] = (float) at[1] / 255;
				varray[0].color[2] = (float) at[2] / 255;
			}
			varray[0].color[3] = (float) alpha / 255;

			memcpy(varray[1].color, varray[0].color, sizeof(varray[0].color));
			memcpy(varray[2].color, varray[0].color, sizeof(varray[0].color));
			memcpy(varray[3].color, varray[0].color, sizeof(varray[0].color));

			scale = part->scale;

			up_scale[0] = up[0] * scale;
			up_scale[1] = up[1] * scale;
			up_scale[2] = up[2] * scale;

			right_scale[0] = right[0] * scale;
			right_scale[1] = right[1] * scale;
			right_scale[2] = right[2] * scale;

			up_right_scale[0] = (up[0] + right[0]) * scale;
			up_right_scale[1] = (up[1] + right[1]) * scale;
			up_right_scale[2] = (up[2] + right[2]) * scale;

			varray[0].vertex[0] = part->org[0] + up_right_scale[0];
			varray[0].vertex[1] = part->org[1] + up_right_scale[1];
			varray[0].vertex[2] = part->org[2] + up_right_scale[2];

			varray[1].vertex[0] = part->org[0] - up_scale[0] + right_scale[0];
			varray[1].vertex[1] = part->org[1] - up_scale[1] + right_scale[1];
			varray[1].vertex[2] = part->org[2] - up_scale[2] + right_scale[2];

			varray[2].vertex[0] = part->org[0] - up_right_scale[0];
			varray[2].vertex[1] = part->org[1] - up_right_scale[1];
			varray[2].vertex[2] = part->org[2] - up_right_scale[2];

			varray[3].vertex[0] = part->org[0] + up_scale[0] - right_scale[0];
			varray[3].vertex[1] = part->org[1] + up_scale[1] - right_scale[1];
			varray[3].vertex[2] = part->org[2] + up_scale[2] - right_scale[2];

			glBindTexture (GL_TEXTURE_2D, part->tex);
			glDrawArrays (GL_QUADS, 0, 4);
		}

		for (i = 0; i < 3; i++)
			part->org[i] += part->vel[i] * host_frametime;

		switch (part->type) {
			case pt_static:
				break;
			case pt_blob:
				for (i = 0; i < 3; i++)
					part->vel[i] += part->vel[i] * dvel;
				part->vel[2] -= grav;
				break;
			case pt_blob2:
				for (i = 0; i < 2; i++)
					part->vel[i] -= part->vel[i] * dvel;
				part->vel[2] -= grav;
				break;
			case pt_grav:
				part->vel[2] -= grav;
				break;
			case pt_smoke:
				if ((part->alpha -= host_frametime * 90) < 1)
					part->die = -1;
				part->scale += host_frametime * 6;
//				part->org[2] += host_frametime * 30;
				break;
			case pt_smokering:
				if ((part->alpha -= host_frametime * 130) < 1)
					part->die = -1;
				part->scale += host_frametime * 10;
//				part->org[2] += host_frametime * 30;
				break;
			case pt_smokecloud:
				if ((part->alpha -= host_frametime * 128) < 1)
				{
					part->die = -1;
					break;
				}
				part->scale += host_frametime * 60;
				part->org[2] += host_frametime * 30;
				break;
			case pt_bloodcloud:
				if ((part->alpha -= host_frametime * 64) < 1)
				{
					part->die = -1;
					break;
				}
				part->scale += host_frametime * 4;
				part->vel[2] -= grav;
				break;
			case pt_fadespark:
				if ((part->alpha -= host_frametime * 256) < 1)
					part->die = -1;
				part->vel[2] -= grav;
				break;
			case pt_fadespark2:
				if ((part->alpha -= host_frametime * 512) < 1)
					part->die = -1;
				part->vel[2] -= grav;
				break;
			case pt_fallfadespark:
				if ((part->alpha -= host_frametime * 256) < 1)
					part->die = -1;
				part->vel[2] -= fast_grav;
				break;
			default:
				Con_DPrintf ("unhandled particle type %d\n", part->type);
				break;
		}
		// LordHavoc: immediate removal of unnecessary particles (must
		// be done to ensure compactor below operates properly in all
		// cases)
		if (part->die <= cl.time)
			freeparticles[j++] = part;
	}
	k = 0;
	while (maxparticle >= activeparticles) {
		*freeparticles[k++] = particles[maxparticle--];
		while (maxparticle >= activeparticles && 
				particles[maxparticle].die <= cl.time)
			maxparticle--;
	}
	numparticles = activeparticles;

	glColor3ubv (lighthalf_v);
	glDepthMask (GL_TRUE);
}
