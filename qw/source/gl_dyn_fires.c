/*
	r_part.c

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
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include <stdlib.h>

#include "cl_main.h"
#include "QF/cmd.h"
#include "QF/console.h"
#include "glquake.h"

#define MAX_FIRES				128		// rocket flames

static fire_t r_fires[MAX_FIRES];
extern cvar_t *gl_fires;
extern cvar_t *r_firecolor;

/*
	R_AddFire

	Nifty ball of fire GL effect.  Kinda a meshing of the dlight and
	particle engine code.
*/

void
R_AddFire (vec3_t start, vec3_t end, entity_t *ent)
{
	float       len;
	fire_t     *f;
	vec3_t      vec;
	int         key;

	if (!gl_fires->int_val)
		return;

	VectorSubtract (end, start, vec);
	len = VectorNormalize (vec);
	key = ent->keynum;

	if (len) {
		f = R_AllocFire (key);
		VectorCopy (end, f->origin);
		VectorCopy (start, f->owner);
		f->size = 10;
		f->die = cl.time + 0.5;
		f->decay = 1;
		VectorCopy (r_firecolor->vec, f->color);
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
		AddLightBlend (f->color[0], f->color[1], f->color[2], f->size * 0.0003);	// we are
		return;
	}
	// we're not - draw it
	glBegin (GL_TRIANGLE_FAN);
	if (gl_lightmode->int_val)
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
	glColor3ubv (white_v);
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

	glDepthMask (GL_FALSE);
	glDisable (GL_TEXTURE_2D);
	glBlendFunc (GL_ONE, GL_ONE);

	f = r_fires;
	for (i = 0; i < MAX_FIRES; i++, f++) {
		if (f->die < cl.time || !f->size)
			continue;
		f->size += f->decay;
		R_DrawFire (f);
	}

	glEnable (GL_TEXTURE_2D);
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDepthMask (GL_TRUE);
}
