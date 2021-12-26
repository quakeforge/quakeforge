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

#include "QF/sys.h"

#include "tools/qfbsp/include/brush.h"
#include "tools/qfbsp/include/bsp5.h"
#include "tools/qfbsp/include/csg4.h"
#include "tools/qfbsp/include/draw.h"
#include "tools/qfbsp/include/merge.h"
#include "tools/qfbsp/include/surfaces.h"

/**	\addtogroup qfbsp_merge
*/
//@{

#define CONTINUOUS_EPSILON	0.001


/**	Try to merge two polygons.

	If two polygons share a common edge and the edges that meet at the common
	points are both inside the other polygons, merge them. The two polygons
	must be on the same plane, the same side of the plane, have the same
	texture and have the same contents on each side.

	\param f1		The first face.
	\param f2		The second face.
	\return			The new face or NULL if the faces could not be merged.

	\note The originals will NOT be freed.
*/
static face_t *
TryMerge (const face_t *f1, const face_t *f2)
{
	face_t     *newf;
	int         i, j, k, l;
	plane_t    *plane;
	qboolean    keep1, keep2;
	vec3_t      normal, delta, planenormal;
	vec_t       dot;
	vec_t      *p1, *p2, *p3, *p4, *back;
	winding_t  *f1p, *f2p, *newfp;

	if (!(f1p = f1->points) || !(f2p = f2->points))
		return NULL;
	if (f1->planeside != f2->planeside)
		return NULL;
	if (f1->texturenum != f2->texturenum)
		return NULL;
	if (f1->contents[0] != f2->contents[0])
		return NULL;
	if (f1->contents[1] != f2->contents[1])
		return NULL;

	p1 = p2 = NULL;

	// find a common edge
	for (i = 0; i < f1p->numpoints; i++) {
		p1 = f1p->points[i];
		p2 = f1p->points[(i + 1) % f1p->numpoints];
		for (j = 0; j < f2p->numpoints; j++) {
			p3 = f2p->points[j];
			p4 = f2p->points[(j + 1) % f2p->numpoints];
			for (k = 0; k < 3; k++) {
				if (fabs (p1[k] - p4[k]) > EQUAL_EPSILON)
					break;
				if (fabs (p2[k] - p3[k]) > EQUAL_EPSILON)
					break;
			}
			if (k == 3)
				goto found_edge;
		}
	}
	return NULL;					// no matching edges
found_edge:
	// check slope of connected lines
	// if the slopes are colinear, the point can be removed
	plane = &planes.a[f1->planenum];
	VectorCopy (plane->normal, planenormal);
	if (f1->planeside)
		VectorNegate (planenormal, planenormal);

	back = f1p->points[(i + f1p->numpoints - 1) % f1p->numpoints];
	VectorSubtract (p1, back, delta);
	CrossProduct (planenormal, delta, normal);
	_VectorNormalize (normal);

	back = f2p->points[(j + 2) % f2p->numpoints];
	VectorSubtract (back, p1, delta);
	dot = DotProduct (delta, normal);
	if (dot > CONTINUOUS_EPSILON)
		return NULL;					// not a convex polygon
	keep1 = dot < -CONTINUOUS_EPSILON;

	back = f1p->points[(i + 2) % f1p->numpoints];
	VectorSubtract (back, p2, delta);
	CrossProduct (planenormal, delta, normal);
	_VectorNormalize (normal);

	back = f2p->points[(j + f2p->numpoints - 1) % f2p->numpoints];
	VectorSubtract (back, p2, delta);
	dot = DotProduct (delta, normal);
	if (dot > CONTINUOUS_EPSILON)
		return NULL;					// not a convex polygon
	keep2 = dot < -CONTINUOUS_EPSILON;

	// build the new polygon

	k = f1p->numpoints + f2p->numpoints - 4 + keep1 + keep2;
	newf = NewFaceFromFace (f1);
	newfp = newf->points = NewWinding (k);

	// copy first polygon
	for (k = (i + 1) % f1p->numpoints; k != i; k = (k + 1) % f1p->numpoints) {
		if (k == (i + 1) % f1p->numpoints && !keep2)
			continue;

		VectorCopy (f1p->points[k], newfp->points[newfp->numpoints]);
		newfp->numpoints++;
	}

	// copy second polygon
	for (l = (j + 1) % f2p->numpoints; l != j; l = (l + 1) % f2p->numpoints) {
		if (l == (j + 1) % f2p->numpoints && !keep1)
			continue;
		VectorCopy (f2p->points[l], newfp->points[newfp->numpoints]);
		newfp->numpoints++;
	}

	return newf;
}

qboolean    mergedebug;
face_t *
MergeFaceToList (face_t *face, face_t *list)
{
	face_t     *newf, *f;

	for (f = list; f; f = f->next) {
//		CheckColinear (f);
		if (mergedebug) {
			Draw_ClearWindow ();
			Draw_DrawFace (face);
			Draw_DrawFace (f);
			Draw_SetBlack ();
		}
		newf = TryMerge (face, f);
		if (!newf)
			continue;
		FreeFace (face);
		FreeWinding (f->points);	// merged out
		f->points = 0;
		return MergeFaceToList (newf, list);
	}

	// didn't merge, so add at start
	face->next = list;
	return face;
}

face_t *
FreeMergeListScraps (face_t *merged)
{
	face_t     *head, *next;

	head = NULL;
	for (; merged; merged = next) {
		next = merged->next;
		if (!merged->points)
			FreeFace (merged);
		else {
			merged->next = head;
			head = merged;
		}
	}

	return head;
}

void
MergePlaneFaces (surface_t *plane)
{
	face_t     *merged, *next, *f1;

	merged = NULL;

	for (f1 = plane->faces; f1; f1 = next) {
		next = f1->next;
		merged = MergeFaceToList (f1, merged);
	}

	// chain all of the non-empty faces to the plane
	plane->faces = FreeMergeListScraps (merged);
}

void
MergeAll (surface_t *surfhead)
{
	face_t     *f;
	int         mergefaces;
	surface_t  *surf;

	printf ("---- MergeAll ----\n");

	mergefaces = 0;
	for (surf = surfhead; surf; surf = surf->next) {
		MergePlaneFaces (surf);
		Draw_ClearWindow ();
		for (f = surf->faces; f; f = f->next) {
			Draw_DrawFace (f);
			mergefaces++;
		}
	}

	printf ("%i mergefaces\n", mergefaces);
}

//@}
