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

#include "QF/console.h"
#include "QF/cvar.h"
#include "QF/render.h"
#include "QF/tga.h"
#include "QF/vfs.h"
#include "QF/vid.h"
#include "QF/GL/defines.h"
#include "QF/GL/funcs.h"
#include "QF/GL/qf_rlight.h"
#include "QF/GL/qf_sky.h"
#include "QF/GL/qf_textures.h"
#include "QF/GL/qf_vid.h"

#include "compat.h"
#include "r_cvar.h"
#include "r_shared.h"
#include "view.h"

char       *suf[6] = { "rt", "bk", "lf", "ft", "up", "dn" };
int         solidskytexture;
int         alphaskytexture;

// Set to true if a valid skybox is loaded --KB
qboolean    skyloaded = false;

extern int      skytexturenum;
extern model_t *loadmodel;


void
R_LoadSkys (const char *skyname)
{
	char        name[64];
	int         i;
	VFile      *f;

	if (strcasecmp (skyname, "none") == 0) {
		skyloaded = false;
		return;
	}

	skyloaded = true;
	for (i = 0; i < 6; i++) {
		byte       *targa_rgba;

		qfglBindTexture (GL_TEXTURE_2D, SKY_TEX + i);
		snprintf (name, sizeof (name), "env/%s%s.tga", skyname, suf[i]);
		COM_FOpenFile (name, &f);
		if (!f) {
			Con_DPrintf ("Couldn't load %s\n", name);
			skyloaded = false;
			continue;
		}
		targa_rgba = LoadTGA (f);

		qfglTexImage2D (GL_TEXTURE_2D, 0, gl_solid_format, 256, 256, 0,
						GL_RGBA, GL_UNSIGNED_BYTE, targa_rgba);

		free (targa_rgba);

		qfglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		qfglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}
	if (!skyloaded)
		Con_Printf ("Unable to load skybox %s, using normal sky\n", skyname);
}

inline void
R_SkyBoxPolyVec (vec5_t v)
{
	// avoid interpolation seams
//	s = s * (254.0/256.0) + (1.0/256.0);
//	t = t * (254.0/256.0) + (1.0/256.0);
	qfglTexCoord2fv (v);
	qfglVertex3f (r_refdef.vieworg[0] + v[2], r_refdef.vieworg[1] + v[3],
				  r_refdef.vieworg[2] + v[4]);
}

#define ftc(x) (x * (254.0/256.0) + (1.0/256.0))
vec5_t      skyvec[6][4] = {
	{
		// right +y
		{ftc (1), ftc (0), 1024, 1024, 1024},
		{ftc (1), ftc (1), 1024, 1024, -1024},
		{ftc (0), ftc (1), -1024, 1024, -1024},
		{ftc (0), ftc (0), -1024, 1024, 1024}
	},
	{
		// back -x
		{ftc (1), ftc (0), -1024, 1024, 1024},
		{ftc (1), ftc (1), -1024, 1024, -1024},
		{ftc (0), ftc (1), -1024, -1024, -1024},
		{ftc (0), ftc (0), -1024, -1024, 1024}
	},
	{
		// left -y
		{ftc (1), ftc (0), -1024, -1024, 1024},
		{ftc (1), ftc (1), -1024, -1024, -1024},
		{ftc (0), ftc (1), 1024, -1024, -1024},
		{ftc (0), ftc (0), 1024, -1024, 1024}
	},
	{
		// front +x
		{ftc (1), ftc (0), 1024, -1024, 1024},
		{ftc (1), ftc (1), 1024, -1024, -1024},
		{ftc (0), ftc (1), 1024, 1024, -1024},
		{ftc (0), ftc (0), 1024, 1024, 1024}
	},
	{
		// up +z
		{ftc (1), ftc (0), 1024, -1024, 1024},
		{ftc (1), ftc (1), 1024, 1024, 1024},
		{ftc (0), ftc (1), -1024, 1024, 1024},
		{ftc (0), ftc (0), -1024, -1024, 1024}
	},
	{
		// down -z
		{ftc (1), ftc (0), 1024, 1024, -1024},
		{ftc (1), ftc (1), 1024, -1024, -1024},
		{ftc (0), ftc (1), -1024, -1024, -1024},
		{ftc (0), ftc (0), -1024, 1024, -1024}
	}
};
#undef ftc

void
R_DrawSkyBox (void)
{
	int         i, j;

	qfglDisable (GL_DEPTH_TEST);
	qfglDepthRange (gldepthmax, gldepthmax);
	for (i = 0; i < 6; i++) {
		qfglBindTexture (GL_TEXTURE_2D, SKY_TEX + i);
		qfglBegin (GL_QUADS);
		for (j = 0; j < 4; j++)
			R_SkyBoxPolyVec (skyvec[i][j]);
		qfglEnd ();
	}

	qfglEnable (GL_DEPTH_TEST);
	qfglDepthRange (gldepthmin, gldepthmax);
}

