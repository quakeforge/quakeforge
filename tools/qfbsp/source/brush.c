/*
	Copyright (C) 1996-1997  Id Software, Inc.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

	See file, 'COPYING', for details.
*/
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/sys.h"

#include "compat.h"
#include "bsp5.h"

int         numbrushplanes;
plane_t     planes[MAX_MAP_PLANES];

int         numbrushfaces;
mface_t     faces[128];		// beveled clipping hull can generate many extra


/*
	CheckFace

	Note: this will not catch 0 area polygons
*/
static void
CheckFace (face_t *f)
{
	int		 i, j;
	vec_t	*p1, *p2;
	vec_t	 d, edgedist;
	vec3_t	 dir, edgenormal, facenormal;

	if (f->numpoints < 3)
		Sys_Error ("CheckFace: %i points", f->numpoints);

	VectorCopy (planes[f->planenum].normal, facenormal);
	if (f->planeside)
		VectorSubtract (vec3_origin, facenormal, facenormal);

	for (i = 0; i < f->numpoints; i++) {
		p1 = f->pts[i];

		for (j = 0; j < 3; j++)
			if (p1[j] > BOGUS_RANGE || p1[j] < -BOGUS_RANGE)
				Sys_Error ("CheckFace: BUGUS_RANGE: %f", p1[j]);

		j = i + 1 == f->numpoints ? 0 : i + 1;

		// check the point is on the face plane
		d = DotProduct (p1, planes[f->planenum].normal)
			- planes[f->planenum].dist;

		if (d < -ON_EPSILON || d > ON_EPSILON)
			Sys_Error ("CheckFace: point off plane");

		// check the edge isn't degenerate
		p2 = f->pts[j];
		VectorSubtract (p2, p1, dir);

		if (VectorLength (dir) < ON_EPSILON)
			Sys_Error ("CheckFace: degenerate edge");

		CrossProduct (facenormal, dir, edgenormal);
		_VectorNormalize (edgenormal);
		edgedist = DotProduct (p1, edgenormal);
		edgedist += ON_EPSILON;

		// all other points must be on front side
		for (j = 0; j < f->numpoints; j++) {
			if (j == i)
				continue;
			d = DotProduct (f->pts[j], edgenormal);
			if (d > edgedist)
				Sys_Error ("CheckFace: non-convex");
		}
	}
}

static void
ClearBounds (brushset_t *bs)
{
	int		i, j;

	for (j = 0; j < NUM_HULLS; j++)
		for (i = 0; i < 3; i++) {
			bs->mins[i] = 99999;
			bs->maxs[i] = -99999;
		}
}

static void
AddToBounds (brushset_t *bs, vec3_t v)
{
	int		i;

	for (i = 0; i < 3; i++) {
		if (v[i] < bs->mins[i])
			bs->mins[i] = v[i];
		if (v[i] > bs->maxs[i])
			bs->maxs[i] = v[i];
	}
}

int
PlaneTypeForNormal (vec3_t normal)
{
	float	ax, ay, az;

	// NOTE: should these have an epsilon around 1.0?
	if (normal[0] == 1.0)
		return PLANE_X;
	if (normal[1] == 1.0)
		return PLANE_Y;
	if (normal[2] == 1.0)
		return PLANE_Z;
	if (normal[0] == -1.0 || normal[1] == -1.0 || normal[2] == -1.0)
		Sys_Error ("PlaneTypeForNormal: not a canonical vector");

	ax = fabs(normal[0]);
	ay = fabs(normal[1]);
	az = fabs(normal[2]);

	if (ax >= ay && ax >= az)
		return PLANE_ANYX;
	if (ay >= ax && ay >= az)
		return PLANE_ANYY;
	return PLANE_ANYZ;
}

#define	DISTEPSILON		0.01
#define	ANGLEEPSILON	0.00001

