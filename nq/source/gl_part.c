
/*
	gl_part.c

	OpenGL particle rendering

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

#include "r_shared.h"
#include "r_local.h"
#include "glquake.h"
#include "server.h"
#include "QF/console.h"
#include "QF/cmd.h"

#define MAX_FIRES	128					// rocket flames
fire_t      r_fires[MAX_FIRES];

/*
===============
R_DrawParticles
===============
*/

void
R_DrawParticles (void)
{
	particle_t *p, *kill;
	float       grav;
	int         i;
	float       time2, time3;
	float       time1;
	float       dvel;
	float       frametime;

	vec3_t      up, right;
	float       scale;

	unsigned char *at;
	unsigned char theAlpha;
	qboolean    alphaTestEnabled;
	qboolean    blendEnabled;

	glBindTexture (GL_TEXTURE_2D, particletexture);
	// LordHavoc: particles should not affect zbuffer
	glDepthMask (0);

	if (!(blendEnabled = glIsEnabled (GL_BLEND))) {
		glEnable (GL_BLEND);
	}
	if ((alphaTestEnabled = glIsEnabled (GL_ALPHA_TEST))) {
		glDisable (GL_ALPHA_TEST);
	}
	glBegin (GL_TRIANGLES);

	VectorScale (vup, 1.5, up);
	VectorScale (vright, 1.5, right);

	frametime = cl.time - cl.oldtime;
	time3 = frametime * 15;
	time2 = frametime * 10;				// 15;
	time1 = frametime * 5;
	grav = frametime * sv_gravity->value * 0.05;
	dvel = frametime * 4;

	for (;;) {
		kill = active_particles;
		if (kill && kill->die < cl.time) {
			active_particles = kill->next;
			kill->next = free_particles;
			free_particles = kill;
			continue;
		}
		break;
	}

	for (p = active_particles; p; p = p->next) {
		for (;;) {
			kill = p->next;
			if (kill && kill->die < cl.time) {
				p->next = kill->next;
				kill->next = free_particles;
				free_particles = kill;
				continue;
			}
			break;
		}

		// hack a scale up to keep particles from disapearing
		scale =
			(p->org[0] - r_origin[0]) * vpn[0] + (p->org[1] -
												  r_origin[1]) * vpn[1] +
			(p->org[2] - r_origin[2]) * vpn[2];
		if (scale < 20)
			scale = 1;
		else
			scale = 1 + scale * 0.004;
		at = (byte *) & d_8to24table[(int) p->color];
		if (p->type == pt_fire)
			theAlpha = 255 * (6 - p->ramp) / 6;
		else
			theAlpha = 255;
		if (lighthalf)
			glColor4ub ((byte) ((int) at[0] >> 1), (byte) ((int) at[1] >> 1),
						(byte) ((int) at[2] >> 1), theAlpha);
		else
			glColor4ub (at[0], at[1], at[2], theAlpha);
		glTexCoord2f (0, 0);
		glVertex3fv (p->org);
		glTexCoord2f (1, 0);
		glVertex3f (p->org[0] + up[0] * scale, p->org[1] + up[1] * scale,
					p->org[2] + up[2] * scale);
		glTexCoord2f (0, 1);
		glVertex3f (p->org[0] + right[0] * scale, p->org[1] + right[1] * scale,
					p->org[2] + right[2] * scale);

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

			case pt_grav:
#ifdef QUAKE2
			p->vel[2] -= grav * 20;
			break;
#endif
			case pt_slowgrav:
			p->vel[2] -= grav;
			break;
		}
	}

	glEnd ();
	if (alphaTestEnabled) {
		glEnable (GL_ALPHA_TEST);
	}
	if (!blendEnabled) {
		glDisable (GL_BLEND);
	}
	glDepthMask (1);
}

/*
	R_AddFire

	Nifty ball of fire GL effect.  Kinda a meshing of the dlight and
	particle engine code.
*/
float       r_firecolor_flame[3] = { 0.9, 0.7, 0.3 };
float       r_firecolor_light[3] = { 0.9, 0.7, 0.3 };

