/*
	r_light.c

	common lightmap code.

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

#include <math.h>
#include <stdio.h>

#include "qfalloca.h"

#include "QF/cvar.h"
#include "QF/render.h"

#include "compat.h"
#include "r_internal.h"

dlight_t    *r_dlights;
vec3_t      ambientcolor;

unsigned int r_maxdlights;

void
R_FindNearLights (const vec3_t pos, int count, dlight_t **lights)
{
	float      *scores = alloca (count * sizeof (float));
	float       score;
	dlight_t   *dl;
	unsigned    i;
	int         num = 0, j;
	vec3_t      d;

	dl = r_dlights;
	for (i = 0; i < r_maxdlights; i++, dl++) {
		if (dl->die < vr_data.realtime || !dl->radius)
			continue;
		VectorSubtract (dl->origin, pos, d);
		score = DotProduct (d, d) / dl->radius;
		if (!num) {
			scores[0] = score;
			lights[0] = dl;
			num = 1;
		} else if (score <= scores[0]) {
			memmove (&lights[1], &lights[0],
					 (count - 1) * sizeof (dlight_t *));
			memmove (&scores[1], &scores[0], (count - 1) * sizeof (float));
			scores[0] = score;
			lights[0] = dl;
			if (num < count)
				num++;
		} else if (score > scores[num - 1]) {
			if (num < count) {
				scores[num] = score;
				lights[num] = dl;
				num++;
			}
		} else {
			for (j = num - 1; j > 0; j--) {
				if (score > scores[j - 1]) {
					memmove (&lights[j + 1], &lights[j],
							 (count - j) * sizeof (dlight_t *));
					memmove (&scores[j + 1], &scores[j],
							 (count - j) * sizeof (float));
					scores[j] = score;
					lights[j] = dl;
					if (num < count)
						num++;
					break;
				}
			}
		}
	}
	for (j = num; j < count; j++)
		lights[j] = 0;
}

void
R_MaxDlightsCheck (cvar_t *var)
{
	r_maxdlights = bound (0, var->int_val, MAX_DLIGHTS);

	if (r_dlights)
		free (r_dlights);

	r_dlights=0;

	if (r_maxdlights)
		r_dlights = (dlight_t *) calloc (r_maxdlights, sizeof (dlight_t));

	R_ClearDlights();
}

void
R_AnimateLight (void)
{
	int         i, j, k;

	// light animations
	// 'm' is normal light, 'a' is no light, 'z' is double bright
	i = (int) (vr_data.realtime * 10);
	for (j = 0; j < MAX_LIGHTSTYLES; j++) {
		if (!vr_data.lightstyle[j].length) {
			d_lightstylevalue[j] = 256;
			continue;
		}
		if (r_flatlightstyles->int_val == 2) {
			k = vr_data.lightstyle[j].peak - 'a';
		} else if (r_flatlightstyles->int_val == 1) {
			k = vr_data.lightstyle[j].average - 'a';
		} else {
			k = i % vr_data.lightstyle[j].length;
			k = vr_data.lightstyle[j].map[k] - 'a';
		}
		d_lightstylevalue[j] = k * 22;
	}
}

static inline void
real_mark_surfaces (float dist, msurface_t *surf, const vec3_t lightorigin,
					dlight_t *light, unsigned lightnum)
{
	float      dist2, is, it;
	float      maxdist = light->radius * light->radius;
	vec3_t     impact;
	unsigned   ind, bit;

	dist2 = maxdist - dist * dist;
	VectorMultSub (light->origin, dist, surf->plane->normal, impact);

	is = DotProduct (impact, surf->texinfo->vecs[0])
	 	+ surf->texinfo->vecs[0][3] - surf->texturemins[0];
	it = DotProduct (impact, surf->texinfo->vecs[1])
		+ surf->texinfo->vecs[1][3] - surf->texturemins[1];

	// compress the square to a point
	if (is > surf->extents[0])
		is -= surf->extents[0];
	else if (is > 0)
		is = 0;
	if (it > surf->extents[1])
		it -= surf->extents[1];
	else if (it > 0)
		it = 0;
	if (is * is + it * it > dist2)
		return;

	if (surf->dlightframe != r_framecount) {
		memset (surf->dlightbits, 0, sizeof (surf->dlightbits));
		surf->dlightframe = r_framecount;
	}
	ind = lightnum / 32;
	bit = 1 << (lightnum % 32);
	surf->dlightbits[ind] |= bit;
}

static inline void
mark_surfaces (msurface_t *surf, const vec3_t lightorigin, dlight_t *light,
			   int lightnum)
{
	float      dist;

	dist = PlaneDiff(lightorigin, surf->plane);
	if (surf->flags & SURF_PLANEBACK)
		dist = -dist;
	if ((dist < 0 && !(surf->flags & SURF_LIGHTBOTHSIDES))
		|| dist > light->radius)
		return;

	real_mark_surfaces (dist, surf, lightorigin, light, lightnum);
}

// LordHavoc: heavily modified, to eliminate unnecessary texture uploads,
//            and support bmodel lighting better
void
R_RecursiveMarkLights (mod_brush_t *brush, const vec3_t lightorigin,
					   dlight_t *light, int lightnum, mnode_t *node)
{
	unsigned    i;
	float       ndist, maxdist;
	plane_t    *splitplane;
	msurface_t *surf;
	//XXX mvertex_t  *vertices;

	//XXX vertices = r_worldentity.model->vertexes;
	maxdist = light->radius;

loc0:
	if (node->contents < 0)
		return;

	splitplane = node->plane;
	ndist = DotProduct (lightorigin, splitplane->normal) - splitplane->dist;

	if (ndist > maxdist * maxdist) {
		// Save time by not pushing another stack frame.
		if (node->children[0]->contents >= 0) {
			node = node->children[0];
			goto loc0;
		}
		return;
	}
	if (ndist < -maxdist * maxdist) {
		// Save time by not pushing another stack frame.
		if (node->children[1]->contents >= 0) {
			node = node->children[1];
			goto loc0;
		}
		return;
	}

	// mark the polygons
	surf = brush->surfaces + node->firstsurface;
	for (i = 0; i < node->numsurfaces; i++, surf++) {
		mark_surfaces (surf, lightorigin, light, lightnum);
	}

	if (node->children[0]->contents >= 0) {
		if (node->children[1]->contents >= 0)
			R_RecursiveMarkLights (brush, lightorigin, light, lightnum,
								   node->children[1]);
		node = node->children[0];
		goto loc0;
	} else if (node->children[1]->contents >= 0) {
		node = node->children[1];
		goto loc0;
	}
}


void
R_MarkLights (const vec3_t lightorigin, dlight_t *light, int lightnum,
			  model_t *model)
{
	mod_brush_t *brush = &model->brush;
	mleaf_t    *pvsleaf = Mod_PointInLeaf (lightorigin, model);

	if (!pvsleaf->compressed_vis) {
		mnode_t *node = brush->nodes + brush->hulls[0].firstclipnode;
		R_RecursiveMarkLights (brush, lightorigin, light, lightnum, node);
	} else {
		float       radius = light->radius;
		vec3_t      mins, maxs;
		unsigned    leafnum = 0;
		byte       *in = pvsleaf->compressed_vis;
		byte        vis_bits;

		mins[0] = lightorigin[0] - radius;
		mins[1] = lightorigin[1] - radius;
		mins[2] = lightorigin[2] - radius;
		maxs[0] = lightorigin[0] + radius;
		maxs[1] = lightorigin[1] + radius;
		maxs[2] = lightorigin[2] + radius;
		while (leafnum < brush->numleafs) {
			int         b;
			if (!(vis_bits = *in++)) {
				leafnum += (*in++) * 8;
				continue;
			}
			for (b = 1; b < 256 && leafnum < brush->numleafs;
				 b <<= 1, leafnum++) {
				int      m;
				mleaf_t *leaf  = &brush->leafs[leafnum + 1];
				if (!(vis_bits & b))
					continue;
				if (leaf->visframe != r_visframecount)
					continue;
				if (leaf->mins[0] > maxs[0] || leaf->maxs[0] < mins[0]
					|| leaf->mins[1] > maxs[1] || leaf->maxs[1] < mins[1]
					|| leaf->mins[2] > maxs[2] || leaf->maxs[2] < mins[2])
					continue;
				if (R_CullBox (leaf->mins, leaf->maxs))
					continue;
				for (m = 0; m < leaf->nummarksurfaces; m++) {
					msurface_t *surf = leaf->firstmarksurface[m];
					if (surf->visframe != r_visframecount)
						continue;
					mark_surfaces (surf, lightorigin, light, lightnum);
				}
			}
		}
	}
}

void
R_PushDlights (const vec3_t entorigin)
{
	unsigned int i;
	dlight_t   *l;
	vec3_t      lightorigin;

	if (!r_dlight_lightmap->int_val)
		return;

	l = r_dlights;

	for (i = 0; i < r_maxdlights; i++, l++) {
		if (l->die < vr_data.realtime || !l->radius)
			continue;
		VectorSubtract (l->origin, entorigin, lightorigin);
		R_MarkLights (lightorigin, l, i, r_worldentity.renderer.model);
	}
}

/* LIGHT SAMPLING */