static void
NormalizePlane (plane_t *dp)
{
	vec_t	ax, ay, az;

	if (dp->normal[0] == -1.0) {
		dp->normal[0] = 1.0;
		dp->dist = -dp->dist;
	}
	if (dp->normal[1] == -1.0) {
		dp->normal[1] = 1.0;
		dp->dist = -dp->dist;
	}
	if (dp->normal[2] == -1.0) {
		dp->normal[2] = 1.0;
		dp->dist = -dp->dist;
	}

	if (dp->normal[0] == 1.0) {
		dp->type = PLANE_X;
		return;
	}
	if (dp->normal[1] == 1.0) {
		dp->type = PLANE_Y;
		return;
	}
	if (dp->normal[2] == 1.0) {
		dp->type = PLANE_Z;
		return;
	}

	ax = fabs (dp->normal[0]);
	ay = fabs (dp->normal[1]);
	az = fabs (dp->normal[2]);

	if (ax >= ay && ax >= az)
		dp->type = PLANE_ANYX;
	else if (ay >= ax && ay >= az)
		dp->type = PLANE_ANYY;
	else
		dp->type = PLANE_ANYZ;
	if (dp->normal[dp->type - PLANE_ANYX] < 0) {
		VectorSubtract (vec3_origin, dp->normal, dp->normal);
		dp->dist = -dp->dist;
	}
}

/*
	FindPlane

	Returns a global plane number and the side that will be the front
*/
int
FindPlane (plane_t *dplane, int *side)
{
	int		 i;
	plane_t	*dp, pl;
	vec_t	 dot;

	dot = VectorLength(dplane->normal);
	if (dot < 1.0 - ANGLEEPSILON || dot > 1.0 + ANGLEEPSILON)
		Sys_Error ("FindPlane: normalization error");

	pl = *dplane;
	NormalizePlane (&pl);
	if (DotProduct (pl.normal, dplane->normal) > 0)
		*side = 0;
	else
		*side = 1;

	dp = planes;
	for (i = 0; i < numbrushplanes; i++, dp++) {
		dot = DotProduct (dp->normal, pl.normal);
		if (dot > 1.0 - ANGLEEPSILON 
			&& fabs(dp->dist - pl.dist) < DISTEPSILON) {	// regular match
			return i;
		}
	}

	if (numbrushplanes == MAX_MAP_PLANES)
		Sys_Error ("numbrushplanes == MAX_MAP_PLANES");

	planes[numbrushplanes] = pl;

	numbrushplanes++;

	return numbrushplanes - 1;
}

/*
	Turn brushes into groups of faces.
*/

vec3_t      brush_mins, brush_maxs;
face_t     *brush_faces;

#define	ZERO_EPSILON	0.001
static void
CreateBrushFaces (void)
{
	face_t     *f;
	int         i, j, k;
	mface_t    *mf;
	plane_t     plane;
	vec_t       r;
	winding_t  *w;

	brush_mins[0] = brush_mins[1] = brush_mins[2] = 99999;
	brush_maxs[0] = brush_maxs[1] = brush_maxs[2] = -99999;

	brush_faces = NULL;

	for (i = 0; i < numbrushfaces; i++) {
		mf = &faces[i];

		w = BaseWindingForPlane (&mf->plane);

		for (j = 0; j < numbrushfaces && w; j++) {
			if (j == i)
				continue;
			// flip the plane, because we want to keep the back side
			VectorSubtract (vec3_origin, faces[j].plane.normal, plane.normal);
			plane.dist = -faces[j].plane.dist;

			w = ClipWinding (w, &plane, false);
		}

		if (!w)
			continue;					// overcontrained plane

		// this face is a keeper
		f = AllocFace ();
		f->numpoints = w->numpoints;
		if (f->numpoints > MAXEDGES)
			Sys_Error ("f->numpoints > MAXEDGES");

		for (j = 0; j < w->numpoints; j++) {
			for (k = 0; k < 3; k++) {
				r = RINT (w->points[j][k]);
				if (fabs (w->points[j][k] - r) < ZERO_EPSILON)
					f->pts[j][k] = r;
				else
					f->pts[j][k] = w->points[j][k];

				if (f->pts[j][k] < brush_mins[k])
					brush_mins[k] = f->pts[j][k];
				if (f->pts[j][k] > brush_maxs[k])
					brush_maxs[k] = f->pts[j][k];
			}

		}
		FreeWinding (w);
		f->texturenum = mf->texinfo;
		f->planenum = FindPlane (&mf->plane, &f->planeside);
		f->next = brush_faces;
		brush_faces = f;
		CheckFace (f);
	}
}

