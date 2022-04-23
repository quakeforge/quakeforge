/*
	gl_model_brush.c

	gl support routines for model loading and caching

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
// models are the only shared resource between a client and server running
// on the same machine.

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
#include "QF/dstring.h"
#include "QF/image.h"
#include "QF/qendian.h"
#include "QF/quakefs.h"
#include "QF/sys.h"
#include "QF/va.h"
#include "QF/GL/qf_rsurf.h"
#include "QF/GL/qf_textures.h"

#include "compat.h"
#include "mod_internal.h"
#include "r_internal.h"

static gltex_t gl_notexture = { };

static tex_t *
Mod_LoadAnExternalTexture (const char *tname, const char *mname)
{
	char		rname[32];
	tex_t	   *image;

	memcpy (rname, tname, strlen (tname) + 1);

	if (rname[0] == '*') rname[0] = '#';

	image = LoadImage (va (0, "textures/%.*s/%s", (int) strlen (mname + 5) - 4,
						   mname + 5, rname), 1);
	if (!image)
		image = LoadImage (va (0, "maps/%.*s/%s", (int) strlen (mname + 5) - 4,
							   mname + 5, rname), 1);
//	if (!image)
//			image = LoadImage (va (0, "textures/bmodels/%s", rname));
	if (!image)
			image = LoadImage (va (0, "textures/%s", rname), 1);
	if (!image)
			image = LoadImage (va (0, "maps/%s", rname), 1);

	return image;
}

static int
Mod_LoadExternalTextures (model_t *mod, texture_t *tx)
{
	tex_t	   *base, *luma;
	gltex_t    *gltx;
	int         external = 0;

	gltx = tx->render;
	if ((base = Mod_LoadAnExternalTexture (tx->name, mod->path))) {
		external = 1;
		gltx->gl_texturenum =
			GL_LoadTexture (tx->name, base->width, base->height,
							base->data, true, false,
							base->format > 2 ? base->format : 1);

		luma = Mod_LoadAnExternalTexture (va (0, "%s_luma", tx->name),
										  mod->path);
		if (!luma)
			luma = Mod_LoadAnExternalTexture (va (0, "%s_glow", tx->name),
											  mod->path);

		gltx->gl_fb_texturenum = 0;

		if (luma) {
			gltx->gl_fb_texturenum =
				GL_LoadTexture (va (0, "fb_%s", tx->name), luma->width,
								luma->height, luma->data, true, true,
								luma->format > 2 ? luma->format : 1);
		} else if (base->format < 3) {
			gltx->gl_fb_texturenum =
				Mod_Fullbright (base->data, base->width, base->height,
								va (0, "fb_%s", tx->name));
		}
	}
	return external;
}

void
gl_Mod_ProcessTexture (model_t *mod, texture_t *tx)
{
	const char *name;

	if (!tx) {
		r_notexture_mip->render = &gl_notexture;
		return;
	}
	if (gl_textures_external) {
		if (Mod_LoadExternalTextures (mod, tx)) {
			return;
		}
	}
	if (strncmp (tx->name, "sky", 3) == 0) {
		return;
	}
	gltex_t    *gltex = tx->render;
	name = va (0, "fb_%s", tx->name);
	gltex->gl_fb_texturenum =
		Mod_Fullbright ((byte *) (tx + 1), tx->width, tx->height, name);
	gltex->gl_texturenum =
		GL_LoadTexture (tx->name, tx->width, tx->height, (byte *) (tx + 1),
						true, false, 1);
}

void
gl_Mod_LoadLighting (model_t *mod, bsp_t *bsp)
{
	byte        d;
	byte       *in, *out, *data;
	dstring_t  *litfilename = dstring_new ();
	size_t      i;
	int         ver;
	QFile      *lit_file;
	mod_brush_t *brush = &mod->brush;

	dstring_copystr (litfilename, mod->path);
	brush->lightdata = NULL;
	if (mod_lightmap_bytes > 1) {
		// LordHavoc: check for a .lit file to load
		QFS_StripExtension (litfilename->str, litfilename->str);
		dstring_appendstr (litfilename, ".lit");
		lit_file = QFS_VOpenFile (litfilename->str, 0, mod->vpath);
		data = (byte *) QFS_LoadHunkFile (lit_file);
		if (data) {
			if (data[0] == 'Q' && data[1] == 'L' && data[2] == 'I'
				&& data[3] == 'T') {
				ver = LittleLong (((int32_t *) data)[1]);
				if (ver == 1) {
					Sys_MaskPrintf (SYS_dev, "%s loaded", litfilename->str);
					brush->lightdata = data + 8;
					return;
				} else
					Sys_MaskPrintf (SYS_dev,
									"Unknown .lit file version (%d)\n", ver);
			} else
				Sys_MaskPrintf (SYS_dev, "Corrupt .lit file (old version?)\n");
		}
	}
	// LordHavoc: oh well, expand the white lighting data
	if (!bsp->lightdatasize) {
		dstring_delete (litfilename);
		return;
	}
	brush->lightdata = Hunk_AllocName (0,
									   bsp->lightdatasize * mod_lightmap_bytes,
									   litfilename->str);
	in = bsp->lightdata;
	out = brush->lightdata;

	if (mod_lightmap_bytes > 1)
		for (i = 0; i < bsp->lightdatasize ; i++) {
			d = vid.gammatable[*in++];
			*out++ = d;
			*out++ = d;
			*out++ = d;
		}
	else
		for (i = 0; i < bsp->lightdatasize ; i++)
			*out++ = vid.gammatable[*in++];
	dstring_delete (litfilename);
}

msurface_t *warpface;


static void
BoundPoly (int numverts, float *verts, vec3_t mins, vec3_t maxs)
{
	float      *v;
	int         i, j;

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

static void
SubdividePolygon (int numverts, float *verts)
{
	float       frac, m, s, t;
	float       dist[64];
	float      *v;
	int         b, f, i, j, k;
	glpoly_t   *poly;
	vec3_t      mins, maxs;
	vec3_t      front[64], back[64];

	if (numverts < 3 || numverts > 60)
		Sys_Error ("numverts = %i", numverts);

	BoundPoly (numverts, verts, mins, maxs);

	for (i = 0; i < 3; i++) {
		m = (mins[i] + maxs[i]) * 0.5;
		m = gl_subdivide_size * floor (m / gl_subdivide_size + 0.5);
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

	poly = Hunk_Alloc (0, sizeof (glpoly_t) + (numverts - 4) * VERTEXSIZE *
					   sizeof (float));
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
	Mod_SubdivideSurface

	Breaks a polygon up along axial 64 unit
	boundaries so that turbulent and sky warps
	can be done reasonably.
*/
void
gl_Mod_SubdivideSurface (model_t *mod, msurface_t *fa)
{
	float      *vec;
	int         lindex, numverts, i;
	vec3_t      verts[64];
	mod_brush_t *brush = &mod->brush;

	warpface = fa;

	// convert edges back to a normal polygon
	numverts = 0;
	for (i = 0; i < fa->numedges; i++) {
		lindex = brush->surfedges[fa->firstedge + i];

		if (lindex > 0)
			vec = brush->vertexes[brush->edges[lindex].v[0]].position;
		else
			vec = brush->vertexes[brush->edges[-lindex].v[1]].position;
		VectorCopy (vec, verts[numverts]);
		numverts++;
	}

	if (numverts > 3) {
		SubdividePolygon (numverts, verts[0]);
	}
}
