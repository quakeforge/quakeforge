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
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#if defined(_WIN32) && defined(HAVE_MALLOC_H)
#include <malloc.h>
#endif
#include <stdlib.h>

#include "qfalloca.h"

#include "QF/sys.h"

#include "QF/winding.h"

#define BOGUS (18000.0)

int         c_activewindings, c_peakwindings;

winding_t *
BaseWindingForPlane (const plane_t *p)
{
	int         i, x;
	vec_t       max, v;
	vec3_t      org, vright, vup;
	winding_t  *w;

	// find the major axis

	max = -BOGUS;
	x = -1;
	for (i = 0; i < 3; i++) {
		v = fabs (p->normal[i]);
		if (v > max) {
			x = i;
			max = v;
		}
	}
	if (x == -1)
		Sys_Error ("BaseWindingForPlane: no axis found");

	VectorZero (vup);
	switch (x) {
		case 0:
		case 1:
			vup[2] = 1;
			break;
		case 2:
			vup[0] = 1;
			break;
	}

	v = DotProduct (vup, p->normal);
	VectorMultSub (vup, v, p->normal, vup);
	_VectorNormalize (vup);

	VectorScale (p->normal, p->dist, org);

	CrossProduct (vup, p->normal, vright);

	VectorScale (vup, BOGUS, vup);
	VectorScale (vright, BOGUS, vright);

	// project a really big axis aligned box onto the plane
	w = NewWinding (4);

	VectorSubtract (org, vright, w->points[0]);
	VectorAdd (w->points[0], vup, w->points[0]);

	VectorAdd (org, vright, w->points[1]);
	VectorAdd (w->points[1], vup, w->points[1]);

	VectorAdd (org, vright, w->points[2]);
	VectorSubtract (w->points[2], vup, w->points[2]);

	VectorSubtract (org, vright, w->points[3]);
	VectorSubtract (w->points[3], vup, w->points[3]);

	w->numpoints = 4;

	return w;
}

winding_t *
CopyWinding (const winding_t *w)
{
	size_t      size;
	winding_t  *c;

	size = (size_t) (uintptr_t) &((winding_t *) 0)->points[w->numpoints];
	c = malloc (size);
	memcpy (c, w, size);
	return c;
}

winding_t *
CopyWindingReverse (const winding_t *w)
{
	int         i;
	size_t      size;
	winding_t  *c;

	size = (size_t) (uintptr_t) &((winding_t *) 0)->points[w->numpoints];
	c = malloc (size);
	c->numpoints = w->numpoints;
	for (i = 0; i < w->numpoints; i++) {
		// add points backwards
		VectorCopy (w->points[w->numpoints - 1 - i], c->points[i]);
	}
	return c;
}

winding_t *
WindingVectors (const winding_t *w, int unit)
{
	int         i;
	size_t      size;
	winding_t  *c;

	size = (size_t) (uintptr_t) &((winding_t *) 0)->points[w->numpoints];
	c = malloc (size);
	c->numpoints = w->numpoints;
	for (i = 0; i < w->numpoints; i++) {
		VectorSubtract (w->points[(i + 1) % w->numpoints], w->points[i],
						c->points[i]);
		if (unit)
			VectorNormalize (c->points[i]);
	}
	return c;
}

