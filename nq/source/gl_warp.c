/*
	gl_warp.c

	sky and water polygons

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

#include <string.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

#include "QF/qtypes.h"
#include "QF/console.h"
#include "model.h"
#include "QF/quakefs.h"
#include "glquake.h"
#include "QF/sys.h"

extern double realtime;
extern model_t *loadmodel;

extern int  skytexturenum;
extern qboolean lighthalf;

int         solidskytexture;
int         alphaskytexture;
float       speedscale;					// for top sky and bottom sky

// Set to true if a valid skybox is loaded --KB
qboolean    skyloaded = false;

msurface_t *warpface;

extern cvar_t *gl_subdivide_size;

void
BoundPoly (int numverts, float *verts, vec3_t mins, vec3_t maxs)
{
	int         i, j;
	float      *v;

	mins[0] = mins[1] = mins[2] = 9999;
	maxs[0] = maxs[1] = maxs[2] = -9999;
	v = verts;
	for (i = 0; i < numverts; i++)
		for (j = 0; j < 3; j++, v++) {
			if (*v < mins[j])
				mins[j] = *v;
			if (*v > maxs[j])
				maxs[j] = *v;
		}
}

void
SubdividePolygon (int numverts, float *verts)
{
	int         i, j, k;
	vec3_t      mins, maxs;
	float       m;
	float      *v;
	vec3_t      front[64], back[64];
	int         f, b;
	float       dist[64];
	float       frac;
	glpoly_t   *poly;
	float       s, t;

	if (numverts > 60)
		Sys_Error ("numverts = %i", numverts);

	BoundPoly (numverts, verts, mins, maxs);

	for (i = 0; i < 3; i++) {
		m = (mins[i] + maxs[i]) * 0.5;
		m =
			gl_subdivide_size->value * floor (m / gl_subdivide_size->value +
											  0.5);
		if (maxs[i] - m < 8)
			continue;
		if (m - mins[i] < 8)
			continue;

		// cut it
		v = verts + i;
		for (j = 0; j < numverts; j++, v += 3)
			dist[j] = *v - m;

		// wrap cases
		dist[j] = dist[0];
		v -= i;
		VectorCopy (verts, v);

		f = b = 0;
		v = verts;
		for (j = 0; j < numverts; j++, v += 3) {
			if (dist[j] >= 0) {
				VectorCopy (v, front[f]);
				f++;
			}
			if (dist[j] <= 0) {
				VectorCopy (v, back[b]);
				b++;
			}
			if (dist[j] == 0 || dist[j + 1] == 0)
				continue;
			if ((dist[j] > 0) != (dist[j + 1] > 0)) {
				// clip point
				frac = dist[j] / (dist[j] - dist[j + 1]);
				for (k = 0; k < 3; k++)
					front[f][k] = back[b][k] = v[k] + frac * (v[3 + k] - v[k]);
				f++;
				b++;
			}
		}

		SubdividePolygon (f, front[0]);
		SubdividePolygon (b, back[0]);
		return;
	}

	poly =

		Hunk_Alloc (sizeof (glpoly_t) +
					(numverts - 4) * VERTEXSIZE * sizeof (float));
	poly->next = warpface->polys;
	warpface->polys = poly;
	poly->numverts = numverts;
	for (i = 0; i < numverts; i++, verts += 3) {
		VectorCopy (verts, poly->verts[i]);
		s = DotProduct (verts, warpface->texinfo->vecs[0]);
		t = DotProduct (verts, warpface->texinfo->vecs[1]);
		poly->verts[i][3] = s;
		poly->verts[i][4] = t;
	}
}

/*
================
GL_SubdivideSurface

Breaks a polygon up along axial 64 unit
boundaries so that turbulent and sky warps
can be done reasonably.
================
*/
void
GL_SubdivideSurface (msurface_t *fa)
{
	vec3_t      verts[64];
	int         numverts;
	int         i;
	int         lindex;
	float      *vec;

	warpface = fa;

	// 
	// convert edges back to a normal polygon
	// 
	numverts = 0;
	for (i = 0; i < fa->numedges; i++) {
		lindex = loadmodel->surfedges[fa->firstedge + i];

		if (lindex > 0)
			vec = loadmodel->vertexes[loadmodel->edges[lindex].v[0]].position;
		else
			vec = loadmodel->vertexes[loadmodel->edges[-lindex].v[1]].position;
		VectorCopy (vec, verts[numverts]);
		numverts++;
	}

	SubdividePolygon (numverts, verts[0]);
}

