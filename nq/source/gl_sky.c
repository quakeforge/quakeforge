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

#include "QF/compat.h"
#include "QF/console.h"
#include "QF/quakefs.h"
#include "QF/tga.h"
#include "QF/vid.h"

#include "glquake.h"
#include "QF/render.h"
#include "view.h"

extern double realtime;
extern model_t *loadmodel;

extern int  skytexturenum;
extern qboolean lighthalf;

int         solidskytexture;
int         alphaskytexture;
float       speedscale;					// for top sky and bottom sky

// Set to true if a valid skybox is loaded --KB
qboolean    skyloaded = false;


char       *suf[6] = { "rt", "bk", "lf", "ft", "up", "dn" };
void
R_LoadSkys (const char *skyname)
{
	int         i;
	QFile      *f;
	char        name[64];

	if (strcasecmp (skyname, "none") == 0) {
		skyloaded = false;
		return;
	}

	skyloaded = true;
	for (i = 0; i < 6; i++) {
		byte       *targa_rgba;

		glBindTexture (GL_TEXTURE_2D, SKY_TEX + i);
		snprintf (name, sizeof (name), "env/%s%s.tga", skyname, suf[i]);
		COM_FOpenFile (name, &f);
		if (!f) {
			Con_DPrintf ("Couldn't load %s\n", name);
			skyloaded = false;
			continue;
		}
		targa_rgba = LoadTGA (f);

		glTexImage2D (GL_TEXTURE_2D, 0, gl_solid_format, 256, 256, 0, GL_RGBA,
					  GL_UNSIGNED_BYTE, targa_rgba);

		free (targa_rgba);

		glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}
	if (!skyloaded)
		Con_Printf ("Unable to load skybox %s, using normal sky\n", skyname);
}


void
R_SkyBoxPolyVec (vec5_t v)
{
	// avoid interpolation seams
//  s = s * (254.0/256.0) + (1.0/256.0);
//  t = t * (254.0/256.0) + (1.0/256.0);
	glTexCoord2fv (v);
	glVertex3f (r_refdef.vieworg[0] + v[2],
				r_refdef.vieworg[1] + v[3], r_refdef.vieworg[2] + v[4]);
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

	glDisable (GL_DEPTH_TEST);
	glDepthRange (gldepthmax, gldepthmax);
	for (i = 0; i < 6; i++) {
		glBindTexture (GL_TEXTURE_2D, SKY_TEX + i);
		glBegin (GL_QUADS);
		for (j = 0; j < 4; j++)
			R_SkyBoxPolyVec (skyvec[i][j]);
		glEnd ();
	}

	glEnable (GL_DEPTH_TEST);
	glDepthRange (gldepthmin, gldepthmax);
}


vec3_t      domescale;

void
R_DrawSkyLayer (float s)
{
	int         a, b;
	float       x, y, a1x, a1y, a2x, a2y;
	vec3_t      v;

	for (a = 0; a < 16; a++) {
		a1x = bubble_costable[a * 2] * domescale[0];
		a1y = -bubble_sintable[a * 2] * domescale[1];
		a2x = bubble_costable[(a + 1) * 2] * domescale[0];
		a2y = -bubble_sintable[(a + 1) * 2] * domescale[1];

		glBegin (GL_TRIANGLE_STRIP);
		glTexCoord2f (0.5 + s * (1.0 / 128.0), 0.5 + s * (1.0 / 128.0));
		glVertex3f (r_refdef.vieworg[0],
					r_refdef.vieworg[1], r_refdef.vieworg[2] + domescale[2]);
		for (b = 1; b < 8; b++) {
			x = bubble_costable[b * 2 + 16];
			y = -bubble_sintable[b * 2 + 16];

			v[0] = a1x * x;
			v[1] = a1y * x;
			v[2] = y * domescale[2];
			glTexCoord2f ((v[0] + s) * (1.0 / 128.0),
						  (v[1] + s) * (1.0 / 128.0));
			glVertex3f (v[0] + r_refdef.vieworg[0],
						v[1] + r_refdef.vieworg[1], v[2] + r_refdef.vieworg[2]);

			v[0] = a2x * x;
			v[1] = a2y * x;
			v[2] = y * domescale[2];
			glTexCoord2f ((v[0] + s) * (1.0 / 128.0),
						  (v[1] + s) * (1.0 / 128.0));
			glVertex3f (v[0] + r_refdef.vieworg[0],
						v[1] + r_refdef.vieworg[1], v[2] + r_refdef.vieworg[2]);
		}
		glTexCoord2f (0.5 + s * (1.0 / 128.0), 0.5 + s * (1.0 / 128.0));
		glVertex3f (r_refdef.vieworg[0],
					r_refdef.vieworg[1], r_refdef.vieworg[2] - domescale[2]);
		glEnd ();
	}
}


void
R_DrawSkyDome (void)
{
	glDisable (GL_DEPTH_TEST);
	glDepthRange (gldepthmax, gldepthmax);

	glDisable (GL_BLEND);

	// base sky
	glBindTexture (GL_TEXTURE_2D, solidskytexture);
	domescale[0] = 512;
	domescale[1] = 512;
	domescale[2] = 128;
	speedscale = realtime * 8;
	speedscale -= (int) speedscale & ~127;
	R_DrawSkyLayer (speedscale);
	glEnable (GL_BLEND);

	// clouds
	if (gl_skymultipass->int_val) {
		glBindTexture (GL_TEXTURE_2D, alphaskytexture);
		domescale[0] = 512;
		domescale[1] = 512;
		domescale[2] = 128;
		speedscale = realtime * 16;
		speedscale -= (int) speedscale & ~127;
		R_DrawSkyLayer (speedscale);
	}

	glEnable (GL_DEPTH_TEST);
	glDepthRange (gldepthmin, gldepthmax);
}


void
R_DrawSky (void)
{
	float l = 1 / (256 * brightness->value);

	glColor3f (lighthalf_v[0] * l, lighthalf_v[1] * l, lighthalf_v[2] * l);

	if (skyloaded)
		R_DrawSkyBox ();
	else
		R_DrawSkyDome ();

	glColor3ubv (lighthalf_v);
}


/*
	R_InitSky

	A sky texture is 256*128, with the right side being a masked overlay
*/
void
R_InitSky (texture_t *mt)
{
	int         i, j, p;
	byte       *src;
	unsigned int trans[128 * 128];
	unsigned int transpix;
	int         r, g, b;
	unsigned int *rgba;

	src = (byte *) mt + mt->offsets[0];

	// make an average value for the back to avoid
	// a fringe on the top level

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
	glBindTexture (GL_TEXTURE_2D, solidskytexture);
	glTexImage2D (GL_TEXTURE_2D, 0, gl_solid_format, 128, 128, 0, GL_RGBA,
				  GL_UNSIGNED_BYTE, trans);
	glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);


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
	glBindTexture (GL_TEXTURE_2D, alphaskytexture);
	glTexImage2D (GL_TEXTURE_2D, 0, gl_alpha_format, 128, 128, 0, GL_RGBA,
				  GL_UNSIGNED_BYTE, trans);
	glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}
