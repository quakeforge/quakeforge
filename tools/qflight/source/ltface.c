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

static __attribute__ ((unused)) const char rcsid[] =
	"$Id$";

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

#include "light.h"
#include "entities.h"
#include "options.h"
#include "threads.h"

#define	SINGLEMAP	(18*18*4)

typedef struct {
	vec_t lightmaps[MAXLIGHTMAPS][SINGLEMAP];
	int numlightstyles;
	vec_t *light;
	vec_t facedist;
	vec3_t facenormal;

	int numsurfpt;
	vec3_t surfpt[SINGLEMAP];

	vec3_t texorg;
	vec3_t worldtotex[2];	// s = (world - texorg) . worldtotex[0]
	vec3_t textoworld[2];	// world = texorg + s * textoworld[0]

	vec_t exactmins[2], exactmaxs[2];

	int texmins[2], texsize[2];
	int lightstyles[256];
	int surfnum;
	dface_t *face;
} lightinfo_t;

int c_bad;
int c_culldistplane, c_proper;

/*
	CastRay

	Returns the distance between the points, or -1 if blocked
*/
static vec_t
CastRay (vec3_t p1, vec3_t p2)
{
	int			i;
	qboolean	trace;
	vec_t		t;

	trace = TestLine (p1, p2);

	if (!trace)
		return -1;		// ray was blocked

	t = 0;
	for (i = 0; i < 3; i++)
		t += (p2[i] - p1[i]) * (p2[i] - p1[i]);

	if (t == 0)
		t = 1;			// don't blow up...
	return sqrt (t);
}

/*
SAMPLE POINT DETERMINATION

void SetupBlock (dface_t *f) Returns with surfpt[] set

This is a little tricky because the lightmap covers more area than the face.
If done in the straightforward fashion, some of the
sample points will be inside walls or on the other side of walls, causing
false shadows and light bleeds.

To solve this, I only consider a sample point valid if a line can be drawn
between it and the exact midpoint of the face.  If invalid, it is adjusted
towards the center until it is valid.

(this doesn't completely work)
*/


