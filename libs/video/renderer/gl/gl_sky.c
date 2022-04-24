/*
	gl_sky.c

	sky polygons

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

#include "QF/cvar.h"
#include "QF/image.h"
#include "QF/quakefs.h"
#include "QF/render.h"
#include "QF/sys.h"
#include "QF/va.h"
#include "QF/vid.h"
#include "QF/GL/defines.h"
#include "QF/GL/funcs.h"
#include "QF/GL/qf_rlight.h"
#include "QF/GL/qf_sky.h"
#include "QF/GL/qf_textures.h"
#include "QF/GL/qf_vid.h"

#include "compat.h"
#include "r_internal.h"

// Note that the cube face names are from the perspective of looking at the
// cube from the outside on the -ve y axis with +x to the right, +y going in,
// +z up, and front is the nearest face.
static const char *suf[6] = { "rt", "bk", "lf", "ft", "up", "dn" };
GLuint      gl_solidskytexture;
GLuint      gl_alphaskytexture;

// Set to true if a valid skybox is loaded --KB
qboolean    gl_skyloaded = false;


vec5_t      gl_skyvec[6][4] = {
	{
		// right +x
		{0, 0,  1024,  1024,  1024},
		{0, 1,  1024,  1024, -1024},
		{1, 1,  1024, -1024, -1024},
		{1, 0,  1024, -1024,  1024}
	},
	{
		// back +y
		{0, 0, -1024,  1024,  1024},
		{0, 1, -1024,  1024, -1024},
		{1, 1,  1024,  1024, -1024},
		{1, 0,  1024,  1024,  1024}
	},
	{
		// left -x
		{0, 0, -1024, -1024,  1024},
		{0, 1, -1024, -1024, -1024},
		{1, 1, -1024,  1024, -1024},
		{1, 0, -1024,  1024,  1024}
	},
	{
		// front -y
		{0, 0,  1024, -1024,  1024},
		{0, 1,  1024, -1024, -1024},
		{1, 1, -1024, -1024, -1024},
		{1, 0, -1024, -1024,  1024}
	},
	{
		// up +z
		{0, 0, -1024,  1024,  1024},
		{0, 1,  1024,  1024,  1024},
		{1, 1,  1024, -1024,  1024},
		{1, 0, -1024, -1024,  1024}
	},
	{
		// down -z
		{0, 0,  1024,  1024, -1024},
		{0, 1, -1024,  1024, -1024},
		{1, 1, -1024, -1024, -1024},
		{1, 0,  1024, -1024, -1024}
	}
};

void
gl_R_LoadSkys (const char *skyname)
{
	const char *name;
	int         i;	// j

	if (!skyname || !*skyname)
		skyname = r_skyname;

	if (!*skyname || strcasecmp (skyname, "none") == 0) {
		gl_skyloaded = false;
		return;
	}

	gl_skyloaded = true;
	for (i = 0; i < 6; i++) {
		tex_t	*targa;

		qfglBindTexture (GL_TEXTURE_2D, SKY_TEX + i);

		targa = LoadImage (name = va (0, "env/%s%s", skyname, suf[i]), 1);
		if (!targa || targa->format < 3) {	// FIXME Can't do PCX right now
			Sys_MaskPrintf (SYS_dev, "Couldn't load %s\n", name);
			// also look in gfx/env, where Darkplaces looks for skies
			targa = LoadImage (name = va (0, "gfx/env/%s%s", skyname,
										  suf[i]), 1);
			if (!targa) {
				Sys_MaskPrintf (SYS_dev, "Couldn't load %s\n", name);
				gl_skyloaded = false;
				continue;
			}
		}

		// FIXME need better texture loading
		qfglTexImage2D (GL_TEXTURE_2D, 0, gl_solid_format, targa->width,
						targa->height, 0,
						targa->format == 3 ? GL_RGB : GL_RGBA,
						GL_UNSIGNED_BYTE, &targa->data);

		qfglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		qfglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		if (gl_Anisotropy)
			qfglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT,
							   gl_aniso);
		qfglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		qfglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

#if 0
		for (j = 0; j < 4; j++) {
			// set the texture coords to be 1/2 pixel in from the edge
			if (gl_skyvec[i][j][0] < 0.5)
				gl_skyvec[i][j][0] = 0.5 / targa->width;
			else
				gl_skyvec[i][j][0] = 1 - 0.5 / targa->width;

			if (gl_skyvec[i][j][1] < 0.5)
				gl_skyvec[i][j][1] = 0.5 / targa->height;
			else
				gl_skyvec[i][j][1] = 1 - 0.5 / targa->height;
		}
#endif
	}
	if (!gl_skyloaded)
		Sys_Printf ("Unable to load skybox %s, using normal sky\n", skyname);
}

static void
R_DrawSkyBox (void)
{
	int         i, j;

	for (i = 0; i < 6; i++) {
		qfglBindTexture (GL_TEXTURE_2D, SKY_TEX + i);
		qfglBegin (GL_QUADS);
		for (j = 0; j < 4; j++) {
			float *v = (float *) gl_skyvec[i][j];

			qfglTexCoord2fv (v);
			qfglVertex3f (r_refdef.frame.position[0] + v[2],
						  r_refdef.frame.position[1] + v[3],
						  r_refdef.frame.position[2] + v[4]);
		}
		qfglEnd ();
	}
}

static vec3_t      domescale = {2048.0, 2048.0,  512.0};
static vec3_t      zenith    = {   0.0,    0.0,  512.0};
static vec3_t      nadir     = {   0.0,    0.0, -512.0};

static inline void
skydome_vertex (const vec3_t v, float speedscale)
{
	float       length, s, t;
	vec3_t      dir, point;

	VectorCopy (v, dir);
	dir[2] *= 3.0;

	length = DotProduct (dir, dir);
	length = 2.953125 / sqrt (length);

	dir[0] *= length;
	dir[1] *= length;

	s = speedscale + dir[0];
	t = speedscale + dir[1];

	VectorAdd (r_refdef.frame.position, v, point);

	qfglTexCoord2f (s, t);
	qfglVertex3fv (point);
}

static void
skydome_debug (void)
{
	float       x, y, a1x, a1y, a2x, a2y;
	int         a, b, h, t, i;
	vec3_t      v[3];

	qfglDisable (GL_TEXTURE_2D);
	qfglBegin (GL_LINES);
	for (a = 0; a < 16; a++) {
		a1x = gl_bubble_costable[a * 2] * domescale[0];
		a1y = -gl_bubble_sintable[a * 2] * domescale[1];
		a2x = gl_bubble_costable[(a + 1) * 2] * domescale[0];
		a2y = -gl_bubble_sintable[(a + 1) * 2] * domescale[1];

		h = 1;
		t = 0;
		VectorAdd (zenith, r_refdef.frame.position, v[0]);
		for (b = 1; b <= 8; b++) {
			x = gl_bubble_costable[b + 8];
			y = -gl_bubble_sintable[b + 8];

			v[h][0] = a1x * x;
			v[h][1] = a1y * x;
			v[h][2] = y * domescale[2];
			VectorAdd (v[h], r_refdef.frame.position, v[h]);
			for (i = t; i != h; i = (i + 1) % 3) {
				qfglVertex3fv (v[i]);
				qfglVertex3fv (v[h]);
			}
			h = (h + 1) % 3;
			if (h == t)
				t = (t + 1) % 3;

			v[h][0] = a2x * x;
			v[h][1] = a2y * x;
			v[h][2] = y * domescale[2];
			VectorAdd (v[h], r_refdef.frame.position, v[h]);
			for (i = t; i != h; i = (i + 1) % 3) {
				qfglVertex3fv (v[i]);
				qfglVertex3fv (v[h]);
			}
			h = (h + 1) % 3;
			if (h == t)
				t = (t + 1) % 3;
		}

		h = 1;
		t = 0;
		VectorAdd (nadir, r_refdef.frame.position, v[0]);
		for (b = 15; b >= 8; b--) {
			x = gl_bubble_costable[b + 8];
			y = -gl_bubble_sintable[b + 8];

			v[h][0] = a2x * x;
			v[h][1] = a2y * x;
			v[h][2] = y * domescale[2];
			VectorAdd (v[h], r_refdef.frame.position, v[h]);
			for (i = t; i != h; i = (i + 1) % 3) {
				qfglVertex3fv (v[i]);
				qfglVertex3fv (v[h]);
			}
			h = (h + 1) % 3;
			if (h == t)
				t = (t + 1) % 3;

			v[h][0] = a1x * x;
			v[h][1] = a1y * x;
			v[h][2] = y * domescale[2];
			VectorAdd (v[h], r_refdef.frame.position, v[h]);
			for (i = t; i != h; i = (i + 1) % 3) {
				qfglVertex3fv (v[i]);
				qfglVertex3fv (v[h]);
			}
			h = (h + 1) % 3;
			if (h == t)
				t = (t + 1) % 3;
		}
	}
	qfglEnd ();
	qfglEnable (GL_TEXTURE_2D);
}

static void
R_DrawSkyLayer (float speedscale)
{
	float       x, y, a1x, a1y, a2x, a2y;
	int         a, b;
	vec3_t      v;

	for (a = 0; a < 16; a++) {
		a1x = gl_bubble_costable[a * 2] * domescale[0];
		a1y = -gl_bubble_sintable[a * 2] * domescale[1];
		a2x = gl_bubble_costable[(a + 1) * 2] * domescale[0];
		a2y = -gl_bubble_sintable[(a + 1) * 2] * domescale[1];

		qfglBegin (GL_TRIANGLE_STRIP);
		skydome_vertex (zenith, speedscale);
		for (b = 1; b <= 8; b++) {
			x = gl_bubble_costable[b + 8];
			y = -gl_bubble_sintable[b + 8];

			v[0] = a1x * x;
			v[1] = a1y * x;
			v[2] = y * domescale[2];
			skydome_vertex (v, speedscale);

			v[0] = a2x * x;
			v[1] = a2y * x;
			v[2] = y * domescale[2];
			skydome_vertex (v, speedscale);
		}
		qfglEnd ();

		qfglBegin (GL_TRIANGLE_STRIP);
		skydome_vertex (nadir, speedscale);
		for (b = 15; b >= 8; b--) {
			x = gl_bubble_costable[b + 8];
			y = -gl_bubble_sintable[b + 8];

			v[0] = a2x * x;
			v[1] = a2y * x;
			v[2] = y * domescale[2];
			skydome_vertex (v, speedscale);

			v[0] = a1x * x;
			v[1] = a1y * x;
			v[2] = y * domescale[2];
			skydome_vertex (v, speedscale);
		}
		qfglEnd ();
	}
}

static void
R_DrawSkyDome (void)
{
	float       speedscale;						// for top sky and bottom sky

	// base sky
	qfglDisable (GL_BLEND);
	qfglBindTexture (GL_TEXTURE_2D, gl_solidskytexture);
	speedscale = vr_data.realtime / 16.0;
	speedscale -= floor (speedscale);
	R_DrawSkyLayer (speedscale);
	qfglEnable (GL_BLEND);

	// clouds
	if (gl_sky_multipass) {
		qfglBindTexture (GL_TEXTURE_2D, gl_alphaskytexture);
		speedscale = vr_data.realtime / 8.0;
		speedscale -= floor (speedscale);
		R_DrawSkyLayer (speedscale);
	}
	if (gl_sky_debug) {
		skydome_debug ();
	}
}

void
gl_R_DrawSky (void)
{
	qfglDisable (GL_DEPTH_TEST);
	qfglDepthMask (GL_FALSE);
	if (gl_skyloaded)
		R_DrawSkyBox ();
	else
		R_DrawSkyDome ();
	qfglDepthMask (GL_TRUE);
	qfglEnable (GL_DEPTH_TEST);
}

/*
	R_InitSky

	A sky texture is 256*128, with the right side being a masked overlay
*/
void
gl_R_InitSky (texture_t *mt)
{
	byte         *src;
	int           i, j, p, r, g, b;
	unsigned int  transpix;
	unsigned int  trans[128 * 128];
	unsigned int *rgba;

	src = (byte *) mt + mt->offsets[0];

	// make an average value for the back to avoid a fringe on the top level

	r = g = b = 0;
	for (i = 0; i < 128; i++)
		for (j = 0; j < 128; j++) {
			p = src[i * 256 + j + 128];
			rgba = &d_8to24table[p];
			trans[(i * 128) + j] = *rgba;
			r += ((byte *) rgba)[0];
			g += ((byte *) rgba)[1];
			b += ((byte *) rgba)[2];
		}

	((byte *) & transpix)[0] = r / (128 * 128);
	((byte *) & transpix)[1] = g / (128 * 128);
	((byte *) & transpix)[2] = b / (128 * 128);
	((byte *) & transpix)[3] = 0;

	if (!gl_solidskytexture) {
		qfglGenTextures (1, &gl_solidskytexture);
	}
	qfglBindTexture (GL_TEXTURE_2D, gl_solidskytexture);
	qfglTexImage2D (GL_TEXTURE_2D, 0, gl_solid_format, 128, 128, 0, GL_RGBA,
					GL_UNSIGNED_BYTE, trans);
	qfglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	qfglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	if (gl_Anisotropy)
        qfglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT,
                           gl_aniso);

	for (i = 0; i < 128; i++)
		for (j = 0; j < 128; j++) {
			p = src[i * 256 + j];
			if (p == 0)
				trans[(i * 128) + j] = transpix;
			else
				trans[(i * 128) + j] = d_8to24table[p];
		}

	if (!gl_alphaskytexture) {
		qfglGenTextures (1, &gl_alphaskytexture);
	}
	qfglBindTexture (GL_TEXTURE_2D, gl_alphaskytexture);
	qfglTexImage2D (GL_TEXTURE_2D, 0, gl_alpha_format, 128, 128, 0, GL_RGBA,
					GL_UNSIGNED_BYTE, trans);
	qfglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	qfglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	if (gl_Anisotropy)
        qfglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT,
                           gl_aniso);
}
