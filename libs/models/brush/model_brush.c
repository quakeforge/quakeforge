/*
	model_brush.c

	model loading and caching

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

#include <math.h>

#include "QF/checksum.h"
#include "QF/cvar.h"
#include "QF/dstring.h"
#include "QF/model.h"
#include "QF/qendian.h"
#include "QF/quakefs.h"
#include "QF/render.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "QF/plugin/vid_render.h"

#include "compat.h"
#include "mod_internal.h"

static byte mod_novis[MAP_PVS_BYTES];

VISIBLE cvar_t		*gl_sky_divide; //FIXME visibility?
VISIBLE int   mod_lightmap_bytes = 1;	//FIXME should this be visible?

VISIBLE mleaf_t *
Mod_PointInLeaf (const vec3_t p, model_t *model)
{
	float       d;
	mnode_t    *node;
	plane_t    *plane;

	if (!model || !model->brush.nodes)
		Sys_Error ("Mod_PointInLeaf: bad model");

	node = model->brush.nodes;
	while (1) {
		if (node->contents < 0)
			return (mleaf_t *) node;
		plane = node->plane;
		d = DotProduct (p, plane->normal) - plane->dist;
		if (d >= 0)
			node = node->children[0];
		else
			node = node->children[1];
	}

	return NULL;						// never reached
}

static inline void
Mod_DecompressVis_set (const byte *in, const mod_brush_t *brush, byte defvis,
					   byte *out)
{
	byte       *start = out;
	int			row, c;

	row = (brush->numleafs + 7) >> 3;

	if (!in) {							// no vis info, so make all visible
		while (row) {
			*out++ = defvis;
			row--;
		}
		return;
	}

	do {
		if (*in) {
			*out++ = *in++;
			continue;
		}

		c = in[1];
		in += 2;
		while (c) {
			*out++ = 0;
			c--;
		}
	} while (out - start < row);
}

static inline void
Mod_DecompressVis_mix (const byte *in, const mod_brush_t *brush, byte defvis,
					   byte *out)
{
	byte       *start = out;
	int			row, c;

	row = (brush->numleafs + 7) >> 3;

	if (!in) {							// no vis info, so make all visible
		while (row) {
			*out++ |= defvis;
			row--;
		}
		return;
	}

	do {
		if (*in) {
			*out++ |= *in++;
			continue;
		}

		c = in[1];
		in += 2;
		out += c;
	} while (out - start < row);
}

VISIBLE byte *
Mod_LeafPVS (const mleaf_t *leaf, const model_t *model)
{
	static byte decompressed[MAP_PVS_BYTES];
	if (leaf == model->brush.leafs) {
		if (!mod_novis[0]) {
			memset (mod_novis, 0xff, sizeof (mod_novis));
		}
		return mod_novis;
	}
	Mod_DecompressVis_set (leaf->compressed_vis, &model->brush, 0xff,
						   decompressed);
	return decompressed;
}

VISIBLE void
Mod_LeafPVS_set (const mleaf_t *leaf, const model_t *model, byte defvis,
				 byte *out)
{
	if (leaf == model->brush.leafs) {
		memset (out, defvis, sizeof (mod_novis));
		return;
	}
	Mod_DecompressVis_set (leaf->compressed_vis, &model->brush, defvis, out);
}

VISIBLE void
Mod_LeafPVS_mix (const mleaf_t *leaf, const model_t *model, byte defvis,
				 byte *out)
{
	if (leaf == model->brush.leafs) {
		for (int i = MAP_PVS_BYTES; i-- > 0; ) {
			*out++ |= defvis;
		}
		return;
	}
	Mod_DecompressVis_mix (leaf->compressed_vis, &model->brush, defvis, out);
}

// BRUSHMODEL LOADING =========================================================

//FIXME SLOW! However, it doesn't seem to be a big issue. Leave alone?
static void
mod_unique_miptex_name (texture_t **textures, texture_t *tx, int ind)
{
	char       *name;
	int         num = 0, i;
	dstring_t  *tag = 0;

	name = tx->name;
	do {
		for (i = 0; i < ind; i++)
			if (textures[i] && !strcmp (textures[i]->name, tx->name))
				break;
		if (i == ind)
			break;
		if (!tag) {
			tag = dstring_new ();
		}
		dsprintf (tag, "%s~%x", name, num++);
		tx->name = tag->str;
	} while (1);

	if (tag) {
		tx->name = dstring_freeze (tag);
		free(name);
	}
}

static void
Mod_LoadTextures (model_t *mod, bsp_t *bsp)
{
	dmiptexlump_t  *m;
	int				i, j, pixels, num, max, altmax;
	miptex_t	   *mt;
	texture_t	   *tx, *tx2;
	texture_t	   *anims[10], *altanims[10];
	mod_brush_t    *brush = &mod->brush;

	if (!bsp->texdatasize) {
		brush->textures = NULL;
		return;
	}
	m = (dmiptexlump_t *) bsp->texdata;

	brush->numtextures = m->nummiptex;
	brush->textures = Hunk_AllocName (m->nummiptex * sizeof (*brush->textures),
									  mod->name);

	for (i = 0; i < m->nummiptex; i++) {
		if (m->dataofs[i] == -1)
			continue;
		mt = (miptex_t *) ((byte *) m + m->dataofs[i]);
		mt->width = LittleLong (mt->width);
		mt->height = LittleLong (mt->height);
		for (j = 0; j < MIPLEVELS; j++)
			mt->offsets[j] = LittleLong (mt->offsets[j]);

		if ((mt->width & 15) || (mt->height & 15))
			Sys_Error ("Texture %s is not 16 aligned", mt->name);
		pixels = mt->width * mt->height / 64 * 85;
		tx = Hunk_AllocName (sizeof (texture_t) + pixels, mod->name);

		brush->textures[i] = tx;

		tx->name = strndup(mt->name, sizeof (mt->name));
		mod_unique_miptex_name (brush->textures, tx, i);
		tx->width = mt->width;
		tx->height = mt->height;
		for (j = 0; j < MIPLEVELS; j++)
			tx->offsets[j] =
				mt->offsets[j] + sizeof (texture_t) - sizeof (miptex_t);
		// the pixels immediately follow the structures
		memcpy (tx + 1, mt + 1, pixels);

		if (!strncmp (mt->name, "sky", 3))
			brush->skytexture = tx;
	}
	if (mod_funcs && mod_funcs->Mod_ProcessTexture) {
		size_t      render_size = mod_funcs->texture_render_size;
		byte       *render_data = 0;
		if (render_size) {
			render_data = Hunk_AllocName (m->nummiptex * render_size,
										  mod->name);
		}
		for (i = 0; i < m->nummiptex; i++) {
			if (!(tx = brush->textures[i])) {
				continue;
			}
			tx->render = render_data;
			render_data += render_size;
			mod_funcs->Mod_ProcessTexture (mod, tx);
		}
		// signal the end of the textures
		mod_funcs->Mod_ProcessTexture (mod, 0);
	}

	// sequence the animations
	for (i = 0; i < m->nummiptex; i++) {
		tx = brush->textures[i];
		if (!tx || tx->name[0] != '+')
			continue;
		if (tx->anim_next)
			continue;					// already sequenced

		// find the number of frames in the animation
		memset (anims, 0, sizeof (anims));
		memset (altanims, 0, sizeof (altanims));

		max = tx->name[1];
		if (max >= 'a' && max <= 'z')
			max -= 'a' - 'A';
		if (max >= '0' && max <= '9') {
			max -= '0';
			altmax = 0;
			anims[max] = tx;
			max++;
		} else if (max >= 'A' && max <= 'J') {
			altmax = max - 'A';
			max = 0;
			altanims[altmax] = tx;
			altmax++;
		} else
			Sys_Error ("Bad animating texture %s", tx->name);

		for (j = i + 1; j < m->nummiptex; j++) {
			tx2 = brush->textures[j];
			if (!tx2 || tx2->name[0] != '+')
				continue;
			if (strcmp (tx2->name + 2, tx->name + 2))
				continue;

			num = tx2->name[1];
			if (num >= 'a' && num <= 'z')
				num -= 'a' - 'A';
			if (num >= '0' && num <= '9') {
				num -= '0';
				anims[num] = tx2;
				if (num + 1 > max)
					max = num + 1;
			} else if (num >= 'A' && num <= 'J') {
				num = num - 'A';
				altanims[num] = tx2;
				if (num + 1 > altmax)
					altmax = num + 1;
			} else
				Sys_Error ("Bad animating texture %s", tx->name);
		}

#define	ANIM_CYCLE	2
		// link them all together
		for (j = 0; j < max; j++) {
			tx2 = anims[j];
			if (!tx2)
				Sys_Error ("Missing frame %i of %s", j, tx->name);
			tx2->anim_total = max * ANIM_CYCLE;
			tx2->anim_min = j * ANIM_CYCLE;
			tx2->anim_max = (j + 1) * ANIM_CYCLE;
			tx2->anim_next = anims[(j + 1) % max];
			if (altmax)
				tx2->alternate_anims = altanims[0];
		}
		for (j = 0; j < altmax; j++) {
			tx2 = altanims[j];
			if (!tx2)
				Sys_Error ("Missing frame %i of %s", j, tx->name);
			tx2->anim_total = altmax * ANIM_CYCLE;
			tx2->anim_min = j * ANIM_CYCLE;
			tx2->anim_max = (j + 1) * ANIM_CYCLE;
			tx2->anim_next = altanims[(j + 1) % altmax];
			if (max)
				tx2->alternate_anims = anims[0];
		}
	}
}

static void
Mod_LoadVisibility (model_t *mod, bsp_t *bsp)
{
	if (!bsp->visdatasize) {
		mod->brush.visdata = NULL;
		return;
	}
	mod->brush.visdata = Hunk_AllocName (bsp->visdatasize, mod->name);
	memcpy (mod->brush.visdata, bsp->visdata, bsp->visdatasize);
}

static void
Mod_LoadEntities (model_t *mod, bsp_t *bsp)
{
	if (!bsp->entdatasize) {
		mod->brush.entities = NULL;
		return;
	}
	mod->brush.entities = Hunk_AllocName (bsp->entdatasize, mod->name);
	memcpy (mod->brush.entities, bsp->entdata, bsp->entdatasize);
}

static void
Mod_LoadVertexes (model_t *mod, bsp_t *bsp)
{
	dvertex_t  *in;
	int         count, i;
	mvertex_t  *out;

	in = bsp->vertexes;
	count = bsp->numvertexes;
	out = Hunk_AllocName (count * sizeof (*out), mod->name);

	mod->brush.vertexes = out;
	mod->brush.numvertexes = count;

	for (i = 0; i < count; i++, in++, out++)
		VectorCopy (in->point, out->position);
}

static void
Mod_LoadSubmodels (model_t *mod, bsp_t *bsp)
{
	dmodel_t   *in, *out;
	int         count, i, j;
	mod_brush_t *brush = &mod->brush;

	in = bsp->models;
	count = bsp->nummodels;
	out = Hunk_AllocName (count * sizeof (*out), mod->name);

	brush->submodels = out;
	brush->numsubmodels = count;

	for (i = 0; i < count; i++, in++, out++) {
		static vec3_t offset = {1, 1, 1};
		// spread the mins / maxs by a pixel
		VectorSubtract (in->mins, offset, out->mins);
		VectorAdd (in->maxs, offset, out->maxs);
		VectorCopy (in->origin, out->origin);
		for (j = 0; j < MAX_MAP_HULLS; j++)
			out->headnode[j] = in->headnode[j];
		out->visleafs = in->visleafs;
		out->firstface = in->firstface;
		out->numfaces = in->numfaces;
	}

	out = brush->submodels;

	if (out->visleafs > MAX_MAP_LEAFS) {
		Sys_Error ("Mod_LoadSubmodels: too many visleafs (%d, max = %d) in %s",
				   out->visleafs, MAX_MAP_LEAFS, mod->path);
	}

	if (out->visleafs > 8192)
		Sys_MaskPrintf (SYS_warn,
						"%i visleafs exceeds standard limit of 8192.\n",
						out->visleafs);
}

static void
Mod_LoadEdges (model_t *mod, bsp_t *bsp)
{
	dedge_t    *in;
	int         count, i;
	medge_t    *out;

	in = bsp->edges;
	count = bsp->numedges;
	out = Hunk_AllocName ((count + 1) * sizeof (*out), mod->name);

	mod->brush.edges = out;
	mod->brush.numedges = count;

	for (i = 0; i < count; i++, in++, out++) {
		out->v[0] = in->v[0];
		out->v[1] = in->v[1];
	}
}

static void
Mod_LoadTexinfo (model_t *mod, bsp_t *bsp)
{
	float       len1, len2;
	int         count, miptex, i, j;
	mtexinfo_t *out;
	texinfo_t  *in;

	in = bsp->texinfo;
	count = bsp->numtexinfo;
	out = Hunk_AllocName (count * sizeof (*out), mod->name);

	mod->brush.texinfo = out;
	mod->brush.numtexinfo = count;

	for (i = 0; i < count; i++, in++, out++) {
		for (j = 0; j < 4; j++) {
			out->vecs[0][j] = in->vecs[0][j];
			out->vecs[1][j] = in->vecs[1][j];
		}
		len1 = VectorLength (out->vecs[0]);
		len2 = VectorLength (out->vecs[1]);

		len1 = (len1 + len2) / 2;
		if (len1 < 0.32)
			out->mipadjust = 4;
		else if (len1 < 0.49)
			out->mipadjust = 3;
		else if (len1 < 0.99)
			out->mipadjust = 2;
		else
			out->mipadjust = 1;

		miptex = in->miptex;
		out->flags = in->flags;

		if (!mod->brush.textures) {
			out->texture = r_notexture_mip;	// checkerboard texture
			out->flags = 0;
		} else {
			if (miptex >= mod->brush.numtextures)
				Sys_Error ("miptex >= mod->brush.numtextures");
			out->texture = mod->brush.textures[miptex];
			if (!out->texture) {
				out->texture = r_notexture_mip;	// texture not found
				out->flags = 0;
			}
		}
	}
}

/*
	CalcSurfaceExtents

	Fills in s->texturemins[] and s->extents[]
*/
static void
CalcSurfaceExtents (model_t *mod, msurface_t *s)
{
	float		mins[2], maxs[2], val;
	int			e, i, j;
	int			bmins[2], bmaxs[2];
	mtexinfo_t *tex;
	mvertex_t  *v;
	mod_brush_t *brush = &mod->brush;

	mins[0] = mins[1] = 999999;
	maxs[0] = maxs[1] = -99999;

	tex = s->texinfo;

	for (i = 0; i < s->numedges; i++) {
		e = brush->surfedges[s->firstedge + i];
		if (e >= 0)
			v = &brush->vertexes[brush->edges[e].v[0]];
		else
			v = &brush->vertexes[brush->edges[-e].v[1]];

		for (j = 0; j < 2; j++) {
			val = v->position[0] * tex->vecs[j][0] +
				v->position[1] * tex->vecs[j][1] +
				v->position[2] * tex->vecs[j][2] + tex->vecs[j][3];
			if (val < mins[j])
				mins[j] = val;
			if (val > maxs[j])
				maxs[j] = val;
		}
	}

	for (i = 0; i < 2; i++) {
		bmins[i] = floor (mins[i] / 16);
		bmaxs[i] = ceil (maxs[i] / 16);

		s->texturemins[i] = bmins[i] * 16;
		s->extents[i] = (bmaxs[i] - bmins[i]) * 16;
		// FIXME even 2000 is really too small, need a saner test
		if (!(tex->flags & TEX_SPECIAL) && s->extents[i] > 2000)
			Sys_Error ("Bad surface extents: %d %x %d %d", i, tex->flags,
					   s->extents[i], LongSwap (s->extents[i]));
	}
}

