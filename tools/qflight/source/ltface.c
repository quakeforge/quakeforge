/*
	trace.c

	(description)

	Copyright (C) 1996-1997  Id Software, Inc.
	Copyright (C) 2002 Colin Thompson

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

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef HAVE_IO_H
# include <io.h>
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#include <stdlib.h>

#include "QF/bspfile.h"
#include "QF/dstring.h"
#include "QF/mathlib.h"
#include "QF/qtypes.h"
#include "QF/quakefs.h"
#include "QF/sys.h"

#include "compat.h"

#include "tools/qflight/include/light.h"
#include "tools/qflight/include/entities.h"
#include "tools/qflight/include/noise.h"
#include "tools/qflight/include/options.h"
#include "tools/qflight/include/threads.h"

int c_bad;
int c_culldistplane, c_proper;

const char *
get_tex_name (int texindex)
{
	dmiptexlump_t *mtl;
	miptex_t   *mt;
	int         miptex;

	if (bsp->texdatasize) {
		mtl = (dmiptexlump_t *) bsp->texdata;
		miptex = bsp->texinfo[texindex].miptex;
		if (mtl->dataofs[miptex] != -1) {
			mt = (miptex_t *) (bsp->texdata + mtl->dataofs[miptex]);
			return mt->name;
		}
	}
	return "notex";
}

/*
SAMPLE POINT DETERMINATION

void SetupBlock (dface_t *f) Returns with surfpt[] set

This is a little tricky because the lightmap covers more area than the face.
If done in the straightforward fashion, some of the
sample points will be inside walls or on the other side of walls, causing
false shadows and light bleeds.

To solve this, I consider a sample point valid only if a line can be drawn
between it and the exact midpoint of the face.  If invalid, it is adjusted
towards the center until it is valid.

(this doesn't completely work)
*/


/*
	CalcFaceVectors

	Fills in texorg, worldtotex. and textoworld
*/
static void
CalcFaceVectors (lightinfo_t *l, vec3_t faceorg)
{
	int			i, j;
	float		distscale;
	vec3_t		texnormal;
	vec_t		dist, len;
	texinfo_t	*tex;

	tex = &bsp->texinfo[l->face->texinfo];

	// convert from float to vec_t
	for (i = 0; i < 2; i++)
		for (j = 0; j < 3; j++)
			l->worldtotex[i][j] = tex->vecs[i][j];

	// calculate a normal to the texture axis.  points can
	// be moved along this without changing their S/T
	texnormal[0] = tex->vecs[1][1] * tex->vecs[0][2] -
		tex->vecs[1][2] * tex->vecs[0][1];
	texnormal[1] = tex->vecs[1][2] * tex->vecs[0][0] -
		tex->vecs[1][0] * tex->vecs[0][2];
	texnormal[2] = tex->vecs[1][0] * tex->vecs[0][1] -
		tex->vecs[1][1] * tex->vecs[0][0];
	VectorNormalize (texnormal);

	// flip it towards plane normal
	distscale = DotProduct (texnormal, l->facenormal);
	if (!distscale)
		fprintf (stderr, "Texture axis perpendicular to face");
	if (distscale < 0) {
		distscale = -distscale;
		VectorNegate (texnormal, texnormal);
	}

	// distscale is the ratio of the distance along the
	// texture normal to the distance along the plane normal
	distscale = 1 / distscale;

	for (i = 0; i < 2; i++) {
		len = VectorLength (l->worldtotex[i]);
		dist = DotProduct (l->worldtotex[i], l->facenormal);
		dist *= distscale;
		VectorMultSub (l->worldtotex[i], dist, texnormal, l->textoworld[i]);
		VectorScale (l->textoworld[i], (1 / len) * (1 / len),
					 l->textoworld[i]);
	}

	// calculate texorg on the texture plane
	for (i = 0; i < 3; i++)
		l->texorg[i] = -tex->vecs[0][3] * l->textoworld[0][i] -
						tex->vecs[1][3] * l->textoworld[1][i];

	VectorAdd (l->texorg, faceorg, l->texorg);

	// project back to the face plane
	dist = DotProduct (l->texorg, l->facenormal) - l->facedist - 1;
	dist *= distscale;
	VectorMultSub (l->texorg, dist, texnormal, l->texorg);
}

