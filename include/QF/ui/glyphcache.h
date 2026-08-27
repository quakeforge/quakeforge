/*
	glyphcache.h

	Cache management for glyphs

	Copyright (C) 2026 Bill Currie

	Author: Bill Currie <bill@taniwha.org>
	Date: 2026/08/20

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

#ifndef __QF_scene_glyphcache_h
#define __QF_scene_glyphcache_h

#include "QF/ecs.h"
#include "QF/ui/view.h"

typedef struct canvas_system_s canvas_system_t;

typedef union glyphkey_s {
	uint64_t    raw_key;
	struct {
		uint64_t    glyphid:16;
		uint64_t    fontid:10;
		uint64_t    ptsize:14;	// 8.6 fixed-point
		uint64_t    subpixel_x:2;
		uint64_t    subpixel_y:2;
		uint64_t    render_mode:2;
		uint64_t    _reserved:18;
	};
} glyphkey_t;

static_assert(sizeof(glyphkey_t) == sizeof(uint64_t));

typedef struct glyphobj_s {
	glyphkey_t  key;
	uint64_t    cacheind:16;
	int64_t     x:24;			// 18.6 fixed-point
	int64_t     y:24;			// 18.6 fixed-point
} glyphobj_t;

typedef struct glyphcache_s {
	glyphkey_t *map;
	glyphkey_t *objs;
	vrect_t    *rects;
	rscrap_t   *scrap;
} glyphcache_t;

glyphcache_t *glyphcache_new (void);
void glyphcache_load_glyphs (canvas_system_t canvas_sys);

#endif//__QF_scene_glyphcache_h