plane_t    *lightplane;
vec3_t      lightspot;

static int
calc_lighting_1 (msurface_t  *surf, int ds, int dt)
{
	int         se_s = ((surf->extents[0] >> 4) + 1);
	int         se_t = ((surf->extents[0] >> 4) + 1);
	int         se_size = se_s * se_t;
	int         r = 0, maps;
	byte       *lightmap;
	unsigned int scale;

	ds >>= 4;
	dt >>= 4;

	lightmap = surf->samples;
	if (lightmap) {
		lightmap += dt * se_s + ds;

		for (maps = 0; maps < MAXLIGHTMAPS && surf->styles[maps] != 255;
			 maps++) {
			scale = d_lightstylevalue[surf->styles[maps]];
			r += *lightmap * scale;
			lightmap += se_size;
		}

		r >>= 8;
	}

	ambientcolor[2] = ambientcolor[1] = ambientcolor[0] = r;

	return r;
}

static int
calc_lighting_3 (msurface_t  *surf, int ds, int dt)
{
	int         se_s = ((surf->extents[0] >> 4) + 1);
	int         se_t = ((surf->extents[0] >> 4) + 1);
	int         se_size = se_s * se_t * 3;
	int         r = 0, maps;
	byte       *lightmap;
	float       scale;

	ds >>= 4;
	dt >>= 4;

	VectorZero (ambientcolor);

	lightmap = surf->samples;
	if (lightmap) {
		lightmap += (dt * se_s + ds) * 3;

		for (maps = 0; maps < MAXLIGHTMAPS && surf->styles[maps] != 255;
			 maps++) {
			scale = d_lightstylevalue[surf->styles[maps]] / 256.0;
			VectorMultAdd (ambientcolor, scale, lightmap, ambientcolor);
			lightmap += se_size;
		}
	}

	r = (ambientcolor[0] + ambientcolor[1] + ambientcolor[2]) / 3;
	return r;
}

