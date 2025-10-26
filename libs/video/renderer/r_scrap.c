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
#include <string.h>

#include "QF/set.h"
#include "QF/sys.h"

#include "QF/ui/vrect.h"

#include "compat.h"
#include "r_scrap.h"

typedef struct scrapset_s {
	int         users;
	union {
		scrapset_t *sets[256];
		vrect_t    *rects[16][16];
	};
} scrapset_t;

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

static vrect_t *
r_scrap_pull_rect (rscrap_t *scrap, int width, int height)
{
	auto col = &scrap->free_rects->sets[width / 16];
	if (!*col) {
		return nullptr;
	}
	auto row = &(*col)->sets[height / 16];
	if (!*row) {
		return nullptr;
	}
	auto cell = &(*row)->rects[width % 16][height % 16];
	auto rect = *cell;
	if (!rect) {
		return nullptr;
	}
	if (!(*cell = rect->next)) {
		if (!--(*row)->users) {
			free (*row);
			*row = nullptr;
			if (!--(*col)->users) {
				free (*col);
				*col = nullptr;
				--scrap->free_rects->users;
			}
		}
	}
	if (!--scrap->w_counts[width]) {
		set_remove (scrap->free_x, width);
	}
	if (!--scrap->h_counts[height]) {
		set_remove (scrap->free_y, height);
	}
	rect->next = nullptr;
	return rect;
}

static void
r_scrap_push_rect (rscrap_t *scrap, vrect_t *rect)
{
	int width = rect->width - 1;
	int height = rect->height - 1;
	if (!scrap->free_rects) {
		scrap->free_rects = calloc (1, sizeof (scrapset_t));
	}
	auto col = &scrap->free_rects->sets[width / 16];
	if (!*col) {
		scrap->free_rects->users++;
		*col = calloc (1, sizeof (scrapset_t));
	}
	auto row = &(*col)->sets[height / 16];
	if (!*row) {
		(*col)->users++;
		*row = calloc (1, sizeof (scrapset_t));
	}
	auto cell = &(*row)->rects[width % 16][height % 16];
	if (!*cell) {
		(*row)->users++;
	}
	rect->next = *cell;
	*cell = rect;

	set_add (scrap->free_x, width);
	set_add (scrap->free_y, height);
	scrap->w_counts[width]++;
	scrap->h_counts[height]++;
}

VISIBLE void
R_ScrapInit (rscrap_t *scrap, int width, int height)
{
	width = pow2rup (width);
	height = pow2rup (height);
	if (width > 4096 || height > 4096) {
		Sys_Printf ("%dx%d scrap not supported", width, height);
	}
	*scrap = (rscrap_t) {
		.width = width,
		.height = height,
		.free_x = set_new (),
		.free_y = set_new (),
		.w_counts = calloc (width, sizeof (int)),
		.h_counts = calloc (height, sizeof (int)),
	};
	r_scrap_push_rect (scrap, VRect_New (0, 0, width, height));
}

VISIBLE void
R_ScrapDelete (rscrap_t *scrap)
{
	if (!scrap) {
		return;
	}
	R_ScrapClear (scrap);
}

VISIBLE vrect_t *
R_ScrapAlloc (rscrap_t *scrap, int width, int height)
{
	qfZoneScoped (true);

	auto avail_x = set_start (scrap->free_x, width - 1);
	auto avail_y = set_start (scrap->free_y, height - 1);
	if (!avail_x || !avail_y) {
		// no large enough region is available
		return nullptr;
	}

	const unsigned w = width - 1;
	const unsigned h = height - 1;
	auto old = r_scrap_pull_rect (scrap, avail_x->element, avail_y->element);
	if (!old) {
		if (avail_y->element == h) {
			// perfect fit for height, check all the widths
			for (avail_x = set_next (avail_x); avail_x;
				 avail_x = set_next (avail_x)) {
				old = r_scrap_pull_rect (scrap, avail_x->element,
										 avail_y->element);
				if (old) {
					break;
				}
			}
			if (!old) {
				avail_x = set_start (scrap->free_x, w);
				avail_y = set_next (avail_y);
			}
		} else if (avail_x->element == w) {
			// perfect fit for width, check all the heights
			for (avail_y = set_next (avail_y); avail_y;
				 avail_y = set_next (avail_y)) {
				old = r_scrap_pull_rect (scrap, avail_x->element,
										 avail_y->element);
				if (old) {
					break;
				}
			}
			if (!old) {
				avail_x = set_next (avail_x);
				avail_y = set_start (scrap->free_y, h);
			}
		}
		if (!old) {
			// there's no perfect fit, so find the smallest region... or at
			// least try to. Things get a little wierd sometimes for the quake
			// 2d pic set, but lightmaps seem to behave nicely.
			do {
				old = r_scrap_pull_rect (scrap, avail_x->element,
										 avail_y->element);
				if (old) {
					break;
				}
				if (avail_y->element < avail_x->element) {
					avail_y = set_next (avail_y);
					if (!avail_y && (avail_x = set_next (avail_x))) {
						avail_y = set_start (scrap->free_y, h);
					}
				} else {
					avail_x = set_next (avail_x);
					if (!avail_x && (avail_y = set_next (avail_y))) {
						avail_x = set_start (scrap->free_x, w);
					}
				}
			} while (avail_x && avail_y);
		}
	}
	if (!old) {
		Sys_Error ("the bits lied!");
	}

	auto rect = old;
	if (old->width > width || old->height > height) {
		rect = VRect_SubRect (old, width, height);
		VRect_Delete (old);
		auto frags = rect->next;
		while (frags) {
			// old was bigger than the requested size
			auto next = frags->next;
			r_scrap_push_rect (scrap, frags);
			frags = next;
		}
	}
	rect->next = scrap->rects;
	scrap->rects = rect;

	return rect;
}