winding_t *
ClipWinding (winding_t *in, plane_t *split, qboolean keepon)
{
	int         maxpts, i, j;
	int        *sides;
	int         counts[3];
	vec_t       dot;
	vec_t      *dists;
	vec_t      *p1, *p2;
	vec3_t      mid;
	winding_t  *neww;

	counts[0] = counts[1] = counts[2] = 0;

	// +1 for duplicating the first point
	sides = alloca ((in->numpoints + 1) * sizeof (int));
	dists = alloca ((in->numpoints + 1) * sizeof (vec_t));

	// determine sides for each point
	for (i = 0; i < in->numpoints; i++) {
		dot = DotProduct (in->points[i], split->normal);
		dot -= split->dist;
		dists[i] = dot;
		if (dot > ON_EPSILON)
			sides[i] = SIDE_FRONT;
		else if (dot < -ON_EPSILON)
			sides[i] = SIDE_BACK;
		else {
			sides[i] = SIDE_ON;
		}
		counts[sides[i]]++;
	}
	// duplicate the first point
	sides[i] = sides[0];
	dists[i] = dists[0];

	if (keepon && !counts[SIDE_FRONT] && !counts[SIDE_BACK])
		return in;

	if (!counts[SIDE_FRONT]) {
		FreeWinding (in);
		return NULL;
	}
	if (!counts[SIDE_BACK])
		return in;
	for (maxpts = 0, i = 0; i < in->numpoints; i++) {
		if (!(sides[i] & 1))
			maxpts++;
		if ((sides[i] ^ 1) == sides[i + 1])
			maxpts++;
	}
	neww = NewWinding (maxpts);

	for (i = 0; i < in->numpoints; i++) {
		p1 = in->points[i];

		if (sides[i] == SIDE_ON) {
			if (neww->numpoints == maxpts)
				Sys_Error ("ClipWinding: points exceeded estimate");
			VectorCopy (p1, neww->points[neww->numpoints]);
			neww->numpoints++;
			continue;
		}

		if (sides[i] == SIDE_FRONT) {
			if (neww->numpoints == maxpts)
				Sys_Error ("ClipWinding: points exceeded estimate");
			VectorCopy (p1, neww->points[neww->numpoints]);
			neww->numpoints++;
		}

		if (sides[i + 1] == SIDE_ON || sides[i + 1] == sides[i])
			continue;

		if (neww->numpoints == maxpts)
			Sys_Error ("ClipWinding: points exceeded estimate");

		// generate a split point
		p2 = in->points[(i + 1) % in->numpoints];

		dot = dists[i] / (dists[i] - dists[i + 1]);
		for (j = 0; j < 3; j++) {		// avoid round off error when possible
			if (split->normal[j] == 1)
				mid[j] = split->dist;
			else if (split->normal[j] == -1)
				mid[j] = -split->dist;
			else
				mid[j] = p1[j] + dot * (p2[j] - p1[j]);
		}

		VectorCopy (mid, neww->points[neww->numpoints]);
		neww->numpoints++;
	}

	// free the original winding
	FreeWinding (in);

	return neww;
}

void
DivideWinding (winding_t *in, plane_t *split, winding_t **front,
			   winding_t **back)
{
	int         i;
	int         counts[3];
	plane_t     plane;
	vec_t       dot;
	winding_t  *tmp;

	counts[0] = counts[1] = counts[2] = 0;

	// determine sides for each point
	for (i = 0; i < in->numpoints; i++) {
		dot = DotProduct (in->points[i], split->normal) - split->dist;
		if (dot > ON_EPSILON)
			counts[SIDE_FRONT]++;
		else if (dot < -ON_EPSILON)
			counts[SIDE_BACK]++;
	}

	*front = *back = NULL;

	if (!counts[SIDE_FRONT]) {
		*back = in;
		return;
	}
	if (!counts[SIDE_BACK]) {
		*front = in;
		return;
	}

	tmp = CopyWinding (in);
	*front = ClipWinding (tmp, split, 0);

	plane.dist = -split->dist;
	VectorNegate (split->normal, plane.normal);

	tmp = CopyWinding (in);
	*back = ClipWinding (tmp, &plane, 0);
}

winding_t *
NewWinding (int points)
{
	size_t      size;
	winding_t  *w;

	if (points < 3)
		Sys_Error ("NewWinding: %i points", points);

	c_activewindings++;
	if (c_activewindings > c_peakwindings)
		c_peakwindings = c_activewindings;

	size = (size_t) (uintptr_t) &((winding_t *) 0)->points[points];
	w = malloc (size);
	memset (w, 0, size);

	return w;
}

void
FreeWinding (winding_t *w)
{
	c_activewindings--;
	free (w);
}