/*
	BEVELED CLIPPING HULL GENERATION

	This is done by brute force, and could easily get a lot faster if anyone
	cares.
*/

vec3_t      hull_size[3][2] = {
	{{0, 0, 0}, {0, 0, 0}},
	{{-16, -16, -32}, {16, 16, 24}},
	{{-32, -32, -64}, {32, 32, 24}}

};

#define	MAX_HULL_POINTS	64
#define	MAX_HULL_EDGES	128

int         num_hull_points;
vec3_t      hull_points[MAX_HULL_POINTS];
vec3_t      hull_corners[MAX_HULL_POINTS * 8];
int         num_hull_edges;
int         hull_edges[MAX_HULL_EDGES][2];

static void
AddBrushPlane (plane_t *plane)
{
	float       l;
	int         i;
	plane_t    *pl;

	if (numbrushfaces == MAX_FACES)
		Sys_Error ("AddBrushPlane: numbrushfaces == MAX_FACES");
	l = VectorLength (plane->normal);
	if (l < 0.999 || l > 1.001)
		Sys_Error ("AddBrushPlane: bad normal");

	for (i = 0; i < numbrushfaces; i++) {
		pl = &faces[i].plane;
		if (_VectorCompare (pl->normal, plane->normal)
			&& fabs (pl->dist - plane->dist) < ON_EPSILON)
			return;
	}
	faces[i].plane = *plane;
	faces[i].texinfo = faces[0].texinfo;
	numbrushfaces++;
}

/*
	TestAddPlane

	Adds the given plane to the brush description if all of the original brush
	vertexes can be put on the front side
*/
static void
TestAddPlane (plane_t *plane)
{
	int         c, i;
	int         counts[3];
	plane_t     flip;
	plane_t    *pl;
	vec_t       d;
	vec_t      *corner;
	vec3_t      inv;

	// see if the plane has allready been added
	for (i = 0; i < numbrushfaces; i++) {
		pl = &faces[i].plane;
		if (_VectorCompare (plane->normal, pl->normal)
			&& fabs (plane->dist - pl->dist) < ON_EPSILON)
			return;
		VectorSubtract (vec3_origin, plane->normal, inv);
		if (_VectorCompare (inv, pl->normal)
			&& fabs (plane->dist + pl->dist) < ON_EPSILON) return;
	}

	// check all the corner points
	counts[0] = counts[1] = counts[2] = 0;
	c = num_hull_points * 8;

	corner = hull_corners[0];
	for (i = 0; i < c; i++, corner += 3) {
		d = DotProduct (corner, plane->normal) - plane->dist;
		if (d < -ON_EPSILON) {
			if (counts[0])
				return;
			counts[1]++;
		} else if (d > ON_EPSILON) {
			if (counts[1])
				return;
			counts[0]++;
		} else
			counts[2]++;
	}

	// the plane is a seperator
	if (counts[0]) {
		VectorSubtract (vec3_origin, plane->normal, flip.normal);
		flip.dist = -plane->dist;
		plane = &flip;
	}

	AddBrushPlane (plane);
}