static void
Mod_LoadFaces (model_t *mod, bsp_t *bsp)
{
	dface_t    *in;
	int			count, planenum, side, surfnum, i;
	msurface_t *out;
	mod_brush_t *brush = &mod->brush;

	in = bsp->faces;
	count = bsp->numfaces;
	out = Hunk_AllocName (count * sizeof (*out), mod->name);

	if (count > 32767) {
		Sys_MaskPrintf (SYS_warn,
						"%i faces exceeds standard limit of 32767.\n", count);
	}

	brush->surfaces = out;
	brush->numsurfaces = count;

	for (surfnum = 0; surfnum < count; surfnum++, in++, out++) {
		out->firstedge = in->firstedge;
		out->numedges = in->numedges;
		out->flags = 0;

		planenum = in->planenum;
		side = in->side;
		if (side)
			out->flags |= SURF_PLANEBACK;

		out->plane = brush->planes + planenum;

		out->texinfo = brush->texinfo + in->texinfo;

		CalcSurfaceExtents (mod, out);

		// lighting info

		for (i = 0; i < MAXLIGHTMAPS; i++)
			out->styles[i] = in->styles[i];
		i = in->lightofs;
		if (i == -1)
			out->samples = NULL;
		else
			out->samples = brush->lightdata + (i * mod_lightmap_bytes);

		// set the drawing flags flag
		if (!out->texinfo->texture || !out->texinfo->texture->name)
			continue;				// avoid crashing on null textures

		if (!strncmp (out->texinfo->texture->name, "sky", 3)) {	// sky
			out->flags |= (SURF_DRAWSKY | SURF_DRAWTILED);
			if (gl_sky_divide && gl_sky_divide->int_val)
				if (mod_funcs && mod_funcs->Mod_SubdivideSurface)
					mod_funcs->Mod_SubdivideSurface (mod, out);
			continue;
		}

		if (out->texinfo->texture->name[0] == '*') {	// turbulent
			out->flags |= (SURF_DRAWTURB
						   | SURF_DRAWTILED
						   | SURF_LIGHTBOTHSIDES);
			for (i = 0; i < 2; i++) {
				out->extents[i] = 16384;
				out->texturemins[i] = -8192;
			}
			if (mod_funcs && mod_funcs->Mod_SubdivideSurface) {
				// cut up polygon for warps
				mod_funcs->Mod_SubdivideSurface (mod, out);
			}
			continue;
		}
	}
}

