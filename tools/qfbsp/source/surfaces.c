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
# include "config.h"
#endif

static __attribute__ ((used)) const char rcsid[] =
	"$Id$";

#ifdef HAVE_STRING_H
# include "string.h"
#endif
#include <stdlib.h>

#include "QF/sys.h"

#include "bsp5.h"
#include "csg4.h"
#include "options.h"
#include "region.h"
#include "surfaces.h"
#include "winding.h"

/*
	a surface has all of the faces that could be drawn on a given plane

	the outside filling stage can remove some of them so a better bsp can be
	generated
*/
surface_t   newcopy_t;
int         subdivides;
int         c_activefaces, c_peakfaces;
int         c_activesurfaces, c_peaksurfaces;


face_t *
AllocFace (void)
{
	face_t     *f;

	c_activefaces++;
	if (c_activefaces > c_peakfaces)
		c_peakfaces = c_activefaces;

	f = malloc (sizeof (face_t));
	memset (f, 0, sizeof (face_t));
	f->planenum = -1;

	return f;
}

void
FreeFace (face_t *f)
{
	c_activefaces--;
	free (f);
}

surface_t *
AllocSurface (void)
{
	surface_t  *s;

	s = malloc (sizeof (surface_t));
	memset (s, 0, sizeof (surface_t));

	c_activesurfaces++;
	if (c_activesurfaces > c_peaksurfaces)
		c_peaksurfaces = c_activesurfaces;

	return s;
}

void
FreeSurface (surface_t *s)
{
	c_activesurfaces--;
	free (s);
}

/*
	SubdivideFace

	If the face is >256 in either texture direction, carve a valid sized
	piece off and insert the remainder in the next link
*/
void
SubdivideFace (face_t *f, face_t **prevptr)
{
	face_t     *front, *back, *next;
	float       mins, maxs;
	int         axis, i;
	plane_t     plane;
	texinfo_t  *tex;
	vec_t       v;

	if (f->texturenum < 0)	// don't subdivide HINT or SKIP
		return;

	// special (non-surface cached) faces don't need subdivision
	tex = &bsp->texinfo[f->texturenum];

	if (tex->flags & TEX_SPECIAL)
		return;


	for (axis = 0; axis < 2; axis++) {
		while (1) {
			mins = BOGUS_RANGE;
			maxs = -BOGUS_RANGE;

			for (i = 0; i < f->points->numpoints; i++) {
				v = DotProduct (f->points->points[i], tex->vecs[axis]);
				if (v < mins)
					mins = v;
				if (v > maxs)
					maxs = v;
			}

			if (maxs - mins <= options.subdivide_size)
				break;

			// split it
			subdivides++;

			VectorCopy (tex->vecs[axis], plane.normal);
			v = VectorLength (plane.normal);
			_VectorNormalize (plane.normal);
			plane.dist = (mins + options.subdivide_size - 16) / v;
			next = f->next;
			SplitFace (f, &plane, &front, &back);
			if (!front || !back)
				Sys_Error ("SubdivideFace: didn't split the polygon");
			*prevptr = back;
			back->next = front;
			front->next = next;
			f = back;
		}
	}
}

/*
	GatherNodeFaces

	Frees the current node tree and returns a new chain of the surfaces that
	have inside faces.
*/
static void
GatherNodeFaces_r (node_t *node)
{
	face_t     *next, *f;

	if (node->planenum != PLANENUM_LEAF) {
		// decision node
		for (f = node->faces; f; f = next) {
			next = f->next;
			if (!f->points) {		// face was removed outside
				FreeFace (f);
			} else {
				f->next = validfaces[f->planenum];
				validfaces[f->planenum] = f;
			}
		}

		GatherNodeFaces_r (node->children[0]);
		GatherNodeFaces_r (node->children[1]);

		free (node);
	} else {
		// leaf node
		free (node);
	}
}

surface_t *
GatherNodeFaces (node_t *headnode)
{
	memset (validfaces, 0, sizeof (validfaces));
	GatherNodeFaces_r (headnode);
	return BuildSurfaces ();
}

//===========================================================================

typedef struct hashvert_s {
	struct hashvert_s *next;
	vec3_t      point;
	int         num;
	int         numplanes;				// for corner determination
	int         planenums[2];
	int         numedges;
} hashvert_t;

#define	POINT_EPSILON	0.01

int         c_cornerverts;

hashvert_t  hvertex[MAX_MAP_VERTS];
hashvert_t *hvert_p;

face_t     *edgefaces[MAX_MAP_EDGES][2];
int         firstmodeledge = 1;
int         firstmodelface;

//============================================================================

#define	NUM_HASH	4096

hashvert_t *hashverts[NUM_HASH];

static vec3_t hash_min, hash_scale;