//=========================================================



// speed up sin calculations - Ed
float       turbsin[] = {
#	include "gl_warp_sin.h"
};

#define TURBSCALE (256.0 / (2 * M_PI))

/*
=============
EmitWaterPolys

Does a water warp on the pre-fragmented glpoly_t chain
=============
*/
void
EmitWaterPolys (msurface_t *fa)
{
	glpoly_t   *p;
	float      *v;
	int         i;
	float       s, t, os, ot;
	vec3_t      nv;

	for (p = fa->polys; p; p = p->next) {
		glBegin (GL_POLYGON);
		for (i = 0, v = p->verts[0]; i < p->numverts; i++, v += VERTEXSIZE) {
			os = v[3];
			ot = v[4];

			s = os + turbsin[(int) ((ot * 0.125 + realtime) * TURBSCALE) & 255];
			s *= (1.0 / 64);

			t = ot + turbsin[(int) ((os * 0.125 + realtime) * TURBSCALE) & 255];
			t *= (1.0 / 64);

			glTexCoord2f (s, t);

			VectorCopy (v, nv);
			nv[2] += r_waterripple->value
				* turbsin[(int) ((v[3] * 0.125 + realtime) * TURBSCALE) & 255]
				* turbsin[(int) ((v[4] * 0.125 + realtime) * TURBSCALE) & 255]
				* (1.0 / 64.0);

			glVertex3fv (nv);
		}
		glEnd ();
	}
}


/*
=================================================================

  Quake 2 environment sky

=================================================================
*/

#define	SKY_TEX		2000

/*
=========================================================

TARGA LOADING

=========================================================
*/

typedef struct _TargaHeader {
	unsigned char id_length, colormap_type, image_type;
	unsigned short colormap_index, colormap_length;
	unsigned char colormap_size;
	unsigned short x_origin, y_origin, width, height;
	unsigned char pixel_size, attributes;
} TargaHeader;


TargaHeader targa_header;
byte       *targa_rgba;

int
fgetLittleShort (QFile *f)
{
	byte        b1, b2;

	b1 = Qgetc (f);
	b2 = Qgetc (f);

	return (short) (b1 + b2 * 256);
}

int
fgetLittleLong (QFile *f)
{
	byte        b1, b2, b3, b4;

	b1 = Qgetc (f);
	b2 = Qgetc (f);
	b3 = Qgetc (f);
	b4 = Qgetc (f);

	return b1 + (b2 << 8) + (b3 << 16) + (b4 << 24);
}


