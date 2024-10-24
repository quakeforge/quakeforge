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
#include "QF/va.h"

#include "compat.h"

#include "tools/qfbsp/include/brush.h"
#include "tools/qfbsp/include/bsp5.h"
#include "tools/qfbsp/include/draw.h"
#include "tools/qfbsp/include/options.h"
#include "tools/qfbsp/include/surfaces.h"

#define NORMAL_EPSILON 0.000001
#define DIST_EPSILON 0.0001

/**	\addtogroup qfbsp_brush
*/
//@{

planeset_t  planes = DARRAY_STATIC_INIT (1024);

int         numbrushfaces;
mface_t     faces[MAX_FACES];	// beveled clipping hull can generate many extra

static entity_t *CurrentEntity;

brush_t *
AllocBrush (void)
{
	brush_t    *b;

	b = malloc (sizeof (brush_t));
	memset (b, 0, sizeof (brush_t));

	return b;
}

/**	Check the face for validity.

	\param f		The face to check.

	\note Does not catch 0 area polygons.
*/
static void
CheckFace (const face_t *f)
{
	int		 i, j;
	vec_t	*p1, *p2;
	vec_t	 d, edgedist;
	vec3_t	 dir, edgenormal, facenormal;

	if (f->points->numpoints < 3)
		Sys_Error ("CheckFace: %i points", f->points->numpoints);

	VectorCopy (planes.a[f->planenum].normal, facenormal);
	if (f->planeside)
		VectorNegate (facenormal, facenormal);

	for (i = 0; i < f->points->numpoints; i++) {
		p1 = f->points->points[i];

		for (j = 0; j < 3; j++)
			if (p1[j] > BOGUS_RANGE || p1[j] < -BOGUS_RANGE)
				Sys_Error ("CheckFace: BUGUS_RANGE: %f", p1[j]);

		j = i + 1 == f->points->numpoints ? 0 : i + 1;

		// check the point is on the face plane
		d = PlaneDiff (p1, &planes.a[f->planenum]);

		if (d < -ON_EPSILON || d > ON_EPSILON) {
			if (!options.fix_point_off_plane) {
				Sys_Error ("CheckFace: point off plane: %g @ (%g %g %g)\n", d,
						   p1[0], p1[1], p1[2]);
			}
			// point off plane autofix
			if (options.verbosity > 1)
				printf ("CheckFace: point off plane: %g @ (%g %g %g)\n", d,
						p1[0], p1[1], p1[2]);
			_VectorMA (p1, -d, planes.a[f->planenum].normal, p1);
		}

		// check the edge isn't degenerate
		p2 = f->points->points[j];
		VectorSubtract (p2, p1, dir);

		if (VectorLength (dir) < ON_EPSILON)
			Sys_Error ("CheckFace: degenerate edge");

		CrossProduct (facenormal, dir, edgenormal);
		_VectorNormalize (edgenormal);
		edgedist = DotProduct (p1, edgenormal);
		edgedist += ON_EPSILON;

		// all other points must be on front side
		for (j = 0; j < f->points->numpoints; j++) {
			if (j == i)
				continue;
			d = DotProduct (f->points->points[j], edgenormal);
			if (d > edgedist)
				Sys_Error ("CheckFace: non-convex");
		}
	}
}

/**	Initialize the bounding box of the brush set.

	\param bs		The brush set of which to initialize the bounding box.
*/
static void
ClearBounds (brushset_t *bs)
{
	int		i, j;

	for (j = 0; j < NUM_HULLS; j++) {
		for (i = 0; i < 3; i++) {
			bs->mins[i] = BOGUS_RANGE;
			bs->maxs[i] = -BOGUS_RANGE;
		}
	}
}

/**	Grow the bounding box of the brush set to include the vector.

	\param bs		The brush set of which to grown the bounding box.
	\param v		The vector to be included in the bounding box.
*/
static void
AddToBounds (brushset_t *bs, const vec3_t v)
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
PlaneTypeForNormal (const vec3_t normal)
{
	float	ax, ay, az;
	int     type;

	// NOTE: should these have an epsilon around 1.0?
	if (normal[0] == 1.0)
		return PLANE_X;
	if (normal[1] == 1.0)
		return PLANE_Y;
	if (normal[2] == 1.0)
		return PLANE_Z;
	if (normal[0] == -1.0 || normal[1] == -1.0 || normal[2] == -1.0)
		Sys_Error ("PlaneTypeForNormal: not a canonical vector (%g %g %g)",
				   normal[0], normal[1], normal[2]);

	ax = fabs(normal[0]);
	ay = fabs(normal[1]);
	az = fabs(normal[2]);

	if (az >= ax && az >= ay)
		type = PLANE_ANYZ;
	else if (ay >= ax && ay >= az)
		type = PLANE_ANYY;
	else
		type = PLANE_ANYX;
	if (normal[type - PLANE_ANYX] < 0)
		Sys_Error ("PlaneTypeForNormal: not a canonical vector (%g %g %g) %d",
				   normal[0], normal[1], normal[2], type);
	return type;
}

