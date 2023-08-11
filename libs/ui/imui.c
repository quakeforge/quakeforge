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
#include "QF/va.h"

#include "QF/input/event.h"

#include "QF/ui/canvas.h"
#include "QF/ui/font.h"
#include "QF/ui/imui.h"
#include "QF/ui/shaper.h"
#include "QF/ui/text.h"

#define IMUI_context ctx

#define c_percent_x (ctx->csys.imui_base + imui_percent_x)
#define c_percent_y (ctx->csys.imui_base + imui_percent_y)
#define c_reference (ctx->csys.imui_base + imui_reference)
#define c_glyphs (ctx->csys.base + canvas_glyphs)
#define c_passage_glyphs (ctx->csys.text_base + text_passage_glyphs)
#define c_color (ctx->tsys.text_base + text_color)
#define c_fill  (ctx->csys.base + canvas_fill)

#define imui_draw_group ((1 << 14) - 1)
#define imui_ontop ((1 << 15) - 1)
#define imui_onbottom (-(1 << 15) + 1)

const component_t imui_components[imui_comp_count] = {
	[imui_percent_x] = {
		.size = sizeof (int),
		.name = "percent x",
	},
	[imui_percent_y] = {
		.size = sizeof (int),
		.name = "percent y",
	},
	[imui_reference] = {
		.size = sizeof (imui_reference_t),
		.name = "reference",
	},
};

typedef struct imui_state_s {
	struct imui_state_s *next;
	struct imui_state_s **prev;
	char       *label;
	uint32_t    label_len;
	int         key_offset;
	imui_window_t *menu;
	int16_t		draw_order;	// for window canvases
	uint32_t    frame_count;
	uint32_t    entity;
	uint32_t    content;
} imui_state_t;

struct imui_ctx_s {
	canvas_system_t csys;
	uint32_t    canvas;
	ecs_system_t vsys;
	text_system_t tsys;

	text_shaper_t *shaper;

	hashctx_t  *hashctx;
	hashtab_t  *tab;
	PR_RESMAP (imui_state_t) state_map;
	imui_state_t *states;
	font_t     *font;

	int64_t     frame_start;
	int64_t     frame_draw;
	int64_t     frame_end;
	uint32_t    frame_count;

	view_t      root_view;
	view_t      current_parent;
	struct DARRAY_TYPE(view_t) parent_stack;
	struct DARRAY_TYPE(imui_state_t *) windows;
	int16_t     draw_order;
	imui_window_t *current_menu;
	imui_state_t *current_state;

	dstring_t  *dstr;

