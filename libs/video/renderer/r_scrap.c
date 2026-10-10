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

#define FREE_Y(w) &(set_t) \
	{ .map = scrap->free_y[w], .size = SET_SIZE (scrap->height) }

typedef struct scrapset_s {
	int         users;
	union {
		scrapset_t *sets[256];
		uint32_t    rects[16][16];
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

static scrapbox_t *
sb_ptr (rscrap_t *scrap, uint32_t id)
{
	return id != nullent ? &scrap->rects[id / 1024][id % 1024] : nullptr;
}

static scrapbox_t *
sb_new (rscrap_t *scrap, uint16_t x, uint16_t y, uint16_t layer,
		uint16_t w, uint16_t h)
{
	uint32_t id = ECS_NewId (&scrap->idpool);
	if (!scrap->rects[id / 1024]) {
		scrap->rects[id / 1024] = malloc (sizeof (scrapbox_t[1024]));
		memset (scrap->rects[id / 1024], ~0, sizeof (scrapbox_t[1024]));
	}
	auto sb = sb_ptr (scrap, id);
	*sb = (scrapbox_t) {
		.x = x,
		.y = y,
		.layer = layer,
		.width = w,
		.height = h,
		.scrap_id = scrap->scrap_id,
		.id = id,
	};
	return sb;
}

static scrapbox_t *
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
	auto rect = sb_ptr (scrap, *cell);
	if (!rect) {
		return nullptr;
	}
	uint32_t id = *cell;
	if (!(*cell = rect->id)) {
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
	set_remove (FREE_Y (width), height);
	if (!--scrap->w_counts[width]) {
		set_remove (scrap->free_x, width);
	}
	--scrap->h_counts[height];
	rect->id = id;
	return rect;
}

static void
r_scrap_push_rect (rscrap_t *scrap, scrapbox_t *rect)
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
		memset ((*row)->rects, ~0, sizeof ((*row)->rects));
	}
	auto cell = &(*row)->rects[width % 16][height % 16];
	if (!ECS_IdValid (&scrap->idpool, *cell)) {
		(*row)->users++;
	}
	uint32_t id = rect->id;
	rect->id = *cell;
	*cell = id;

	set_add (scrap->free_x, width);
	if (!scrap->free_y[width]) {
		scrap->free_y[width] = calloc (1, SET_SIZE (scrap->height) / 8);
	}
	set_add (FREE_Y (width), height);
	scrap->w_counts[width]++;
	scrap->h_counts[height]++;
}

VISIBLE void
R_ScrapInit (rscrap_t *scrap, int width, int height)
{
	width = pow2rup (width);
	height = pow2rup (height);
	if (width > 4096 || height > 4096) {
		Sys_Error ("%dx%d scrap not supported", width, height);
	}
	*scrap = (rscrap_t) {
		.width = width,
		.height = height,
		.free_x = set_new (),
		.free_y = calloc (width, sizeof (set_bits_t *)),
		.w_counts = calloc (width, sizeof (int)),
		.h_counts = calloc (height, sizeof (int)),
		.rects = calloc (1024, sizeof (scrapbox_t *)),
	};
	R_ScrapClear (scrap);
}

VISIBLE void
R_ScrapDelete (rscrap_t *scrap)
{
	if (!scrap) {
		return;
	}
	R_ScrapClear (scrap);
}

VISIBLE scrapbox_t *
R_ScrapAlloc (rscrap_t *scrap, int width, int height)
{
	qfZoneScoped (true);

	const unsigned w = width - 1;
	const unsigned h = height - 1;

	scrapbox_t *old = nullptr;
	for (auto avail_x = set_start (scrap->free_x, w); avail_x;
		 avail_x = set_next (avail_x)) {
		if (auto avail_y = set_start (FREE_Y (avail_x->element), h)) {
			old = r_scrap_pull_rect (scrap, avail_x->element,
									 avail_y->element);
			goto found;
		}
	}
found:
	if (!old) {
		R_ScrapDump (scrap);
		int count = 0;
		size_t area = R_ScrapArea (scrap, &count);
		Sys_Error ("the bits lied! [%d, %d], %zd %d",
				   width, height, area, count);
	}

	auto rect = old;
	if (old->width > width || old->height > height) {
		auto old_vr = VRect_New (old->x, old->y, old->width, old->height);
		auto split = VRect_SubRect (old_vr, width, height);
		rect = sb_new (scrap, split->x, split->y, old->layer,
					   split->width, split->height);
		VRect_Delete (old_vr);
		auto frags = split->next;
		while (frags) {
			// old was bigger than the requested size
			auto next = frags->next;
			auto f = sb_new (scrap, frags->x, frags->y, old->layer,
							 frags->width, frags->height);
			VRect_Delete (frags);
			frags = next;

			r_scrap_push_rect (scrap, f);
		}
	}

	return rect;
}

VISIBLE void
R_ScrapFree (rscrap_t *scrap, scrapbox_t *rect)
{
	if (rect->scrap_id != scrap->scrap_id
		|| sb_ptr (scrap, rect->id) != rect) {
		Sys_Error ("R_ScrapFree: broken rect");
	}
	ECS_DelId (&scrap->idpool, rect->id);

	r_scrap_push_rect (scrap, rect);
}

VISIBLE void
R_ScrapClear (rscrap_t *scrap)
{
	memset (scrap->w_counts, 0, sizeof (int[scrap->width]));
	memset (scrap->h_counts, 0, sizeof (int[scrap->height]));

	ECS_IdPool_Reset (&scrap->idpool);

	r_scrap_push_rect (scrap, sb_new (scrap, 0, 0, 0,
									  scrap->width, scrap->height));
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
						for (auto rect = sb_ptr (scrap, row->rects[k][l]);
							 rect; rect = sb_ptr (scrap, rect->id)) {
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
	for (int i = 0; i < 1024; i++) {
		if (scrap->rects[i]) {
			for (int j = 0; j < 1024; j++) {
				auto rect = &scrap->rects[i][j];
				if (sb_ptr (scrap, rect->id) == rect) {
					Sys_Printf ("%d %d %d %d %d\n",
								rect->x, rect->y, rect->layer,
								rect->width, rect->height);
				}
			}
		}
	}
	if (scrap->free_rects && scrap->free_rects->users) {
		Sys_Printf ("free:\n");
		Sys_Printf ("widths : %s\n", set_as_string (scrap->free_x));
		for (auto wi = set_first (scrap->free_x); wi; wi = set_next (wi)) {
			auto free_y = FREE_Y (wi->element);
			Sys_Printf (" %4d %s\n", scrap->w_counts[wi->element],
						set_as_string (free_y));
			Sys_Printf ("heights: %s\n", set_as_string (free_y));
			for (auto hi = set_first (free_y); hi; hi = set_next (hi)) {
				Sys_Printf (" %d", scrap->h_counts[hi->element]);
			}
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
						for (auto rect = sb_ptr (scrap, row->rects[k][l]);
							 rect; rect = sb_ptr (scrap, rect->id)) {
							Sys_Printf ("%d:%d:%d:%d %d %d %d %d %d\n",
										i, j, k, l,
										rect->x, rect->y,
										rect->width, rect->height, rect->layer);
						}
					}
				}
			}
		}
	}
}