static void
Mod_SetParent (mod_brush_t *brush, mnode_t *node, mnode_t *parent)
{
	if (node->contents < 0) {
		brush->leaf_parents[(mleaf_t *)node - brush->leafs] = parent;
		return;
	}
	brush->node_parents[node - brush->nodes] = parent;
	Mod_SetParent (brush, node->children[0], node);
	Mod_SetParent (brush, node->children[1], node);
}

static void
Mod_SetLeafFlags (mod_brush_t *brush)
{
	for (int i = 0; i < brush->numleafs; i++) {
		int         flags = 0;
		mleaf_t    *leaf = &brush->leafs[i];
		for (int j = 0; j < leaf->nummarksurfaces; j++) {
			msurface_t *surf = leaf->firstmarksurface[j];
			flags |= surf->flags;
		}
		brush->leaf_flags[i] = flags;
	}
}

static void
Mod_LoadNodes (model_t *mod, bsp_t *bsp)
{
	dnode_t    *in;
	int			count, i, j, p;
	mnode_t    *out;
	mod_brush_t *brush = &mod->brush;

	in = bsp->nodes;
	count = bsp->numnodes;
	out = Hunk_AllocName (count * sizeof (*out), mod->name);

	if (count > 32767) {
		Sys_MaskPrintf (SYS_warn,
						"%i nodes exceeds standard limit of 32767.\n", count);
	}

	brush->nodes = out;
	brush->numnodes = count;

	for (i = 0; i < count; i++, in++, out++) {
		for (j = 0; j < 3; j++) {
			out->minmaxs[j] = in->mins[j];
			out->minmaxs[3 + j] = in->maxs[j];
		}

		p = in->planenum;
		out->plane = brush->planes + p;

		out->firstsurface = in->firstface;
		out->numsurfaces = in->numfaces;

		for (j = 0; j < 2; j++) {
			p = in->children[j];
			// this check is for extended bsp 29 files
			if (p >= 0) {
				out->children[j] = brush->nodes + p;
			} else {
				p = ~p;
				if (p < brush->numleafs) {
					out->children[j] = (mnode_t *) (brush->leafs + p);
				} else {
					Sys_Printf ("Mod_LoadNodes: invalid leaf index %i "
								"(file has only %i leafs)\n", p,
								brush->numleafs);
					//map it to the solid leaf
					out->children[j] = (mnode_t *)(brush->leafs);
				}
			}
		}
	}

	size_t      size = (brush->numleafs + brush->numnodes) * sizeof (mnode_t *);
	size += brush->numleafs * sizeof (int);
	brush->node_parents = Hunk_AllocName (size, mod->name);
	brush->leaf_parents = brush->node_parents + brush->numnodes;
	brush->leaf_flags = (int *) (brush->leaf_parents + brush->numleafs);
	Mod_SetParent (brush, brush->nodes, NULL);	// sets nodes and leafs
	Mod_SetLeafFlags (brush);
}