static int
RecursiveLightPoint (mod_brush_t *brush, mnode_t *node, const vec3_t start,
					 const vec3_t end)
{
	unsigned    i;
	int         r, s, t, ds, dt, side;
	float       front, back, frac;
	plane_t    *plane;
	msurface_t *surf;
	mtexinfo_t *tex;
	vec3_t      mid;
loop:
	if (node->contents < 0)
		return -1;						// didn't hit anything

	// calculate mid point
	plane = node->plane;
	front = DotProduct (start, plane->normal) - plane->dist;
	back = DotProduct (end, plane->normal) - plane->dist;
	side = front < 0;

	if ((back < 0) == side) {
		node = node->children[side];
		goto loop;
	}

	frac = front / (front - back);
	mid[0] = start[0] + (end[0] - start[0]) * frac;
	mid[1] = start[1] + (end[1] - start[1]) * frac;
	mid[2] = start[2] + (end[2] - start[2]) * frac;

	// go down front side
	r = RecursiveLightPoint (brush, node->children[side], start, mid);
	if (r >= 0)
		return r;						// hit something

	if ((back < 0) == side)
		return -1;						// didn't hit anything

	// check for impact on this node
	VectorCopy (mid, lightspot);
	lightplane = plane;

	surf = brush->surfaces + node->firstsurface;
	for (i = 0; i < node->numsurfaces; i++, surf++) {
		if (surf->flags & SURF_DRAWTILED)
			continue;					// no lightmaps

		tex = surf->texinfo;

		s = DotProduct (mid, tex->vecs[0]) + tex->vecs[0][3];
		t = DotProduct (mid, tex->vecs[1]) + tex->vecs[1][3];

		if (s < surf->texturemins[0] || t < surf->texturemins[1])
			continue;

		ds = s - surf->texturemins[0];
		dt = t - surf->texturemins[1];

		if (ds > surf->extents[0] || dt > surf->extents[1])
			continue;

		if (!surf->samples)
			return 0;

		if (mod_lightmap_bytes == 1)
			return calc_lighting_1 (surf, ds, dt);
		else
			return calc_lighting_3 (surf, ds, dt);

		return r;
	}

	// go down back side
	return RecursiveLightPoint (brush, node->children[!side], mid, end);
}