/*
	CalcFaceExtents

	Fills in s->texmins[] and s->texsize[]
	also sets exactmins[] and exactmaxs[]
*/
static void
CalcFaceExtents (lightinfo_t *l)
{
	int			i, j, e;
	vec_t		mins[2], maxs[2], val;
	dface_t		*s;
	dvertex_t	*v;
	texinfo_t	*tex;

	s = l->face;

	mins[0] = mins[1] = BOGUS_RANGE;
	maxs[0] = maxs[1] = -BOGUS_RANGE;

	tex = &bsp->texinfo[s->texinfo];

	for (i = 0; i < s->numedges; i++) {
		e = bsp->surfedges[s->firstedge + i];
		if (e >= 0)
			v = bsp->vertexes + bsp->edges[e].v[0];
		else
			v = bsp->vertexes + bsp->edges[-e].v[1];

		for (j = 0; j < 2; j++) {
			val = DotProduct (v->point, tex->vecs[j]) + tex->vecs[j][3];
			if (val < mins[j])
				mins[j] = val;
			if (val > maxs[j])
				maxs[j] = val;
		}
	}

	for (i = 0; i < 2; i++) {
		l->exactmins[i] = mins[i];
		l->exactmaxs[i] = maxs[i];

		mins[i] = floor (mins[i] / 16);
		maxs[i] = ceil (maxs[i] / 16);

		l->texmins[i] = mins[i];
		l->texsize[i] = maxs[i] - mins[i] + 1;
		if (l->texsize[i] > 256)
			fprintf (stderr, "Bad surface extents");
	}
}

static inline void
CalcSamples (lightinfo_t *l)
{
	l->numsamples = l->texsize[0] * l->texsize[1];
}

/*
	CalcPoints

	For each texture aligned grid point, back project onto the plane
	to get the world xyz value of the sample point
*/
static void
CalcPoints (lightinfo_t *l)
{
	int			realw, realh, stepbit, j, s, t, w, h;
	vec_t		mids, midt, starts, startt, us, ut;
	vec3_t		facemid, v;
	lightpoint_t *point;

	// fill in surforg
	// the points are biased towards the center of the surface
	// to help avoid edge cases just inside walls
	mids = (l->exactmaxs[0] + l->exactmins[0]) / 2;
	midt = (l->exactmaxs[1] + l->exactmins[1]) / 2;

	for (j = 0; j < 3; j++)
		facemid[j] = l->texorg[j] +
			l->textoworld[0][j] * mids +
			l->textoworld[1][j] * midt;

	realw = l->texsize[0];
	realh = l->texsize[1];
	starts = l->texmins[0] * 16;
	startt = l->texmins[1] * 16;

	stepbit = 4 - options.extrabit;

	w = realw << options.extrabit;
	h = realh << options.extrabit;

	if (stepbit < 4) {
		starts -= 1 << stepbit;
		startt -= 1 << stepbit;
	}

	point = l->point;
	l->numpoints = w * h;
	for (t = 0; t < h; t++) {
		for (s = 0; s < w; s++, point++) {
			us = starts + (s << stepbit);
			ut = startt + (t << stepbit);
			point->samplepos = ((t >> options.extrabit) * realw
								+ (s >> options.extrabit));

			// calculate texture point
			for (j = 0; j < 3; j++)
				point->v[j] = l->texorg[j] +
					l->textoworld[0][j] * us + l->textoworld[1][j] * ut;

			if (!TestLine (l, facemid, point->v)) {
				VectorCopy(l->testlineimpact, point->v);
				VectorSubtract(facemid, point->v, v);
				VectorNormalize(v);
				VectorMultAdd (point->v, 0.25, v, point->v);
			}
		}
	}
}

