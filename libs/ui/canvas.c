/*
	canvas.c

	Integration of 2d and 3d objects

	Copyright (C) 2022 Bill Currie

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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "QF/ui/canvas.h"
#include "QF/ui/text.h"
#include "QF/ui/view.h"

#include "QF/plugin/vid_render.h"

#define canvas_rangeid(c) \
static uint32_t \
canvas_##c##_rangeid (ecs_registry_t *reg, uint32_t ent, uint32_t comp) \
{ \
	comp += canvas_canvas - canvas_##c; \
	canvas_t   *canvas = Ent_GetComponent (ent, comp, reg); \
	return canvas->range[canvas_##c]; \
}
canvas_rangeid(update)
canvas_rangeid(updateonce)
canvas_rangeid(tile)
canvas_rangeid(pic)
canvas_rangeid(fitpic)
canvas_rangeid(subpic)
canvas_rangeid(cachepic)
canvas_rangeid(fill)
canvas_rangeid(charbuff)
canvas_rangeid(func)
canvas_rangeid(outline)
#undef canvas_rangeid

static void
canvas_canvas_destroy (void *_canvas)
{
}

const component_t canvas_components[canvas_comp_count] = {
	[canvas_update] = {
		.size = sizeof (canvas_update_f),
		.name = "update",
		.rangeid = canvas_update_rangeid,
	},
	[canvas_updateonce] = {
		.size = sizeof (canvas_update_f),
		.name = "updateonce",
		.rangeid = canvas_updateonce_rangeid,
	},
	[canvas_tile] = {
		.size = sizeof (byte),
		.name = "tile",
		.rangeid = canvas_tile_rangeid,
	},
	[canvas_pic] = {
		.size = sizeof (qpic_t *),
		.name = "pic",
		.rangeid = canvas_pic_rangeid,
	},
	[canvas_fitpic] = {
		.size = sizeof (qpic_t *),
		.name = "fitpic",
		.rangeid = canvas_fitpic_rangeid,
	},
	[canvas_subpic] = {
		.size = sizeof (canvas_subpic_t),
		.name = "subpic",
		.rangeid = canvas_subpic_rangeid,
	},
	[canvas_cachepic] = {
		.size = sizeof (const char *),
		.name = "cachepic",
		.rangeid = canvas_cachepic_rangeid,
	},
	[canvas_fill] = {
		.size = sizeof (byte),
		.name = "fill",
		.rangeid = canvas_fill_rangeid,
	},
	[canvas_charbuff] = {
		.size = sizeof (draw_charbuffer_t *),
		.name = "charbuffer",
		.rangeid = canvas_charbuff_rangeid,
	},
	[canvas_func] = {
		.size = sizeof (canvas_func_f),
		.name = "func",
		.rangeid = canvas_func_rangeid,
	},
	[canvas_outline] = {
		.size = sizeof (byte),
		.name = "outline",
		.rangeid = canvas_outline_rangeid,
	},
	[canvas_canvas] = {
		.size = sizeof (canvas_t),
		.name = "canvas",
		.destroy = canvas_canvas_destroy,
	},
};

typedef void (*canvas_sysfunc_f) (canvas_system_t *canvas_sys,
								  ecs_pool_t *pool, ecs_range_t range);
static void
draw_update (canvas_system_t *canvas_sys, ecs_pool_t *pool, ecs_range_t range)
{
	ecs_system_t viewsys = { canvas_sys->reg, canvas_sys->view_base };
	uint32_t    count = range.end - range.start;
	uint32_t   *ent = pool->dense;
	__auto_type func = (canvas_update_f *) pool->data + range.start;
	while (count-- > 0) {
		(*func++) (View_FromEntity (viewsys, *ent++));
	}
}

static void
draw_updateonce (canvas_system_t *canvas_sys, ecs_pool_t *pool,
				 ecs_range_t range)
{
	draw_update (canvas_sys, pool, range);
	pool->count = 0;//XXX FIXME hmm, what to do with ranges
}

static void
draw_tile_views (canvas_system_t *canvas_sys, ecs_pool_t *pool,
				 ecs_range_t range)
{
	ecs_system_t viewsys = { canvas_sys->reg, canvas_sys->view_base };
	uint32_t    count = range.end - range.start;
	uint32_t   *ent = pool->dense;
	while (count-- > 0) {
		view_t      view = View_FromEntity (viewsys, *ent++);
		if (View_GetVisible (view)) {
			view_pos_t  pos = View_GetAbs (view);
			view_pos_t  len = View_GetLen (view);
			r_funcs->Draw_TileClear (pos.x, pos.y, len.x, len.y);
		}
	}
}

static void
draw_pic_views (canvas_system_t *canvas_sys, ecs_pool_t *pool,
				ecs_range_t range)
{
	ecs_system_t viewsys = { canvas_sys->reg, canvas_sys->view_base };
	uint32_t    count = range.end - range.start;
	uint32_t   *ent = pool->dense;
	__auto_type pic = (qpic_t **) pool->data + range.start;
	while (count-- > 0) {
		view_t      view = View_FromEntity (viewsys, *ent++);
		if (View_GetVisible (view)) {
			view_pos_t  pos = View_GetAbs (view);
			r_funcs->Draw_Pic (pos.x, pos.y, *pic);
		}
		pic++;
	}
}

static void
draw_fitpic_views (canvas_system_t *canvas_sys, ecs_pool_t *pool,
				   ecs_range_t range)
{
	ecs_system_t viewsys = { canvas_sys->reg, canvas_sys->view_base };
	uint32_t    count = range.end - range.start;
	uint32_t   *ent = pool->dense;
	__auto_type pic = (qpic_t **) pool->data + range.start;
	while (count-- > 0) {
		view_t      view = View_FromEntity (viewsys, *ent++);
		if (View_GetVisible (view)) {
			view_pos_t  pos = View_GetAbs (view);
			view_pos_t  len = View_GetLen (view);
			r_funcs->Draw_FitPic (pos.x, pos.y, len.x, len.y, *pic);
		}
		pic++;
	}
}

static void
draw_subpic_views (canvas_system_t *canvas_sys, ecs_pool_t *pool,
				   ecs_range_t range)
{
	ecs_system_t viewsys = { canvas_sys->reg, canvas_sys->view_base };
	uint32_t    count = range.end - range.start;
	uint32_t   *ent = pool->dense;
	__auto_type subpic = (canvas_subpic_t *) pool->data + range.start;
	while (count-- > 0) {
		view_t      view = View_FromEntity (viewsys, *ent++);
		if (View_GetVisible (view)) {
			view_pos_t  pos = View_GetAbs (view);
			r_funcs->Draw_SubPic (pos.x, pos.y, subpic->pic,
								  subpic->x, subpic->y, subpic->w, subpic->h);
		}
		subpic++;
	}
}

static void
draw_cachepic_views (canvas_system_t *canvas_sys, ecs_pool_t *pool,
					 ecs_range_t range)
{
	ecs_system_t viewsys = { canvas_sys->reg, canvas_sys->view_base };
	uint32_t    count = range.end - range.start;
	uint32_t   *ent = pool->dense;
	__auto_type name = (const char **) pool->data + range.start;
	while (count-- > 0) {
		view_t      view = View_FromEntity (viewsys, *ent++);
		if (View_GetVisible (view)) {
			view_pos_t  pos = View_GetAbs (view);
			qpic_t     *pic = r_funcs->Draw_CachePic (*name, 1);
			r_funcs->Draw_Pic (pos.x, pos.y, pic);
		}
		name++;
	}
}

static void
draw_fill_views (canvas_system_t *canvas_sys, ecs_pool_t *pool,
				 ecs_range_t range)
{
	ecs_system_t viewsys = { canvas_sys->reg, canvas_sys->view_base };
	uint32_t    count = range.end - range.start;
	uint32_t   *ent = pool->dense;
	__auto_type fill = (byte *) pool->data + range.start;
	while (count-- > 0) {
		view_t      view = View_FromEntity (viewsys, *ent++);
		if (View_GetVisible (view)) {
			view_pos_t  pos = View_GetAbs (view);
			view_pos_t  len = View_GetLen (view);
			r_funcs->Draw_Fill (pos.x, pos.y, len.x, len.y, *fill);
		}
		fill++;
	}
}

static void
draw_charbuff_views (canvas_system_t *canvas_sys, ecs_pool_t *pool,
					 ecs_range_t range)
{
	ecs_system_t viewsys = { canvas_sys->reg, canvas_sys->view_base };
	uint32_t    count = range.end - range.start;
	uint32_t   *ent = pool->dense;
	__auto_type charbuff = (draw_charbuffer_t **) pool->data + range.start;
	while (count-- > 0) {
		view_t      view = View_FromEntity (viewsys, *ent++);
		if (View_GetVisible (view)) {
			view_pos_t  pos = View_GetAbs (view);
			r_funcs->Draw_CharBuffer (pos.x, pos.y, *charbuff);
		}
		charbuff++;
	}
}

static void
draw_func_views (canvas_system_t *canvas_sys, ecs_pool_t *pool,
				 ecs_range_t range)
{
	ecs_system_t viewsys = { canvas_sys->reg, canvas_sys->view_base };
	uint32_t    count = range.end - range.start;
	uint32_t   *ent = pool->dense;
	__auto_type func = (canvas_func_f *) pool->data + range.start;
	while (count-- > 0) {
		view_t      view = View_FromEntity (viewsys, *ent++);
		if (View_GetVisible (view)) {
			view_pos_t  pos = View_GetAbs (view);
			view_pos_t  len = View_GetLen (view);
			(*func) (pos, len);
		}
		func++;
	}
}

static void
draw_outline_views (canvas_system_t *canvas_sys, ecs_pool_t *pool,
					ecs_range_t range)
{
	ecs_system_t viewsys = { canvas_sys->reg, canvas_sys->view_base };
	uint32_t    count = range.end - range.start;
	uint32_t   *ent = pool->dense;
	__auto_type col = (byte *) pool->data + range.start;
	__auto_type line = r_funcs->Draw_Line;
	while (count-- > 0) {
		view_t      view = View_FromEntity (viewsys, *ent++);
		byte        c = *col++;
		if (View_GetVisible (view)) {
			view_pos_t  p = View_GetAbs (view);
			view_pos_t  l = View_GetLen (view);
			view_pos_t  q = { p.x + l.x - 1, p.y + l.y - 1 };
			line (p.x, p.y, q.x, p.y, c);
			line (p.x, q.y, q.x, q.y, c);
			line (p.x, p.y, p.x, q.y, c);
			line (q.x, p.y, q.x, q.y, c);
		}
	}
}

void
Canvas_Draw (canvas_system_t canvas_sys)
{
	static canvas_sysfunc_f draw_func[canvas_comp_count] = {
		[canvas_update]     = draw_update,
		[canvas_updateonce] = draw_updateonce,
		[canvas_tile]       = draw_tile_views,
		[canvas_pic]        = draw_pic_views,
		[canvas_fitpic]     = draw_fitpic_views,
		[canvas_subpic]     = draw_subpic_views,
		[canvas_cachepic]   = draw_cachepic_views,
		[canvas_fill]       = draw_fill_views,
		[canvas_charbuff]   = draw_charbuff_views,
		[canvas_func]       = draw_func_views,
		[canvas_outline]    = draw_outline_views,
	};

	uint32_t    comp = canvas_sys.base + canvas_canvas;
	ecs_pool_t *canvas_pool = &canvas_sys.reg->comp_pools[comp];
	uint32_t    count = canvas_pool->count;
	//uint32_t   *entities = canvas_pool->dense;
	__auto_type canvases = (canvas_t *) canvas_pool->data;

	while (count-- > 0) {
		canvas_t   *canvas = canvases++;
		//uint32_t    ent = *entities++;

		for (int i = 0; i < canvas_comp_count; i++) {
			uint32_t    c = canvas_sys.base + i;
			uint32_t    rid = canvas->range[i];
			ecs_range_t range = ECS_GetSubpoolRange (canvas_sys.reg, c, rid);
			if (draw_func[i]) {
				ecs_pool_t *pool = &canvas_sys.reg->comp_pools[c];
				draw_func[i] (&canvas_sys, pool, range);
			}
		}
	}
}

void
Canvas_AddToEntity (canvas_system_t canvas_sys, uint32_t ent)
{
	canvas_t    canvas = { };
	for (uint32_t i = 0; i < canvas_comp_count; i++) {
		canvas.range[i] = ECS_NewSubpoolRange (canvas_sys.reg,
											   canvas_sys.base + i);
	}
	Ent_SetComponent (ent, canvas_sys.base + canvas_canvas, canvas_sys.reg,
					  &canvas);
	View_AddToEntity (ent,
					  (ecs_system_t) { canvas_sys.reg, canvas_sys.view_base },
					  nullview);
}

static int
canvas_href_cmp (const void *_a, const void *_b, void *arg)
{
	uint32_t    enta = *(const uint32_t *)_a;
	uint32_t    entb = *(const uint32_t *)_b;
	canvas_system_t *canvas_sys = arg;
	ecs_registry_t *reg = canvas_sys->reg;
	uint32_t    href = canvas_sys->view_base + view_href;
	hierref_t  *ref_a = Ent_GetComponent (enta, href, reg);
	hierref_t  *ref_b = Ent_GetComponent (entb, href, reg);
	if (ref_a->hierarchy == ref_b->hierarchy) {
		return ref_a->index - ref_b->index;
	}
	ptrdiff_t  diff = ref_a->hierarchy - ref_b->hierarchy;
	return diff > 0 ? 1 : diff < 0 ? -1 : 0;
}

void
Canvas_SortComponentPool (canvas_system_t canvas_sys, uint32_t ent,
						  uint32_t component)
{
	canvas_t   *canvas = Ent_GetComponent (ent, canvas_sys.base + canvas_canvas,
										   canvas_sys.reg);
	uint32_t    rid = canvas->range[component - canvas_sys.base];
	ecs_range_t range = ECS_GetSubpoolRange (canvas_sys.reg, component, rid);
	ECS_SortComponentPoolRange (canvas_sys.reg, component, range,
								canvas_href_cmp, &canvas_sys);
}