	uint32_t    hot;
	uint32_t    active;
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

static imui_state_t *
imui_state_new (imui_ctx_t *ctx)
{
	imui_state_t *state = PR_RESNEW (ctx->state_map);
	*state = (imui_state_t) {
		.next = ctx->states,
		.prev = &ctx->states,
		.entity = nullent,
	};
	if (ctx->states) {
		ctx->states->prev = &state->next;
	}
	ctx->states = state;
	return state;
}

static void
imui_state_free (imui_ctx_t *ctx, imui_state_t *state)
{
	if (state->next) {
		state->next->prev = state->prev;
	}
	*state->prev = state->next;
	PR_RESFREE (ctx->state_map, state);
}

static imui_state_t *
imui_find_state (imui_ctx_t *ctx, const char *label)
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
imui_get_state (imui_ctx_t *ctx, const char *label)
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
	auto state = imui_find_state (ctx, label);
	if (state) {
		state->frame_count = ctx->frame_count;
		ctx->current_state = state;
		return state;
	}
	state = imui_state_new (ctx);
	state->label = strdup (label);
	state->label_len = label_len == ~0u ? strlen (label) : label_len;
	state->key_offset = key_offset;
	state->frame_count = ctx->frame_count;
	Hash_Add (ctx->tab, state);
	ctx->current_state = state;
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

void
IMUI_DestroyContext (imui_ctx_t *ctx)
{
	for (auto s = ctx->states; s; s = s->next) {
		free (s->label);
	}
	PR_RESDELMAP (ctx->state_map);

	if (ctx->font) {
		Font_Free (ctx->font);
	}

	DARRAY_CLEAR (&ctx->parent_stack);
	DARRAY_CLEAR (&ctx->windows);
	DARRAY_CLEAR (&ctx->style_stack);
	dstring_delete (ctx->dstr);

	Hash_DelTable (ctx->tab);
	Hash_DelContext (ctx->hashctx);
	Shaper_Delete (ctx->shaper);
	free (ctx);
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

		unsigned old = ctx->mouse_buttons;
		unsigned new = m->buttons;
		ctx->mouse_pressed = (old ^ new) & new;
		ctx->mouse_released = (old ^ new) & ~new;
		ctx->mouse_buttons = m->buttons;
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

void
IMUI_BeginFrame (imui_ctx_t *ctx)
{
	Shaper_FlushUnused (ctx->shaper);
	uint32_t    root_ent = ctx->root_view.id;
	auto root_size = View_GetLen (ctx->root_view);
	Ent_RemoveComponent (root_ent, ctx->root_view.comp, ctx->root_view.reg);
	ctx->root_view = View_AddToEntity (root_ent, ctx->vsys, nullview);
	auto ref = View_GetRef (ctx->root_view);
	Hierarchy_SetTreeMode (ref->hierarchy, true);
	View_SetLen (ctx->root_view, root_size.x, root_size.y);
	ctx->frame_start = Sys_LongTime ();
	ctx->frame_count++;
	ctx->current_parent = ctx->root_view;
	for (uint32_t i = 0; i < ctx->windows.size; i++) {
		auto window = View_FromEntity (ctx->vsys, ctx->windows.a[i]->entity);
		View_Delete (window);
	}
	ctx->draw_order = ctx->windows.size;
	DARRAY_RESIZE (&ctx->parent_stack, 0);
	DARRAY_RESIZE (&ctx->windows, 0);
	DARRAY_RESIZE (&ctx->style_stack, 0);
	ctx->current_menu = 0;
}

static void
prune_objects (imui_ctx_t *ctx)
{
	for (auto s = &ctx->states; *s; ) {
		if ((*s)->frame_count == ctx->frame_count) {
			s = &(*s)->next;
		} else {
			Hash_Del (ctx->tab, (*s)->label + (*s)->key_offset);
			free ((*s)->label);
			imui_state_free (ctx, *s);
		}
	}
}

#define DFL "\e[39;49m"
#define BLK "\e[30;40m"
#define RED "\e[31;40m"
#define GRN "\e[32;40m"
#define ONG "\e[33;40m"
#define BLU "\e[34;40m"
#define MAG "\e[35;40m"
#define CYN "\e[36;40m"
#define WHT "\e[37;40m"

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
		case imui_size_percent: return ONG;
		case imui_size_fitchildren: return MAG;
		case imui_size_expand: return RED;
	}
	return BLU;
}

static void __attribute__((used))
dump_tree (hierarchy_t *h, uint32_t ind, int level, imui_ctx_t *ctx)
{
	view_pos_t *len = h->components[view_len];
	auto c = ((viewcont_t *)h->components[view_control])[ind];
	uint32_t e = h->ent[ind];
	printf ("%2d: %*s[%s%d %s%d"DFL"] %c %s%d %s%d"DFL, ind,
			level * 3, "",
			view_color (h, ind, ctx, false), len[ind].x,
			view_color (h, ind, ctx, true),  len[ind].y,
			c.vertical ? 'v' : 'h',
			view_color (h, ind, ctx, false), c.semantic_x,
			view_color (h, ind, ctx, true),  c.semantic_y);
	for (uint32_t j = 0; j < h->reg->components.size; j++) {
		if (Ent_HasComponent (e, j, h->reg)) {
			printf (", %s", h->reg->components.a[j].name);
			if (j == c_percent_x || j == c_percent_y) {
				auto val = *(int *) Ent_GetComponent (e, j, h->reg);
				printf ("(%s%d"DFL")", view_color (h, ind, ctx,
												   j == c_percent_y), val);
			}
		}
	}
	printf (DFL"\n");

	if (h->childIndex[ind] > ind) {
		for (uint32_t i = 0; i < h->childCount[ind]; i++) {
			if (h->childIndex[ind] + i >= h->num_objects) {
				break;
			}
			dump_tree (h, h->childIndex[ind] + i, level + 1, ctx);
		}
	}
	if (!level) {
		puts ("");
	}
}