vec3_t      domescale;

void
R_DrawSkyLayer (float s)
{
	float       x, y, a1x, a1y, a2x, a2y;
	int         a, b;
	vec3_t      v;

	for (a = 0; a < 16; a++) {
		a1x = bubble_costable[a * 2] * domescale[0];
		a1y = -bubble_sintable[a * 2] * domescale[1];
		a2x = bubble_costable[(a + 1) * 2] * domescale[0];
		a2y = -bubble_sintable[(a + 1) * 2] * domescale[1];

		qfglBegin (GL_TRIANGLE_STRIP);
		qfglTexCoord2f (0.5 + s * (1.0 / 128.0), 0.5 + s * (1.0 / 128.0));
		qfglVertex3f (r_refdef.vieworg[0],
					r_refdef.vieworg[1], r_refdef.vieworg[2] + domescale[2]);
		for (b = 1; b < 8; b++) {
			x = bubble_costable[b * 2 + 16];
			y = -bubble_sintable[b * 2 + 16];

			v[0] = a1x * x;
			v[1] = a1y * x;
			v[2] = y * domescale[2];
			qfglTexCoord2f ((v[0] + s) * (1.0 / 128.0),
						  (v[1] + s) * (1.0 / 128.0));
			qfglVertex3f (v[0] + r_refdef.vieworg[0], v[1] +
						  r_refdef.vieworg[1], v[2] + r_refdef.vieworg[2]);

			v[0] = a2x * x;
			v[1] = a2y * x;
			v[2] = y * domescale[2];
			qfglTexCoord2f ((v[0] + s) * (1.0 / 128.0),
						  (v[1] + s) * (1.0 / 128.0));
			qfglVertex3f (v[0] + r_refdef.vieworg[0], v[1] +
						  r_refdef.vieworg[1], v[2] + r_refdef.vieworg[2]);
		}
		qfglTexCoord2f (0.5 + s * (1.0 / 128.0), 0.5 + s * (1.0 / 128.0));
		qfglVertex3f (r_refdef.vieworg[0],
					r_refdef.vieworg[1], r_refdef.vieworg[2] - domescale[2]);
		qfglEnd ();
	}
}

void
R_DrawSkyDome (void)
{
	float       speedscale;					// for top sky and bottom sky

	qfglDisable (GL_DEPTH_TEST);
	qfglDepthRange (gldepthmax, gldepthmax);

	qfglDisable (GL_BLEND);

	// base sky
	qfglBindTexture (GL_TEXTURE_2D, solidskytexture);
	domescale[0] = 512;
	domescale[1] = 512;
	domescale[2] = 128;
	speedscale = r_realtime * 8;
	speedscale -= (int) speedscale & ~127;
	R_DrawSkyLayer (speedscale);
	qfglEnable (GL_BLEND);

	// clouds
	if (gl_skymultipass->int_val) {
		qfglBindTexture (GL_TEXTURE_2D, alphaskytexture);
		domescale[0] = 512;
		domescale[1] = 512;
		domescale[2] = 128;
		speedscale = r_realtime * 16;
		speedscale -= (int) speedscale & ~127;
		R_DrawSkyLayer (speedscale);
	}

	qfglEnable (GL_DEPTH_TEST);
	qfglDepthRange (gldepthmin, gldepthmax);
}

inline void
R_DrawSky (void)
{
	if (skyloaded)
		R_DrawSkyBox ();
	else
		R_DrawSkyDome ();
}

/*
	R_InitSky

	A sky texture is 256*128, with the right side being a masked overlay
*/
void
R_InitSky (texture_t *mt)
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

	if (!solidskytexture)
		solidskytexture = texture_extension_number++;
	qfglBindTexture (GL_TEXTURE_2D, solidskytexture);
	qfglTexImage2D (GL_TEXTURE_2D, 0, gl_solid_format, 128, 128, 0, GL_RGBA,
				  GL_UNSIGNED_BYTE, trans);
	qfglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	qfglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	for (i = 0; i < 128; i++)
		for (j = 0; j < 128; j++) {
			p = src[i * 256 + j];
			if (p == 0)
				trans[(i * 128) + j] = transpix;
			else
				trans[(i * 128) + j] = d_8to24table[p];
		}

	if (!alphaskytexture)
		alphaskytexture = texture_extension_number++;
	qfglBindTexture (GL_TEXTURE_2D, alphaskytexture);
	qfglTexImage2D (GL_TEXTURE_2D, 0, gl_alpha_format, 128, 128, 0, GL_RGBA,
				  GL_UNSIGNED_BYTE, trans);
	qfglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	qfglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}