static void
Mod_LoadLeafs (model_t *mod, bsp_t *bsp)
{
	dleaf_t    *in;
	int			count, i, j, p;
	mleaf_t    *out;
	qboolean    isnotmap = true;
	mod_brush_t *brush = &mod->brush;

	in = bsp->leafs;
	count = bsp->numleafs;
	out = Hunk_AllocName (count * sizeof (*out), mod->name);

	brush->leafs = out;
	brush->numleafs = count;
//	snprintf(s, sizeof (s), "maps/%s.bsp",
//	Info_ValueForKey(cl.serverinfo,"map"));
	if (!strncmp ("maps/", mod->path, 5))
		isnotmap = false;
	for (i = 0; i < count; i++, in++, out++) {
		for (j = 0; j < 3; j++) {
			out->mins[j] = in->mins[j];
			out->maxs[j] = in->maxs[j];
		}

		p = in->contents;
		out->contents = p;

		out->firstmarksurface = brush->marksurfaces + in->firstmarksurface;
		out->nummarksurfaces = in->nummarksurfaces;

		p = in->visofs;
		if (p == -1)
			out->compressed_vis = NULL;
		else
			out->compressed_vis = brush->visdata + p;
		out->efrags = NULL;

		for (j = 0; j < 4; j++)
			out->ambient_sound_level[j] = in->ambient_level[j];

		// gl underwater warp
		if (out->contents != CONTENTS_EMPTY) {
			for (j = 0; j < out->nummarksurfaces; j++)
				out->firstmarksurface[j]->flags |= SURF_UNDERWATER;
		}
		if (isnotmap) {
			for (j = 0; j < out->nummarksurfaces; j++)
				out->firstmarksurface[j]->flags |= SURF_DONTWARP;
		}
	}
}

