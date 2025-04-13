/*
	imui.c

	Immediate mode user inferface

	Copyright (C) 2023 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2023/07/01

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
#include <stdlib.h>
#include <string.h>

#include "QF/darray.h"
#include "QF/dstring.h"
#include "QF/ecs.h"
#include "QF/hash.h"
#include "QF/heapsort.h"
#include "QF/mathlib.h"
#include "QF/progs.h"
#include "QF/quakeio.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "QF/input/event.h"

#include "QF/ui/canvas.h"
#include "QF/ui/font.h"
#include "QF/ui/imui.h"
#include "QF/ui/shaper.h"
#include "QF/ui/text.h"

#define IMUI_context ctx

#define c_fraction_x (ctx->csys.imui_base + imui_fraction_x)
#define c_fraction_y (ctx->csys.imui_base + imui_fraction_y)
#define c_reference (ctx->csys.imui_base + imui_reference)
#define t_passage_glyphs (ctx->csys.text_base + text_passage_glyphs)
#define c_passage_glyphs (ctx->csys.base + canvas_passage_glyphs)
#define c_glyphs (ctx->csys.base + canvas_glyphs)
#define c_color (ctx->tsys.text_base + text_color)
#define c_fill  (ctx->csys.base + canvas_fill)
#define c_updateonce  (ctx->csys.base + canvas_updateonce)

#define imui_draw_group ((1 << 30) - 1)
#define imui_draw_order(x) ((x) << 16)
#define imui_ontop imui_draw_order((1 << 15) - 1)
#define imui_onbottom imui_draw_order(-(1 << 15) + 1)

typedef struct imui_state_map_s {
	struct imui_state_map_s *next;
	struct imui_state_map_s **prev;
	imui_state_t state;
} imui_state_map_t;

struct imui_ctx_s {
	canvas_system_t csys;
	uint32_t    canvas;
	ecs_system_t vsys;
	text_system_t tsys;

	text_shaper_t *shaper;

	hashctx_t  *hashctx;
	hashtab_t  *tab;
	PR_RESMAP (imui_state_map_t) state_map;
	imui_state_map_t *state_wrappers;
	font_t     *font;

	int64_t     frame_start;
	int64_t     frame_draw;
	int64_t     frame_end;
	uint32_t    frame_count;

	view_t      root_view;
	view_t      current_parent;
	struct DARRAY_TYPE(view_t) parent_stack;
	struct DARRAY_TYPE(imui_state_t *) windows;
	struct DARRAY_TYPE(imui_state_t *) links;
	struct DARRAY_TYPE(imui_state_t *) scrollers;
	struct DARRAY_TYPE(imui_window_t *) registered_windows;
	int32_t     draw_order;
	imui_window_t *current_menu;
	imui_state_t *current_state;

	ecs_idpool_t window_ids;
	ecs_idpool_t state_ids;

	dstring_t  *dstr;

	uint32_t    hot;
	uint32_t    active;
	view_pos_t  hot_position;
	view_pos_t  active_position;
	view_pos_t  mouse_active;
	uint32_t    mouse_pressed;
	uint32_t    mouse_released;
	uint32_t    mouse_buttons;
	view_pos_t  mouse_position;
	uint32_t    shift;
	int         key_code;
	int         unicode;

	imui_style_t style;
	struct DARRAY_TYPE(imui_style_t) style_stack;
};

static void
imui_reference_destroy (void *_ref, ecs_registry_t *reg)
{
	imui_reference_t *ref = _ref;
	if (ref->ctx) {
#if 1
		//FIXME there's something wrong such that deleting the entity directly
		//instead of via a view results in corrupted href componets and an
		//href component leak
		auto ctx = ref->ctx;
		auto view = View_FromEntity (ctx->vsys, ref->ref_id);
		View_Delete (view);
#else
		ECS_DelEntity (ref->ctx->csys.reg, ref->ref_id);
#endif
	}
}

const component_t imui_components[imui_comp_count] = {
	[imui_fraction_x] = {
		.size = sizeof (imui_frac_t),
		.name = "fraction x",
	},
	[imui_fraction_y] = {
		.size = sizeof (imui_frac_t),
		.name = "fraction y",
	},
	[imui_reference] = {
		.size = sizeof (imui_reference_t),
		.name = "reference",
		.destroy = imui_reference_destroy,
	},
};

static int32_t
imui_next_window (imui_ctx_t *ctx)
{
	ctx->draw_order &= ~(imui_draw_order (1) - 1);
	ctx->draw_order += imui_draw_order (1);
	return ctx->draw_order;
}

static imui_state_t *
imui_state_new (imui_ctx_t *ctx, uint32_t entity)
{
	auto state = PR_RESNEW (ctx->state_map);
	*state = (imui_state_map_t) {
		.next = ctx->state_wrappers,
		.prev = &ctx->state_wrappers,
		.state = {
			.self = ECS_NewId (&ctx->state_ids),
			.old_entity = nullent,
			.entity = entity,
		},
	};
	if (ctx->state_wrappers) {
		ctx->state_wrappers->prev = &state->next;
	}
	ctx->state_wrappers = state;

	state->state.frame_count = ctx->frame_count;
	return &state->state;
}

static void
imui_state_free (imui_ctx_t *ctx, imui_state_map_t *state)
{
	ECS_DelId (&ctx->state_ids, state->state.self);
	if (state->next) {
		state->next->prev = state->prev;
	}
	*state->prev = state->next;
	PR_RESFREE (ctx->state_map, state);
}

imui_state_t *
IMUI_CurrentState (imui_ctx_t *ctx)
{
	return ctx->current_state;
}

imui_state_t *
IMUI_FindState (imui_ctx_t *ctx, const char *label)
{
	int         key_offset = 0;
	const char *key = strstr (label, "##");
	if (key) {
		// key is '###': hash only past this
		if (key[2] == '#') {
			key_offset = (key += 3) - label;
		}
	}
	return Hash_Find (ctx->tab, label + key_offset);
}

static imui_state_t *
imui_get_state (imui_ctx_t *ctx, const char *label, uint32_t entity)
{
	int         key_offset = 0;
	uint32_t    label_len = ~0u;
	const char *key = strstr (label, "##");
	if (key) {
		label_len = key - label;
		// key is '###': hash only past this
		if (key[2] == '#') {
			key_offset = (key += 3) - label;
		}
	}
	auto state = IMUI_FindState (ctx, label);
	if (state) {
		state->old_entity = state->entity;
		state->entity = entity;
		state->frame_count = ctx->frame_count;
		ctx->current_state = state;
		Ent_SetComponent (entity, ecs_name, ctx->csys.reg, &state->label);
		return state;
	}
	state = imui_state_new (ctx, entity);
	state->label = strdup (label);
	state->label_len = label_len == ~0u ? strlen (label) : label_len;
	state->key_offset = key_offset;
	Hash_Add (ctx->tab, state);
	ctx->current_state = state;
	Ent_SetComponent (entity, ecs_name, ctx->csys.reg, &state->label);
	return state;
}

static const char *
imui_state_getkey (const void *obj, void *data)
{
	auto state = (const imui_state_t *) obj;
	return state->label + state->key_offset;
}

imui_ctx_t *
IMUI_NewContext (canvas_system_t canvas_sys, const char *font, float fontsize)
{
	qfZoneScoped (true);
	imui_ctx_t *ctx = malloc (sizeof (imui_ctx_t));
	uint32_t canvas = Canvas_New (canvas_sys);
	*ctx = (imui_ctx_t) {
		.csys = canvas_sys,
		.canvas = canvas,
		.vsys = { canvas_sys.reg, canvas_sys.view_base },
		.tsys = { canvas_sys.reg, canvas_sys.view_base, canvas_sys.text_base },
		.shaper = Shaper_New (),
		.root_view = Canvas_GetRootView (canvas_sys, canvas),
		.parent_stack = DARRAY_STATIC_INIT (8),
		.windows = DARRAY_STATIC_INIT (8),
		.links = DARRAY_STATIC_INIT (8),
		.scrollers = DARRAY_STATIC_INIT (8),
		.registered_windows = DARRAY_STATIC_INIT (8),
		.dstr = dstring_newstr (),
		.hot = nullent,
		.active = nullent,
		.mouse_position = {-1, -1},
		.style_stack = DARRAY_STATIC_INIT (8),
		.style = {
			.background = {
				.normal = 0x04,
				.hot = 0x04,
				.active = 0x04,
			},
			.foreground = {
				.normal = 0x08,
				.hot = 0x0f,
				.active = 0x0c,
			},
			.text = {
				.normal = 0xfe,
				.hot = 0x5f,
				.active = 0x6f,
			},
		},
	};
	// "allocate" window and state id 0 so 0 can be treated as invalid
	ECS_NewId (&ctx->window_ids);
	ECS_NewId (&ctx->state_ids);

	*Canvas_DrawGroup (ctx->csys, ctx->canvas) = imui_draw_group;
	ctx->tab = Hash_NewTable (511, imui_state_getkey, 0, ctx, &ctx->hashctx);
	ctx->current_parent = ctx->root_view;

	auto fpath = Font_SystemFont (font);
	if (fpath) {
		QFile *file = Qopen (fpath, "rb");
		if (file) {
			ctx->font = Font_Load (file, fontsize);
			//Qclose (file); FIXME closed by QFS_LoadFile
		}
		free (fpath);
	}
	return ctx;
}

static void
clear_items (imui_ctx_t *ctx)
{
	uint32_t    root_ent = ctx->root_view.id;

	// delete the root view (but not the root entity)
	Ent_RemoveComponent (root_ent, ctx->root_view.comp, ctx->root_view.reg);

	for (uint32_t i = 0; i < ctx->windows.size; i++) {
		auto window = View_FromEntity (ctx->vsys, ctx->windows.a[i]->entity);
		View_Delete (window);
	}
	DARRAY_RESIZE (&ctx->parent_stack, 0);
	DARRAY_RESIZE (&ctx->windows, 0);
	DARRAY_RESIZE (&ctx->links, 0);
	DARRAY_RESIZE (&ctx->scrollers, 0);
	DARRAY_RESIZE (&ctx->style_stack, 0);
}

void
IMUI_DestroyContext (imui_ctx_t *ctx)
{
	clear_items (ctx);
	for (auto s = ctx->state_wrappers; s; s = s->next) {
		free (s->state.label);
	}
	PR_RESDELMAP (ctx->state_map);

	if (ctx->font) {
		Font_Free (ctx->font);
	}

	DARRAY_CLEAR (&ctx->parent_stack);
	DARRAY_CLEAR (&ctx->windows);
	DARRAY_CLEAR (&ctx->links);
	DARRAY_CLEAR (&ctx->scrollers);
	DARRAY_CLEAR (&ctx->style_stack);
	dstring_delete (ctx->dstr);

	Hash_DelTable (ctx->tab);
	Hash_DelContext (ctx->hashctx);
	Shaper_Delete (ctx->shaper);

	auto reg = ctx->csys.reg;
	auto pool = &reg->comp_pools[c_reference];
	for (uint32_t i = 0; i < pool->count; i++) {
		auto ref = &((imui_reference_t *)pool->data)[i];
		if (ref->ctx == ctx) {
			ref->ctx = 0;
		}
	}
	free (ctx);
}

uint32_t
IMUI_RegisterWindow (imui_ctx_t *ctx, imui_window_t *window)
{
	uint32_t    wid = ECS_NewId (&ctx->window_ids);

	if (ctx->registered_windows.size != ctx->window_ids.max_ids) {
		size_t old_size = ctx->registered_windows.size;
		size_t delta = ctx->window_ids.max_ids - old_size;
		DARRAY_RESIZE (&ctx->registered_windows, ctx->window_ids.max_ids);
		memset (ctx->registered_windows.a + old_size, 0,
				delta * sizeof (imui_window_t *));
	}
	ctx->registered_windows.a[Ent_Index (wid)] = window;
	window->self = wid;
	return wid;
}

void
IMUI_DeregisterWindow (imui_ctx_t *ctx, imui_window_t *window)
{
	if (window->self && ECS_DelId (&ctx->window_ids, window->self)) {
		ctx->registered_windows.a[Ent_Index (window->self)] = nullptr;
	}
	window->self = 0;
}

imui_window_t *
IMUI_GetWindow (imui_ctx_t *ctx, uint32_t wid)
{
	if (wid && ECS_IdValid (&ctx->window_ids, wid)) {
		return ctx->registered_windows.a[Ent_Index (wid)];
	}
	return nullptr;
}

void
IMUI_SetVisible (imui_ctx_t *ctx, bool visible)
{
	if (!visible) {
		ctx->active = nullent;
	}
	*Canvas_Visible (ctx->csys, ctx->canvas) = visible;
	for (uint32_t i = 0; i < ctx->windows.size; i++) {
		*Canvas_Visible (ctx->csys, ctx->windows.a[i]->entity) = visible;
	}
}

void
IMUI_SetSize (imui_ctx_t *ctx, int xlen, int ylen)
{
	Canvas_SetLen (ctx->csys, ctx->canvas, (view_pos_t) { xlen, ylen });
}

bool
IMUI_ProcessEvent (imui_ctx_t *ctx, const IE_event_t *ie_event)
{
	if (ie_event->type == ie_mouse) {
		auto m = &ie_event->mouse;
		ctx->mouse_position = (view_pos_t) { m->x, m->y };

		if (ie_event->mouse.type == ie_mousedown
			|| ie_event->mouse.type == ie_mouseup) {
			unsigned old = ctx->mouse_buttons;
			unsigned new = m->buttons;
			ctx->mouse_pressed = (old ^ new) & new;
			ctx->mouse_released = (old ^ new) & ~new;
			ctx->mouse_buttons = m->buttons;
		}
	} else {
		auto k = &ie_event->key;
		//printf ("imui: %d %d %x\n", k->code, k->unicode, k->shift);
		ctx->shift = k->shift;
		ctx->key_code = k->code;
		ctx->unicode = k->unicode;
	}
	return ctx->hot != nullent || ctx->active != nullent;
}

imui_io_t
IMUI_GetIO (imui_ctx_t *ctx)
{
	return (imui_io_t) {
		.mouse = ctx->mouse_position,
		.buttons = ctx->mouse_buttons,
		.pressed = ctx->mouse_pressed,
		.released = ctx->mouse_released,
		.hot = ctx->hot,
		.active = ctx->active,
	};
}

static void
set_hierarchy_tree_mode (imui_ctx_t *ctx, hierref_t ref, bool tree)
{
	auto reg = ctx->csys.reg;
	hierarchy_t *h = Ent_GetComponent (ref.id, ecs_hierarchy, reg);
	Hierarchy_SetTreeMode (h, tree);

	viewcont_t *cont = h->components[view_control];
	uint32_t   *ent = h->ent;
	for (uint32_t i = 0; i < h->num_objects; i++) {
		if (cont[i].is_link) {
			imui_reference_t *sub = Ent_GetComponent (ent[i], c_reference, reg);
			auto sub_view = View_FromEntity (ctx->vsys, sub->ref_id);
			set_hierarchy_tree_mode (ctx, View_GetRef (sub_view), tree);
		}
	}
}

void
IMUI_BeginFrame (imui_ctx_t *ctx)
{
	Shaper_FlushUnused (ctx->shaper);
	uint32_t    root_ent = ctx->root_view.id;
	auto root_size = View_GetLen (ctx->root_view);

	ctx->draw_order = imui_draw_order (ctx->windows.size);
	clear_items (ctx);
	ctx->root_view = View_AddToEntity (root_ent, ctx->vsys, nullview, true);
	set_hierarchy_tree_mode (ctx, View_GetRef (ctx->root_view), true);
	View_SetLen (ctx->root_view, root_size.x, root_size.y);

	ctx->frame_start = Sys_LongTime ();
	ctx->frame_count++;
	ctx->current_parent = ctx->root_view;
	ctx->current_menu = 0;
}

static void
prune_objects (imui_ctx_t *ctx)
{
	for (auto s = &ctx->state_wrappers; *s; ) {
		if ((*s)->state.frame_count == ctx->frame_count) {
			s = &(*s)->next;
		} else {
			if ((*s)->state.label) {
				Hash_Del (ctx->tab, (*s)->state.label + (*s)->state.key_offset);
				free ((*s)->state.label);
			}
			imui_state_free (ctx, *s);
		}
	}
}

static const char *
view_color (hierarchy_t *h, uint32_t ind, imui_ctx_t *ctx, bool for_y)
{
	auto reg = h->reg;
	uint32_t e = h->ent[ind];
	viewcont_t *cont = h->components[view_control];

	auto semantic = (imui_size_t) cont[ind].semantic_x;
	if (for_y) {
		semantic = cont[ind].semantic_y;
	}

	switch (semantic) {
		case imui_size_none:
			if (Ent_HasComponent (e, c_glyphs, reg)) {
				return CYN;
			}
			return DFL;
		case imui_size_pixels: return GRN;
		case imui_size_fittext: return CYN;
		case imui_size_fraction: return ONG;
		case imui_size_fitchildren: return MAG;
		case imui_size_expand: return RED;
	}
	return BLU;
}

static void __attribute__((used))
dump_tree (hierref_t href, int level, imui_ctx_t *ctx)
{
	auto reg = ctx->csys.reg;
	uint32_t ind = href.index;
	hierarchy_t *h = Ent_GetComponent (href.id, ecs_hierarchy, reg);
	view_pos_t *abs = h->components[view_abs];
	view_pos_t *len = h->components[view_len];
	view_resize_f *resize = h->components[view_onresize];
	view_move_f *move = h->components[view_onmove];
	auto c = ((viewcont_t *)h->components[view_control])[ind];
	uint32_t e = h->ent[ind];
	printf ("%3d:%08x %*s[%s%d %s%d"DFL"] [%s%d %s%d"DFL"] %c%s%s %s%d %s%d"DFL,
			ind, e,
			level * 3, "",
			view_color (h, ind, ctx, false), abs[ind].x,
			view_color (h, ind, ctx, true),  abs[ind].y,
			view_color (h, ind, ctx, false), len[ind].x,
			view_color (h, ind, ctx, true),  len[ind].y,
			c.vertical ? 'v' : 'h',
			resize[ind] ? "R" : "",
			move[ind] ? "M" : "",
			view_color (h, ind, ctx, false), c.semantic_x,
			view_color (h, ind, ctx, true),  c.semantic_y);
	for (uint32_t j = 0; j < reg->components.size; j++) {
		if (Ent_HasComponent (e, j, reg)) {
			printf (", %s", reg->components.a[j].name);
			if (j == c_fraction_x || j == c_fraction_y) {
				auto val = *(imui_frac_t *) Ent_GetComponent (e, j, reg);
				printf ("(%s%d/%d"DFL")",
						view_color (h, ind, ctx, j == c_fraction_y),
						val.num, val.den);
			}
			if (j == ecs_name) {
				auto name = *(const char **) Ent_GetComponent (e, j, reg);
				printf ("(%s)", name);
			}
			if (j == c_reference) {
				auto ref = *(imui_reference_t *) Ent_GetComponent (e, j, reg);
				printf ("(%08x)", ref.ref_id);
			}
		}
	}
	printf (DFL"\n");

	if (c.is_link) {
		printf (GRN"%3d: %*slink"DFL"\n", ind, 8 + level * 3, "");
		auto reg = ctx->csys.reg;
		uint32_t    ent = h->ent[ind];
		imui_reference_t *sub = Ent_GetComponent (ent, c_reference, reg);
		auto sub_view = View_FromEntity (ctx->vsys, sub->ref_id);
		auto href = View_GetRef (sub_view);
		dump_tree (href, level + 1, ctx);
		printf (RED"%3d: %*slink"DFL"\n", ind, 8 + level * 3, "");
	}
	if (h->childIndex[ind] > ind) {
		for (uint32_t i = 0; i < h->childCount[ind]; i++) {
			if (h->childIndex[ind] + i >= h->num_objects) {
				break;
			}
			hierref_t cref = { .id = href.id, .index = h->childIndex[ind] + i };
			dump_tree (cref, level + 1, ctx);
		}
	}
	if (!level) {
		puts ("");
	}
}

typedef struct {
	bool        x, y;
	uint32_t    num_views;		// number of views in sub-hierarchy
} downdep_t;

static int
fraction (uint32_t ent, int len, uint32_t fcomp, ecs_registry_t *reg)
{
	auto f = *(imui_frac_t *) Ent_GetComponent (ent, fcomp, reg);
	int val = len;
	if (f.den > 0) {
		val = (len * f.num) / f.den;
		if (val > len) {
			val = len;
		}
		if (val < 1) {
			val = 1;
		}
	}
	return val;
}

static void
calc_upwards_dependent (imui_ctx_t *ctx, hierref_t href,
						downdep_t *down_depend)
{
	auto reg = ctx->csys.reg;
	hierarchy_t *h = Ent_GetComponent (href.id, ecs_hierarchy, reg);
	uint32_t   *ent = h->ent;
	view_pos_t *len = h->components[view_len];
	viewcont_t *cont = h->components[view_control];
	uint32_t   *parent = h->parentIndex;
	downdep_t  *side_depend = down_depend + h->num_objects;

	down_depend[0].num_views = h->num_objects;
	for (uint32_t i = 0; i < h->num_objects; i++) {
		if (i > 0) {
			down_depend[i].num_views = 0;
		}
		if (cont[i].semantic_x == imui_size_fitchildren
			|| cont[i].semantic_x == imui_size_expand) {
			down_depend[i].x = true;
		} else if (!(i > 0 && (down_depend[i].x = down_depend[parent[i]].x))
				   && cont[i].semantic_x == imui_size_fraction) {
			len[i].x = fraction (ent[i], len[parent[i]].x, c_fraction_x, reg);
		} else if (cont[i].semantic_x == imui_size_pixels
				   && Ent_HasComponent (ent[i], c_fraction_x, reg)) {
			//FIXME uses numerator for pixels
			int *pixels = Ent_GetComponent (ent[i], c_fraction_x, reg);
			len[i].x = *pixels;
			down_depend[i].x = false;
		}
		if (cont[i].semantic_y == imui_size_fitchildren
			|| cont[i].semantic_y == imui_size_expand) {
			down_depend[i].y = true;
		} else if (!(i > 0 && (down_depend[i].y = down_depend[parent[i]].y))
				   && cont[i].semantic_y == imui_size_fraction) {
			len[i].y = fraction (ent[i], len[parent[i]].y, c_fraction_y, reg);
		} else if (cont[i].semantic_y == imui_size_pixels
				   && Ent_HasComponent (ent[i], c_fraction_y, reg)) {
			//FIXME uses numerator for pixels
			int *pixels = Ent_GetComponent (ent[i], c_fraction_y, reg);
			len[i].y = *pixels;
			down_depend[i].y = false;
		}
		if (cont[i].is_link) {
			imui_reference_t *sub = Ent_GetComponent (ent[i], c_reference, reg);
			auto sub_view = View_FromEntity (ctx->vsys, sub->ref_id);
			auto href = View_GetRef (sub_view);
			// control logic was propagated from the linked hierarcy, so
			// propagate down_depend to the linked hierarcy.
			side_depend[0] = down_depend[i];
			calc_upwards_dependent (ctx, href, side_depend);
			down_depend[0].num_views += side_depend[0].num_views;
			side_depend += side_depend[0].num_views;
		}
	}
}

static void
calc_downwards_dependent (imui_ctx_t *ctx, hierref_t href)
{
	auto reg = ctx->csys.reg;
	hierarchy_t *h = Ent_GetComponent (href.id, ecs_hierarchy, reg);
	uint32_t   *ent = h->ent;
	view_pos_t *len = h->components[view_len];
	viewcont_t *cont = h->components[view_control];
	for (uint32_t i = h->num_objects; i-- > 0; ) {
		if (cont[i].is_link) {
			imui_reference_t *sub = Ent_GetComponent (ent[i], c_reference, reg);
			auto sub_view = View_FromEntity (ctx->vsys, sub->ref_id);
			calc_downwards_dependent (ctx, View_GetRef (sub_view));
			len[i] = View_GetLen (sub_view);
		}
		view_pos_t  clen = len[i];
		if (cont[i].semantic_x == imui_size_fitchildren
			|| cont[i].semantic_x == imui_size_expand) {
			clen.x = 0;
			if (cont[i].vertical) {
				for (uint32_t j = 0; j < h->childCount[i]; j++) {
					uint32_t child = h->childIndex[i] + j;
					clen.x = max (clen.x, len[child].x);
				}
			} else {
				for (uint32_t j = 0; j < h->childCount[i]; j++) {
					uint32_t child = h->childIndex[i] + j;
					clen.x += len[child].x;
				}
			}
		}
		if (cont[i].semantic_y == imui_size_fitchildren
			|| cont[i].semantic_y == imui_size_expand) {
			clen.y = 0;
			if (!cont[i].vertical) {
				for (uint32_t j = 0; j < h->childCount[i]; j++) {
					uint32_t child = h->childIndex[i] + j;
					clen.y = max (clen.y, len[child].y);
				}
			} else {
				for (uint32_t j = 0; j < h->childCount[i]; j++) {
					uint32_t child = h->childIndex[i] + j;
					clen.y += len[child].y;
				}
			}
		}
		len[i] = clen;
	}
}

static void
calc_expansions (imui_ctx_t *ctx, hierref_t href)
{
	auto reg = ctx->csys.reg;
	hierarchy_t *h = Ent_GetComponent (href.id, ecs_hierarchy, reg);
	uint32_t   *ent = h->ent;
	uint32_t   *parent = h->parentIndex;
	view_pos_t *len = h->components[view_len];
	viewcont_t *cont = h->components[view_control];

	for (uint32_t i = 0; i < h->num_objects; i++) {
		if (i && cont[i].semantic_x == imui_size_fraction) {
			len[i].x = fraction (ent[i], len[parent[i]].x, c_fraction_x, reg);
		}
		if (i && cont[i].semantic_y == imui_size_fraction) {
			len[i].y = fraction (ent[i], len[parent[i]].y, c_fraction_y, reg);
		}

		view_pos_t  tlen = {};
		view_pos_t  elen = {};
		view_pos_t  ecount = {};
		for (uint32_t j = 0; j < h->childCount[i]; j++) {
			uint32_t child = h->childIndex[i] + j;
			tlen.x += len[child].x;
			tlen.y += len[child].y;
			if (cont[child].semantic_x == imui_size_expand) {
				//FIXME uses numerator for weight
				int *p = Ent_GetComponent (ent[child], c_fraction_x, reg);
				elen.x += *p;
				ecount.x++;
			}
			if (cont[child].semantic_y == imui_size_expand) {
				//FIXME uses numerator for weight
				int *p = Ent_GetComponent (ent[child], c_fraction_y, reg);
				elen.y += *p;
				ecount.y++;
			}
		}
		if (!ecount.x && !ecount.y) {
			continue;
		}
		if (cont[i].vertical) {
			int space = len[i].y - tlen.y;
			int filled = 0;
			for (uint32_t j = 0; j < h->childCount[i]; j++) {
				uint32_t child = h->childIndex[i] + j;
				if (cont[child].semantic_x == imui_size_expand) {
					len[child].x = len[i].x;
				}
				if (cont[child].semantic_y == imui_size_expand) {
					//FIXME uses numerator for weight
					int *p = Ent_GetComponent (ent[child], c_fraction_y, reg);
					int delta = *p * space / elen.y;
					len[child].y += max (delta, 0);
					filled += max (delta, 0);
				}
			}
			if (ecount.y && filled < space) {
				space -= filled;
				for (uint32_t j = 0; space && j < h->childCount[i]; j++) {
					uint32_t child = h->childIndex[i] + j;
					if (cont[child].semantic_y == imui_size_expand) {
						len[child].y++;
						space--;
					}
				}
			}
		} else {
			int space = len[i].x - tlen.x;
			int filled = 0;
			for (uint32_t j = 0; j < h->childCount[i]; j++) {
				uint32_t child = h->childIndex[i] + j;
				if (cont[child].semantic_x == imui_size_expand) {
					//FIXME uses numerator for weight
					int *p = Ent_GetComponent (ent[child], c_fraction_x, reg);
					int delta = *p * space / elen.x;
					len[child].x += max (delta, 0);
					filled += max (delta, 0);
				}
				if (cont[child].semantic_y == imui_size_expand) {
					len[child].y = len[i].y;
				}
			}
			if (ecount.x && filled < space) {
				space -= filled;
				for (uint32_t j = 0; space && j < h->childCount[i]; j++) {
					uint32_t child = h->childIndex[i] + j;
					if (cont[child].semantic_x == imui_size_expand) {
						len[child].x++;
						space--;
					}
				}
			}
		}
	}
	for (uint32_t i = 0; i < h->num_objects; i++) {
		if (cont[i].is_link) {
			imui_reference_t *sub = Ent_GetComponent (ent[i], c_reference, reg);
			auto sub_view = View_FromEntity (ctx->vsys, sub->ref_id);
			View_SetLen (sub_view, len[i].x, len[i].y);
			calc_expansions (ctx, View_GetRef (sub_view));
			if (sub->update) {
				View_UpdateHierarchy (sub_view);
				if (cont[i].semantic_x == imui_size_fitchildren) {
					len[i].x = View_GetLen (sub_view).x;
				}
				if (cont[i].semantic_y == imui_size_fitchildren) {
					len[i].y = View_GetLen (sub_view).y;
				}
			}
		}
	}
}

static uint32_t __attribute__((pure))
count_views (imui_ctx_t *ctx, hierref_t href)
{
	auto reg = ctx->csys.reg;
	hierarchy_t *h = Ent_GetComponent (href.id, ecs_hierarchy, reg);
	uint32_t    count = h->num_objects;
	viewcont_t *cont = h->components[view_control];
	uint32_t   *ent = h->ent;

	// the root object is never a link (if it has a reference component, it's
	// to the pseudo-parent of the hierarchy)
	for (uint32_t i = 1; i < h->num_objects; i++) {
		if (cont[i].is_link) {
			imui_reference_t *sub = Ent_GetComponent (ent[i], c_reference, reg);
			auto sub_view = View_FromEntity (ctx->vsys, sub->ref_id);
			count += count_views (ctx, View_GetRef (sub_view));
		}
	}
	return count;
}

static void
position_views (imui_ctx_t *ctx, view_t root_view)
{
	auto reg = ctx->vsys.reg;
	auto href = View_GetRef (root_view);
	hierarchy_t *h = Ent_GetComponent (href.id, ecs_hierarchy, reg);

	uint32_t   *ent = h->ent;
	view_pos_t *pos = h->components[view_pos];
	view_pos_t *len = h->components[view_len];
	viewcont_t *cont = h->components[view_control];
	uint32_t   *parent = h->parentIndex;

	if (Ent_HasComponent (root_view.id, c_reference, ctx->vsys.reg)) {
		auto ent = root_view.id;
		imui_reference_t *reference = Ent_GetComponent (ent, c_reference, reg);
		auto anchor = View_FromEntity (ctx->vsys, reference->ref_id);
		pos[0] = View_GetAbs (anchor);
	}
	view_pos_t  cpos = {};
	uint32_t    cur_parent = 0;
	for (uint32_t i = 0; i < h->num_objects; i++) {
		if (parent[i] != cur_parent) {
			cur_parent = parent[i];
			cpos = (view_pos_t) {};
		}
		if (!cont[i].free_x && !cont[i].free_y) {
			pos[i] = cpos;
		} else if (!cont[i].free_x) {
			pos[i].x = cpos.x;
		} else if (!cont[i].free_y) {
			pos[i].y = cpos.y;
		}
		if (i > 0 && cont[parent[i]].vertical) {
			cpos.y += cont[i].semantic_y == imui_size_none ? 0 : len[i].y;
		} else {
			cpos.x += cont[i].semantic_x == imui_size_none ? 0 : len[i].x;
		}
	}

	View_UpdateHierarchy (root_view);
	for (uint32_t i = 0; i < h->num_objects; i++) {
		if (cont[i].is_link) {
			auto reg = ctx->vsys.reg;
			imui_reference_t *sub = Ent_GetComponent (ent[i], c_reference, reg);
			auto sub_view = View_FromEntity (ctx->vsys, sub->ref_id);
			position_views (ctx, sub_view);
		}
	}
}

//FIXME currently works properly only for grav_northwest
static void
layout_objects (imui_ctx_t *ctx, view_t root_view)
{
	auto href = View_GetRef (root_view);

	downdep_t   down_depend[count_views (ctx, href)];

	// the root view size is always explicit
	down_depend[0] = (downdep_t) { };
	calc_upwards_dependent (ctx, href, down_depend);
	calc_downwards_dependent (ctx, href);
	calc_expansions (ctx, href);
	//dump_tree (href, 0, ctx);
	// resolve conflicts
	//fflush (stdout);

	position_views (ctx, root_view);
	//dump_tree (href, 0, ctx);
}

static void
check_inside (imui_ctx_t *ctx, view_t root_view)
{
	auto reg = ctx->vsys.reg;
	auto href = View_GetRef (root_view);
	hierarchy_t *h = Ent_GetComponent (href.id, ecs_hierarchy, reg);

	uint32_t   *ent = h->ent;
	view_pos_t *abs = h->components[view_abs];
	view_pos_t *len = h->components[view_len];
	viewcont_t *cont = h->components[view_control];
	auto mp = ctx->mouse_position;

	for (uint32_t i = 0; i < h->num_objects; i++) {
		if (cont[i].active
			&& mp.x >= abs[i].x && mp.y >= abs[i].y
			&& mp.x < abs[i].x + len[i].x && mp.y < abs[i].y + len[i].y) {
			if (ctx->active == ent[i] || ctx->active == nullent) {
				ctx->hot = ent[i];
				ctx->hot_position = abs[i];
			}
		}
		if (ent[i] == ctx->active) {
			ctx->active_position = abs[i];
		}
	}
	//printf ("check_inside: %8x %8x\n", ctx->hot, ctx->active);
}

static int
imui_window_cmp (const void *a, const void *b)
{
	auto windowa = *(imui_state_t **) a;
	auto windowb = *(imui_state_t **) b;
	return windowa->draw_order - windowb->draw_order;
}

static void
sort_windows (imui_ctx_t *ctx)
{
	heapsort (ctx->windows.a, ctx->windows.size, sizeof (imui_state_t *),
			  imui_window_cmp);
	for (uint32_t i = 0; i < ctx->windows.size; i++) {
		auto window = ctx->windows.a[i];
		window->draw_order = imui_draw_order (i + 1);
		*Canvas_DrawOrder (ctx->csys, window->entity) = window->draw_order;
		for (uint32_t j = 0; j < window->num_links; j++) {
			auto link = ctx->links.a[window->first_link + j];
			link->draw_order = window->draw_order + j + 1;
			*Canvas_DrawOrder (ctx->csys, link->entity) = link->draw_order;
		}
	}
}

void
IMUI_Draw (imui_ctx_t *ctx)
{
	ctx->frame_draw = Sys_LongTime ();
	ctx->mouse_pressed = 0;
	ctx->mouse_released = 0;
	sort_windows (ctx);
	auto href = View_GetRef (ctx->root_view);
	set_hierarchy_tree_mode (ctx, href, false);
	for (uint32_t i = 0; i < ctx->windows.size; i++) {
		auto win = ctx->windows.a[i];
		auto window = View_FromEntity (ctx->vsys, win->entity);
		View_SetPos (window, win->pos.x, win->pos.y);
		if (win->auto_fit) {
			View_SetLen (window, 0, 0);
		} else {
			View_SetLen (window, win->len.x, win->len.y);
		}
		set_hierarchy_tree_mode (ctx, View_GetRef (window), false);
	}
	Canvas_DrawSort (ctx->csys);

	prune_objects (ctx);
	layout_objects (ctx, ctx->root_view);
	for (uint32_t i = 0; i < ctx->windows.size; i++) {
		auto window = View_FromEntity (ctx->vsys, ctx->windows.a[i]->entity);
		layout_objects (ctx, window);
	}
	ctx->hot = nullent;
	check_inside (ctx, ctx->root_view);
	for (uint32_t i = 0; i < ctx->windows.size; i++) {
		auto window = View_FromEntity (ctx->vsys, ctx->windows.a[i]->entity);
		check_inside (ctx, window);
	}

	for (uint32_t i = 0; i < ctx->scrollers.size; i++) {
		auto scroller = ctx->scrollers.a[i];
		auto view = View_FromEntity (ctx->vsys, scroller->entity);
		scroller->len = View_GetLen (view);
	}

	ctx->frame_end = Sys_LongTime ();
}

int
IMUI_PushLayout (imui_ctx_t *ctx, bool vertical)
{
	DARRAY_APPEND (&ctx->parent_stack, ctx->current_parent);
	auto view = View_New (ctx->vsys, ctx->current_parent);
	auto pcont = View_Control (ctx->current_parent);
	ctx->current_parent = view;
	auto x_size = pcont->vertical ? imui_size_expand
								  : imui_size_fitchildren;
	auto y_size = pcont->vertical ? imui_size_fitchildren
								  : imui_size_expand;
	*View_Control (view) = (viewcont_t) {
		.gravity = grav_northwest,
		.visible = 1,
		.semantic_x = x_size,
		.semantic_y = y_size,
		.vertical = vertical,
	};
	auto state = imui_state_new (ctx, view.id);
	ctx->current_state = state;
	View_SetLen (view, 0, 0);
	if (x_size == imui_size_expand) {
		Ent_SetComponent (view.id, c_fraction_x, ctx->csys.reg,
						  &(imui_frac_t) { 100, 100 });
	}
	if (y_size == imui_size_expand) {
		Ent_SetComponent (view.id, c_fraction_y, ctx->csys.reg,
						  &(imui_frac_t) { 100, 100 });
	}
	return 0;
}

void
IMUI_PopLayout (imui_ctx_t *ctx)
{
	ctx->current_parent = DARRAY_REMOVE (&ctx->parent_stack);
}

void
IMUI_Layout_SetXSize (imui_ctx_t *ctx, imui_size_t size, int value)
{
	auto pcont = View_Control (ctx->current_parent);
	uint32_t id = ctx->current_parent.id;
	pcont->semantic_x = size;
	if (size == imui_size_fraction || size == imui_size_expand
		|| size == imui_size_pixels) {
		Ent_SetComponent (id, c_fraction_x, ctx->csys.reg,
						  &(imui_frac_t) { value, 100 });
	}
}

void
IMUI_Layout_SetYSize (imui_ctx_t *ctx, imui_size_t size, int value)
{
	auto pcont = View_Control (ctx->current_parent);
	uint32_t id = ctx->current_parent.id;
	pcont->semantic_y = size;
	if (size == imui_size_fraction || size == imui_size_expand
		|| size == imui_size_pixels) {
		Ent_SetComponent (id, c_fraction_y, ctx->csys.reg,
						  &(imui_frac_t) { value, 100 });
	}
}

int
IMUI_PushStyle (imui_ctx_t *ctx, const imui_style_t *style)
{
	DARRAY_APPEND (&ctx->style_stack, ctx->style);
	if (style) {
		ctx->style = *style;
	}
	return 0;
}

void
IMUI_PopStyle (imui_ctx_t *ctx)
{
	ctx->style = DARRAY_REMOVE (&ctx->style_stack);
}

void
IMUI_Style_Update (imui_ctx_t *ctx, const imui_style_t *style)
{
	ctx->style = *style;
}

void
IMUI_Style_Fetch (const imui_ctx_t *ctx, imui_style_t *style)
{
	*style = ctx->style;
}

static bool
check_button_state (imui_ctx_t *ctx, uint32_t entity)
{
	bool result = false;
	//printf ("check_button_state: h:%8x a:%8x e:%8x\n", ctx->hot, ctx->active, entity);
	if (ctx->active == entity) {
		if (ctx->mouse_released & 1) {
			result = ctx->hot == entity;
			ctx->active = nullent;
		}
	} else if (ctx->hot == entity) {
		if (ctx->mouse_pressed & 1) {
			ctx->active = entity;
		}
	}
	return result;
}

static view_pos_t
check_drag_delta (imui_ctx_t *ctx, uint32_t entity)
{
	view_pos_t  delta = {};
	if (ctx->active == entity) {
		auto active = ctx->active_position;
		auto anchor = VP_add (active, ctx->mouse_active);
		delta = VP_sub (ctx->mouse_position, anchor);
		if (ctx->mouse_released & 1) {
			ctx->active = nullent;
		}
	} else if (ctx->hot == entity) {
		if (ctx->mouse_pressed & 1) {
			ctx->mouse_active = VP_sub (ctx->mouse_position, ctx->hot_position);
			ctx->active = entity;
		}
	}
	return delta;
}

static view_t
add_text (imui_ctx_t *ctx, view_t view, imui_state_t *state, int mode)
{
	auto     reg = ctx->csys.reg;

	auto text = Text_StringView (ctx->tsys, view, ctx->font,
								 state->label, state->label_len, 0, 0,
								 ctx->shaper);

	int ascender = ctx->font->face->size->metrics.ascender / 64;
	int descender = ctx->font->face->size->metrics.descender / 64;
	auto len = View_GetLen (text);
	View_SetLen (text, len.x, ascender - descender);
	// text is positioned such that 0 is the baseline, and +y offset moves
	// the text down. The font's global ascender is used to find the baseline
	// relative to the top of the view.
	auto pos = View_GetPos (text);
	View_SetPos (text, pos.x, pos.y - len.y + ascender);
	View_SetGravity (text, grav_northwest);
	// prevent the layout system from repositioning the text view
	View_Control (text)->free_x = 1;
	View_Control (text)->free_y = 1;

	View_SetVisible (text, 1);
	Ent_SetComponent (text.id, c_glyphs, reg,
					  Ent_GetComponent (text.id, t_passage_glyphs, reg));

	len = View_GetLen (text);
	View_SetLen (view, len.x, len.y);

	uint32_t color = ctx->style.text.color[mode];
	*(uint32_t *) Ent_AddComponent (text.id, c_color, ctx->tsys.reg) = color;

	return text;
}

static int
update_hot_active (imui_ctx_t *ctx, imui_state_t *state)
{
	int mode = 0;
	if (state->old_entity != nullent) {
		if (ctx->hot == state->old_entity) {
			ctx->hot = state->entity;
			mode = 1;
		}
		if (ctx->active == state->old_entity) {
			ctx->active = state->entity;
			mode = 2;
		}
	}
	return mode;
}

static void
set_fill (imui_ctx_t *ctx, view_t view, byte color)
{
	*(byte*) Ent_AddComponent (view.id, c_fill, ctx->csys.reg) = color;
}

static void
set_control (imui_ctx_t *ctx, view_t view, bool active)
{
	*View_Control (view) = (viewcont_t) {
		.gravity = grav_northwest,
		.visible = 1,
		.semantic_x = imui_size_pixels,
		.semantic_y = imui_size_pixels,
		.active = active,
	};
}

static void
set_expand_x (imui_ctx_t *ctx, view_t view, int weight)
{
	View_Control (view)->semantic_x = imui_size_expand;
	Ent_SetComponent (view.id, c_fraction_x, ctx->csys.reg,
					  &(imui_frac_t) { weight, 100 });
}

void
IMUI_SetFill (imui_ctx_t *ctx, byte color)
{
	auto state = ctx->current_state;
	auto view = View_FromEntity (ctx->vsys, state->entity);
	set_fill (ctx, view, color);
}

void
IMUI_Label (imui_ctx_t *ctx, const char *label)
{
	auto view = View_New (ctx->vsys, ctx->current_parent);
	set_control (ctx, view, true);

	auto state = imui_get_state (ctx, label, view.id);

	set_fill (ctx, view, ctx->style.background.normal);
	add_text (ctx, view, state, 0);
	auto len = View_GetLen (view);
	state->len = len;
}

void
IMUI_Labelf (imui_ctx_t *ctx, const char *fmt, ...)
{
	va_list     args;
	va_start (args, fmt);
	dvsprintf (ctx->dstr, fmt, args);
	va_end (args);
	IMUI_Label (ctx, ctx->dstr->str);
}

void
IMUI_Passage (imui_ctx_t *ctx, const char *name, struct passage_s *passage)
{
	auto anchor_view = View_New (ctx->vsys, ctx->current_parent);
	*View_Control (anchor_view) = (viewcont_t) {
		.gravity = grav_northwest,
		.visible = 1,
		.semantic_x = imui_size_expand,
		.semantic_y = imui_size_fitchildren,
		.free_y = 1,
		.vertical = true,
		.active = 1,
	};
	auto reg = ctx->csys.reg;
	Ent_SetComponent (anchor_view.id, c_fraction_x, reg,
					  &(imui_frac_t) { 100, 100 });

	uint32_t    parent = ctx->current_parent.id;
	if (Ent_HasComponent (parent, ecs_name, ctx->csys.reg)) {
		name = *(char **) Ent_GetComponent (parent, ecs_name, ctx->csys.reg);
	}
	auto state = imui_get_state (ctx, va ("%s#content", name), anchor_view.id);
	DARRAY_APPEND (&ctx->scrollers, state);
	update_hot_active (ctx, state);

	View_SetPos (anchor_view, -state->pos.x, -state->pos.y);

	auto psg_view = Text_PassageView (ctx->tsys, nullview,
									  ctx->font, passage, ctx->shaper);
	Canvas_SetReference (ctx->csys, psg_view.id,
						 Canvas_Entity (ctx->csys,
										View_GetRoot (anchor_view).id));
	Ent_SetComponent (psg_view.id, c_passage_glyphs, reg,
					  Ent_GetComponent (psg_view.id, t_passage_glyphs, reg));
	*View_Control (psg_view) = (viewcont_t) {
		.gravity = grav_northwest,
		.visible = 1,
		.free_x = 1,
		.free_y = 1,
		.vertical = true,
		.active = 1,
	};

	View_Control (anchor_view)->is_link = 1;
	imui_reference_t link = {
		.ref_id = psg_view.id,
		.update = true,
	};
	Ent_SetComponent (anchor_view.id, c_reference, anchor_view.reg, &link);

	imui_reference_t anchor = {
		.ref_id = anchor_view.id,
	};
	Ent_SetComponent (psg_view.id, c_reference, psg_view.reg, &anchor);
}

bool
IMUI_Button (imui_ctx_t *ctx, const char *label)
{
	auto view = View_New (ctx->vsys, ctx->current_parent);
	set_control (ctx, view, true);

	auto state = imui_get_state (ctx, label, view.id);
	int mode = update_hot_active (ctx, state);

	set_fill (ctx, view, ctx->style.foreground.color[mode]);
	add_text (ctx, view, state, mode);

	return check_button_state (ctx, state->entity);
}

bool
IMUI_Checkbox (imui_ctx_t *ctx, bool *flag, const char *label)
{
	auto view = View_New (ctx->vsys, ctx->current_parent);
	set_control (ctx, view, true);
	View_Control (view)->semantic_x = imui_size_fitchildren;
	View_Control (view)->semantic_y = imui_size_fitchildren;

	auto state = imui_get_state (ctx, label, view.id);
	int mode = update_hot_active (ctx, state);

	set_fill (ctx, view, ctx->style.background.color[mode]);

	auto checkbox = View_New (ctx->vsys, view);
	set_control (ctx, checkbox, false);
	View_SetLen (checkbox, 20, 20);
	set_fill (ctx, checkbox, ctx->style.foreground.color[mode]);
	if (!*flag) {
		auto punch = View_New (ctx->vsys, checkbox);
		set_control (ctx, punch, false);
		View_SetGravity (punch, grav_center);
		View_SetLen (punch, 14, 14);
		set_fill (ctx, punch, ctx->style.background.color[mode]);
	}

	if (state->label_len) {
		auto text = View_New (ctx->vsys, view);
		set_control (ctx, text, false);
		add_text (ctx, text, state, mode);
	}

	if (check_button_state (ctx, state->entity)) {
		*flag = !*flag;
	}
	return *flag;
}

void
IMUI_Radio (imui_ctx_t *ctx, int *curvalue, int value, const char *label)
{
	auto view = View_New (ctx->vsys, ctx->current_parent);
	set_control (ctx, view, true);
	View_Control (view)->semantic_x = imui_size_fitchildren;
	View_Control (view)->semantic_y = imui_size_fitchildren;

	auto state = imui_get_state (ctx, label, view.id);
	int mode = update_hot_active (ctx, state);

	set_fill (ctx, view, ctx->style.background.color[mode]);

	auto checkbox = View_New (ctx->vsys, view);
	set_control (ctx, checkbox, false);
	View_SetLen (checkbox, 20, 20);
	set_fill (ctx, checkbox, ctx->style.foreground.color[mode]);
	if (*curvalue != value) {
		auto punch = View_New (ctx->vsys, checkbox);
		set_control (ctx, punch, false);
		View_SetGravity (punch, grav_center);
		View_SetLen (punch, 14, 14);
		set_fill (ctx, punch, ctx->style.background.color[mode]);
	}

	if (state->label_len) {
		auto text = View_New (ctx->vsys, view);
		set_control (ctx, text, false);
		add_text (ctx, text, state, mode);
	}

	if (check_button_state (ctx, state->entity)) {
		*curvalue = value;
	}
}

void
IMUI_Slider (imui_ctx_t *ctx, float *value, float minval, float maxval,
			 const char *label)
{
}

static view_t
sized_view (imui_ctx_t *ctx,
			imui_size_t xsize, int xvalue,
			imui_size_t ysize, int yvalue, bool active)
{
	auto view = View_New (ctx->vsys, ctx->current_parent);
	View_SetLen (ctx->current_parent, 0, 0);

	int         xlen = 0;
	int         ylen = 0;
	if (xsize == imui_size_pixels) {
		xlen = xvalue;
	}
	if (ysize == imui_size_pixels) {
		ylen = yvalue;
	}
	set_control (ctx, view, active);
	View_SetLen (view, xlen, ylen);
	View_Control (view)->semantic_x = xsize;
	View_Control (view)->semantic_y = ysize;
	auto reg = ctx->csys.reg;
	if (xsize == imui_size_fraction || xsize == imui_size_expand) {
		Ent_SetComponent (view.id, c_fraction_x, reg,
						  &(imui_frac_t) { xvalue, 100 });
	}
	if (ysize == imui_size_fraction || ysize == imui_size_expand) {
		Ent_SetComponent (view.id, c_fraction_y, reg,
						  &(imui_frac_t) { yvalue, 100 });
	}
	return view;
}

void
IMUI_Spacer (imui_ctx_t *ctx,
			 imui_size_t xsize, int xvalue,
			 imui_size_t ysize, int yvalue)
{
	auto view = sized_view (ctx, xsize, xvalue, ysize, yvalue, false);
	set_fill (ctx, view, ctx->style.background.normal);
}

static void
create_reference_anchor (imui_ctx_t *ctx, uint32_t ent, imui_window_t *panel)
{
	auto state = IMUI_FindState (ctx, panel->reference);
	if (!state) {
		Sys_Printf ("IMUI: unknown widget '%s' for '%s'\n", panel->reference,
					panel->name);
		return;
	}
	if (!ECS_EntValid (state->entity, ctx->csys.reg)) {
		Sys_Printf ("IMUI: invalid widget reference '%s' for '%s'\n",
					panel->reference, panel->name);
		return;
	}
	auto refview = View_FromEntity (ctx->vsys, state->entity);
	auto anchor = View_New (ctx->vsys, refview);
	View_SetPos (anchor, 0, 0);
	View_SetLen (anchor, 0, 0);
	View_SetGravity (anchor, panel->anchor_gravity);
	imui_reference_t reference = {
		.ref_id = anchor.id,
	};
	Ent_SetComponent (ent, c_reference, ctx->vsys.reg, &reference);
}

view_pos_t
IMUI_Dragable (imui_ctx_t *ctx,
			   imui_size_t xsize, int xvalue,
			   imui_size_t ysize, int yvalue,
			   const char *name)
{
	auto view = sized_view (ctx, xsize, xvalue, ysize, yvalue, true);

	auto state = imui_get_state (ctx, name, view.id);
	int mode = update_hot_active (ctx, state);
	auto delta = check_drag_delta (ctx, state->entity);

	set_fill (ctx, view, ctx->style.foreground.color[mode]);
	return delta;
}

static void
drag_window_tl (view_pos_t delta, imui_window_t *window)
{
	int x = max (window->xlen - delta.x, 20);
	int y = max (window->ylen - delta.y, 20);

	window->xpos += window->xlen - x;
	window->ypos += window->ylen - y;
	window->xlen = x;
	window->ylen = y;
}

static void
drag_window_tc (view_pos_t delta, imui_window_t *window)
{
	int y = max (window->ylen - delta.y, 20);

	window->ypos += window->ylen - y;
	window->ylen = y;
}

static void
drag_window_tr (view_pos_t delta, imui_window_t *window)
{
	int x = max (window->xlen + delta.x, 20);
	int y = max (window->ylen - delta.y, 20);

	window->ypos += window->ylen - y;
	window->xlen = x;
	window->ylen = y;
}

static void
drag_window_cl (view_pos_t delta, imui_window_t *window)
{
	int x = max (window->xlen - delta.x, 20);

	window->xpos += window->xlen - x;
	window->xlen = x;
}

static void
drag_window_cr (view_pos_t delta, imui_window_t *window)
{
	int x = max (window->xlen + delta.x, 20);

	window->xlen = x;
}

static void
drag_window_bl (view_pos_t delta, imui_window_t *window)
{
	int x = max (window->xlen - delta.x, 20);
	int y = max (window->ylen + delta.y, 20);

	window->xpos += window->xlen - x;
	window->xlen = x;
	window->ylen = y;
}

static void
drag_window_bc (view_pos_t delta, imui_window_t *window)
{
	int y = max (window->ylen + delta.y, 20);

	window->ylen = y;
}

static void
drag_window_br (view_pos_t delta, imui_window_t *window)
{
	int x = max (window->xlen + delta.x, 20);
	int y = max (window->ylen + delta.y, 20);

	window->xlen = x;
	window->ylen = y;
}

#define drag(c,e,sx,xv,sy,yv,n,p) \
	drag_window_##c (IMUI_Dragable (ctx, sx, xv, sy, yv, \
									va ("%s##drag_" #c #e, n)), p);

int
IMUI_StartPanel (imui_ctx_t *ctx, imui_window_t *panel)
{
	if (!panel->is_open) {
		return 1;
	}
	auto canvas = Canvas_New (ctx->csys);
	int draw_group = imui_draw_group + panel->group_offset;
	*Canvas_DrawGroup (ctx->csys, canvas) = draw_group;
	auto panel_view = Canvas_GetRootView (ctx->csys, canvas);

	auto state = imui_get_state (ctx, panel->name, panel_view.id);
	state->draw_group = draw_group;
	panel->mode = update_hot_active (ctx, state);

	DARRAY_APPEND (&ctx->parent_stack, ctx->current_parent);

	set_hierarchy_tree_mode (ctx, View_GetRef (panel_view), true);

	grav_t      gravity = grav_northwest;
	if (panel->reference) {
		create_reference_anchor (ctx, canvas, panel);
		gravity = panel->reference_gravity;
	}

	state->first_link = ctx->links.size;
	state->num_links = 0;
	DARRAY_APPEND (&ctx->windows, state);

	if (!state->draw_order) {
		state->draw_order = imui_next_window (ctx);
	}

	auto semantic = panel->auto_fit ? imui_size_fitchildren : imui_size_pixels;

	ctx->current_parent = panel_view;
	*View_Control (panel_view) = (viewcont_t) {
		.gravity = gravity,
		.visible = 1,
		.semantic_x = semantic,
		.semantic_y = semantic,
		.free_x = 1,
		.free_y = 1,
		.vertical = true,
		.active = 1,
	};

	auto bg = ctx->style.background.normal;
	UI_Vertical {
		ctx->style.background.normal = 0;
		if (!panel->auto_fit) {
			IMUI_Layout_SetYSize (ctx, imui_size_expand, 100);
			UI_Horizontal {
				drag (tl,t, imui_size_pixels, 12,
							imui_size_pixels, 4, panel->name, panel);
				drag (tc, , imui_size_expand, 100,
							imui_size_pixels, 4, panel->name, panel);
				drag (tr,t, imui_size_pixels, 12,
							imui_size_pixels, 4, panel->name, panel);
			}
		} else {
			IMUI_Spacer (ctx, imui_size_expand, 100, imui_size_pixels, 2);
		}
		UI_Horizontal {
			if (!panel->auto_fit) {
				IMUI_Layout_SetYSize (ctx, imui_size_expand, 100);
				UI_Vertical {
					drag (tl,s, imui_size_pixels, 4,
								imui_size_pixels, 8, panel->name, panel);
					drag (cl, , imui_size_pixels, 4,
								imui_size_expand, 100, panel->name, panel);
					drag (bl,s, imui_size_pixels, 4,
								imui_size_pixels, 8, panel->name, panel);
				}
			} else {
				IMUI_Spacer (ctx, imui_size_pixels, 2, imui_size_expand, 100);
			}
			UI_Vertical {
				IMUI_Layout_SetXSize (ctx, imui_size_expand, 100);
				panel_view = ctx->current_parent;
			}
			if (!panel->auto_fit) {
				UI_Vertical {
					drag (tr,s, imui_size_pixels, 4,
								imui_size_pixels, 8, panel->name, panel);
					drag (cr, , imui_size_pixels, 4,
								imui_size_expand, 100, panel->name, panel);
					drag (br,s, imui_size_pixels, 4,
								imui_size_pixels, 8, panel->name, panel);
				}
			} else {
				IMUI_Spacer (ctx, imui_size_pixels, 2, imui_size_expand, 100);
			}
		}
		if (!panel->auto_fit) {
			UI_Horizontal {
				drag (bl,b, imui_size_pixels, 12,
							imui_size_pixels, 4, panel->name, panel);
				drag (bc, , imui_size_expand, 100,
							imui_size_pixels, 4, panel->name, panel);
				drag (br,b, imui_size_pixels, 12,
							imui_size_pixels, 4, panel->name, panel);
			}
		} else {
			IMUI_Spacer (ctx, imui_size_expand, 100, imui_size_pixels, 2);
		}
	}
	state->pos = (view_pos_t) {
		.x = panel->xpos,
		.y = panel->ypos,
	};
	state->len = (view_pos_t) {
		.x = panel->xlen,
		.y = panel->ylen,
	};
	state->auto_fit = panel->auto_fit;
	ctx->style.background.normal = bg;
	ctx->current_parent = panel_view;
	state->content = panel_view.id;
	return 0;
}

int
IMUI_ExtendPanel (imui_ctx_t *ctx, const char *panel_name)
{
	auto state = IMUI_FindState (ctx, panel_name);
	if (!state || !ECS_EntValid (state->entity, ctx->vsys.reg)) {
		return 1;
	}
	DARRAY_APPEND (&ctx->parent_stack, ctx->current_parent);
	ctx->current_parent = View_FromEntity (ctx->vsys, state->content);
	return 0;
}

void
IMUI_EndPanel (imui_ctx_t *ctx)
{
	IMUI_PopLayout (ctx);
}

int
IMUI_StartMenu (imui_ctx_t *ctx, imui_window_t *menu, bool vertical)
{
	menu->parent = ctx->current_menu ? ctx->current_menu->self : 0;
	ctx->current_menu = menu;

	if (menu->parent) {
		auto parent = IMUI_GetWindow (ctx, menu->parent);
		if (!menu->reference) {
			menu->reference = nva ("%s##item:%s", menu->name,
								   parent->name);
		}
		if (parent->is_open) {
			UI_ExtendPanel (parent->name) {
				if (IMUI_MenuItem (ctx, menu->reference, false)) {
					menu->is_open = true;
				}
			}
		}
	}
	if (!menu->is_open) {
		ctx->current_menu = IMUI_GetWindow (ctx, menu->parent);
		return 1;
	}

	IMUI_StartPanel (ctx, menu);

	auto state = ctx->windows.a[ctx->windows.size - 1];
	state->menu = menu->self;

	auto menu_view = ctx->current_parent;
	if (vertical) {
		IMUI_Layout_SetXSize (ctx, imui_size_fitchildren, 0);
	} else {
		UI_Horizontal {
			IMUI_Layout_SetYSize (ctx, imui_size_fitchildren, 0);
			menu_view = ctx->current_parent;
		}
	}
	ctx->current_parent = menu_view;
	state->content = menu_view.id;
	return 0;
}

void
IMUI_EndMenu (imui_ctx_t *ctx)
{
	IMUI_PopLayout (ctx);
}

bool
IMUI_MenuItem (imui_ctx_t *ctx, const char *label, bool collapse)
{
	auto res = IMUI_Button (ctx, label);
	//auto state = ctx->current_state;
	if (res && collapse) {
		for (auto m = ctx->current_menu; m && !m->no_collapse;
			 m = IMUI_GetWindow (ctx, m->parent)) {
			m->is_open = false;
		}
	}
	return res;
}

void
IMUI_TitleBar (imui_ctx_t *ctx, imui_window_t *window)
{
	auto state = ctx->windows.a[ctx->windows.size - 1];
	auto title_bar = View_New (ctx->vsys, ctx->current_parent);

	auto tb_state = imui_get_state (ctx, va ("%s##title_bar", window->name),
									title_bar.id);
	int tb_mode = update_hot_active (ctx, tb_state);

	auto delta = check_drag_delta (ctx, tb_state->entity);
	if (ctx->active == tb_state->entity) {
		state->draw_order = imui_ontop;
		window->xpos += delta.x;
		window->ypos += delta.y;
	}

	set_control (ctx, title_bar, true);
	set_expand_x (ctx, title_bar, 100);
	set_fill (ctx, title_bar, ctx->style.foreground.color[tb_mode]);

	auto title = add_text (ctx, title_bar, state, window->mode);
	View_Control (title)->gravity = grav_center;
}

void
IMUI_CollapseButton (imui_ctx_t *ctx, imui_window_t *window)
{
	char cbutton = window->is_collapsed ? '>' : 'v';
	if (UI_Button (va ("%c##collapse_%s", cbutton, window->name))) {
		window->is_collapsed = !window->is_collapsed;
	}
}

void
IMUI_CloseButton (imui_ctx_t *ctx, imui_window_t *window)
{
	if (UI_Button (va ("X##close_%s", window->name))) {
		window->is_open = false;
	}
}

int
IMUI_StartWindow (imui_ctx_t *ctx, imui_window_t *window)
{
	if (!window->is_open) {
		return 1;
	}
	IMUI_StartPanel (ctx, window);
	auto state = ctx->windows.a[ctx->windows.size - 1];

	UI_Horizontal {
		if (window->auto_fit) {
			IMUI_CollapseButton (ctx, window);
		}
		IMUI_TitleBar (ctx, window);
		IMUI_CloseButton (ctx, window);
	}
	state->pos = (view_pos_t) {
		.x = window->xpos,
		.y = window->ypos,
	};
	return 0;
}

void
IMUI_EndWindow (imui_ctx_t *ctx)
{
	IMUI_PopLayout (ctx);
}

int
IMUI_StartScrollBox (imui_ctx_t *ctx, const char *name)
{
	auto anchor_view = View_New (ctx->vsys, ctx->current_parent);
	*View_Control (anchor_view) = (viewcont_t) {
		.gravity = grav_northwest,
		.visible = 1,
		.semantic_x = imui_size_expand,
		.semantic_y = imui_size_expand,
		.active = 0,
	};
	Ent_SetComponent (anchor_view.id, c_fraction_x, ctx->csys.reg,
					  &(imui_frac_t) { 100, 100 });
	Ent_SetComponent (anchor_view.id, c_fraction_y, ctx->csys.reg,
					  &(imui_frac_t) { 100, 100 });

	auto panel = ctx->windows.a[ctx->windows.size - 1];

	auto canvas = Canvas_New (ctx->csys);
	*Canvas_DrawGroup (ctx->csys, canvas) = panel->draw_group;
	auto scroll_box = Canvas_GetRootView (ctx->csys, canvas);

	auto state = imui_get_state (ctx, name, scroll_box.id);
	update_hot_active (ctx, state);

	DARRAY_APPEND (&ctx->links, state);
	panel->num_links++;

	DARRAY_APPEND (&ctx->scrollers, state);

	DARRAY_APPEND (&ctx->parent_stack, ctx->current_parent);

	set_hierarchy_tree_mode (ctx, View_GetRef (scroll_box), true);

	View_Control (anchor_view)->is_link = 1;
	imui_reference_t link = {
		.ref_id = scroll_box.id,
		.ctx = ctx,
	};
	Ent_SetComponent (anchor_view.id, c_reference, anchor_view.reg, &link);
	imui_reference_t anchor = {
		.ref_id = anchor_view.id,
	};
	Ent_SetComponent (scroll_box.id, c_reference, scroll_box.reg, &anchor);

	if (!state->draw_order) {
		state->draw_order = ++ctx->draw_order;
	}

	ctx->current_parent = scroll_box;
	*View_Control (scroll_box) = (viewcont_t) {
		.gravity = grav_northwest,
		.visible = 1,
		.semantic_x = imui_size_pixels,
		.semantic_y = imui_size_pixels,
		.free_x = 1,
		.free_y = 1,
		.vertical = true,
		.active = 1,
	};
	state->content = scroll_box.id;
	return 0;
}

void
IMUI_EndScrollBox (imui_ctx_t *ctx)
{
	IMUI_PopLayout (ctx);
}

void
IMUI_ScrollBar (imui_ctx_t *ctx, const char *name)
{
	auto pcont = View_Control (ctx->current_parent);
	// if the current layout is vertical, then the scroll bar will be placed
	// under the previous item (or above the next) and thus should be
	// horizontal, but if the layout is horizontal, then the scroll bar will
	// be paced to the side of the next/previous item and thus should be
	// vertical
	bool vertical = !pcont->vertical;

	IMUI_PushLayout (ctx, vertical);
	auto sb_view = ctx->current_parent;
	if (vertical) {
		IMUI_Layout_SetXSize (ctx, imui_size_pixels, 12);
	} else {
		IMUI_Layout_SetYSize (ctx, imui_size_pixels, 12);
	}
	View_Control (sb_view)->active = true;

	auto tt_view = View_New (ctx->vsys, sb_view);
	*View_Control (tt_view) = (viewcont_t) {
		.gravity = grav_northwest,
		.visible = 1,
		.semantic_x = vertical ? imui_size_expand : imui_size_fraction,
		.semantic_y = vertical ? imui_size_fraction : imui_size_expand,
		.free_x = !vertical,
		.free_y = vertical,
		.active = true,
	};
	IMUI_PopLayout (ctx);
	auto sb_state = imui_get_state (ctx, va ("scrollbar#%s", name),
									sb_view.id);
	auto tt_state = imui_get_state (ctx, va ("thumbtab#%s", name),
									tt_view.id);
	int sb_mode = update_hot_active (ctx, sb_state);
	int tt_mode = update_hot_active (ctx, tt_state);
	set_fill (ctx, sb_view, ctx->style.background.color[sb_mode]);
	set_fill (ctx, tt_view, ctx->style.foreground.color[tt_mode]);
	auto scroller = IMUI_FindState (ctx, name);
	if (scroller) {
		auto slen = scroller->len;
		tt_state->fraction.num = vertical ? slen.y : slen.x;
		auto content = IMUI_FindState (ctx, va ("%s#content", name));
		if (content) {
			auto delta = check_drag_delta (ctx, tt_state->entity);
			auto clen = content->len;
			if (vertical) {
				content->pos.y += delta.y;
				int max = clen.y - slen.y;
				content->pos.y = bound (0, content->pos.y, max);
			} else {
				content->pos.x += delta.x;
				int max = clen.x - slen.x;
				content->pos.x = bound (0, content->pos.x, max);
			}
			auto cpos = content->pos;
			tt_state->fraction.den = vertical ? clen.y : clen.x;
			tt_state->pos = (view_pos_t) {0, 0};
			if (vertical) {
				if (clen.y) {
					tt_state->pos.y = cpos.y * slen.y / clen.y;
				}
			} else {
				if (clen.x) {
					tt_state->pos.x = cpos.x * slen.x / clen.x;
				}
			}
		}
	}
	View_SetPos (tt_view, tt_state->pos.x, tt_state->pos.y);
	imui_frac_t efrac = { 100, 100 };
	imui_frac_t tfrac = tt_state->fraction;
	Ent_SetComponent (tt_view.id, c_fraction_x, ctx->csys.reg,
					  vertical ? &efrac : &tfrac);
	Ent_SetComponent (tt_view.id, c_fraction_y, ctx->csys.reg,
					  vertical ? &tfrac : &efrac);
}

int
IMUI_StartScroller (imui_ctx_t *ctx)
{
	auto anchor_view = View_New (ctx->vsys, ctx->current_parent);
	*View_Control (anchor_view) = (viewcont_t) {
		.gravity = grav_northwest,
		.visible = 1,
		.semantic_x = imui_size_expand,
		.semantic_y = imui_size_fitchildren,
		.free_y = 1,
		.vertical = true,
		.active = 1,
	};
	auto reg = ctx->csys.reg;
	Ent_SetComponent (anchor_view.id, c_fraction_x, reg,
					  &(imui_frac_t) { 100, 100 });

	uint32_t    parent = ctx->current_parent.id;
	const char *name = nullptr;
	if (Ent_HasComponent (parent, ecs_name, ctx->csys.reg)) {
		name = *(char **) Ent_GetComponent (parent, ecs_name, ctx->csys.reg);
	}
	auto state = imui_get_state (ctx, va ("%s#content", name), anchor_view.id);
	DARRAY_APPEND (&ctx->scrollers, state);
	update_hot_active (ctx, state);

	View_SetPos (anchor_view, -state->pos.x, -state->pos.y);

	auto scroller = View_New (ctx->vsys, nullview);
	Canvas_SetReference (ctx->csys, scroller.id,
						 Canvas_Entity (ctx->csys,
										View_GetRoot (anchor_view).id));
	*View_Control (scroller) = (viewcont_t) {
		.gravity = grav_northwest,
		.visible = 1,
		.free_x = 1,
		.free_y = 1,
		.vertical = true,
		.active = 1,
	};

	View_Control (anchor_view)->is_link = 1;
	imui_reference_t link = {
		.ref_id = scroller.id,
		.update = true,
		.ctx = ctx,
	};
	Ent_SetComponent (anchor_view.id, c_reference, anchor_view.reg, &link);

	imui_reference_t anchor = {
		.ref_id = anchor_view.id,
	};
	Ent_SetComponent (scroller.id, c_reference, scroller.reg, &anchor);

	DARRAY_APPEND (&ctx->parent_stack, ctx->current_parent);
	ctx->current_parent = scroller;
	return 0;
}

void
IMUI_EndScroller (imui_ctx_t *ctx)
{
	IMUI_PopLayout (ctx);
}

void
IMUI_SetViewPos (imui_ctx_t *ctx, view_pos_t pos)
{
	uint32_t id = ctx->current_state->entity;
	auto view = View_FromEntity (ctx->vsys, id);
	View_SetPos (view, pos.x, pos.y);
}

void
IMUI_SetViewLen (imui_ctx_t *ctx, view_pos_t len)
{
	uint32_t id = ctx->current_state->entity;
	auto view = View_FromEntity (ctx->vsys, id);
	View_SetLen (view, len.x, len.y);
}