static void
SingleLightFace (entity_t *light, lightinfo_t *l)
{
	int			mapnum, i;
	qboolean	hit;
	vec3_t		incoming, spotvec;
	vec_t		angle, dist, idist, lightfalloff, lightsubtract, spotcone;
	vec_t       add = 0.0;
	lightpoint_t *point;
	lightsample_t *sample;

	dist = DotProduct (light->origin, l->facenormal) - l->facedist;
	dist *= options.distance;

	// don't bother with lights behind the surface
	if (dist <= -0.25)
		return;

	lightfalloff = light->falloff;
	lightsubtract = light->subbrightness;

	// don't bother with light too far away
	if (light->radius && dist > light->radius) {
		c_culldistplane++;
		return;
	}
	if (lightsubtract > (1.0 / (dist * dist * lightfalloff + LIGHTDISTBIAS))) {
		c_culldistplane++;
		return;
	}

	for (mapnum = 0; mapnum < MAXLIGHTMAPS; mapnum++) {
		if (l->lightstyles[mapnum] == light->style)
			break;
		if (l->lightstyles[mapnum] == 255) {
			memset (l->sample[mapnum], 0,
					sizeof (lightsample_t) * l->numsamples);
			break;
		}
	}
	if (mapnum == MAXLIGHTMAPS) {
		printf ("WARNING: Too many light styles on a face\n");
		return;
	}

	spotcone = light->spotcone;
	VectorCopy(light->spotdir, spotvec);

	// check it for real
	hit = false;
	c_proper++;

	for (i = 0, point = l->point; i < l->numpoints; i++, point++) {
		VectorSubtract (light->origin, point->v, incoming);
		// avoid float roundoff
		dist = sqrt (DotProduct(incoming, incoming));
		idist = 1.0 / dist;
		VectorScale (incoming, idist, incoming);

		if (light->radius && dist > light->radius)
			continue;

		// spotlight cutoff
		if (spotcone && DotProduct (spotvec, incoming) > spotcone)
			continue;

		angle = DotProduct (incoming, l->facenormal);

		switch (light->attenuation) {
			case LIGHT_LINEAR:
				add = fabs (light->light) - dist;
				break;
			case LIGHT_RADIUS:
				add = fabs (light->light) * (light->radius - dist);
				add /= light->radius;
				break;
			case LIGHT_INVERSE:
				add = fabs (light->light) / dist;
				break;
			case LIGHT_REALISTIC:
				add = fabs (light->light) / (dist * dist);
				break;
			case LIGHT_NO_ATTEN:
				add = fabs (light->light);
				break;
			case LIGHT_LH:
				add = 1 / (dist * dist * lightfalloff + LIGHTDISTBIAS);
				// LordHavoc: changed to be more realistic (entirely different
				// lighting model)
				// LordHavoc: use subbrightness on all lights, simply to have
				// some distance culling
				add -= lightsubtract;
				break;
		}

		if (light->noise) {
			int         seed = light - entities;
			vec3_t      snap;
			vec_t		intensity = 0.0;
			lightpoint_t *noise_point = point;

			if (options.extrascale) {
				// FIXME not correct for extrascale > 2
				// We don't want to oversample noise because that just
				// waters it down.  So we "undersample" noise by using
				// the same surf coord for every group of 4 lightmap pixels
				// ("undersampling", "pixelation", "anti-interpolation" :-)
				int         width = (l->texsize[0] + 1) * 2;
				int         x = i % width;
				int         y = i / width;

				if (x % 2 && y % 2)
					noise_point -= width * 3 + 3;
				else if (y % 2)
					noise_point -= width * 3;
				else if (x % 2)
					noise_point -= 3;
			}

			if (light->noisetype == NOISE_SMOOTH) {
				snap_vector (noise_point->v, snap, 0);
				intensity = noise_scaled (snap, light->resolution, seed);
			} else
				snap_vector (noise_point->v, snap, light->resolution);

			if (light->noisetype == NOISE_RANDOM)
				intensity = noise3d (snap, seed);
			if (light->noisetype == NOISE_PERLIN)
				intensity = noise_perlin (snap, light->persistence, seed);

			add *= intensity * light->noise + 1.0 - light->noise;
		}

		if (add <= 0)
			continue;
		if (!TestLine (l, point->v, light->origin))
			continue;

		if (light->attenuation == LIGHT_LH) {
			// LordHavoc: FIXME: decide this 0.5 bias based on shader
			// properties (some are dull, some are shiny)
			add *= angle * 0.5 + 0.5;
		} else {
			add *= angle;
		}
		add *= options.extrascale;

		if (light->light < 0)
			add *= -1;				// negative light

		sample = &l->sample[mapnum][point->samplepos];
		VectorMultAdd (sample->c, add, light->color, sample->c);
		if (!hit && ((sample->c[0] + sample->c[1]  + sample->c[2]) >= 1))
			hit = true;
	}

	// if the style has some data now, make sure it is in the list
	if (hit)
		l->lightstyles[mapnum] = light->style;
}