static void
Mod_LoadClipnodes (model_t *mod, bsp_t *bsp)
{
	dclipnode_t *in;
	mclipnode_t *out;
	hull_t		*hull;
	int			 count, i;
	mod_brush_t *brush = &mod->brush;

	in = bsp->clipnodes;
	count = bsp->numclipnodes;
	out = Hunk_AllocName (count * sizeof (*out), mod->name);

	if (count > 32767) {
		Sys_MaskPrintf (SYS_warn,
						"%i clilpnodes exceeds standard limit of 32767.\n",
						count);
	}

	brush->clipnodes = out;
	brush->numclipnodes = count;

	hull = &brush->hulls[1];
	brush->hull_list[1] = hull;
	hull->clipnodes = out;
	hull->firstclipnode = 0;
	hull->lastclipnode = count - 1;
	hull->planes = brush->planes;
	hull->clip_mins[0] = -16;
	hull->clip_mins[1] = -16;
	hull->clip_mins[2] = -24;
	hull->clip_maxs[0] = 16;
	hull->clip_maxs[1] = 16;
	hull->clip_maxs[2] = 32;

	hull = &brush->hulls[2];
	brush->hull_list[2] = hull;
	hull->clipnodes = out;
	hull->firstclipnode = 0;
	hull->lastclipnode = count - 1;
	hull->planes = brush->planes;
	hull->clip_mins[0] = -32;
	hull->clip_mins[1] = -32;
	hull->clip_mins[2] = -24;
	hull->clip_maxs[0] = 32;
	hull->clip_maxs[1] = 32;
	hull->clip_maxs[2] = 64;

	for (i = 0; i < count; i++, out++, in++) {
		out->planenum = in->planenum;
		if (out->planenum < 0 || out->planenum >= brush->numplanes)
			Sys_Error ("Mod_LoadClipnodes: planenum out of bounds");
		out->children[0] = in->children[0];
		out->children[1] = in->children[1];
		// these checks are for extended bsp 29 files
		if (out->children[0] >= count)
			out->children[0] -= 65536;
		if (out->children[1] >= count)
			out->children[1] -= 65536;
		if ((out->children[0] >= 0
			 && (out->children[0] < hull->firstclipnode
				 || out->children[0] > hull->lastclipnode))
			|| (out->children[1] >= 0
				&& (out->children[1] < hull->firstclipnode
					|| out->children[1] > hull->lastclipnode)))
			Sys_Error ("Mod_LoadClipnodes: bad node number");
	}
}