void
R_AddFire (vec3_t start, vec3_t end, entity_t *ent)
{
	float       len;
	fire_t     *f;
	dlight_t   *dl;
	vec3_t      vec;
	int         key;

	if (!gl_fires->int_val)
		return;

	VectorSubtract (end, start, vec);
	len = VectorNormalize (vec);
	key = ent - cl_entities + 1;

	if (len) {
		f = R_AllocFire (key);
		VectorCopy (end, f->origin);
		VectorCopy (start, f->owner);
		f->size = 10;
		f->die = cl.time + 0.5;
		f->decay = 1;
		VectorCopy (r_firecolor_flame, f->color);

		dl = CL_AllocDlight (key);
		VectorCopy (end, dl->origin);
		dl->radius = 200;
		dl->die = cl.time + 0.5;
		VectorCopy (r_firecolor_light, dl->color);
	}
}

/*
	R_AllocFire

	Clears out and returns a new fireball
*/
fire_t     *
R_AllocFire (int key)
{
	int         i;
	fire_t     *f;

	if (key)							// first try to find/reuse a keyed
		// spot
	{
		f = r_fires;
		for (i = 0; i < MAX_FIRES; i++, f++)
			if (f->key == key) {
				memset (f, 0, sizeof (*f));
				f->key = key;
				return f;
			}
	}

	f = r_fires;						// no match, look for a free spot
	for (i = 0; i < MAX_FIRES; i++, f++) {
		if (f->die < cl.time) {
			memset (f, 0, sizeof (*f));
			f->key = key;
			return f;
		}
	}

	f = &r_fires[0];
	memset (f, 0, sizeof (*f));
	f->key = key;
	return f;
}

/*
	R_DrawFire

	draws one fireball - probably never need to call this directly
*/
void
R_DrawFire (fire_t *f)
{
	int         i, j;
	vec3_t      vec, vec2;
	float       radius;
	float      *b_sin, *b_cos;

	b_sin = bubble_sintable;
	b_cos = bubble_costable;

	radius = f->size - 0.5;

	// figure out if we're inside the area of effect
	VectorSubtract (f->origin, r_origin, vec);
	if (Length (vec) < radius) {
		AddLightBlend (1, 0.5, 0, f->size * 0.0003);	// we are
		return;
	}
	// we're not - draw it
	glBegin (GL_TRIANGLE_FAN);
	if (lighthalf)
		glColor3f (f->color[0] * 0.5, f->color[1] * 0.5, f->color[2] * 0.5);
	else
		glColor3fv (f->color);
	for (i = 0; i < 3; i++)
		vec[i] = f->origin[i] - vpn[i] * radius;
	glVertex3fv (vec);
	glColor3f (0.0, 0.0, 0.0);

	// don't panic, this just draws a bubble...
	for (i = 16; i >= 0; i--) {
		for (j = 0; j < 3; j++) {
			vec[j] = f->origin[j] + (*b_cos * vright[j]
									 + vup[j] * (*b_sin)) * radius;
			vec2[j] = f->owner[j] + (*b_cos * vright[j]
									 + vup[j] * (*b_sin)) * radius;
		}
		glVertex3fv (vec);
		glVertex3fv (vec2);

		b_sin += 2;
		b_cos += 2;
	}
	glEnd ();
}

/*
	R_UpdateFires

	Draws each fireball in sequence
*/
void
R_UpdateFires (void)
{
	int         i;
	fire_t     *f;

	if (!gl_fires->int_val)
		return;

	glDepthMask (0);
	glDisable (GL_TEXTURE_2D);
	glShadeModel (GL_SMOOTH);
	glEnable (GL_BLEND);
	glBlendFunc (GL_ONE, GL_ONE);

	f = r_fires;
	for (i = 0; i < MAX_FIRES; i++, f++) {
		if (f->die < cl.time || !f->size)
			continue;
		f->size += f->decay;
		R_DrawFire (f);
	}

	glColor3f (1.0, 1.0, 1.0);
	glDisable (GL_BLEND);
	glEnable (GL_TEXTURE_2D);
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDepthMask (1);
}

void
R_FireColor_f (void)
{
	int         i;

	if (Cmd_Argc () == 1) {
		Con_Printf ("r_firecolor %f %f %f\n",
					r_firecolor_flame[0],
					r_firecolor_flame[1], r_firecolor_flame[2]);
		return;
	}
	if (Cmd_Argc () == 5 || Cmd_Argc () == 6) {
		Con_Printf
			("Warning: obsolete 4th and 5th parameters to r_firecolor ignored\n");
	} else if (Cmd_Argc () != 4) {
		Con_Printf ("Usage r_firecolor R G B\n");
		return;
	}
	for (i = 0; i < 4; i++) {
		r_firecolor_flame[i] = atof (Cmd_Argv (i + 1));
		r_firecolor_light[i] = r_firecolor_flame[i];
	}
}
