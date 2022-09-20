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

typedef struct rscrap_s {
	struct vrect_s *free_rects;			///< linked list of free areas
	struct vrect_s *rects;				///< linked list of allocated rects
	int         width;					///< overall width of scrap
	int         height;					///< overall height of scrap
} rscrap_t;

void R_ScrapInit (rscrap_t *scrap, int width, int height);
void R_ScrapDelete (rscrap_t *scrap);
struct vrect_s *R_ScrapAlloc (rscrap_t *scrap, int width, int height);
void R_ScrapFree (rscrap_t *scrap, struct vrect_s *rect);
void R_ScrapClear (rscrap_t *scrap);
size_t R_ScrapArea (rscrap_t *scrap, int *count) __attribute__((pure));
void R_ScrapDump (rscrap_t *scrap);

#endif//__r_scrap_h