/*
	Mod_MakeHull0

	Replicate the drawing hull structure as a clipping hull
*/
static void
Mod_MakeHull0 (model_t *mod)
{
	mclipnode_t *out;
	hull_t		*hull;
	int			 count, i, j;
	mnode_t		*in, *child;
	mod_brush_t *brush = &mod->brush;

	hull = &brush->hulls[0];
	brush->hull_list[0] = hull;

	in = brush->nodes;
	count = brush->numnodes;
	out = Hunk_AllocName (count * sizeof (*out), mod->name);

	hull->clipnodes = out;
	hull->firstclipnode = 0;
	hull->lastclipnode = count - 1;
	hull->planes = brush->planes;

	for (i = 0; i < count; i++, out++, in++) {
		out->planenum = in->plane - brush->planes;
		for (j = 0; j < 2; j++) {
			child = in->children[j];
			if (child->contents < 0)
				out->children[j] = child->contents;
			else
				out->children[j] = child - brush->nodes;
		}
	}
}

static void
Mod_LoadMarksurfaces (model_t *mod, bsp_t *bsp)
{
	int			 count, i, j;
	msurface_t **out;
	uint32_t    *in;
	mod_brush_t *brush = &mod->brush;

	in = bsp->marksurfaces;
	count = bsp->nummarksurfaces;
	out = Hunk_AllocName (count * sizeof (*out), mod->name);

	if (count > 32767) {
		Sys_MaskPrintf (SYS_warn,
						"%i marksurfaces exceeds standard limit of 32767.\n",
						count);
	}

	brush->marksurfaces = out;
	brush->nummarksurfaces = count;

	for (i = 0; i < count; i++) {
		j = in[i];
		if (j >= brush->numsurfaces)
			Sys_Error ("Mod_ParseMarksurfaces: bad surface number");
		out[i] = brush->surfaces + j;
	}
}