#define	DISTEPSILON		0.01
#define	ANGLEEPSILON	0.00001

int
NormalizePlane (plane_t *dp)
{
	vec_t       ax, ay, az;
	int         flip = 0;

	// Make axis aligned planes point to +inf.
	if (dp->normal[0] == -1.0) {
		dp->normal[0] = 1.0;
		dp->dist = -dp->dist;
		flip = 1;
	} else if (dp->normal[1] == -1.0) {
		dp->normal[1] = 1.0;
		dp->dist = -dp->dist;
		flip = 1;
	} else if (dp->normal[2] == -1.0) {
		dp->normal[2] = 1.0;
		dp->dist = -dp->dist;
		flip = 1;
	}

	// For axis aligned planes, set the plane type and ensure the normal
	// vector is mathematically correct.
	if (dp->normal[0] == 1.0) {
		dp->type = PLANE_X;
		dp->normal[1] = dp->normal[2] = 0.0;
		return flip;
	}
	if (dp->normal[1] == 1.0) {
		dp->type = PLANE_Y;
		dp->normal[0] = dp->normal[2] = 0.0;
		return flip;
	}
	if (dp->normal[2] == 1.0) {
		dp->type = PLANE_Z;
		dp->normal[0] = dp->normal[1] = 0.0;
		return flip;
	}

	// Find out with which axis the plane is most aligned.
	ax = fabs (dp->normal[0]);
	ay = fabs (dp->normal[1]);
	az = fabs (dp->normal[2]);

	if (az >= ax && az >= ay)
		dp->type = PLANE_ANYZ;
	else if (ay >= ax && ay >= az)
		dp->type = PLANE_ANYY;
	else
		dp->type = PLANE_ANYX;
	// Make the plane's normal point towards +inf along its primary axis.
	if (dp->normal[dp->type - PLANE_ANYX] < 0) {
		VectorNegate (dp->normal, dp->normal);
		dp->dist = -dp->dist;
		flip = 1;
	}
	return flip;
}

bool
PlaneEqual (const plane_t *p1, const plane_t *p2)
{
	return (fabs(p1->normal[0] - p2->normal[0]) < NORMAL_EPSILON
			&& fabs(p1->normal[1] - p2->normal[1]) < NORMAL_EPSILON
			&& fabs(p1->normal[2] - p2->normal[2]) < NORMAL_EPSILON
			&& fabs(p1->dist - p2->dist) < DIST_EPSILON);
}

int
FindPlane (const plane_t *dplane, int *side)
{
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

	dp = planes.a;
	for (size_t i = 0; i < planes.size; i++, dp++) {
		dot = DotProduct (dp->normal, pl.normal);
		if (dot > 1.0 - ANGLEEPSILON
			&& fabs(dp->dist - pl.dist) < DISTEPSILON) {	// regular match
			return i;
		}
	}

	DARRAY_APPEND (&planes, pl);

	return planes.size - 1;
}

/*
	Turn brushes into groups of faces.
*/

vec3_t      brush_mins, brush_maxs;
face_t     *brush_faces;

/**	Find the entity with the matching target name.

	\param targetname The target name for which to search.
	\return			The matching entity or NULL if not found.
*/
static entity_t *
FindTargetEntity (const char *targetname)
{
	int         entnum;

	for (entnum = 0; entnum < num_entities; entnum++)
		if (!strcmp (targetname,
					 ValueForKey (&entities[entnum], "targetname")))
			return &entities[entnum];
	return 0;
}

#define	ZERO_EPSILON	0.001