typedef struct {
	bool x, y;
} boolpair_t;

static void
calc_upwards_dependent (imui_ctx_t *ctx, hierarchy_t *h,
						boolpair_t *down_depend)
{
	auto reg = ctx->csys.reg;
	uint32_t   *ent = h->ent;
	view_pos_t *len = h->components[view_len];
	viewcont_t *cont = h->components[view_control];
	uint32_t   *parent = h->parentIndex;
	for (uint32_t i = 0; i < h->num_objects; i++) {
		if (down_depend
			&& (cont[i].semantic_x == imui_size_fitchildren
				|| cont[i].semantic_x == imui_size_expand)) {
			down_depend[i].x = true;
		} else if ((!down_depend
					|| !(i > 0
						 && (down_depend[i].x = down_depend[parent[i]].x)))
				   && cont[i].semantic_x == imui_size_percent) {
			int *percent = Ent_GetComponent (ent[i], c_percent_x, reg);
			int x = (len[parent[i]].x * *percent) / 100;
			len[i].x = x;
		}
		if (down_depend
			&& (cont[i].semantic_y == imui_size_fitchildren
				|| cont[i].semantic_y == imui_size_expand)) {
			down_depend[i].y = true;
		} else if ((!down_depend
					|| !(i > 0
						 && (down_depend[i].y = down_depend[parent[i]].y)))
				   && cont[i].semantic_y == imui_size_percent) {
			int *percent = Ent_GetComponent (ent[i], c_percent_y, reg);
			int y = (len[parent[i]].y * *percent) / 100;
			len[i].y = y;
		}
	}
}