static void
Mod_LoadSurfedges (model_t *mod, bsp_t *bsp)
{
	int          count, i;
	int32_t     *in;
	int         *out;
	mod_brush_t *brush = &mod->brush;

	in = bsp->surfedges;
	count = bsp->numsurfedges;
	out = Hunk_AllocName (count * sizeof (*out), mod->name);

	brush->surfedges = out;
	brush->numsurfedges = count;

	for (i = 0; i < count; i++)
		out[i] = in[i];
}

static void
Mod_LoadPlanes (model_t *mod, bsp_t *bsp)
{
	dplane_t   *in;
	int			bits, count, i, j;
	plane_t    *out;
	mod_brush_t *brush = &mod->brush;

	in = bsp->planes;
	count = bsp->numplanes;
	out = Hunk_AllocName (count * 2 * sizeof (*out), mod->name);

	brush->planes = out;
	brush->numplanes = count;

	for (i = 0; i < count; i++, in++, out++) {
		bits = 0;
		for (j = 0; j < 3; j++) {
			out->normal[j] = in->normal[j];
			if (out->normal[j] < 0)
				bits |= 1 << j;
		}

		out->dist = in->dist;
		out->type = in->type;
		out->signbits = bits;
	}
}

static void
do_checksums (const bsp_t *bsp, void *_mod)
{
	int         i;
	model_t    *mod = (model_t *) _mod;
	byte       *base;
	mod_brush_t *brush = &mod->brush;

	base = (byte *) bsp->header;

	// checksum all of the map, except for entities
	brush->checksum = 0;
	brush->checksum2 = 0;
	for (i = 0; i < HEADER_LUMPS; i++) {
		lump_t     *lump = bsp->header->lumps + i;
		int         csum;

		if (i == LUMP_ENTITIES)
			continue;
		csum = Com_BlockChecksum (base + lump->fileofs, lump->filelen);
		brush->checksum ^= csum;

		if (i != LUMP_VISIBILITY && i != LUMP_LEAFS && i != LUMP_NODES)
			brush->checksum2 ^= csum;
	}
}

