/*
	r_scrap.h

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

#ifndef __r_scrap_h
#define __r_scrap_h

#include <QF/ecs.h>
#include <QF/set.h>

typedef struct scrapset_s scrapset_t;

typedef struct scrapbox_s {
	uint16_t    x, y, layer;
	uint16_t    width, height;
	uint16_t    scrap_id;	// which scrap
	uint32_t    id;			// scrap internal id
} scrapbox_t;

typedef struct rscrap_s {
	/// For a free region of size width,height, that size will be a member of
	/// the x set for width, and y set for height.
	set_t      *free_x;			///< set of free regions by width
	set_bits_t**free_y;			///< sets of free regions by height per width
	int        *w_counts;
	int        *h_counts;
	scrapset_t *free_rects;		///< set of width sets
	scrapbox_t**rects;
	ecs_idpool_t idpool;
	uint16_t    width;			///< overall width of scrap
	uint16_t    height;			///< overall height of scrap
	uint16_t    layers;
	uint16_t    scrap_id;
} rscrap_t;

void R_ScrapInit (rscrap_t *scrap, int width, int height);
void R_ScrapDelete (rscrap_t *scrap);
scrapbox_t *R_ScrapAlloc (rscrap_t *scrap, int width, int height);
void R_ScrapAddLayer (rscrap_t *scrap);
void R_ScrapFree (rscrap_t *scrap, scrapbox_t *rect);
void R_ScrapClear (rscrap_t *scrap);
size_t R_ScrapArea (rscrap_t *scrap, int *count) __attribute__((pure));
void R_ScrapDump (rscrap_t *scrap);

#endif//__r_scrap_h
