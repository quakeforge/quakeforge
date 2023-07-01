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

#include "QF/ecs.h"
#include "QF/hash.h"
#include "QF/progs.h"
#include "QF/quakeio.h"

#include "QF/input/event.h"

#include "QF/ui/canvas.h"
#include "QF/ui/font.h"
#include "QF/ui/imui.h"
#include "QF/ui/text.h"

typedef struct imui_state_s {
	struct imui_state_s *next;
	struct imui_state_s **prev;
	char       *label;
	uint32_t    label_len;
	int         key_offset;
	uint32_t    entity;
} imui_state_t;

struct imui_ctx_s {
	canvas_system_t csys;
	uint32_t    canvas;
	ecs_system_t vsys;
	text_system_t tsys;
	view_t      root_view;
	hashctx_t  *hashctx;
	hashtab_t  *tab;
	PR_RESMAP (imui_state_t) state_map;
	imui_state_t *states;
	font_t     *font;

	int64_t     frame_start;
	int64_t     frame_draw;
	int64_t     frame_end;
	uint32_t    framecount;
	uint32_t    new_hot;
	uint32_t    hot;
	uint32_t    active;
	bool        mouse_pressed;
	bool        mouse_released;
	unsigned    mouse_buttons;
	view_pos_t  mouse_position;
	unsigned    shift;
	int         key_code;
	int         unicode;
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
/*
static void
imui_state_free (imui_ctx_t *ctx, imui_state_t *state)
{
	if (state->next) {
		state->next->prev = state->prev;
	}
	*state->prev = state->next;
	PR_RESFREE (ctx->state_map, state);
}
*/
static imui_state_t *
imui_get_state (imui_ctx_t *ctx, const char *label)
{
	int         key_offset = 0;
	uint32_t    label_len = ~0u;
	const char *key = strstr (label, "##");
	if (key) {
		// key is '###': hash only past this
		if (key[2] == '#') {
			key_offset = (key += 3) - label;
		}
		label_len = key - label;
	} else {
		key = label;
	}
	imui_state_t *state = Hash_Find (ctx->tab, key);
	if (state) {
		return state;
	}
	state = imui_state_new (ctx);
	state->label = strdup (label);
	state->label_len = label_len == ~0u ? strlen (label) : label_len;
	state->key_offset = key_offset;
	Hash_Add (ctx->tab, state);
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
	uint32_t canvas;
	*ctx = (imui_ctx_t) {
		.csys = canvas_sys,
		.canvas = canvas = Canvas_New (canvas_sys),
		.vsys = { canvas_sys.reg, canvas_sys.view_base },
		.tsys = { canvas_sys.reg, canvas_sys.view_base, canvas_sys.text_base },
		.root_view = Canvas_GetRootView (canvas_sys, canvas),
		.tab = Hash_NewTable (511, imui_state_getkey, 0, ctx, &ctx->hashctx),
		.new_hot = nullent,
		.hot = nullent,
		.active = nullent,
		.mouse_position = {-1, -1},
	};

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

	Hash_DelTable (ctx->tab);
	Hash_DelContext (ctx->hashctx);
	free (ctx);
}

void
IMUI_SetVisible (imui_ctx_t *ctx, bool visible)
{
	*Canvas_Visible (ctx->csys, ctx->canvas) = visible;
}

void
IMUI_SetSize (imui_ctx_t *ctx, int xlen, int ylen)
{
	View_SetLen (ctx->root_view, xlen, ylen);
	View_UpdateHierarchy (ctx->root_view);
}

void
IMUI_ProcessEvent (imui_ctx_t *ctx, const IE_event_t *ie_event)
{
	if (ie_event->type == ie_mouse) {
		auto m = &ie_event->mouse;
		ctx->mouse_position = (view_pos_t) { m->x, m->y };

		unsigned old = ctx->mouse_buttons & 1;
		unsigned new = m->buttons & 1;
		ctx->mouse_pressed = (old ^ new) & new;
		ctx->mouse_released = (old ^ new) & !new;
		ctx->mouse_buttons = m->buttons;
	} else {
		auto k = &ie_event->key;
		//printf ("imui: %d %d %x\n", k->code, k->unicode, k->shift);
		ctx->shift = k->shift;
		ctx->key_code = k->code;
		ctx->unicode = k->unicode;
	}
}

void
IMUI_BeginFrame (imui_ctx_t *ctx)
{
	ctx->frame_start = Sys_LongTime ();
	ctx->framecount++;
	ctx->hot = ctx->new_hot;
	ctx->new_hot = nullent;
}

void
IMUI_Draw (imui_ctx_t *ctx)
{
	ctx->frame_draw = Sys_LongTime ();

	View_UpdateHierarchy (ctx->root_view);
	ctx->frame_end = Sys_LongTime ();
}

static bool
check_button_state (imui_ctx_t *ctx, uint32_t entity)
{
	bool result = false;
	if (ctx->active == entity) {
		if (ctx->mouse_released) {
			result = ctx->hot == entity;
			ctx->active = nullent;
		}
	} else if (ctx->hot == entity) {
		if (ctx->mouse_pressed) {
			ctx->active = entity;
		}
	}
	return result;
}

static void
check_inside (imui_ctx_t *ctx, view_pos_t pos, view_pos_t len, uint32_t entity)
{
	auto mp = ctx->mouse_position;
	if (mp.x >= pos.x && mp.y >= pos.y
		&& mp.x < pos.x + len.x && mp.y < pos.y + len.y) {
		if (ctx->active == entity || ctx->active == nullent) {
			ctx->new_hot = entity;
		}
	}
}

static view_t
add_text (view_t view, imui_state_t *state, imui_ctx_t *ctx)
{
	uint32_t c_glyphs = ctx->csys.base + canvas_glyphs;
	uint32_t c_passage_glyphs = ctx->csys.text_base + text_passage_glyphs;
	auto     reg = ctx->csys.reg;

	auto text = Text_StringView (ctx->tsys, view, ctx->font,
								 state->label, state->label_len, 0, 0);

	int ascender = ctx->font->face->size->metrics.ascender / 64;
	int descender = ctx->font->face->size->metrics.descender / 64;
	auto len = View_GetLen (text);
	View_SetLen (text, len.x, ascender - descender);

	View_SetVisible (text, 1);
	Ent_SetComponent (text.id, c_glyphs, reg,
					  Ent_GetComponent (text.id, c_passage_glyphs, reg));
	return text;
}

bool
IMUI_Button (imui_ctx_t *ctx, const char *label)
{
	auto state = imui_get_state (ctx, label);
	if (state->entity == nullent) {
		auto view = View_New (ctx->vsys, ctx->root_view);
		state->entity = view.id;

		*(byte*) Ent_AddComponent (view.id, ctx->csys.base + canvas_fill,
								   ctx->csys.reg) = 0;

		View_SetVisible (view, 1);
		View_SetGravity (view, grav_northwest);
		View_SetResize (view, 0, 0);

		auto text = add_text (view, state, ctx);
		auto len = View_GetLen (text);
		View_SetLen (view, len.x, len.y);
	}
	auto view = View_FromEntity (ctx->vsys, state->entity);
	auto len = View_GetLen (view);
	auto pos = View_GetAbs (view);
	bool result = check_button_state (ctx, state->entity);
	check_inside (ctx, pos, len, state->entity);
	return result;
}

bool
IMUI_Checkbox (imui_ctx_t *ctx, bool *flag, const char *label)
{
	auto state = imui_get_state (ctx, label);
	if (state->entity == nullent) {
	}
	return *flag;
}

void
IMUI_Radio (imui_ctx_t *ctx, int *state, int value, const char *label)
{
}

void
IMUI_Slider (imui_ctx_t *ctx, float *value, float minval, float maxval,
			 const char *label)
{
}