VISIBLE void
R_ScrapFree (rscrap_t *scrap, vrect_t *rect)
{
	vrect_t   **t;

	for (t = &scrap->rects; *t; t = &(*t)->next)
		if (*t == rect)
			break;
	if (*t != rect)
		Sys_Error ("R_ScrapFree: broken rect");
	*t = rect->next;

#if 0
	vrect_t *merge;
	do {
		merge = nullptr;
		int h = rect->height - 1;
		for (int i = 0; i < 256; i++) {
			scrapset_t **row;
			if (!scrap->free_rects->sets[i]
				|| !*(row = &scrap->free_rects->sets[i]->sets[h / 16])) {
				continue;
			}
			for (int j = 0; j < 16; j++) {
				for (t = &(*row)->rects[j][h % 16]; *t; t = &(*t)->next) {
					merge = VRect_Merge (*t, rect);
					if (merge) {
						auto old = *t;
						*t = (*t)->next;
						VRect_Delete (old);
						VRect_Delete (rect);
						rect = merge;
						break;
					}
				}
			}
		}
	} while (merge);
#endif
	r_scrap_push_rect (scrap, rect);
}

VISIBLE void
R_ScrapClear (rscrap_t *scrap)
{
	if (scrap->free_rects) {
		for (int i = 0; i < 256; i++) {
			auto col = scrap->free_rects->sets[i];
			for (int j = 0; col && j < 256; j++) {
				auto row = col->sets[j];
				for (int k = 0; row && k < 16; k++) {
					for (int l = 0; l < 16; l++) {
						auto cell = &row->rects[k][l];
						while (*cell) {
							auto rect = *cell;
							*cell = rect->next;
							VRect_Delete (rect);
						}
					}
				}
				free (row);
			}
			free (col);
		}
		free (scrap->free_rects);
		scrap->free_rects = nullptr;
	}
	while (scrap->rects) {
		auto t = scrap->rects;
		scrap->rects = t->next;
		VRect_Delete (t);
	}

	memset (scrap->w_counts, 0, sizeof (int[scrap->width]));
	memset (scrap->h_counts, 0, sizeof (int[scrap->height]));

	r_scrap_push_rect (scrap, VRect_New (0, 0, scrap->width, scrap->height));
}

VISIBLE size_t
R_ScrapArea (rscrap_t *scrap, int *count)
{
	size_t      area = 0;
	int         c = 0;

	if (scrap->free_rects) {
		for (int i = 0; i < 256; i++) {
			auto col = scrap->free_rects->sets[i];
			for (int j = 0; col && j < 256; j++) {
				auto row = col->sets[j];
				for (int k = 0; row && k < 16; k++) {
					for (int l = 0; l < 16; l++) {
						for (auto rect = row->rects[k][l]; rect;
							 rect = rect->next) {
							area += rect->width * rect->height;
							c++;
						}
					}
				}
			}
		}
	}
	if (count) {
		*count = c;
	}
	return area;
}

VISIBLE void
R_ScrapDump (rscrap_t *scrap)
{
	if (scrap->rects) {
		Sys_Printf ("allocated:\n");
	}
	for (auto rect = scrap->rects; rect; rect = rect->next) {
		Sys_Printf ("%d %d %d %d\n", rect->x, rect->y,
					rect->width, rect->height);
	}
	if (scrap->free_rects && scrap->free_rects->users) {
		Sys_Printf ("free:\n");
		Sys_Printf ("widths : %s\n", set_as_string (scrap->free_x));
		for (auto wi = set_first (scrap->free_x); wi; wi = set_next (wi)) {
			Sys_Printf (" %d", scrap->w_counts[wi->element]);
		}
		Sys_Printf ("\nheights: %s\n", set_as_string (scrap->free_y));
		for (auto hi = set_first (scrap->free_y); hi; hi = set_next (hi)) {
			Sys_Printf (" %d", scrap->h_counts[hi->element]);
		}
		Sys_Printf ("\n");
		for (int i = 0; i < 256; i++) {
			auto col = scrap->free_rects->sets[i];
			if (!col) {
				continue;
			}
			for (int j = 0; j < 256; j++) {
				auto row = col->sets[j];
				if (!row) {
					continue;
				}
				for (int k = 0; k < 16; k++) {
					for (int l = 0; l < 16; l++) {
						for (auto rect = row->rects[k][l]; rect;
							 rect = rect->next) {
							Sys_Printf ("%d:%d:%d:%d %d %d %d %d\n",
										i, j, k, l,
										rect->x, rect->y,
										rect->width, rect->height);
						}
					}
				}
			}
		}
	}
}
