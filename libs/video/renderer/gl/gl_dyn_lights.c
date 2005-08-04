/*
	gl_dyn_lights.c

	polyblended dynamic lights

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

#include <math.h>
#include <stdio.h>

#include "QF/cvar.h"
#include "QF/render.h"
#include "QF/GL/defines.h"
#include "QF/GL/funcs.h"
#include "QF/GL/qf_rlight.h"

#include "r_cvar.h"
#include "r_dynamic.h"
#include "r_local.h"
#include "r_shared.h"

float       bubble_sintable[33], bubble_costable[33];


void
R_InitBubble (void)
{
	float       a;
	float      *bub_sin, *bub_cos;
	int         i;

	bub_sin = bubble_sintable;
	bub_cos = bubble_costable;

	for (i = 32; i >= 0; i--) {
		a = i * (M_PI / 16.0);
		*bub_sin++ = sin (a);
		*bub_cos++ = cos (a);
	}
}

static void
R_RenderDlight (dlight_t *light)
{
	float       rad;
	float      *bub_sin, *bub_cos;
	int         i, j;
	vec3_t      v;

	bub_sin = bubble_sintable;
	bub_cos = bubble_costable;
	rad = light->radius * 0.35;

	VectorSubtract (light->origin, r_origin, v);
	if (VectorLength (v) < rad)				// view is inside the dlight
		return;

	qfglBegin (GL_TRIANGLE_FAN);

	qfglColor4fv (light->color);

	VectorSubtract (r_origin, light->origin, v);
	VectorNormalize (v);

	for (i = 0; i < 3; i++)
		v[i] = light->origin[i] + v[i] * rad;

	qfglVertex3fv (v);
	qfglColor3ubv (color_black);

	for (i = 16; i >= 0; i--) {
		for (j = 0; j < 3; j++)
			v[j] = light->origin[j] + (vright[j] * (*bub_cos) +
						   vup[j] * (*bub_sin)) * rad;
		bub_sin += 2;
		bub_cos += 2;
		qfglVertex3fv (v);
	}

	qfglEnd ();
}

void
R_RenderDlights (void)
{
	unsigned int i;
	dlight_t   *l;

	if (!gl_dlight_polyblend->int_val)
		return;

	qfglDepthMask (GL_FALSE);
	qfglDisable (GL_TEXTURE_2D);
	qfglBlendFunc (GL_ONE, GL_ONE);
	qfglShadeModel (GL_SMOOTH);

	l = r_dlights;
	for (i = 0; i < r_maxdlights; i++, l++) {
		if (l->die < r_realtime || !l->radius)
			continue;
		R_RenderDlight (l);
	}

	if (!gl_dlight_smooth->int_val)
		qfglShadeModel (GL_FLAT);
	qfglColor3ubv (color_white);
	qfglEnable (GL_TEXTURE_2D);
	qfglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	qfglDepthMask (GL_TRUE);
}
