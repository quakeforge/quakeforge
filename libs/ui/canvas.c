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

#define IMPLEMENT_CANVAS_Funcs
#include "QF/ui/canvas.h"
#include "QF/ui/imui.h"
#include "QF/ui/text.h"
#include "QF/ui/view.h"

#include "QF/plugin/vid_render.h"

static uint32_t
_canvas_rangeid (ecs_registry_t *reg, uint32_t ent, uint32_t comp, uint32_t c)
{
	uint32_t    rcomp = comp - c + canvas_canvasref;
	uint32_t    ccomp = comp - c + canvas_canvas;
	// view components come immediately after canvas components
	uint32_t    vcomp = view_href + canvas_comp_count + comp - c;
	auto href = *(hierref_t *) Ent_GetComponent (ent, vcomp, reg);
	hierarchy_t *h = Ent_GetComponent (href.id, ecs_hierarchy, reg);
	// the root entity of the hierarchy has the canvasref or canvas component
	uint32_t    cent = h->ent[0];
	cent = *(uint32_t *) Ent_GetComponent (cent, rcomp, reg);
	canvas_t   *canvas = Ent_GetComponent (cent, ccomp, reg);
	return canvas->range[c];
}

#define canvas_rangeid(c) \
static uint32_t \
canvas_##c##_rangeid (ecs_registry_t *reg, uint32_t ent, uint32_t comp) \
{ \
	return _canvas_rangeid (reg, ent, comp, canvas_##c); \
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
canvas_rangeid(passage_glyphs)
canvas_rangeid(glyphs)
canvas_rangeid(func)
canvas_rangeid(lateupdate)
canvas_rangeid(outline)
#undef canvas_rangeid

static void
canvas_canvas_destroy (void *_canvas, ecs_registry_t *reg)
{
	canvas_t *canvas = _canvas;
	for (uint32_t i = 0; i < canvas_subpool_count; i++) {
		ECS_DelSubpoolRange (reg, canvas->base + i, canvas->range[i]);
	}
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
	[canvas_passage_glyphs] = {
		.size = sizeof (glyphset_t),
		.name = "passage glyphs (copy)",
		.rangeid = canvas_passage_glyphs_rangeid,
	},
	[canvas_glyphs] = {
		.size = sizeof (glyphset_t),
		.name = "glyphs (copy)",
		.rangeid = canvas_glyphs_rangeid,
	},
	[canvas_func] = {
		.size = sizeof (canvas_func_f),
		.name = "func",
		.rangeid = canvas_func_rangeid,
	},
	[canvas_lateupdate] = {
		.size = sizeof (canvas_update_f),
		.name = "lateupdate",
		.rangeid = canvas_lateupdate_rangeid,
	},
	[canvas_outline] = {
		.size = sizeof (byte),
		.name = "outline",
		.rangeid = canvas_outline_rangeid,
	},

	[canvas_canvasref] = {
		.size = sizeof (uint32_t),
		.name = "canvasref",
	},
	[canvas_canvas] = {
		.size = sizeof (canvas_t),
		.name = "canvas",
		.destroy = canvas_canvas_destroy,
	},
};

#define c_canvasref (canvas_sys.base + canvas_canvasref)
#define c_canvas (canvas_sys.base + canvas_canvas)

typedef void (*canvas_sysfunc_f) (canvas_system_t *canvas_sys,
								  ecs_pool_t *pool, ecs_range_t range);
static void
draw_update (canvas_system_t *canvas_sys, ecs_pool_t *pool, ecs_range_t range)
{
	qfZoneNamed (zone, true);
	ecs_system_t viewsys = { canvas_sys->reg, canvas_sys->view_base };
	uint32_t    count = range.end - range.start;
	uint32_t   *ent = pool->dense + range.start;
	__auto_type func = (canvas_update_f *) pool->data + range.start;
	while (count-- > 0) {
		(*func++) (View_FromEntity (viewsys, *ent++));
	}
}

static void
draw_tile_views (canvas_system_t *canvas_sys, ecs_pool_t *pool,
				 ecs_range_t range)
{
	qfZoneNamed (zone, true);
	ecs_system_t viewsys = { canvas_sys->reg, canvas_sys->view_base };
	uint32_t    count = range.end - range.start;
	uint32_t   *ent = pool->dense + range.start;
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
	qfZoneNamed (zone, true);
	ecs_system_t viewsys = { canvas_sys->reg, canvas_sys->view_base };
	uint32_t    count = range.end - range.start;
	uint32_t   *ent = pool->dense + range.start;
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
	qfZoneNamed (zone, true);
	ecs_system_t viewsys = { canvas_sys->reg, canvas_sys->view_base };
	uint32_t    count = range.end - range.start;
	uint32_t   *ent = pool->dense + range.start;
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
	qfZoneNamed (zone, true);
	ecs_system_t viewsys = { canvas_sys->reg, canvas_sys->view_base };
	uint32_t    count = range.end - range.start;
	uint32_t   *ent = pool->dense + range.start;
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
	qfZoneNamed (zone, true);
	ecs_system_t viewsys = { canvas_sys->reg, canvas_sys->view_base };
	uint32_t    count = range.end - range.start;
	uint32_t   *ent = pool->dense + range.start;
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
	qfZoneNamed (zone, true);
	ecs_system_t viewsys = { canvas_sys->reg, canvas_sys->view_base };
	uint32_t    count = range.end - range.start;
	uint32_t   *ent = pool->dense + range.start;
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
	qfZoneNamed (zone, true);
	ecs_system_t viewsys = { canvas_sys->reg, canvas_sys->view_base };
	uint32_t    count = range.end - range.start;
	uint32_t   *ent = pool->dense + range.start;
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
	qfZoneNamed (zone, true);
	ecs_system_t viewsys = { canvas_sys->reg, canvas_sys->view_base };
	uint32_t    count = range.end - range.start;
	uint32_t   *ent = pool->dense + range.start;
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
	qfZoneNamed (zone, true);
	ecs_system_t viewsys = { canvas_sys->reg, canvas_sys->view_base };
	uint32_t    count = range.end - range.start;
	uint32_t   *ent = pool->dense + range.start;
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

static void
draw_glyph_refs (view_pos_t *abs, glyphset_t *glyphset, glyphref_t *gref,
				 uint32_t color)
{
	uint32_t    count = gref->count;
	glyphobj_t *glyph = glyphset->glyphs + gref->start;

	while (count-- > 0) {
		glyphobj_t *g = glyph++;
		r_funcs->Draw_Glyph (abs->x + g->x, abs->y + g->y,
							 g->fontid, g->glyphid, color);
	}
}

static void
draw_box (view_pos_t *abs, view_pos_t *len, uint32_t ind, int c)
{
	int x = abs[ind].x;
	int y = abs[ind].y;
	int w = len[ind].x;
	int h = len[ind].y;
	r_funcs->Draw_Line (x, y, x + w, y, c);
	r_funcs->Draw_Line (x, y + h, x + w, y + h, c);
	r_funcs->Draw_Line (x, y, x, y + h, c);
	r_funcs->Draw_Line (x + w, y, x + w, y + h, c);
}

static void
draw_glyphs (canvas_system_t *canvas_sys, ecs_pool_t *pool, ecs_range_t range)
{
	qfZoneNamed (zone, true);
	auto        reg = canvas_sys->reg;
	uint32_t    glyphs = canvas_sys->text_base + text_glyphs;
	uint32_t    color = canvas_sys->text_base + text_color;
	uint32_t    vhref = canvas_sys->view_base + view_href;
	uint32_t    count = range.end - range.start;
	uint32_t   *ent = pool->dense + range.start;
	auto        glyphset = (glyphset_t *) pool->data + range.start;

	while (count-- > 0) {
		view_t      view = { .id = *ent++, .reg = reg, .comp = vhref};
		glyphset_t *gs = glyphset++;
		if (View_GetVisible (view)) {
			view_pos_t  abs = View_GetAbs (view);
			glyphref_t *gref = Ent_GetComponent (view.id, glyphs, reg);
			uint32_t   *c = Ent_SafeGetComponent (view.id, color, reg);
			draw_glyph_refs (&abs, gs, gref, c ? *c : 254);
		}
	}
}

static void
draw_passage_glyphs (canvas_system_t *canvas_sys, ecs_pool_t *pool,
					 ecs_range_t range)
{
	qfZoneNamed (zone, true);
	auto        reg = canvas_sys->reg;
	uint32_t    glyphs = canvas_sys->text_base + text_glyphs;
	uint32_t    color = canvas_sys->text_base + text_color;
	uint32_t    vhref = canvas_sys->view_base + view_href;
	uint32_t    count = range.end - range.start;
	uint32_t   *ent = pool->dense + range.start;
	auto        glyphset = (glyphset_t *) pool->data + range.start;

	while (count-- > 0) {
		view_t      psg_view = { .id = *ent++, .reg = reg, .comp = vhref};
		// first child is always a paragraph view, and all views after the
		// first paragraph's first child are all text views
		view_t      para_view = View_GetChild (psg_view, 0);
		view_t      text_view = View_GetChild (para_view, 0);
		hierref_t   href = View_GetRef (text_view);
		glyphset_t *gs = glyphset++;
		hierarchy_t *h = Ent_GetComponent (href.id, ecs_hierarchy, reg);
		view_pos_t *abs = h->components[view_abs];
		view_pos_t *len = h->components[view_len];

		for (uint32_t i = href.index; i < h->num_objects; i++) {
			glyphref_t *gref = Ent_GetComponent (h->ent[i], glyphs, reg);
			uint32_t   *c = Ent_SafeGetComponent (h->ent[i], color, reg);
			draw_glyph_refs (&abs[i], gs, gref, c ? *c : 254);

			if (0) draw_box (abs, len, i, 253);
		}
		if (0) {
			for (uint32_t i = 1; i < href.index; i++) {
				draw_box (abs, len, i, 251);
			}
		}
	}
}

void
Canvas_Draw (canvas_system_t canvas_sys)
{
	qfZoneNamed (zone, true);
	static canvas_sysfunc_f draw_func[canvas_comp_count] = {
		[canvas_update]     = draw_update,
		[canvas_updateonce] = draw_update,
		[canvas_tile]       = draw_tile_views,
		[canvas_pic]        = draw_pic_views,
		[canvas_fitpic]     = draw_fitpic_views,
		[canvas_subpic]     = draw_subpic_views,
		[canvas_cachepic]   = draw_cachepic_views,
		[canvas_fill]       = draw_fill_views,
		[canvas_charbuff]   = draw_charbuff_views,
		[canvas_passage_glyphs] = draw_passage_glyphs,
		[canvas_glyphs]     = draw_glyphs,
		[canvas_func]       = draw_func_views,
		[canvas_lateupdate] = draw_update,
		[canvas_outline]    = draw_outline_views,
	};

	auto        reg = canvas_sys.reg;
	ecs_pool_t *canvas_pool = &reg->comp_pools[c_canvas];
	uint32_t    count = canvas_pool->count;
	uint32_t   *entities = canvas_pool->dense;
	__auto_type canvases = (canvas_t *) canvas_pool->data;

	while (count-- > 0) {
		canvas_t   *canvas = canvases++;
		uint32_t    ent = *entities++;

		if (!canvas->visible) {
			continue;
		}
		view_t      canvas_view = Canvas_GetRootView (canvas_sys, ent);
		view_pos_t  pos = View_GetAbs (canvas_view);
		view_pos_t  len = View_GetLen (canvas_view);
		r_funcs->Draw_SetClip (pos.x, pos.y, len.x, len.y);

		for (int i = 0; i < canvas_comp_count; i++) {
			if (draw_func[i]) {
				uint32_t    c = canvas_sys.base + i;
				uint32_t    rid = canvas->range[i];
				ecs_range_t range = ECS_GetSubpoolRange (reg, c, rid);
				if (range.end - range.start) {
					ecs_pool_t *pool = &reg->comp_pools[c];
					draw_func[i] (&canvas_sys, pool, range);
				}
			}
		}
	}
	r_funcs->Draw_ResetClip ();
	{
		uint32_t    comp = canvas_sys.base + canvas_updateonce;
		ecs_pool_t *pool = &reg->comp_pools[comp];
		ecs_subpool_t *subpool = &reg->subpools[comp];
		pool->count = 0;
		uint32_t    rcount = subpool->num_ranges - subpool->available;
		memset (subpool->ranges, 0, rcount * sizeof (*subpool->ranges));
	}
}

void
Canvas_InitSys (canvas_system_t *canvas_sys, ecs_registry_t *reg)
{
	*canvas_sys = (canvas_system_t) {
		.reg = reg,
		.base = ECS_RegisterComponents (reg, canvas_components,
										canvas_comp_count),
		.view_base = ECS_RegisterComponents (reg, view_components,
											 view_comp_count),
		.text_base = ECS_RegisterComponents (reg, text_components,
											 text_comp_count),
		.imui_base = ECS_RegisterComponents (reg, imui_components,
											 imui_comp_count),
	};
}

void
Canvas_AddToEntity (canvas_system_t canvas_sys, uint32_t ent)
{
	canvas_t    canvas = {
		.reg = canvas_sys.reg,
		.base = canvas_sys.base,
		.visible = true
	};
	for (uint32_t i = 0; i < canvas_subpool_count; i++) {
		canvas.range[i] = ECS_NewSubpoolRange (canvas_sys.reg,
											   canvas_sys.base + i);
	}
	// add a self reference to keep _canvas_rangeid and Canvas_Entity simple
	Ent_SetComponent (ent, c_canvasref, canvas_sys.reg, &ent);
	Ent_SetComponent (ent, c_canvas, canvas_sys.reg, &canvas);
	if (!Ent_HasComponent (ent, canvas_sys.view_base + view_href,
						   canvas_sys.reg)) {
		View_AddToEntity (ent, (ecs_system_t) { canvas_sys.reg,
												canvas_sys.view_base },
						  nullview, true);
	}
}

uint32_t
Canvas_New (canvas_system_t canvas_sys)
{
	uint32_t    ent = ECS_NewEntity (canvas_sys.reg);
	Canvas_AddToEntity (canvas_sys, ent);
	return ent;
}

static int
canvas_href_cmp (const void *_a, const void *_b, void *arg)
{
	uint32_t    enta = *(const uint32_t *)_a;
	uint32_t    entb = *(const uint32_t *)_b;
	canvas_system_t *canvas_sys = arg;
	ecs_registry_t *reg = canvas_sys->reg;
	uint32_t    href = canvas_sys->view_base + view_href;
	auto ref_a = *(hierref_t *) Ent_GetComponent (enta, href, reg);
	auto ref_b = *(hierref_t *) Ent_GetComponent (entb, href, reg);
	if (ref_a.id == ref_b.id) {
		return ref_a.index - ref_b.index;
	}
	int32_t     diff = ref_a.id - ref_b.id;
	return diff > 0 ? 1 : diff < 0 ? -1 : 0;
}

void
Canvas_SortComponentPool (canvas_system_t canvas_sys, uint32_t ent,
						  uint32_t component)
{
	canvas_t   *canvas = Ent_GetComponent (ent, c_canvas, canvas_sys.reg);
	uint32_t    rid = canvas->range[component];
	uint32_t    c = component + canvas_sys.base;
	ecs_range_t range = ECS_GetSubpoolRange (canvas_sys.reg, c, rid);
	ECS_SortComponentPoolRange (canvas_sys.reg, c, range,
								canvas_href_cmp, &canvas_sys);
}

void
Canvas_SetLen (canvas_system_t canvas_sys, uint32_t ent, view_pos_t len)
{
	view_t      view = Canvas_GetRootView (canvas_sys, ent);
	View_SetLen (view, len.x, len.y);
	View_UpdateHierarchy (view);
}

static int
canvas_draw_cmp (const void *_a, const void *_b, void *arg)
{
	uint32_t    enta = *(const uint32_t *)_a;
	uint32_t    entb = *(const uint32_t *)_b;
	auto canvas_sys = *(canvas_system_t *)arg;
	auto reg = canvas_sys.reg;
	auto canvasa = (canvas_t *) Ent_GetComponent (enta, c_canvas, reg);
	auto canvasb = (canvas_t *) Ent_GetComponent (entb, c_canvas, reg);
	int diff = canvasa->draw_group - canvasb->draw_group;
	if (!diff) {
		diff = canvasa->draw_order - canvasb->draw_order;
	}
	if (!diff) {
		// order possibly undefined, but at least stable so long as the entity
		// ids are stable
		diff = enta - entb;
	}
	return diff;
}

void
Canvas_DrawSort (canvas_system_t canvas_sys)
{
	auto reg = canvas_sys.reg;
	ECS_SortComponentPool (reg, c_canvas, canvas_draw_cmp, &canvas_sys);
}