/*
	AddHullPoint

	Doesn't add if duplicated
*/
static int
AddHullPoint (vec3_t p, int hullnum)
{
	int			i, x, y, z;
	vec_t	   *c;

	for (i = 0; i < num_hull_points; i++)
		if (_VectorCompare (p, hull_points[i]))
			return i;

	VectorCopy (p, hull_points[num_hull_points]);

	c = hull_corners[i * 8];

	for (x = 0; x < 2; x++)
		for (y = 0; y < 2; y++)
			for (z = 0; z < 2; z++) {
				c[0] = p[0] + hull_size[hullnum][x][0];
				c[1] = p[1] + hull_size[hullnum][y][1];
				c[2] = p[2] + hull_size[hullnum][z][2];
				c += 3;
			}

	if (num_hull_points == MAX_HULL_POINTS)
		Sys_Error ("MAX_HULL_POINTS");

	num_hull_points++;

	return i;
}

/*
	AddHullEdge

	Creates all of the hull planes around the given edge, if not done already
*/
static void
AddHullEdge (vec3_t p1, vec3_t p2, int hullnum)
{
	int         pt1, pt2, a, b, c, d, e, i;
	plane_t     plane;
	vec3_t      edgevec, planeorg, planevec;
	vec_t       l;

	pt1 = AddHullPoint (p1, hullnum);
	pt2 = AddHullPoint (p2, hullnum);

	for (i = 0; i < num_hull_edges; i++)
		if ((hull_edges[i][0] == pt1 && hull_edges[i][1] == pt2)
			|| (hull_edges[i][0] == pt2 && hull_edges[i][1] == pt1))
			return;						// allready added

	if (num_hull_edges == MAX_HULL_EDGES)
		Sys_Error ("MAX_HULL_EDGES");

	hull_edges[i][0] = pt1;
	hull_edges[i][1] = pt2;
	num_hull_edges++;

	VectorSubtract (p1, p2, edgevec);
	_VectorNormalize (edgevec);

	for (a = 0; a < 3; a++) {
		b = (a + 1) % 3;
		c = (a + 2) % 3;
		for (d = 0; d <= 1; d++)
			for (e = 0; e <= 1; e++) {
				VectorCopy (p1, planeorg);
				planeorg[b] += hull_size[hullnum][d][b];
				planeorg[c] += hull_size[hullnum][e][c];

				VectorCopy (vec3_origin, planevec);
				planevec[a] = 1;

				CrossProduct (planevec, edgevec, plane.normal);
				l = VectorLength (plane.normal);
				if (l < 1 - ANGLEEPSILON || l > 1 + ANGLEEPSILON)
					continue;
				plane.dist = DotProduct (planeorg, plane.normal);
				TestAddPlane (&plane);
			}
	}
}

static void
ExpandBrush (int hullnum)
{
	face_t     *f;
	int         i, x, s;
	plane_t     plane, *p;
	vec3_t      corner;

	num_hull_points = 0;
	num_hull_edges = 0;

	// create all the hull points
	for (f = brush_faces; f; f = f->next)
		for (i = 0; i < f->numpoints; i++)
			AddHullPoint (f->pts[i], hullnum);

	// expand all of the planes
	for (i = 0; i < numbrushfaces; i++) {
		p = &faces[i].plane;
		VectorCopy (vec3_origin, corner);
		for (x = 0; x < 3; x++) {
			if (p->normal[x] > 0)
				corner[x] = hull_size[hullnum][1][x];
			else if (p->normal[x] < 0)
				corner[x] = hull_size[hullnum][0][x];
		}
		p->dist += DotProduct (corner, p->normal);
	}

	// add any axis planes not contained in the brush to bevel off corners
	for (x = 0; x < 3; x++)
		for (s = -1; s <= 1; s += 2) {
			// add the plane
			VectorCopy (vec3_origin, plane.normal);
			plane.normal[x] = s;
			if (s == -1)
				plane.dist = -brush_mins[x] + -hull_size[hullnum][0][x];
			else
				plane.dist = brush_maxs[x] + hull_size[hullnum][1][x];
			AddBrushPlane (&plane);
		}

	// add all of the edge bevels
	for (f = brush_faces; f; f = f->next)
		for (i = 0; i < f->numpoints; i++)
			AddHullEdge (f->pts[i], f->pts[(i + 1) % f->numpoints], hullnum);
}