static void
calc_downwards_dependent (hierarchy_t *h)
{
	view_pos_t *len = h->components[view_len];
	viewcont_t *cont = h->components[view_control];
	for (uint32_t i = h->num_objects; i-- > 0; ) {
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
calc_expansions (imui_ctx_t *ctx, hierarchy_t *h)
{
	auto reg = ctx->csys.reg;

	uint32_t   *ent = h->ent;
	view_pos_t *len = h->components[view_len];
	viewcont_t *cont = h->components[view_control];

	for (uint32_t i = 0; i < h->num_objects; i++) {
		view_pos_t  tlen = {};
		view_pos_t  elen = {};
		view_pos_t  ecount = {};
		for (uint32_t j = 0; j < h->childCount[i]; j++) {
			uint32_t child = h->childIndex[i] + j;
			tlen.x += len[child].x;
			tlen.y += len[child].y;
			if (cont[child].semantic_x == imui_size_expand) {
				int *p = Ent_GetComponent (ent[child], c_percent_x, reg);
				elen.x += *p;
				ecount.x++;
			}
			if (cont[child].semantic_y == imui_size_expand) {
				int *p = Ent_GetComponent (ent[child], c_percent_y, reg);
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
					int *p = Ent_GetComponent (ent[child], c_percent_y, reg);
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
					int *p = Ent_GetComponent (ent[child], c_percent_x, reg);
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
}

//FIXME currently works properly only for grav_northwest
static void
layout_objects (imui_ctx_t *ctx, view_t root_view)
{
	auto ref = View_GetRef (root_view);
	auto h = ref->hierarchy;

	view_pos_t *pos = h->components[view_pos];
	view_pos_t *len = h->components[view_len];
	viewcont_t *cont = h->components[view_control];
	uint32_t   *parent = h->parentIndex;
	boolpair_t  down_depend[h->num_objects];

	// the root view size is always explicit
	down_depend[0] = (boolpair_t) { false, false };
	calc_upwards_dependent (ctx, h, down_depend);
	calc_downwards_dependent (h);
	calc_expansions (ctx, h);
	//dump_tree (h, 0, 0, ctx);
	// resolve conflicts
	//fflush (stdout);

	if (Ent_HasComponent (root_view.id, c_reference, ctx->vsys.reg)) {
		auto ent = root_view.id;
		auto reg = ctx->vsys.reg;
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
}

static void
check_inside (imui_ctx_t *ctx, view_t root_view)
{
	auto ref = View_GetRef (root_view);
	auto h = ref->hierarchy;

	uint32_t   *entity = h->ent;
	view_pos_t *abs = h->components[view_abs];
	view_pos_t *len = h->components[view_len];
	viewcont_t *cont = h->components[view_control];
	auto mp = ctx->mouse_position;

	for (uint32_t i = 0; i < h->num_objects; i++) {
		if (cont[i].active
			&& mp.x >= abs[i].x && mp.y >= abs[i].y
			&& mp.x < abs[i].x + len[i].x && mp.y < abs[i].y + len[i].y) {
			if (ctx->active == entity[i] || ctx->active == nullent) {
				ctx->hot = entity[i];
			}
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
		window->draw_order = i + 1;
		*Canvas_DrawOrder (ctx->csys, window->entity) = window->draw_order;
	}
}

void
IMUI_Draw (imui_ctx_t *ctx)
{
	ctx->frame_draw = Sys_LongTime ();
	ctx->mouse_pressed = 0;
	ctx->mouse_released = 0;
	sort_windows (ctx);
	auto ref = View_GetRef (ctx->root_view);
	Hierarchy_SetTreeMode (ref->hierarchy, false);
	for (uint32_t i = 0; i < ctx->windows.size; i++) {
		auto window = View_FromEntity (ctx->vsys, ctx->windows.a[i]->entity);
		auto ref = View_GetRef (window);
		Hierarchy_SetTreeMode (ref->hierarchy, false);
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
	View_SetLen (view, 0, 0);
	if (x_size == imui_size_expand) {
		*(int*) Ent_AddComponent (view.id, c_percent_x, ctx->csys.reg) = 100;
	}
	if (y_size == imui_size_expand) {
		*(int*) Ent_AddComponent (view.id, c_percent_y, ctx->csys.reg) = 100;
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
	if (size == imui_size_percent || size == imui_size_expand) {
		*(int *) Ent_AddComponent(id, c_percent_x, ctx->csys.reg) = value;
	}
}

void
IMUI_Layout_SetYSize (imui_ctx_t *ctx, imui_size_t size, int value)
{
	auto pcont = View_Control (ctx->current_parent);
	uint32_t id = ctx->current_parent.id;
	pcont->semantic_y = size;
	if (size == imui_size_percent || size == imui_size_expand) {
		*(int *) Ent_AddComponent(id, c_percent_y, ctx->csys.reg) = value;
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
		delta.x = ctx->mouse_position.x - ctx->mouse_active.x;
		delta.y = ctx->mouse_position.y - ctx->mouse_active.y;
		ctx->mouse_active = ctx->mouse_position;
		if (ctx->mouse_released & 1) {
			ctx->active = nullent;
		}
	} else if (ctx->hot == entity) {
		if (ctx->mouse_pressed & 1) {
			ctx->mouse_active = ctx->mouse_position;
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
					  Ent_GetComponent (text.id, c_passage_glyphs, reg));

	len = View_GetLen (text);
	View_SetLen (view, len.x, len.y);

	uint32_t color = ctx->style.text.color[mode];
	*(uint32_t *) Ent_AddComponent (text.id, c_color, ctx->tsys.reg) = color;

	return text;
}

static int
update_hot_active (imui_ctx_t *ctx, uint32_t old_entity, uint32_t new_entity)
{
	int mode = 0;
	if (old_entity != nullent) {
		if (ctx->hot == old_entity) {
			ctx->hot = new_entity;
			mode = 1;
		}
		if (ctx->active == old_entity) {
			ctx->active = new_entity;
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
	*(int *) Ent_AddComponent(view.id, c_percent_x, ctx->csys.reg) = weight;
}

void
IMUI_Label (imui_ctx_t *ctx, const char *label)
{
	auto state = imui_get_state (ctx, label);

	auto view = View_New (ctx->vsys, ctx->current_parent);
	state->entity = view.id;

	set_control (ctx, view, true);
	set_fill (ctx, view, ctx->style.background.normal);
	add_text (ctx, view, state, 0);
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

bool
IMUI_Button (imui_ctx_t *ctx, const char *label)
{
	auto state = imui_get_state (ctx, label);
	uint32_t old_entity = state->entity;

	auto view = View_New (ctx->vsys, ctx->current_parent);
	state->entity = view.id;
	int mode = update_hot_active (ctx, old_entity, state->entity);

	set_control (ctx, view, true);
	set_fill (ctx, view, ctx->style.foreground.color[mode]);
	add_text (ctx, view, state, mode);

	return check_button_state (ctx, state->entity);
}

bool
IMUI_Checkbox (imui_ctx_t *ctx, bool *flag, const char *label)
{
	auto state = imui_get_state (ctx, label);
	uint32_t old_entity = state->entity;

	auto view = View_New (ctx->vsys, ctx->current_parent);
	state->entity = view.id;
	int mode = update_hot_active (ctx, old_entity, state->entity);

	set_control (ctx, view, true);
	View_Control (view)->semantic_x = imui_size_fitchildren;
	View_Control (view)->semantic_y = imui_size_fitchildren;

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
	auto state = imui_get_state (ctx, label);
	uint32_t old_entity = state->entity;

	auto view = View_New (ctx->vsys, ctx->current_parent);
	state->entity = view.id;
	int mode = update_hot_active (ctx, old_entity, state->entity);

	set_control (ctx, view, true);
	View_Control (view)->semantic_x = imui_size_fitchildren;
	View_Control (view)->semantic_y = imui_size_fitchildren;

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

void
IMUI_Spacer (imui_ctx_t *ctx,
			 imui_size_t xsize, int xvalue,
			 imui_size_t ysize, int yvalue)
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
	set_control (ctx, view, false);
	View_SetLen (view, xlen, ylen);
	View_Control (view)->semantic_x = xsize;
	View_Control (view)->semantic_y = ysize;
	auto reg = ctx->csys.reg;
	if (xsize == imui_size_percent || xsize == imui_size_expand) {
		*(int*) Ent_AddComponent (view.id, c_percent_x, reg) = xvalue;
	}
	if (ysize == imui_size_percent || ysize == imui_size_expand) {
		*(int*) Ent_AddComponent (view.id, c_percent_y, reg) = yvalue;
	}

	set_fill (ctx, view, ctx->style.background.normal);
}

static void
create_reference_anchor (imui_ctx_t *ctx, uint32_t ent, imui_window_t *panel)
{
	auto state = imui_find_state (ctx, panel->reference);
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

int
IMUI_StartPanel (imui_ctx_t *ctx, imui_window_t *panel)
{
	if (!panel->is_open) {
		return 1;
	}
	auto state = imui_get_state (ctx, panel->name);
	uint32_t old_entity = state->entity;

	DARRAY_APPEND (&ctx->parent_stack, ctx->current_parent);

	auto canvas = Canvas_New (ctx->csys);
	int draw_group = imui_draw_group + panel->group_offset;
	*Canvas_DrawGroup (ctx->csys, canvas) = draw_group;
	auto panel_view = Canvas_GetRootView (ctx->csys, canvas);
	state->entity = panel_view.id;
	panel->mode = update_hot_active (ctx, old_entity, state->entity);
	auto ref = View_GetRef (panel_view);
	Hierarchy_SetTreeMode (ref->hierarchy, true);

	grav_t      gravity = grav_northwest;
	if (panel->reference) {
		create_reference_anchor (ctx, canvas, panel);
		gravity = panel->reference_gravity;
	}

	DARRAY_APPEND (&ctx->windows, state);

	if (!state->draw_order) {
		state->draw_order = ++ctx->draw_order;
	}

	ctx->current_parent = panel_view;
	*View_Control (panel_view) = (viewcont_t) {
		.gravity = gravity,
		.visible = 1,
		.semantic_x = imui_size_fitchildren,
		.semantic_y = imui_size_fitchildren,
		.free_x = 1,
		.free_y = 1,
		.vertical = true,
		.active = 1,
	};
	View_SetPos (panel_view, panel->xpos, panel->ypos);
	View_SetLen (panel_view, panel->xlen, panel->ylen);

	auto bg = ctx->style.background.normal;
	UI_Vertical {
		ctx->style.background.normal = 0;//FIXME style
		IMUI_Spacer (ctx, imui_size_expand, 100, imui_size_pixels, 2);
		UI_Horizontal {
			IMUI_Spacer (ctx, imui_size_pixels, 2, imui_size_expand, 100);
			UI_Vertical {
				IMUI_Layout_SetXSize (ctx, imui_size_expand, 100);
				panel_view = ctx->current_parent;
			}
			IMUI_Spacer (ctx, imui_size_pixels, 2, imui_size_expand, 100);
		}
		IMUI_Spacer (ctx, imui_size_expand, 100, imui_size_pixels, 2);
	}
	ctx->style.background.normal = bg;
	ctx->current_parent = panel_view;
	state->content = panel_view.id;
	return 0;
}

int
IMUI_ExtendPanel (imui_ctx_t *ctx, const char *panel_name)
{
	auto state = imui_find_state (ctx, panel_name);
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
	menu->parent = ctx->current_menu;
	ctx->current_menu = menu;

	if (menu->parent) {
		if (!menu->reference) {
			menu->reference = nva ("%s##item:%s", menu->name,
								   menu->parent->name);
		}
		if (menu->parent->is_open) {
			UI_ExtendPanel (menu->parent->name) {
				if (IMUI_MenuItem (ctx, menu->reference, false)) {
					menu->is_open = true;
				}
			}
		}
	}
	if (!menu->is_open) {
		ctx->current_menu = menu->parent;
		return 1;
	}

	IMUI_StartPanel (ctx, menu);

	auto state = ctx->windows.a[ctx->windows.size - 1];
	state->menu = menu;

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
		for (auto m = ctx->current_menu; m && !m->no_collapse; m = m->parent) {
			m->is_open = false;
		}
	}
	return res;
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
		char cbutton = window->is_collapsed ? '>' : 'v';
		if (UI_Button (va (0, "%c##collapse_%s", cbutton, window->name))) {
			window->is_collapsed = !window->is_collapsed;
		}

		auto tb_state = imui_get_state (ctx, va (0, "%s##title_bar",
												 window->name));
		uint32_t tb_old_entity = tb_state->entity;
		auto title_bar = View_New (ctx->vsys, ctx->current_parent);
		tb_state->entity = title_bar.id;
		int tb_mode = update_hot_active (ctx, tb_old_entity, tb_state->entity);
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

		if (UI_Button (va (0, "X##close_%s", window->name))) {
			window->is_open = false;
		}
	}
	return 0;
}

void
IMUI_EndWindow (imui_ctx_t *ctx)
{
	IMUI_PopLayout (ctx);
}