int
R_LightPoint (mod_brush_t *brush, const vec3_t p)
{
	vec3_t      end;
	int         r;

	if (!brush->lightdata) {
		// allow dlights to have some effect, so don't go /quite/ fullbright
		ambientcolor[2] = ambientcolor[1] = ambientcolor[0] = 200;
		return 200;
	}

	end[0] = p[0];
	end[1] = p[1];
	end[2] = p[2] - 2048;

	r = RecursiveLightPoint (brush, brush->nodes, p, end);

	if (r == -1)
		r = 0;

	return r;
}

dlight_t *
R_AllocDlight (int key)
{
	unsigned int i;
	dlight_t   *dl;

	if (!r_maxdlights) {
		return NULL;
	}

	// first look for an exact key match
	if (key) {
		dl = r_dlights;
		for (i = 0; i < r_maxdlights; i++, dl++) {
			if (dl->key == key) {
				memset (dl, 0, sizeof (*dl));
				dl->key = key;
				dl->color[0] = dl->color[1] = dl->color[2] = 1;
				return dl;
			}
		}
	}
	// then look for anything else
	dl = r_dlights;
	for (i = 0; i < r_maxdlights; i++, dl++) {
		if (dl->die < vr_data.realtime) {
			memset (dl, 0, sizeof (*dl));
			dl->key = key;
			dl->color[0] = dl->color[1] = dl->color[2] = 1;
			return dl;
		}
	}

	dl = &r_dlights[0];
	memset (dl, 0, sizeof (*dl));
	dl->key = key;
	return dl;
}

void
R_DecayLights (double frametime)
{
	unsigned int i;
	dlight_t   *dl;

	dl = r_dlights;
	for (i = 0; i < r_maxdlights; i++, dl++) {
		if (dl->die < vr_data.realtime || !dl->radius)
			continue;

		dl->radius -= frametime * dl->decay;
		if (dl->radius < 0)
			dl->radius = 0;
	}
}

void
R_ClearDlights (void)
{
	if (r_maxdlights)
		memset (r_dlights, 0, r_maxdlights * sizeof (dlight_t));
}