static void
InitHash (void)
{
	int         i;
	int         newsize[2];
	vec3_t      size;
	vec_t       scale, volume;

	memset (hashverts, 0, sizeof (hashverts));

	for (i = 0; i < 3; i++) {
		hash_min[i] = -8000;
		size[i] = 16000;
	}

	volume = size[0] * size[1];

	scale = sqrt (volume / NUM_HASH);

	newsize[0] = size[0] / scale;
	newsize[1] = size[1] / scale;

	hash_scale[0] = newsize[0] / size[0];
	hash_scale[1] = newsize[1] / size[1];
	hash_scale[2] = newsize[1];

	hvert_p = hvertex;
}

static unsigned
HashVec (vec3_t vec)
{
	unsigned    h;

	h = hash_scale[0] * (vec[0] - hash_min[0]) * hash_scale[2]
		+ hash_scale[1] * (vec[1] - hash_min[1]);
	if (h >= NUM_HASH)
		return NUM_HASH - 1;
	return h;
}

static int
GetVertex (vec3_t in, int planenum)
{
	hashvert_t *hv;
	int         h, i;
	vec3_t      vert;
	dvertex_t   v;

	for (i = 0; i < 3; i++) {
		if (fabs (in[i] - RINT (in[i])) < 0.001)
			vert[i] = RINT (in[i]);
		else
			vert[i] = in[i];
	}

	h = HashVec (vert);

	for (hv = hashverts[h]; hv; hv = hv->next) {
		if (fabs (hv->point[0] - vert[0]) < POINT_EPSILON
			&& fabs (hv->point[1] - vert[1]) < POINT_EPSILON
			&& fabs (hv->point[2] - vert[2]) < POINT_EPSILON) {
			hv->numedges++;
			if (hv->numplanes == 3)
				return hv->num;			// already known to be a corner
			for (i = 0; i < hv->numplanes; i++)
				if (hv->planenums[i] == planenum)
					return hv->num;		// already know this plane
			if (hv->numplanes == 2)
				c_cornerverts++;
			else
				hv->planenums[hv->numplanes] = planenum;
			hv->numplanes++;
			return hv->num;
		}
	}

	hv = hvert_p;
	hv->numedges = 1;
	hv->numplanes = 1;
	hv->planenums[0] = planenum;
	hv->next = hashverts[h];
	hashverts[h] = hv;
	VectorCopy (vert, hv->point);
	hv->num = bsp->numvertexes;
	if (hv->num == MAX_MAP_VERTS)
		Sys_Error ("GetVertex: MAX_MAP_VERTS");
	hvert_p++;

	// emit a vertex
	if (bsp->numvertexes == MAX_MAP_VERTS)
		Sys_Error ("numvertexes == MAX_MAP_VERTS");

	v.point[0] = vert[0];
	v.point[1] = vert[1];
	v.point[2] = vert[2];

	BSP_AddVertex (bsp, &v);

	return hv->num;
}

//===========================================================================

int         c_tryedges;

/*
	GetEdge

	Don't allow four way edges
*/
static int
GetEdge (vec3_t p1, vec3_t p2, face_t *f)
{
	dedge_t     edge;
	int         v1, v2, i;

	if (!f->contents[0])
		Sys_Error ("GetEdge: 0 contents");

	c_tryedges++;
	v1 = GetVertex (p1, f->planenum);
	v2 = GetVertex (p2, f->planenum);
	for (i = firstmodeledge; i < bsp->numedges; i++) {
		if (v1 == bsp->edges[i].v[1] && v2 == bsp->edges[i].v[0]
			&& !edgefaces[i][1]
			&& edgefaces[i][0]->contents[0] == f->contents[0]) {
			edgefaces[i][1] = f;
			return -i;
		}
	}

	// emit an edge
	if (bsp->numedges == MAX_MAP_EDGES)
		Sys_Error ("numedges == MAX_MAP_EDGES");
	edge.v[0] = v1;
	edge.v[1] = v2;
	BSP_AddEdge (bsp, &edge);
	edgefaces[i][0] = f;

	return i;
}

static void
FindFaceEdges (face_t *face)
{
	int         i;
	winding_t  *facep = face->points;

	face->outputnumber = -1;
	face->edges = malloc (sizeof (int) * facep->numpoints);

	for (i = 0; i < facep->numpoints; i++)
		face->edges[i] = GetEdge (facep->points[i],
								  facep->points[(i + 1) % facep->numpoints],
								  face);
}

static void
MakeFaceEdges_r (node_t *node)
{
	face_t     *f;

	if (node->planenum == PLANENUM_LEAF)
		return;

	for (f = node->faces; f; f = f->next)
		if (f->texturenum >= 0)
			FindFaceEdges (f);

	MakeFaceEdges_r (node->children[0]);
	MakeFaceEdges_r (node->children[1]);
}

void
MakeFaceEdges (node_t *headnode)
{
	qprintf ("----- MakeFaceEdges -----\n");

	InitHash ();
	c_tryedges = 0;
	c_cornerverts = 0;

	MakeFaceEdges_r (headnode);

//	CheckEdges ();

	GrowNodeRegions (headnode);

	firstmodeledge = bsp->numedges;
	firstmodelface = bsp->numfaces;
}