static void
recurse_draw_tree (mod_brush_t *brush, mnode_t *node, int depth)
{
	if (!node || node->contents < 0) {
		if (depth > brush->depth)
			brush->depth = depth;
		return;
	}
	recurse_draw_tree (brush, node->children[0], depth + 1);
	recurse_draw_tree (brush, node->children[1], depth + 1);
}

static void
Mod_FindDrawDepth (mod_brush_t *brush)
{
	brush->depth = 0;
	recurse_draw_tree (brush, brush->nodes, 1);
}

void
Mod_LoadBrushModel (model_t *mod, void *buffer)
{
	dmodel_t   *bm;
	int			i, j;
	bsp_t      *bsp;

	mod->type = mod_brush;

	bsp = LoadBSPMem (buffer, qfs_filesize, do_checksums, mod);

	// load into heap
	Mod_LoadVertexes (mod, bsp);
	Mod_LoadEdges (mod, bsp);
	Mod_LoadSurfedges (mod, bsp);
	Mod_LoadTextures (mod, bsp);
	if (mod_funcs && mod_funcs->Mod_LoadLighting) {
		mod_funcs->Mod_LoadLighting (mod, bsp);
	}
	Mod_LoadPlanes (mod, bsp);
	Mod_LoadTexinfo (mod, bsp);
	Mod_LoadFaces (mod, bsp);
	Mod_LoadMarksurfaces (mod, bsp);
	Mod_LoadVisibility (mod, bsp);
	Mod_LoadLeafs (mod, bsp);
	Mod_LoadNodes (mod, bsp);
	Mod_LoadClipnodes (mod, bsp);
	Mod_LoadEntities (mod, bsp);
	Mod_LoadSubmodels (mod, bsp);

	BSP_Free(bsp);

	Mod_MakeHull0 (mod);

	Mod_FindDrawDepth (&mod->brush);
	for (i = 0; i < MAX_MAP_HULLS; i++)
		Mod_FindClipDepth (&mod->brush.hulls[i]);

	mod->numframes = 2;					// regular and alternate animation

	// set up the submodels (FIXME: this is confusing)
	for (i = 0; i < mod->brush.numsubmodels; i++) {
		bm = &mod->brush.submodels[i];

		mod->brush.hulls[0].firstclipnode = bm->headnode[0];
		mod->brush.hull_list[0] = &mod->brush.hulls[0];
		for (j = 1; j < MAX_MAP_HULLS; j++) {
			mod->brush.hulls[j].firstclipnode = bm->headnode[j];
			mod->brush.hulls[j].lastclipnode = mod->brush.numclipnodes - 1;
			mod->brush.hull_list[j] = &mod->brush.hulls[j];
		}

		mod->brush.firstmodelsurface = bm->firstface;
		mod->brush.nummodelsurfaces = bm->numfaces;

		VectorCopy (bm->maxs, mod->maxs);
		VectorCopy (bm->mins, mod->mins);

		mod->radius = RadiusFromBounds (mod->mins, mod->maxs);

		mod->brush.numleafs = bm->visleafs;

		if (i < mod->brush.numsubmodels - 1) {
			// duplicate the basic information
			char	name[12];

			snprintf (name, sizeof (name), "*%i", i + 1);
			model_t    *m = Mod_FindName (name);
			*m = *mod;
			strcpy (m->path, name);
			mod = m;
			// make sure clear is called only for the main model
			m->clear = 0;
			m->data = 0;
		}
	}
}