static void
SkyLightFace (entity_t *ent, int sun, lightinfo_t *l)
{
	int         sun_light  = ent->sun_light[sun != 0];
	const vec_t *sun_color = ent->sun_color[sun != 0];
	const vec_t *sun_dir   = ent->sun_dir[sun];
	float       dist;
	int         i;
	int         mapnum;
	float       angle;
	float       add;
	vec3_t      incoming;
	lightpoint_t *point;
	lightsample_t *sample;

	if (sun_light <= 0)
		return;

	dist = DotProduct (sun_dir, l->facenormal);

	// don't bother with lights behind the surface
	if (dist <= -0.25)
		return;

	// if sunlight is set, use a style 0 light map
	for (mapnum = 0; mapnum < MAXLIGHTMAPS; mapnum++) {
		if (l->lightstyles[mapnum] == 0)
			break;
		if (l->lightstyles[mapnum] == 255) {
			memset (l->sample[mapnum], 0,
					sizeof (lightsample_t) * l->numsamples);
			break;
		}
	}
	if (mapnum == MAXLIGHTMAPS) {
		//printf ("WARNING: Too many light styles on a face\n");
		return;
	}

	// Check each point...
	VectorCopy (sun_dir, incoming);
	VectorNormalize (incoming);
	//anglesense = 0.5;	//FIXME

	// FIXME global

	for (i = 0, point = l->point; i < l->numpoints; i++, point++) {

		angle = DotProduct (incoming, l->facenormal);

		if (!TestSky (l, point->v, sun_dir))
			continue;

		add = sun_light;
		add *= angle;
		add *= options.extrascale;

		sample = &l->sample[mapnum][point->samplepos];
		VectorMultAdd (sample->c, add, sun_color, sample->c);
	}
}

void
LightFace (lightinfo_t *l, int surfnum)
{
	byte       *lit, *out, *outdata, *rgbdata;
	int			ofs, size, red, green, blue, white, i, j;
	dface_t    *f;
	lightchain_t  *lightchain;
	lightsample_t *sample;

	f = bsp->faces + surfnum;

	l->face = f;

	// some surfaces don't need lightmaps
	f->lightofs = -1;
	for (i = 0; i < MAXLIGHTMAPS; i++)
		f->styles[i] = l->lightstyles[i] = 255;

	if (bsp->texinfo[f->texinfo].flags & TEX_SPECIAL)
		return;		// non-lit texture

	// rotate plane
	VectorCopy (bsp->planes[f->planenum].normal, l->facenormal);
	l->facedist = bsp->planes[f->planenum].dist;
	if (f->side) {
		VectorNegate (l->facenormal, l->facenormal);
		l->facedist = -l->facedist;
	}

	CalcFaceVectors (l, surfaceorgs[surfnum]);
	CalcFaceExtents (l);
	CalcSamples (l);
	CalcPoints (l);

	if (l->numsamples > SINGLEMAP)
		fprintf (stderr, "Bad lightmap size");

	// cast all lights
	for (lightchain = surfacelightchain[surfnum]; lightchain;
		 lightchain = lightchain->next) {
		SingleLightFace (lightchain->light, l);
	}
	for (i = 0; i < num_novislights; i++) {
		SingleLightFace (novislights[i], l);
	}
	if (world_entity && world_entity->num_suns) {
		for (i = 0; i < world_entity->num_suns; i++)
			SkyLightFace (world_entity, i, l);
	}

	for (i = 0; i < MAXLIGHTMAPS; i++)
		if (l->lightstyles[i] == 255)
			break;
	size = l->numsamples * i;
	if (!size) {
		// no light styles
		return;
	}

	// save out the values
	for (i = 0; i < MAXLIGHTMAPS; i++)
		f->styles[i] = l->lightstyles[i];

	LOCK;
	outdata = out = malloc (size * 4);
	UNLOCK;
	rgbdata = lit = outdata + size;
	ofs = GetFileSpace (size);
	f->lightofs = ofs;

	for (i = 0; i < MAXLIGHTMAPS && f->styles[i] != 255; i++) {
		for (j = 0, sample = l->sample[i]; j < l->numsamples; j++, sample++) {
			red   = (int) sample->c[0];
			green = (int) sample->c[1];
			blue  = (int) sample->c[2];
			white = (int) ((sample->c[0] + sample->c[1] + sample->c[2])
						   * (1.0 / 3.0));

			red   = bound (0, red,   255);
			green = bound (0, green, 255);
			blue  = bound (0, blue,  255);
			white = bound (0, white, 255);
			*lit++ = red;
			*lit++ = green;
			*lit++ = blue;
			*out++ = white;
		}
	}
	LOCK;
	memcpy (lightdata->str + ofs, outdata, size);
	memcpy (rgblightdata->str + ofs * 3, rgbdata, size * 3);
	free (outdata);
	UNLOCK;
}