/*
=============
LoadTGA
=============
*/
void
LoadTGA (QFile *fin)
{
	int         columns, rows, numPixels;
	byte       *pixbuf;
	int         row, column;
	unsigned char red = 0, green = 0, blue = 0, alphabyte = 0;

	targa_header.id_length = Qgetc (fin);
	targa_header.colormap_type = Qgetc (fin);
	targa_header.image_type = Qgetc (fin);

	targa_header.colormap_index = fgetLittleShort (fin);
	targa_header.colormap_length = fgetLittleShort (fin);
	targa_header.colormap_size = Qgetc (fin);
	targa_header.x_origin = fgetLittleShort (fin);
	targa_header.y_origin = fgetLittleShort (fin);
	targa_header.width = fgetLittleShort (fin);
	targa_header.height = fgetLittleShort (fin);
	targa_header.pixel_size = Qgetc (fin);
	targa_header.attributes = Qgetc (fin);

	if (targa_header.image_type != 2 && targa_header.image_type != 10)
		Sys_Error ("LoadTGA: Only type 2 and 10 targa RGB images supported\n");

	if (targa_header.colormap_type != 0
		|| (targa_header.pixel_size != 32 && targa_header.pixel_size != 24))
		Sys_Error
			("Texture_LoadTGA: Only 32 or 24 bit images supported (no colormaps)\n");

	columns = targa_header.width;
	rows = targa_header.height;
	numPixels = columns * rows;

	targa_rgba = malloc (numPixels * 4);

	if (targa_header.id_length != 0)
		Qseek (fin, targa_header.id_length, SEEK_CUR);	// skip TARGA image
	// comment

	if (targa_header.image_type == 2) {	// Uncompressed, RGB images
		for (row = rows - 1; row >= 0; row--) {
			pixbuf = targa_rgba + row * columns * 4;
			for (column = 0; column < columns; column++) {
				switch (targa_header.pixel_size) {
					case 24:

					blue = Qgetc (fin);
					green = Qgetc (fin);
					red = Qgetc (fin);
					*pixbuf++ = red;
					*pixbuf++ = green;
					*pixbuf++ = blue;
					*pixbuf++ = 255;
					break;
					case 32:
					blue = Qgetc (fin);
					green = Qgetc (fin);
					red = Qgetc (fin);
					alphabyte = Qgetc (fin);
					*pixbuf++ = red;
					*pixbuf++ = green;
					*pixbuf++ = blue;
					*pixbuf++ = alphabyte;
					break;
				}
			}
		}
	} else if (targa_header.image_type == 10) {	// Runlength encoded RGB
		// images
		unsigned char packetHeader, packetSize, j;

		for (row = rows - 1; row >= 0; row--) {
			pixbuf = targa_rgba + row * columns * 4;
			for (column = 0; column < columns;) {
				packetHeader = Qgetc (fin);
				packetSize = 1 + (packetHeader & 0x7f);
				if (packetHeader & 0x80) {	// run-length packet
					switch (targa_header.pixel_size) {
						case 24:
						blue = Qgetc (fin);
						green = Qgetc (fin);
						red = Qgetc (fin);
						alphabyte = 255;
						break;
						case 32:
						blue = Qgetc (fin);
						green = Qgetc (fin);
						red = Qgetc (fin);
						alphabyte = Qgetc (fin);
						break;
					}

					for (j = 0; j < packetSize; j++) {
						*pixbuf++ = red;
						*pixbuf++ = green;
						*pixbuf++ = blue;
						*pixbuf++ = alphabyte;
						column++;
						if (column == columns) {	// run spans across rows
							column = 0;
							if (row > 0)
								row--;
							else
								goto breakOut;
							pixbuf = targa_rgba + row * columns * 4;
						}
					}
				} else {				// non run-length packet
					for (j = 0; j < packetSize; j++) {
						switch (targa_header.pixel_size) {
							case 24:
							blue = Qgetc (fin);
							green = Qgetc (fin);
							red = Qgetc (fin);
							*pixbuf++ = red;
							*pixbuf++ = green;
							*pixbuf++ = blue;
							*pixbuf++ = 255;
							break;
							case 32:
							blue = Qgetc (fin);
							green = Qgetc (fin);
							red = Qgetc (fin);
							alphabyte = Qgetc (fin);
							*pixbuf++ = red;
							*pixbuf++ = green;
							*pixbuf++ = blue;
							*pixbuf++ = alphabyte;
							break;
						}
						column++;
						if (column == columns) {	// pixel packet run spans 
													// 
							// 
							// across rows
							column = 0;
							if (row > 0)
								row--;
							else
								goto breakOut;
							pixbuf = targa_rgba + row * columns * 4;
						}
					}
				}
			}
		  breakOut:;
		}
	}

	Qclose (fin);
}

