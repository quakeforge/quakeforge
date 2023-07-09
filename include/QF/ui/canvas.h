/*
	canvas.h

	Integration of 2d and 3d objects

	Copyright (C) 2022 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2022/12/12

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

#ifndef __QF_scene_canvas_h
#define __QF_scene_canvas_h

#include "QF/ecs.h"
#include "QF/ui/view.h"

enum {
	canvas_update,
	canvas_updateonce,
	canvas_tile,
	canvas_pic,
	canvas_fitpic,	// pic is scaled to fit the view
	canvas_subpic,
	canvas_cachepic,
	canvas_fill,
	canvas_charbuff,
	canvas_passage_glyphs,	// copy of text_passage_glyphs
	canvas_glyphs,	// copy of text_passage_glyphs
	canvas_func,
	canvas_outline,
	canvas_lateupdate,

	// last so deleting an entity removes the grouped components first
	canvas_canvas,

	canvas_comp_count
};

typedef struct canvas_s {
	ecs_registry_t *reg;
	uint32_t    base;
	bool        visible;
	uint32_t    range[canvas_comp_count];
} canvas_t;

extern const struct component_s canvas_components[canvas_comp_count];

typedef struct canvas_system_s {
	ecs_registry_t *reg;
	uint32_t    base;
	uint32_t    view_base;
	uint32_t    text_base;
	uint32_t    imui_base;
} canvas_system_t;

struct view_s;
struct view_pos_s;

typedef void (*canvas_update_f) (struct view_s);
typedef void (*canvas_func_f) (struct view_pos_s, struct view_pos_s);

typedef struct canvas_subpic_s {
	struct qpic_s *pic;
	uint32_t    x, y;
	uint32_t    w, h;
} canvas_subpic_t;

#define CANVASINLINE GNU89INLINE inline

void Canvas_InitSys (canvas_system_t *canvas_sys, ecs_registry_t *reg);
void Canvas_AddToEntity (canvas_system_t canvas_sys, uint32_t ent);
uint32_t Canvas_New (canvas_system_t canvas_sys);
void Canvas_Draw (canvas_system_t canvas_sys);
void Canvas_SortComponentPool (canvas_system_t canvas_sys, uint32_t ent,
							   uint32_t component);
void Canvas_SetLen (canvas_system_t canvas_sys, uint32_t ent, view_pos_t len);
CANVASINLINE view_t Canvas_GetRootView (canvas_system_t canvas_sys,
										uint32_t ent);
CANVASINLINE bool *Canvas_Visible (canvas_system_t canvas_sys, uint32_t ent);

#undef CANVASINLINE
#ifndef IMPLEMENT_CANVAS_Funcs
#define CANVASINLINE GNU89INLINE inline
#else
#define CANVASINLINE VISIBLE
#endif

CANVASINLINE
view_t
Canvas_GetRootView (canvas_system_t canvas_sys, uint32_t ent)
{
	ecs_system_t viewsys = { canvas_sys.reg, canvas_sys.view_base };
	return View_FromEntity (viewsys, ent);
}

CANVASINLINE
bool *
Canvas_Visible (canvas_system_t canvas_sys, uint32_t ent)
{
	uint32_t comp = canvas_sys.base + canvas_canvas;
	auto canvas = (canvas_t *) Ent_GetComponent (ent, comp, canvas_sys.reg);
	return &canvas->visible;
}

#undef CANVASINLINE

#endif//__QF_scene_canvas_h