/*
	LoadBrush

	Converts a mapbrush to a bsp brush
*/
static brush_t    *
LoadBrush (mbrush_t *mb, int hullnum)
{
	brush_t    *b;
	char       *name;
	int         contents;
	mface_t    *f;

	// check texture name for attributes
	if (mb->faces->texinfo < 0) {
		// ignore HINT and SKIP in clip hulls
		if (hullnum)
			return NULL;
		contents = CONTENTS_EMPTY;
	} else {
		name = miptex[bsp->texinfo[mb->faces->texinfo].miptex];

		if (!strcasecmp (name, "clip") && hullnum == 0)
			return NULL;		// "clip" brushes don't show up in the draw hull

		if (name[0] == '*' && worldmodel) {	// entities never use water merging
			if (!strncasecmp (name + 1, "lava", 4))
				contents = CONTENTS_LAVA;
			else if (!strncasecmp (name + 1, "slime", 5))
				contents = CONTENTS_SLIME;
			else
				contents = CONTENTS_WATER;
		} else if (!strncasecmp (name, "sky", 3) && worldmodel && hullnum == 0)
			contents = CONTENTS_SKY;
		else
			contents = CONTENTS_SOLID;

		if (hullnum && contents != CONTENTS_SOLID && contents != CONTENTS_SKY)
			return NULL;		// water brushes don't show up in clipping hulls

		// no seperate textures on clip hull
	}

	// create the faces
	brush_faces = NULL;

	numbrushfaces = 0;
	for (f = mb->faces; f; f = f->next) {
		faces[numbrushfaces] = *f;
		if (hullnum)
			faces[numbrushfaces].texinfo = 0;
		numbrushfaces++;
	}

	CreateBrushFaces ();

	if (!brush_faces) {
		printf ("WARNING: couldn't create brush faces\n");
		return NULL;
	}

	if (hullnum) {
		ExpandBrush (hullnum);
		CreateBrushFaces ();
	} else if (mb->detail) {
		face_t     *f;
		for (f = brush_faces; f; f = f->next);
			f->detail = 1;
	}

	// create the brush
	b = AllocBrush ();

	b->contents = contents;
	b->faces = brush_faces;
	VectorCopy (brush_mins, b->mins);
	VectorCopy (brush_maxs, b->maxs);

	return b;
}

static void
Brush_DrawAll (brushset_t *bs)
{
	brush_t    *b;
	face_t     *f;

	for (b = bs->brushes; b; b = b->next)
		for (f = b->faces; f; f = f->next)
			Draw_DrawFace (f);
}

brushset_t *
Brush_LoadEntity (entity_t *ent, int hullnum)
{
	brush_t    *b, *next, *water, *other;
	brushset_t *bset;
	int         numbrushes;
	mbrush_t   *mbr;

	bset = malloc (sizeof (brushset_t));
	memset (bset, 0, sizeof (brushset_t));
	ClearBounds (bset);

	numbrushes = 0;
	other = water = NULL;

	qprintf ("--- Brush_LoadEntity ---\n");

	for (mbr = ent->brushes; mbr; mbr = mbr->next) {
		b = LoadBrush (mbr, hullnum);
		if (!b)
			continue;

		numbrushes++;

		if (b->contents != CONTENTS_SOLID) {
			b->next = water;
			water = b;
		} else {
			b->next = other;
			other = b;
		}

		AddToBounds (bset, b->mins);
		AddToBounds (bset, b->maxs);
	}

	// add all of the water textures at the start
	for (b = water; b; b = next) {
		next = b->next;
		b->next = other;
		other = b;
	}

	bset->brushes = other;

	brushset = bset;
	Brush_DrawAll (bset);

	qprintf ("%i brushes read\n", numbrushes);

	return bset;
}
