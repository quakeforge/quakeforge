/*
	gl_dyn_fires.c

	dynamic fires

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
#include "QF/console.h"
#include "QF/cvar.h"
#include "QF/render.h"
#include "QF/GL/funcs.h"
#include "QF/GL/defines.h"
#include "QF/GL/qf_rlight.h"

#include "r_cvar.h"
#include "r_dynamic.h"
#include "r_shared.h"


/*
	R_DrawFire

	draws one fireball - probably never need to call this directly
*/
void
R_DrawFire (fire_t *f)
{
	float       radius;
	float      *b_sin, *b_cos;
	int         i, j;
	vec3_t      vec, vec2;

	b_sin = bubble_sintable;
	b_cos = bubble_costable;

	radius = f->size - 0.5;

	// figure out if we're inside the area of effect
	VectorSubtract (f->origin, r_origin, vec);
	if (Length (vec) < radius) {
		AddLightBlend (f->color[0], f->color[1], f->color[2],
					   f->size * 0.0003);	// we are
		return;
	}
	// we're not - draw it
	qfglBegin (GL_TRIANGLE_FAN);
	qfglColor3fv (f->color);
	for (i = 0; i < 3; i++)
		vec[i] = f->origin[i] - vpn[i] * radius;
	qfglVertex3fv (vec);
	qfglColor3ubv (color_black);

	// don't panic, this just draws a bubble...
	for (i = 16; i >= 0; i--) {
		for (j = 0; j < 3; j++) {
			vec[j] = f->origin[j] + (*b_cos * vright[j]
									 + vup[j] * (*b_sin)) * radius;
			vec2[j] = f->owner[j] + (*b_cos * vright[j]
									 + vup[j] * (*b_sin)) * radius;
		}
		qfglVertex3fv (vec);
		qfglVertex3fv (vec2);

		b_sin += 2;
		b_cos += 2;
	}
	qfglEnd ();
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

	qfglDepthMask (GL_FALSE);
	qfglDisable (GL_TEXTURE_2D);
	qfglBlendFunc (GL_ONE, GL_ONE);

	f = r_fires;
	for (i = 0; i < MAX_FIRES; i++, f++) {
		if (f->die < r_realtime || !f->size)
			continue;
		f->size += f->decay;
		R_DrawFire (f);
	}

	qfglColor3ubv (color_white);
	qfglEnable (GL_TEXTURE_2D);
	qfglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	qfglDepthMask (GL_TRUE);
}
