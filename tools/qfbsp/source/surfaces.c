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

#ifdef HAVE_STRING_H
# include "string.h"
#endif
#include <stdlib.h>

#include "QF/progs.h"	// for PR_RESMAP
#include "QF/sys.h"

#include "tools/qfbsp/include/bsp5.h"
#include "tools/qfbsp/include/brush.h"
#include "tools/qfbsp/include/csg4.h"
#include "tools/qfbsp/include/options.h"
#include "tools/qfbsp/include/region.h"
#include "tools/qfbsp/include/surfaces.h"

/**	\addtogroup qfbsp_surface
*/
//@{

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
				f->next = validfaces.a[f->planenum];
				validfaces.a[f->planenum] = f;
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
	DARRAY_RESIZE (&validfaces, planes.size);
	memset (validfaces.a, 0, validfaces.size * sizeof (validfaces.a[0]));
	GatherNodeFaces_r (headnode);
	return BuildSurfaces ();
}

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

static PR_RESMAP (hashvert_t) hvertex;

#define EDGEFACE_CHUNK 4096
int         numedgefaces = 0;
edgeface_t *edgefaces = 0;
int         firstmodeledge = 1;
int         firstmodelface;

#define	NUM_HASH	4096

hashvert_t *hashverts[NUM_HASH];

static vec3_t hash_min, hash_scale;

/**	Initialize the vertex hash table.
*/
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

	PR_RESRESET (hvertex);
}

/**	Calulate the hash value of a vector.

	\param vec		The vector for which to calculate the hash value.
	\return			The hash value of the vector.
*/
static unsigned
HashVec (const vec3_t vec)
{
	unsigned    h;

	h = hash_scale[0] * (vec[0] - hash_min[0]) * hash_scale[2]
		+ hash_scale[1] * (vec[1] - hash_min[1]);
	if (h >= NUM_HASH)
		return NUM_HASH - 1;
	return h;
}

/**	Get the vertex number for the vertex.

	\param in		The vertex for which to get the number.
	\param planenum The plane on which this vertex is.
*/
static int
GetVertex (const vec3_t in, int planenum)
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

	hv = PR_RESNEW (hvertex);
	hv->numedges = 1;
	hv->numplanes = 1;
	hv->planenums[0] = planenum;
	hv->next = hashverts[h];
	hashverts[h] = hv;
	VectorCopy (vert, hv->point);
	hv->num = bsp->numvertexes;

	// emit a vertex
	v.point[0] = vert[0];
	v.point[1] = vert[1];
	v.point[2] = vert[2];

	BSP_AddVertex (bsp, &v);

	return hv->num;
}

int         c_tryedges;

/**	Find an edge for the two vertices.

	If an edge can not be found, create a new one.  Will not create a three
	(or more) face edge.

	\param p1		The first vertex.
	\param p2		The second vertex.
	\param f		The face of which the two vertices form an edge.
	\return			The edge number. For a re-used edge, the edge number will
					be negative, indicating the ends of the edge are reversed.
*/
static int
GetEdge (const vec3_t p1, const vec3_t p2, face_t *f)
{
	dedge_t     edge;
	unsigned    v1, v2;
	int         i;

	if (!f->contents[0])
		Sys_Error ("GetEdge: 0 contents");

	c_tryedges++;
	// get the vertex numbers for the two vertices
	v1 = GetVertex (p1, f->planenum);
	v2 = GetVertex (p2, f->planenum);

	// search for an edge that uses the two vertices in the opposite direction
	// but does not yet have a second face.
	for (i = firstmodeledge; i < bsp->numedges; i++) {
		if (v1 == bsp->edges[i].v[1] && v2 == bsp->edges[i].v[0]
			&& !edgefaces[i].f[1]
			&& edgefaces[i].f[0]->contents[0] == f->contents[0]) {
			edgefaces[i].f[1] = f;
			return -i;
		}
	}

	// Create a new edge.
	while (bsp->numedges >= numedgefaces) {
		numedgefaces += EDGEFACE_CHUNK;
		edgefaces = realloc (edgefaces, numedgefaces * sizeof (edgeface_t));
	}
	edge.v[0] = v1;
	edge.v[1] = v2;
	BSP_AddEdge (bsp, &edge);
	edgefaces[i].f[0] = f;
	edgefaces[i].f[1] = 0;

	return i;
}

/**	Give the face edges.

	\param face		The face to which edges will be given.
*/
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

/**	Recurse through the bsp tree, adding edges to the faces.

	\param node		bsp tree node.
*/
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

//@}
