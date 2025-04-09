/*
	gl_warp.c

	water polygons

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

#include "QF/cvar.h"
#include "QF/sys.h"

#include "QF/GL/defines.h"
#include "QF/GL/funcs.h"
#include "QF/GL/qf_rsurf.h"

#include "r_internal.h"
#include "vid_gl.h"

// speed up sin calculations - Ed
static float turbsin[] = {
#	include "gl_warp_sin.h"
};

#define TURBSCALE (256.0 / (2 * M_PI))
#define TURBFRAC (32.0 / (2 * M_PI))		// an 8th of TURBSCALE

/*
	GL_EmitWaterPolys

	Does a water warp on the pre-fragmented glpoly_t chain
*/
void
GL_EmitWaterPolys (msurface_t *surf)
{
	float		os, ot, s, t, timetemp;

	timetemp = vr_data.realtime * TURBSCALE;

	for (auto p = surf->polys; p; p = p->next) {
		qfglBegin (GL_POLYGON);
		auto v = p->verts;
		for (int i = 0; i < p->numverts; i++, v++) {
			os = turbsin[(int) (v->tex_uv[0] * TURBFRAC + timetemp) & 255];
			ot = turbsin[(int) (v->tex_uv[1] * TURBFRAC + timetemp) & 255];
			s = (v->tex_uv[0] + ot) * (1.0 / 64.0);
			t = (v->tex_uv[1] + os) * (1.0 / 64.0);
			qfglTexCoord2f (s, t);

			if (r_waterripple != 0) {
				vec3_t		nv;

				VectorCopy (v->pos, nv);
				nv[2] += r_waterripple * os * ot * (1.0 / 64.0);
				qfglVertex3fv (nv);
			} else
				qfglVertex3fv (v->pos);
		}
		qfglEnd ();
	}
}

const float S = 0.15625;
const float F = 2.5;
const float A = 0.01;

static void
warp_uv (float *uv, float time)
{
	float       p1 = (time * S + F * uv[1]) * 2 * M_PI;
	float       p2 = (time * S + F * uv[0]) * 2 * M_PI;
	uv[0] = uv[0] * (1 - 2 * A) + A * (1 + sin (p1));
	uv[1] = uv[1] * (1 - 2 * A) + A * (1 + sin (p2));
}

void
gl_WarpScreen (framebuffer_t *fb)
{
	gl_framebuffer_t *buffer = fb->buffer;

	qfglMatrixMode (GL_PROJECTION);
	qfglLoadIdentity ();
	qfglOrtho (0, r_refdef.vrect.width, r_refdef.vrect.height, 0, -9999, 9999);
	qfglMatrixMode (GL_MODELVIEW);
	qfglLoadIdentity ();
	qfglColor3ubv (color_white);

	qfglDisable (GL_DEPTH_TEST);
	qfglFrontFace (GL_CCW);

	qfglBindTexture (GL_TEXTURE_2D, buffer->color);

	int         base_x = r_refdef.vrect.x;
	int         base_y = r_refdef.vrect.y;
	int         width = r_refdef.vrect.width;
	int         height = r_refdef.vrect.height;

	float       time = vr_data.realtime;

	for (int y = 0; y < height; y += 40) {
		qfglBegin (GL_QUAD_STRIP);
		for (int x = 0; x <= width; x+= 40) {
			int         y2 = y + 40;
			int         xy1[] = { base_x + x, base_y + y };
			int         xy2[] = { base_x + x, base_y + y2};
			float       uv1[] = { (float) x / width, 1 - (float) y / height };
			float       uv2[] = { (float) x / width, 1 - (float) y2 / height };
			warp_uv (uv1, time);
			warp_uv (uv2, time);
			qfglTexCoord2fv (uv2);
			qfglVertex2iv (xy2);
			qfglTexCoord2fv (uv1);
			qfglVertex2iv (xy1);
		}
		qfglEnd ();
	}
}