/*
==================
R_LoadSkys
==================
*/
char       *suf[6] = { "rt", "bk", "lf", "ft", "up", "dn" };
void
R_LoadSkys (char *skyname)
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
		glBindTexture (GL_TEXTURE_2D, SKY_TEX + i);
		snprintf (name, sizeof (name), "env/%s%s.tga", skyname, suf[i]);
		COM_FOpenFile (name, &f);
		if (!f) {
			Con_DPrintf ("Couldn't load %s\n", name);
			skyloaded = false;
			continue;
		}
		LoadTGA (f);

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
	 // right
	 {ftc (1), ftc (0), 1024, 1024, 1024},
	 {ftc (1), ftc (1), 1024, 1024, -1024},
	 {ftc (0), ftc (1), -1024, 1024, -1024},
	 {ftc (0), ftc (0), -1024, 1024, 1024}
	 },
	{
	 // back
	 {ftc (1), ftc (0), -1024, 1024, 1024},
	 {ftc (1), ftc (1), -1024, 1024, -1024},
	 {ftc (0), ftc (1), -1024, -1024, -1024},
	 {ftc (0), ftc (0), -1024, -1024, 1024}
	 },
	{
	 // left
	 {ftc (1), ftc (0), -1024, -1024, 1024},
	 {ftc (1), ftc (1), -1024, -1024, -1024},
	 {ftc (0), ftc (1), 1024, -1024, -1024},
	 {ftc (0), ftc (0), 1024, -1024, 1024}
	 },
	{
	 // front
	 {ftc (1), ftc (0), 1024, -1024, 1024},
	 {ftc (1), ftc (1), 1024, -1024, -1024},
	 {ftc (0), ftc (1), 1024, 1024, -1024},
	 {ftc (0), ftc (0), 1024, 1024, 1024}
	 },
	{
	 // up
	 {ftc (1), ftc (0), 1024, -1024, 1024},
	 {ftc (1), ftc (1), 1024, 1024, 1024},
	 {ftc (0), ftc (1), -1024, 1024, 1024},
	 {ftc (0), ftc (0), -1024, -1024, 1024}
	 },
	{
	 // down
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

	glEnable (GL_DEPTH_TEST);
	glDepthFunc (GL_ALWAYS);
//  glDisable (GL_BLEND);
//  glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glDepthRange (gldepthmax, gldepthmax);
	if (lighthalf)
		glColor3f (0.5, 0.5, 0.5);
	else
		glColor3f (1, 1, 1);

	for (i = 0; i < 6; i++) {
		glBindTexture (GL_TEXTURE_2D, SKY_TEX + i);
		glBegin (GL_QUADS);
		for (j = 0; j < 4; j++)
			R_SkyBoxPolyVec (skyvec[i][j]);
		glEnd ();
	}

	glColor3f (1, 1, 1);
	glDepthFunc (GL_LEQUAL);
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
		a1x = bubble_costable[a * 2];
		a1y = -bubble_sintable[a * 2];
		a2x = bubble_costable[(a + 1) * 2];
		a2y = -bubble_sintable[(a + 1) * 2];

		glBegin (GL_TRIANGLE_STRIP);
		glTexCoord2f (0.5 + s * (1.0 / 128.0), 0.5 + s * (1.0 / 128.0));
		glVertex3f (r_refdef.vieworg[0],
					r_refdef.vieworg[1], r_refdef.vieworg[2] + domescale[2]);
		for (b = 1; b < 8; b++) {
			x = bubble_costable[b * 2 + 16];
			y = -bubble_sintable[b * 2 + 16];

			v[0] = a1x * x * domescale[0];
			v[1] = a1y * x * domescale[1];
			v[2] = y * domescale[2];
			glTexCoord2f ((v[0] + s) * (1.0 / 128.0),
						  (v[1] + s) * (1.0 / 128.0));
			glVertex3f (v[0] + r_refdef.vieworg[0],
						v[1] + r_refdef.vieworg[1], v[2] + r_refdef.vieworg[2]);

			v[0] = a2x * x * domescale[0];
			v[1] = a2y * x * domescale[1];
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
	glEnable (GL_DEPTH_TEST);
	glDepthFunc (GL_ALWAYS);
//  glDisable (GL_BLEND);
//  glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDepthRange (gldepthmax, gldepthmax);
	glDisable (GL_BLEND);
	if (lighthalf)
		glColor3f (0.5, 0.5, 0.5);
	else
		glColor3f (1, 1, 1);

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
//  glDisable (GL_BLEND);
	glColor3f (1, 1, 1);
	glDepthFunc (GL_LEQUAL);
	glEnable (GL_DEPTH_TEST);
	glDepthRange (gldepthmin, gldepthmax);
}

void
R_DrawSky (void)
{
	if (skyloaded)
		R_DrawSkyBox ();
	else
		R_DrawSkyDome ();
}



//===============================================================

/*
=============
R_InitSky

A sky texture is 256*128, with the right side being a masked overlay
==============
*/
void
R_InitSky (texture_t *mt)
{
	int         i, j, p;
	byte       *src;
	unsigned    trans[128 * 128];
	unsigned    transpix;
	int         r, g, b;
	unsigned   *rgba;

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

/*
=============
EmitSkyPolys
=============
*/
void
EmitSkyPolys (msurface_t *fa)
{
	glpoly_t   *p;
	float      *v;
	int         i;
	float       s, t;
	vec3_t      dir;
	float       length;

	for (p = fa->polys; p; p = p->next) {
		glBegin (GL_POLYGON);
		for (i = 0, v = p->verts[0]; i < p->numverts; i++, v += VERTEXSIZE) {
			VectorSubtract (v, r_origin, dir);
			dir[2] *= 3;				// flatten the sphere

			length = dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2];
			length = sqrt (length);
			length = 6 * 63 / length;

			dir[0] *= length;
			dir[1] *= length;

			s = (speedscale + dir[0]) * (1.0 / 128);
			t = (speedscale + dir[1]) * (1.0 / 128);

			glTexCoord2f (s, t);
			glVertex3fv (v);
		}
		glEnd ();
	}
}

/*
=================
R_DrawSkyChain
=================
*/
void
R_DrawSkyChain (msurface_t *s)
{
	msurface_t *fa;

	// used when gl_texsort is on
	glBindTexture (GL_TEXTURE_2D, solidskytexture);
	speedscale = realtime * 8;
	speedscale -= (int) speedscale & ~127;

	for (fa = s; fa; fa = fa->texturechain)
		EmitSkyPolys (fa);

	glEnable (GL_BLEND);
	glBindTexture (GL_TEXTURE_2D, alphaskytexture);
	speedscale = realtime * 16;
	speedscale -= (int) speedscale & ~127;

	for (fa = s; fa; fa = fa->texturechain)
		EmitSkyPolys (fa);

	glDisable (GL_BLEND);
}

/*
===============
EmitBothSkyLayers

Does a sky warp on the pre-fragmented glpoly_t chain
This will be called for brushmodels, the world
will have them chained together.
===============
*/
void
EmitBothSkyLayers (msurface_t *fa)
{
	glBindTexture (GL_TEXTURE_2D, solidskytexture);
	speedscale = realtime * 8;
	speedscale -= (int) speedscale & ~127;

	EmitSkyPolys (fa);

	glEnable (GL_BLEND);
	glBindTexture (GL_TEXTURE_2D, alphaskytexture);
	speedscale = realtime * 16;
	speedscale -= (int) speedscale & ~127;

	EmitSkyPolys (fa);

	glDisable (GL_BLEND);
}