/*
	CalcFaceVectors

	Fills in texorg, worldtotex. and textoworld
*/
static void
CalcFaceVectors (lightinfo_t *l)
{
	texinfo_t	*tex;
	int			i, j;
	vec3_t		texnormal;
	float		distscale;
	vec_t		dist, len;

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
	_VectorNormalize (texnormal);

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
	dface_t		*s;
	dvertex_t	*v;
	int			i, j, e;
	texinfo_t	*tex;
	vec_t		mins[2], maxs[2], val;

	s = l->face;

	mins[0] = mins[1] = 999999;
	maxs[0] = maxs[1] = -99999;

	tex = &bsp->texinfo[s->texinfo];

	for (i = 0; i < s->numedges; i++) {
		e = bsp->surfedges[s->firstedge + i];
		if (e >= 0)
			v = bsp->vertexes + bsp->edges[e].v[0];
		else
			v = bsp->vertexes + bsp->edges[-e].v[1];

		for (j = 0; j < 2; j++) {
			val = v->point[0] * tex->vecs[j][0] +
				  v->point[1] * tex->vecs[j][1] +	
				  v->point[2] * tex->vecs[j][2] + tex->vecs[j][3];
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
		l->texsize[i] = maxs[i] - mins[i];
		if (l->texsize[i] > 17)
			fprintf (stderr, "Bad surface extents");
	}
}

/*
	CalcPoints

	For each texture aligned grid point, back project onto the plane
	to get the world xyz value of the sample point
*/
static void
CalcPoints (lightinfo_t *l)
{
	int			step, i, j , s, t, w, h;
	vec_t		mids, midt, starts, startt, us, ut;
	vec_t		*surf;
	vec3_t		facemid, move;

	// fill in surforg
	// the points are biased towards the center of the surface
	// to help avoid edge cases just inside walls
	surf = l->surfpt[0];
	mids = (l->exactmaxs[0] + l->exactmins[0]) / 2;
	midt = (l->exactmaxs[1] + l->exactmins[1]) / 2;

	for (j = 0; j < 3; j++)
		facemid[j] = l->texorg[j] + 
			l->textoworld[0][j] * mids + 
			l->textoworld[1][j] * midt;

	if (extrasamples) {
		// extra filtering
		h = (l->texsize[1] + 1) * 2;
		w = (l->texsize[0] + 1) * 2;
		starts = (l->texmins[0] - 0.5) * 16;
		startt = (l->texmins[1] - 0.5) * 16;
		step = 8;
	} else {
		h = l->texsize[1] + 1;
		w = l->texsize[0] + 1;
		starts = l->texmins[0] * 16;
		startt = l->texmins[1] * 16;
		step = 16;
	}

	l->numsurfpt = w * h;
	for (t = 0; t < h; t++) {
		for (s = 0; s < w; s++, surf += 3) {
			us = starts + s * step;
			ut = startt + t * step;

			// if a line can be traced from surf to facemid,
			// the point is good
			for (i = 0; i < 6; i++) {
			// calculate texture point
				for (j = 0; j < 3; j++)
					surf[j] = l->texorg[j] + 
						l->textoworld[0][j] * us +
						l->textoworld[1][j] * ut;

				if (CastRay (facemid, surf) != -1)
					break;	// got it
				if (i & 1) {
					if (us > mids) {
						us -= 8;
						if (us < mids)
							us = mids;
					} else {
						us += 8;
						if (us > mids)
							us = mids;
					}
				} else {
					if (ut > midt) {
						ut -= 8;
						if (ut < midt)
							ut = midt;
					} else {
						ut += 8;
						if (ut > midt)
							ut = midt;
					}
				}

				// move surf 8 pixels towards the center
				VectorSubtract (facemid, surf, move);
				_VectorNormalize (move);
				VectorMultAdd (surf, 8, move, surf);
			}
			if (i == 2)
				c_bad++;
		}	
	}
}

static void
SingleLightFace (entity_t *light, lightinfo_t *l)
{
	int			mapnum, size, c, i;
	qboolean	hit;
	vec3_t		incoming, rel, spotvec;
	vec_t		add, angle, dist, falloff;
	vec_t		*lightsamp, *surf;

	VectorSubtract (light->origin, bsp_origin, rel);
	dist = options.distance * (DotProduct (rel, l->facenormal) - l->facedist);

	// don't bother with lights behind the surface
	if (dist <= 0)
		return;

	// don't bother with light too far away
	if (dist > light->light) {
		c_culldistplane++;
		return;
	}

	if (light->targetent) {
		VectorSubtract (light->targetent->origin, light->origin, spotvec);
		_VectorNormalize (spotvec);
		if (!light->angle)
			falloff = -cos (20 * M_PI / 180);
		else
			falloff = -cos (light->angle / 2 * M_PI / 180);
	} else
		falloff = 0;
		mapnum = 0;
		for (mapnum = 0; mapnum < l->numlightstyles; mapnum++)
			if (l->lightstyles[mapnum] == light->style)
				break;
		lightsamp = l->lightmaps[mapnum];
		if (mapnum == l->numlightstyles) {	
			// init a new light map
			if (mapnum == MAXLIGHTMAPS) {
				printf ("WARNING: Too many light styles on a face\n");
				return;
			}
			size = (l->texsize[1] + 1) * (l->texsize[0] + 1);
			for (i = 0; i < size; i++)
				lightsamp[i] = 0;
		}

		// check it for real
		hit = false;
		c_proper++;

		surf = l->surfpt[0];
		for (c = 0; c < l->numsurfpt; c++, surf += 3) {
			dist = CastRay (light->origin, surf) * options.distance;
			if (dist < 0)
				continue;		// light doesn't reach

			VectorSubtract (light->origin, surf, incoming);
			_VectorNormalize (incoming);
			angle = DotProduct (incoming, l->facenormal);
			if (light->targetent) {	
				// spotlight cutoff
				if (DotProduct (spotvec, incoming) > falloff)
					continue;
			}

			angle = (1.0 - scalecos) + scalecos * angle;
			add = light->light - dist;
			add *= angle;
			if (add < 0)
				continue;
			lightsamp[c] += add;
			if (lightsamp[c] > 1)	
				// ignore real tiny lights
				hit = true;
		}

		if (mapnum == l->numlightstyles && hit) {
			l->lightstyles[mapnum] = light->style;
			l->numlightstyles++;	// the style has some real data now
		}
}

static void
FixMinlight (lightinfo_t *l)
{
	float		minlight;
	int			i, j;

	minlight = minlights[l->surfnum];

	// if minlight is set, there must be a style 0 light map
	if (!minlight)
		return;

	for (i = 0; i < l->numlightstyles; i++) {
		if (l->lightstyles[i] == 0)
			break;
	}
	if (i == l->numlightstyles) {
		if (l->numlightstyles == MAXLIGHTMAPS)
			return;		// oh well..
		for (j = 0; j < l->numsurfpt; j++)
			l->lightmaps[i][j] = minlight;
			l->lightstyles[i] = 0;
			l->numlightstyles++;
	} else {
		for (j = 0; j < l->numsurfpt; j++)
			if (l->lightmaps[i][j] < minlight)
				l->lightmaps[i][j] = minlight;
	}
}

void
LightFace (int surfnum)
{
	int         ofs;
	byte       *out, *outdata;
	dface_t		*f;
	int			lightmapwidth, lightmapsize, size, c, i, j, s, t, w, h;
	lightinfo_t	l;
	vec_t		total;
	vec_t		*light;

	f = bsp->faces + surfnum;

	// some surfaces don't need lightmaps
	f->lightofs = -1;
	for (j = 0; j < MAXLIGHTMAPS; j++)
		f->styles[j] = 255;

	if (bsp->texinfo[f->texinfo].flags & TEX_SPECIAL)	
		// non-lit texture
		return;

	memset(&l, 0, sizeof (l));
	l.surfnum = surfnum;
	l.face = f;

	// rotate plane
	VectorCopy (bsp->planes[f->planenum].normal, l.facenormal);
	l.facedist = bsp->planes[f->planenum].dist;
	if (f->side) {
		VectorNegate (l.facenormal, l.facenormal);
		l.facedist = -l.facedist;
	}

	CalcFaceVectors (&l);
	CalcFaceExtents (&l);
	CalcPoints (&l);

	lightmapwidth = l.texsize[0] + 1;

	size = lightmapwidth * (l.texsize[1] + 1);
	if (size > SINGLEMAP)
		fprintf (stderr, "Bad lightmap size");

	for (i = 0; i < MAXLIGHTMAPS; i++)
		l.lightstyles[i] = 255;

	// cast all lights
	l.numlightstyles = 0;
	for (i = 0; i < num_entities; i++) {
		if (entities[i].light)
			SingleLightFace (&entities[i], &l);
	}

	FixMinlight (&l);

	if (!l.numlightstyles)	
		// no light hitting it
		return;
   
	// save out the values
	for (i = 0; i < MAXLIGHTMAPS; i++)
		f->styles[i] = l.lightstyles[i];

	lightmapsize = size * l.numlightstyles;

	LOCK;
	outdata = out = malloc (lightmapsize);
	UNLOCK;
	ofs = GetFileSpace (lightmapsize);
	f->lightofs = ofs;

	// extra filtering
	h = (l.texsize[1] + 1) * 2;
	w = (l.texsize[0] + 1) * 2;

	for (i = 0; i < l.numlightstyles; i++) {
		if (l.lightstyles[i] == 0xff)
			fprintf (stderr, "Wrote empty lightmap");
		light = l.lightmaps[i];
		c = 0;
		for (t = 0; t <= l.texsize[1]; t++)
			for (s = 0; s <= l.texsize[0]; s++, c++) {
				if (extrasamples) {	
					// filtered sample
					total =	light[t * 2 * w + s * 2] + 
							light[t * 2 * w + s * 2 + 1] + 
							light[(t * 2 + 1) * w + s * 2] + 
							light[(t * 2 + 1) * w + s * 2 + 1];
					total *= 0.25;
				} else
					total = light[c];
				total *= options.range;	// scale before clamping
				if (total > 255)
					total = 255;
				if (total < 0)
					fprintf (stderr, "light < 0");
				*out++ = total;
			}
	}
	LOCK;
	memcpy (lightdata->str + ofs, outdata, lightmapsize);
	free (outdata);
	UNLOCK;
}
