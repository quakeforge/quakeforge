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
static const char rcsid[] =
	"$Id$";

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#ifdef HAVE_STRING_H
# include "string.h"
#endif

#include "QF/sys.h"

#include "bsp5.h"
#include "options.h"

typedef struct wvert_s {
	vec_t       t;
	struct wvert_s *prev, *next;
} wvert_t;

typedef struct wedge_s {
	struct wedge_s *next;
	vec3_t      dir;
	vec3_t      origin;
	wvert_t     head;
} wedge_t;

int         numwedges, numwverts;
int         tjuncs;
int         tjuncfaces;

#define	MAXWVERTS	0x20000
#define	MAXWEDGES	0x10000

wvert_t     wverts[MAXWVERTS];
wedge_t     wedges[MAXWEDGES];


void
PrintFace (face_t *f)
{
	int         i;

	for (i = 0; i < f->numpoints; i++)
		printf ("(%5.2f, %5.2f, %5.2f)\n", f->pts[i][0], f->pts[i][1],
				f->pts[i][2]);
}

#define	NUM_HASH	1024

wedge_t    *wedge_hash[NUM_HASH];

static vec3_t hash_min, hash_scale;

static void
InitHash (vec3_t mins, vec3_t maxs)
{
	int         newsize[2];
	vec3_t      size;
	vec_t       volume;
	vec_t       scale;

	VectorCopy (mins, hash_min);
	VectorSubtract (maxs, mins, size);
	memset (wedge_hash, 0, sizeof (wedge_hash));

	volume = size[0] * size[1];

	scale = sqrt (volume / NUM_HASH);

	newsize[0] = size[0] / scale;
	newsize[1] = size[1] / scale;

	hash_scale[0] = newsize[0] / size[0];
	hash_scale[1] = newsize[1] / size[1];
	hash_scale[2] = newsize[1];
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

void
CanonicalVector (vec3_t vec)
{
	VectorNormalize (vec);
	if (vec[0] > EQUAL_EPSILON)
		return;
	else if (vec[0] < -EQUAL_EPSILON) {
		VectorSubtract (vec3_origin, vec, vec);
		return;
	} else
		vec[0] = 0;

	if (vec[1] > EQUAL_EPSILON)
		return;
	else if (vec[1] < -EQUAL_EPSILON) {
		VectorSubtract (vec3_origin, vec, vec);
		return;
	} else
		vec[1] = 0;

	if (vec[2] > EQUAL_EPSILON)
		return;
	else if (vec[2] < -EQUAL_EPSILON) {
		VectorSubtract (vec3_origin, vec, vec);
		return;
	} else
		vec[2] = 0;
	Sys_Error ("CanonicalVector: degenerate");
}

wedge_t    *
FindEdge (vec3_t p1, vec3_t p2, vec_t *t1, vec_t *t2)
{
	int         h;
	vec3_t      dir, origin;
	vec_t       temp;
	wedge_t    *w;

	VectorSubtract (p2, p1, dir);
	CanonicalVector (dir);

	*t1 = DotProduct (p1, dir);
	*t2 = DotProduct (p2, dir);

	VectorMA (p1, -*t1, dir, origin);

	if (*t1 > *t2) {
		temp = *t1;
		*t1 = *t2;
		*t2 = temp;
	}

	h = HashVec (origin);

	for (w = wedge_hash[h]; w; w = w->next) {
		temp = w->origin[0] - origin[0];
		if (temp < -EQUAL_EPSILON || temp > EQUAL_EPSILON)
			continue;
		temp = w->origin[1] - origin[1];
		if (temp < -EQUAL_EPSILON || temp > EQUAL_EPSILON)
			continue;
		temp = w->origin[2] - origin[2];
		if (temp < -EQUAL_EPSILON || temp > EQUAL_EPSILON)
			continue;

		temp = w->dir[0] - dir[0];
		if (temp < -EQUAL_EPSILON || temp > EQUAL_EPSILON)
			continue;
		temp = w->dir[1] - dir[1];
		if (temp < -EQUAL_EPSILON || temp > EQUAL_EPSILON)
			continue;
		temp = w->dir[2] - dir[2];
		if (temp < -EQUAL_EPSILON || temp > EQUAL_EPSILON)
			continue;

		return w;
	}

	if (numwedges == MAXWEDGES)
		Sys_Error ("FindEdge: numwedges == MAXWEDGES");
	w = &wedges[numwedges];
	numwedges++;

	w->next = wedge_hash[h];
	wedge_hash[h] = w;

	VectorCopy (origin, w->origin);
	VectorCopy (dir, w->dir);
	w->head.next = w->head.prev = &w->head;
	w->head.t = 99999;
	return w;
}

#define	T_EPSILON	0.01

void
AddVert (wedge_t *w, vec_t t)
{
	wvert_t    *v, *newv;

	v = w->head.next;
	do {
		if (fabs (v->t - t) < T_EPSILON)
			return;
		if (v->t > t)
			break;
		v = v->next;
	} while (1);

	// insert a new wvert before v
	if (numwverts == MAXWVERTS)
		Sys_Error ("AddVert: numwverts == MAXWVERTS");

	newv = &wverts[numwverts];
	numwverts++;

	newv->t = t;
	newv->next = v;
	newv->prev = v->prev;
	v->prev->next = newv;
	v->prev = newv;
}

void
AddEdge (vec3_t p1, vec3_t p2)
{
	wedge_t    *w;
	vec_t       t1, t2;

	w = FindEdge (p1, p2, &t1, &t2);
	AddVert (w, t1);
	AddVert (w, t2);
}

void
AddFaceEdges (face_t *f)
{
	int         i, j;

	for (i = 0; i < f->numpoints; i++) {
		j = (i + 1) % f->numpoints;
		AddEdge (f->pts[i], f->pts[j]);
	}
}

// a specially allocated face that can hold hundreds of edges if needed
byte        superfacebuf[8192];
face_t     *superface = (face_t *) superfacebuf;

void        FixFaceEdges (face_t * f);

face_t     *newlist;

void
SplitFaceForTjunc (face_t *f, face_t *original)
{
	face_t     *new, *chain;
	int         firstcorner, lastcorner, i;
	vec3_t      dir, test;
	vec_t       v;

	chain = NULL;
	do {
		if (f->numpoints <= MAXPOINTS) {
			// the face is now small enough without more cutting so copy it
			// back to the original
			*original = *f;
			original->original = chain;
			original->next = newlist;
			newlist = original;
			return;
		}

		tjuncfaces++;

	  restart:
		// find the last corner 
		VectorSubtract (f->pts[f->numpoints - 1], f->pts[0], dir);
		VectorNormalize (dir);
		for (lastcorner = f->numpoints - 1; lastcorner > 0; lastcorner--) {
			VectorSubtract (f->pts[lastcorner - 1], f->pts[lastcorner], test);
			VectorNormalize (test);
			v = DotProduct (test, dir);
			if (v < 0.9999 || v > 1.00001)
				break;
		}

		// find the first corner    
		VectorSubtract (f->pts[1], f->pts[0], dir);
		VectorNormalize (dir);
		for (firstcorner = 1; firstcorner < f->numpoints - 1; firstcorner++) {
			VectorSubtract (f->pts[firstcorner + 1], f->pts[firstcorner],
							test);
			VectorNormalize (test);
			v = DotProduct (test, dir);
			if (v < 0.9999 || v > 1.00001)
				break;
		}

		if (firstcorner + 2 >= MAXPOINTS) {
			// rotate the point winding
			VectorCopy (f->pts[0], test);
			for (i = 1; i < f->numpoints; i++)
				VectorCopy (f->pts[i], f->pts[i - 1]);
			VectorCopy (test, f->pts[f->numpoints - 1]);
			goto restart;
		}

		// cut off as big a piece as possible, less than MAXPOINTS, and not
		// past lastcorner

		new = NewFaceFromFace (f);
		if (f->original)
			Sys_Error ("SplitFaceForTjunc: f->original");

		new->original = chain;
		chain = new;
		new->next = newlist;
		newlist = new;
		if (f->numpoints - firstcorner <= MAXPOINTS)
			new->numpoints = firstcorner + 2;
		else if (lastcorner + 2 < MAXPOINTS &&
				 f->numpoints - lastcorner <= MAXPOINTS)
				new->numpoints = lastcorner + 2;
		else
			new->numpoints = MAXPOINTS;

		for (i = 0; i < new->numpoints; i++)
			VectorCopy (f->pts[i], new->pts[i]);
		for (i = new->numpoints - 1; i < f->numpoints; i++)
			VectorCopy (f->pts[i], f->pts[i - (new->numpoints - 2)]);
		f->numpoints -= (new->numpoints - 2);
	} while (1);

}

void
FixFaceEdges (face_t *f)
{
	int         i, j, k;
	vec_t       t1, t2;
	wedge_t    *w;
	wvert_t    *v;

	*superface = *f;

  restart:
	for (i = 0; i < superface->numpoints; i++) {
		j = (i + 1) % superface->numpoints;

		w = FindEdge (superface->pts[i], superface->pts[j], &t1, &t2);

		for (v = w->head.next; v->t < t1 + T_EPSILON; v = v->next) {
		}

		if (v->t < t2 - T_EPSILON) {
			tjuncs++;
			// insert a new vertex here
			for (k = superface->numpoints; k > j; k--) {
				VectorCopy (superface->pts[k - 1], superface->pts[k]);
			}
			VectorMA (w->origin, v->t, w->dir, superface->pts[j]);
			superface->numpoints++;
			goto restart;
		}
	}


	if (superface->numpoints <= MAXPOINTS) {
		*f = *superface;
		f->next = newlist;
		newlist = f;
		return;
	}
	// the face needs to be split into multiple faces because of too many edges

	SplitFaceForTjunc (superface, f);
}

//============================================================================

void
tjunc_find_r (node_t *node)
{
	face_t     *f;

	if (node->planenum == PLANENUM_LEAF)
		return;

	for (f = node->faces; f; f = f->next)
		AddFaceEdges (f);

	tjunc_find_r (node->children[0]);
	tjunc_find_r (node->children[1]);
}

void
tjunc_fix_r (node_t *node)
{
	face_t     *next, *f;

	if (node->planenum == PLANENUM_LEAF)
		return;

	newlist = NULL;

	for (f = node->faces; f; f = next) {
		next = f->next;
		FixFaceEdges (f);
	}

	node->faces = newlist;

	tjunc_fix_r (node->children[0]);
	tjunc_fix_r (node->children[1]);
}

void
tjunc (node_t *headnode)
{
	int         i;
	vec3_t      maxs, mins;

	qprintf ("---- tjunc ----\n");

	if (options.notjunc)
		return;

	// identify all points on common edges

	// origin points won't allways be inside the map, so extend the hash area 
	for (i = 0; i < 3; i++) {
		if (fabs (brushset->maxs[i]) > fabs (brushset->mins[i]))
			maxs[i] = fabs (brushset->maxs[i]);
		else
			maxs[i] = fabs (brushset->mins[i]);
	}
	VectorSubtract (vec3_origin, maxs, mins);

	InitHash (mins, maxs);

	numwedges = numwverts = 0;

	tjunc_find_r (headnode);

	qprintf ("%i world edges  %i edge points\n", numwedges, numwverts);

	// add extra vertexes on edges where needed
	tjuncs = tjuncfaces = 0;

	tjunc_fix_r (headnode);

	qprintf ("%i edges added by tjunctions\n", tjuncs);
	qprintf ("%i faces added by tjunctions\n", tjuncfaces);
}
