/*
	vrect.c

	Rectangle manipulation

	Copyright (C) 2012 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2012/1/6

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

static __attribute__ ((used)) const char rcsid[] = "$Id$";

#include <stdlib.h>

#include "QF/mathlib.h"
#include "QF/vrect.h"

#define RECT_BLOCK 128
static vrect_t *free_rects;

VISIBLE vrect_t *
VRect_New (int x, int y, int width, int height)
{
	vrect_t    *r;

	if (!free_rects) {
		int         i;

		free_rects = malloc (RECT_BLOCK * sizeof (vrect_t));
		for (i = 0; i < RECT_BLOCK - 1; i++)
			free_rects[i].next = &free_rects[i + 1];
		free_rects[i].next = 0;
	}
	r = free_rects;
	free_rects = free_rects->next;
	r->next = 0;
	r->x = x;
	r->y = y;
	r->width = width;
	r->height = height;
	return r;
}

VISIBLE void
VRect_Delete (vrect_t *rect)
{
	rect->next = free_rects;
	free_rects = rect;
}

VISIBLE vrect_t *
VRect_Intersect (const vrect_t *r1, const vrect_t *r2)
{
	int         minx = max (VRect_MinX (r1), VRect_MinX (r2));
	int         miny = max (VRect_MinY (r1), VRect_MinY (r2));
	int         maxx = min (VRect_MaxX (r1), VRect_MaxX (r2));
	int         maxy = min (VRect_MaxY (r1), VRect_MaxY (r2));
	return VRect_New (minx, miny, maxx - minx, maxy - miny);
}

VISIBLE vrect_t *
VRect_HSplit (const vrect_t *r, int y)
{
	vrect_t    *r1, *r2;
	int         r1miny = VRect_MinY (r);
	int         r1maxy = min (VRect_MaxY (r), y);
	int         r2miny = max (VRect_MinY (r), y);
	int         r2maxy = VRect_MaxY (r);
	r1 = VRect_New (VRect_MinX (r), r1miny, r->width, r1maxy - r1miny);
	r2 = VRect_New (VRect_MinX (r), r2miny, r->width, r2maxy - r2miny);
	r1->next = r2;
	return r1;
}

VISIBLE vrect_t *
VRect_VSplit (const vrect_t *r, int x)
{
	vrect_t    *r1, *r2;
	int         r1minx = VRect_MinX (r);
	int         r1maxx = min (VRect_MaxX (r), x);
	int         r2minx = max (VRect_MinX (r), x);
	int         r2maxx = VRect_MaxX (r);
	r1 = VRect_New (r1minx, VRect_MinY (r), r1maxx - r1minx, r->height);
	r2 = VRect_New (r2minx, VRect_MinY (r), r2maxx - r2minx, r->height);
	r1->next = r2;
	return r1;
}

VISIBLE vrect_t *
VRect_Difference (const vrect_t *r, const vrect_t *s)
{
	vrect_t    *i, *t, *_r;
	vrect_t    *rects = 0;
#define STASH(t)					\
	do {							\
		if (!VRect_IsEmpty (t)) {	\
			t->next = rects;		\
			rects = t;				\
		} else {					\
			VRect_Delete (t);		\
		}							\
	} while (0)

	// Find the intersection of r (original rect) and s (the rect being
	// subtracted from r). The intersection rect is used to carve the original
	// rect. If there is no intersection (the intersection rect is empty),
	// then there is nothing to subtract and the original rect (actually, a
	// copy of it) is returned.
	i = VRect_Intersect (r, s);
	if (VRect_IsEmpty (i)) {
		// copy r's shape to i, and return i.
		i->x = r->x;
		i->y = r->y;
		i->width = r->width;
		i->height = r->height;
		return i;
	}

	// Split r along the top of the intersection rect and stash the top
	// section if it is not empty.
	t = VRect_HSplit (r, VRect_MinY (i));
	// can't delete r: we don't own it
	_r = t->next;
	STASH (t);			// maybe stash the top section
	// r now represents the portion of the original rect below the top of
	// the intersection rect.

	// Split r along the bottom of the intersection rect and stash the bottom
	// section if it is not empty.
	t = VRect_HSplit (_r, VRect_MaxY (i));
	VRect_Delete (_r);	// finished with that copy of _r
	_r = t;
	STASH (t->next);	// maybe stash the bottom section
	// r now represents the horizontal section that is common with the
	// intersection rect.

	// Split r along the left side of tht intersection rect and stash the
	// left section if it is not empty.
	t = VRect_VSplit (_r, VRect_MinX (i));
	VRect_Delete (_r);	// finished with that copy of _r
	_r = t->next;
	STASH (t);			// maybe stash the left section
	// r now represets the section of the original rect that is between the
	// top and bottom, and to the right of the left side of the intersection
	// rect.

	// Split r along the right side of the intersection rect and stash the
	// right section if it is not empty. The left section is discarded.
	t = VRect_VSplit (_r, VRect_MaxX (i));
	VRect_Delete (_r);	// finished with that copy of _r
	STASH (t->next);	// maybe stash the right section
	VRect_Delete (t);	// discard the left section

	return rects;
}