/**	Create the faces of the active brush.
*/
static void
CreateBrushFaces (void)
{
	face_t     *f;
	int         i, j, k, rotate;
	mface_t    *mf;
	plane_t     plane;
	vec_t       r;
	winding_t  *w;
	vec3_t      offset;

	VectorZero (offset);
	brush_mins[0] = brush_mins[1] = brush_mins[2] = BOGUS_RANGE;
	brush_maxs[0] = brush_maxs[1] = brush_maxs[2] = -BOGUS_RANGE;

	brush_faces = NULL;

	rotate = !strncmp (ValueForKey (CurrentEntity, "classname"), "rotate_", 7);
	if (rotate) {
		entity_t   *FoundEntity;
		const char *searchstring;

		searchstring = ValueForKey (CurrentEntity, "target");
		FoundEntity = FindTargetEntity (searchstring);
		if (FoundEntity)
			GetVectorForKey (FoundEntity, "origin", offset);

		SetKeyValue (CurrentEntity, "origin",
					 va (0, "%g %g %g", VectorExpand (offset)));
	}

	for (i = 0; i < numbrushfaces; i++) {
		mf = &faces[i];

		w = BaseWindingForPlane (&mf->plane);

		for (j = 0; j < numbrushfaces && w; j++) {
			if (j == i)
				continue;
			// flip the plane, because we want to keep the back side
			VectorNegate (faces[j].plane.normal, plane.normal);
			plane.dist = -faces[j].plane.dist;

			w = ClipWinding (w, &plane, options.keepon);
		}

		if (!w)
			continue;					// overconstrained plane

		// this face is a keeper
		f = AllocFace ();
		f->points = w;

		for (j = 0; j < w->numpoints; j++) {
			vec_t      *v = f->points->points[j];
			VectorSubtract (v, offset, v);
			for (k = 0; k < 3; k++) {
				r = RINT (v[k]);
				if (fabs (v[k] - r) < ZERO_EPSILON)
					v[k] = r;

				if (v[k] < brush_mins[k])
					brush_mins[k] = v[k];
				if (v[k] > brush_maxs[k])
					brush_maxs[k] = v[k];
			}

		}

		f->texturenum = mf->texinfo;
		f->planenum = FindPlane (&mf->plane, &f->planeside);
		f->next = brush_faces;
		brush_faces = f;
		CheckFace (f);
	}

	// Rotatable objects have to have a bounding box big enough
	// to account for all its rotations.
	if (rotate) {
		vec_t       delta, min, max;

		min = brush_mins[0];
		if (min > brush_mins[1])
			min = brush_mins[1];
		if (min > brush_mins[2])
			min = brush_mins[2];

		max = brush_maxs[0];
		if (max < brush_maxs[1])
			max = brush_maxs[1];
		if (max < brush_maxs[2])
			max = brush_maxs[2];

		delta = fabs(max);
		if (fabs(min) > delta)
			delta = fabs(min);

		for (k = 0; k < 3; k++) {
			brush_mins[k] = -delta;
			brush_maxs[k] = delta;
		}
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

#define	MAX_HULL_POINTS	256
#define	MAX_HULL_EDGES	MAX_HULL_POINTS * 2

int         num_hull_points;
vec3_t      hull_points[MAX_HULL_POINTS];
vec3_t      hull_corners[MAX_HULL_POINTS * 8];
int         num_hull_edges;
int         hull_edges[MAX_HULL_EDGES][2];

static void
AddBrushPlane (const plane_t *plane)
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
TestAddPlane (const plane_t *plane)
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
		VectorNegate (plane->normal, inv);
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
		VectorNegate (plane->normal, flip.normal);
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
AddHullPoint (const vec3_t p, int hullnum)
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
AddHullEdge (const vec3_t p1, const vec3_t p2, int hullnum)
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

				VectorZero (planevec);
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
		for (i = 0; i < f->points->numpoints; i++)
			AddHullPoint (f->points->points[i], hullnum);

	// expand all of the planes
	for (i = 0; i < numbrushfaces; i++) {
		p = &faces[i].plane;
		VectorZero (corner);
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
			VectorZero (plane.normal);
			plane.normal[x] = s;
			if (s == -1)
				plane.dist = -brush_mins[x] + -hull_size[hullnum][0][x];
			else
				plane.dist = brush_maxs[x] + hull_size[hullnum][1][x];
			AddBrushPlane (&plane);
		}

	// add all of the edge bevels
	for (f = brush_faces; f; f = f->next)
		for (i = 0; i < f->points->numpoints; i++)
			AddHullEdge (f->points->points[i],
						 f->points->points[(i + 1) % f->points->numpoints],
						 hullnum);
}

/*
	LoadBrush

	Converts a mapbrush to a bsp brush
*/
static brush_t *
LoadBrush (const mbrush_t *mb, int hullnum)
{
	brush_t    *b;
	const char *name;
	int         contents;
	const mface_t *f;

	// check texture name for attributes
	if (mb->faces->texinfo < 0) {
		// ignore HINT and SKIP in clip hulls
		if (hullnum)
			return NULL;
		contents = CONTENTS_EMPTY;
	} else {
		name = miptexnames[bsp->texinfo[mb->faces->texinfo].miptex];

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
		for (f = brush_faces; f; f = f->next)
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
Brush_DrawAll (const brushset_t *bs)
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

	CurrentEntity = ent;

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

//@}
