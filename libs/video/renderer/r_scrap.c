/*
	r_scrap.c

	Renderer agnostic scrap management

	Copyright (C) 2021 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2021/1/12

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

#include "QF/sys.h"

#include "QF/ui/vrect.h"

#include "compat.h"
#include "r_scrap.h"

static unsigned
pow2rup (unsigned x)
{
	x--;
	x |= x >> 1;
	x |= x >> 2;
	x |= x >> 4;
	x |= x >> 8;
	x |= x >> 16;
	x++;
	return x;
}

VISIBLE void
R_ScrapInit (rscrap_t *scrap, int width, int height)
{
	width = pow2rup (width);
	height = pow2rup (height);
	scrap->width = width;
	scrap->height = height;
	scrap->rects = 0;
	scrap->free_rects = VRect_New (0, 0, width, height);
}

VISIBLE void
R_ScrapDelete (rscrap_t *scrap)
{
	R_ScrapClear (scrap);
	VRect_Delete (scrap->free_rects);
}

VISIBLE vrect_t *
R_ScrapAlloc (rscrap_t *scrap, int width, int height)
{
	int         w, h;
	vrect_t   **t, **best;
	vrect_t    *old, *frags, *rect;

	w = pow2rup (width);
	h = pow2rup (height);

	best = 0;
	for (t = &scrap->free_rects; *t; t = &(*t)->next) {
		if ((*t)->width < w || (*t)->height < h)
			continue;						// won't fit
		if (!best) {
			best = t;
			continue;
		}
		if ((*t)->width <= (*best)->width || (*t)->height <= (*best)->height)
			best = t;
	}
	if (!best)
		return 0;							// couldn't find a spot
	old = *best;
	*best = old->next;
	rect = VRect_New (old->x, old->y, w, h);
	frags = VRect_Difference (old, rect);
	VRect_Delete (old);
	if (frags) {
		// old was bigger than the requested size
		for (old = frags; old->next; old = old->next)
			;
		old->next = scrap->free_rects;
		scrap->free_rects = frags;
	}
	rect->next = scrap->rects;
	scrap->rects = rect;

	return rect;
}

VISIBLE void
R_ScrapFree (rscrap_t *scrap, vrect_t *rect)
{
	vrect_t    *old, *merge;
	vrect_t   **t;

	for (t = &scrap->rects; *t; t = &(*t)->next)
		if (*t == rect)
			break;
	if (*t != rect)
		Sys_Error ("R_ScrapFree: broken rect");
	*t = rect->next;

	do {
		merge = 0;
		for (t = &scrap->free_rects; *t; t = &(*t)->next) {
			merge = VRect_Merge (*t, rect);
			if (merge) {
				old = *t;
				*t = (*t)->next;
				VRect_Delete (old);
				VRect_Delete (rect);
				rect = merge;
				break;
			}
		}
	} while (merge);
	rect->next = scrap->free_rects;
	scrap->free_rects = rect;
}

VISIBLE void
R_ScrapClear (rscrap_t *scrap)
{
	vrect_t    *t;

	while (scrap->free_rects) {
		t = scrap->free_rects;
		scrap->free_rects = t->next;
		VRect_Delete (t);
	}
	while (scrap->rects) {
		t = scrap->rects;
		scrap->rects = t->next;
		VRect_Delete (t);
	}

	scrap->free_rects = VRect_New (0, 0, scrap->width, scrap->height);
}

VISIBLE size_t
R_ScrapArea (rscrap_t *scrap)
{
	vrect_t    *rect;
	size_t      area;

	for (rect = scrap->free_rects, area = 0; rect; rect = rect->next) {
		area += rect->width * rect->height;
	}
	return area;
}

VISIBLE void
R_ScrapDump (rscrap_t *scrap)
{
	vrect_t    *rect;

	for (rect = scrap->rects; rect; rect = rect->next) {
		Sys_Printf ("%d %d %d %d\n", rect->x, rect->y,
					rect->width, rect->height);
	}
}
