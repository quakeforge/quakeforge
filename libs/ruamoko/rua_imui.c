/*
	r_imui.c

	Ruamoko IMUI support functions

	Copyright (C) 2023       Bill Currie <bill@taniwha.org>

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
#include <stdlib.h>

#include "QF/dstring.h"
#include "QF/progs.h"
#include "QF/simd/types.h"

#include "QF/ui/canvas.h"
#include "QF/ui/imui.h"
#include "QF/ui/passage.h"

#include "rua_internal.h"

typedef struct bi_imui_ctx_s {
	struct bi_imui_ctx_s *next;
	struct bi_imui_ctx_s **prev;
	imui_ctx_t *imui_ctx;
} bi_imui_ctx_t;

typedef struct {
	progs_t    *pr;
	canvas_system_t canvas_sys;

	PR_RESMAP (bi_imui_ctx_t) imui_ctx_map;
	bi_imui_ctx_t *imui_ctxs;
	dstring_t  *dstr;
} imui_resources_t;

static bi_imui_ctx_t *
imui_ctx_new (imui_resources_t *res)
{
	qfZoneScoped (true);
	return PR_RESNEW (res->imui_ctx_map);
}

static void
imui_ctx_free (imui_resources_t *res, bi_imui_ctx_t *imui_ctx)
{
	qfZoneScoped (true);
	PR_RESFREE (res->imui_ctx_map, imui_ctx);
}

static void
imui_ctx_reset (imui_resources_t *res)
{
	qfZoneScoped (true);
	PR_RESRESET (res->imui_ctx_map);
}

static inline bi_imui_ctx_t *
imui_ctx_get (imui_resources_t *res, int index)
{
	qfZoneScoped (true);
	return PR_RESGET(res->imui_ctx_map, index);
}

static inline int __attribute__((pure))
imui_ctx_index (imui_resources_t *res, bi_imui_ctx_t *imui_ctx)
{
	qfZoneScoped (true);
	return PR_RESINDEX(res->imui_ctx_map, imui_ctx);
}

static void
bi_imui_clear (progs_t *pr, void *_res)
{
	qfZoneScoped (true);
	imui_resources_t *res = _res;

	for (auto bi_ctx = res->imui_ctxs; bi_ctx; bi_ctx = bi_ctx->next) {
		IMUI_DestroyContext (bi_ctx->imui_ctx);
	}
	res->imui_ctxs = 0;
	imui_ctx_reset (res);
}

static void
bi_imui_destroy (progs_t *pr, void *_res)
{
	qfZoneScoped (true);
	imui_resources_t *res = _res;
	PR_RESDELMAP (res->imui_ctx_map);
	dstring_delete (res->dstr);
	free (res);
}

#define bi(x) static void bi_##x (progs_t *pr, void *_res)

bi(IMUI_NewWindow)
{
	qfZoneScoped (true);
	const char *name = P_GSTRING (pr, 0);

	imui_window_t *window = PR_Zone_Malloc (pr, sizeof (imui_window_t));
	*window = (imui_window_t) {
		.name = name,
		.is_open = true,
		.auto_fit = true,
	};
	RETURN_POINTER (pr, window);
}

bi(IMUI_DeleteWindow)
{
	qfZoneScoped (true);
	auto window =  (imui_window_t *) P_GPOINTER (pr, 0);
	PR_Zone_Free (pr, window);
}

bi(IMUI_Window_SetPos)
{
	qfZoneScoped (true);
	auto window =  (imui_window_t *) P_GPOINTER (pr, 0);
	window->auto_fit = false;
	window->xpos = P_var (pr, 1, ivec2)[0];
	window->ypos = P_var (pr, 1, ivec2)[1];
}

bi(IMUI_Window_SetSize)
{
	qfZoneScoped (true);
	auto window =  (imui_window_t *) P_GPOINTER (pr, 0);
	window->auto_fit = false;
	window->xlen = P_var (pr, 1, ivec2)[0];
	window->ylen = P_var (pr, 1, ivec2)[1];
}

bi(IMUI_Window_SetOpen)
{
	qfZoneScoped (true);
	auto window =  (imui_window_t *) P_GPOINTER (pr, 0);
	R_INT (pr) = window->is_open;
}

bi(IMUI_Window_SetCollapsed)
{
	qfZoneScoped (true);
	auto window =  (imui_window_t *) P_GPOINTER (pr, 0);
	window->is_collapsed = P_INT (pr, 1);
}

bi(IMUI_Window_SetNoCollapse)
{
	qfZoneScoped (true);
	auto window =  (imui_window_t *) P_GPOINTER (pr, 0);
	window->no_collapse = P_INT (pr, 1);
}

bi(IMUI_Window_SetAutoFit)
{
	qfZoneScoped (true);
	auto window =  (imui_window_t *) P_GPOINTER (pr, 0);
	window->auto_fit = P_INT (pr, 1);
}

bi(IMUI_Window_SetReference)
{
	qfZoneScoped (true);
	auto window =  (imui_window_t *) P_GPOINTER (pr, 0);
	window->reference = P_GSTRING (pr, 1);
}

bi(IMUI_Window_SetReferenceGravity)
{
	qfZoneScoped (true);
	auto window =  (imui_window_t *) P_GPOINTER (pr, 0);
	window->reference_gravity = P_INT (pr, 1);
}

bi(IMUI_Window_SetAnchorGravity)
{
	qfZoneScoped (true);
	auto window =  (imui_window_t *) P_GPOINTER (pr, 0);
	window->anchor_gravity = P_INT (pr, 1);
}

bi(IMUI_Window_SetParent)
{
	qfZoneScoped (true);
	auto window =  (imui_window_t *) P_GPOINTER (pr, 0);
	window->parent = P_UINT (pr, 1);
}

bi(IMUI_Window_GetPos)
{
	qfZoneScoped (true);
	auto window =  (imui_window_t *) P_GPOINTER (pr, 0);
	R_var (pr, ivec2) = (pr_ivec2_t) { window->xpos, window->ypos };
}

bi(IMUI_Window_GetSize)
{
	qfZoneScoped (true);
	auto window =  (imui_window_t *) P_GPOINTER (pr, 0);
	R_var (pr, ivec2) = (pr_ivec2_t) { window->xlen, window->ylen };
}

bi(IMUI_Window_IsOpen)
{
	qfZoneScoped (true);
	auto window =  (imui_window_t *) P_GPOINTER (pr, 0);
	R_INT (pr) = window->is_open;
}

bi(IMUI_Window_IsCollapsed)
{
	qfZoneScoped (true);
	auto window =  (imui_window_t *) P_GPOINTER (pr, 0);
	R_INT (pr) = window->is_collapsed;
}

bi(IMUI_Window_GetNoCollapse)
{
	qfZoneScoped (true);
	auto window =  (imui_window_t *) P_GPOINTER (pr, 0);
	R_INT (pr) = window->no_collapse;
}

bi(IMUI_Window_GetAutoFit)
{
	qfZoneScoped (true);
	auto window =  (imui_window_t *) P_GPOINTER (pr, 0);
	R_INT (pr) = window->auto_fit;
}

bi(IMUI_Window_GetReference)
{
	qfZoneScoped (true);
	auto window =  (imui_window_t *) P_GPOINTER (pr, 0);
	RETURN_STRING (pr, window->reference);
}

bi(IMUI_Window_GetReferenceGravity)
{
	qfZoneScoped (true);
	auto window =  (imui_window_t *) P_GPOINTER (pr, 0);
	R_INT (pr) = window->reference_gravity;
}

bi(IMUI_Window_GetAnchorGravity)
{
	qfZoneScoped (true);
	auto window =  (imui_window_t *) P_GPOINTER (pr, 0);
	R_INT (pr) = window->anchor_gravity;
}

bi(IMUI_Window_GetParent)
{
	qfZoneScoped (true);
	auto window =  (imui_window_t *) P_GPOINTER (pr, 0);
	R_UINT (pr) = window->parent;
}

bi(IMUI_NewContext)
{
	qfZoneScoped (true);
	imui_resources_t *res = _res;
	const char *font = P_GSTRING (pr, 0);
	float       font_size = P_FLOAT (pr, 1);

	auto bi_ctx = imui_ctx_new (res);
	bi_ctx->imui_ctx = IMUI_NewContext (res->canvas_sys, font, font_size);

	bi_ctx->next = res->imui_ctxs;
	bi_ctx->prev = &res->imui_ctxs;
	if (res->imui_ctxs) {
		res->imui_ctxs->prev = &bi_ctx->next;
	}
	res->imui_ctxs = bi_ctx;

	R_INT (pr) = imui_ctx_index (res, bi_ctx);
}

static bi_imui_ctx_t *__attribute__((pure))
_get_imui_ctx (imui_resources_t *res, const char *name, int index)
{
	qfZoneScoped (true);
	auto bi_ctx = imui_ctx_get (res, index);

	if (!bi_ctx) {
		PR_RunError (res->pr, "invalid imui context index passed to %s",
					 name + 3);
	}
	return bi_ctx;
}
#define get_imui_ctx(index) _get_imui_ctx(res,__FUNCTION__,index)

bi(IMUI_DestroyContext)
{
	qfZoneScoped (true);
	auto res = (imui_resources_t *) _res;
	auto bi_ctx = get_imui_ctx (P_INT (pr, 0));
	IMUI_DestroyContext (bi_ctx->imui_ctx);
	*bi_ctx->prev = bi_ctx->next;
	if (bi_ctx->next) {
		bi_ctx->next->prev = bi_ctx->prev;
	}
	imui_ctx_free (res, bi_ctx);
}

bi(IMUI_State_SetPos)
{
	qfZoneScoped (true);
	auto res = (imui_resources_t *) _res;
	auto bi_ctx = get_imui_ctx (P_INT (pr, 0));
	auto label = P_GSTRING (pr, 1);
	imui_state_t *state;
	if (label) {
		state = IMUI_FindState (bi_ctx->imui_ctx, label);
	} else {
		state = IMUI_CurrentState (bi_ctx->imui_ctx);
	}
	if (state) {
		state->pos = (view_pos_t) { VEC2_EXP (P_var (pr, 2, ivec2)) };
	}
}

bi(IMUI_State_SetLen)
{
	qfZoneScoped (true);
	auto res = (imui_resources_t *) _res;
	auto bi_ctx = get_imui_ctx (P_INT (pr, 0));
	auto label = P_GSTRING (pr, 1);
	imui_state_t *state;
	if (label) {
		state = IMUI_FindState (bi_ctx->imui_ctx, label);
	} else {
		state = IMUI_CurrentState (bi_ctx->imui_ctx);
	}
	if (state) {
		state->len = (view_pos_t) { VEC2_EXP (P_var (pr, 2, ivec2)) };
	}
}

bi(IMUI_State_GetPos)
{
	qfZoneScoped (true);
	auto res = (imui_resources_t *) _res;
	auto bi_ctx = get_imui_ctx (P_INT (pr, 0));
	auto label = P_GSTRING (pr, 1);
	imui_state_t *state;
	if (label) {
		state = IMUI_FindState (bi_ctx->imui_ctx, label);
	} else {
		state = IMUI_CurrentState (bi_ctx->imui_ctx);
	}
	if (state) {
		R_var (pr, ivec2) = (pr_ivec2_t) { state->pos.x, state->pos.x };
	}
}

bi(IMUI_State_GetLen)
{
	qfZoneScoped (true);
	auto res = (imui_resources_t *) _res;
	auto bi_ctx = get_imui_ctx (P_INT (pr, 0));
	auto label = P_GSTRING (pr, 1);
	imui_state_t *state;
	if (label) {
		state = IMUI_FindState (bi_ctx->imui_ctx, label);
	} else {
		state = IMUI_CurrentState (bi_ctx->imui_ctx);
	}
	if (state) {
		R_var (pr, ivec2) = (pr_ivec2_t) { state->len.x, state->len.x };
	}
}

bi (IMUI_SetVisible)
{
	qfZoneScoped (true);
	auto res = (imui_resources_t *) _res;
	auto bi_ctx = get_imui_ctx (P_INT (pr, 0));
	IMUI_SetVisible (bi_ctx->imui_ctx, P_INT (pr, 1));
}

bi (IMUI_SetSize)
{
	qfZoneScoped (true);
	auto res = (imui_resources_t *) _res;
	auto bi_ctx = get_imui_ctx (P_INT (pr, 0));
	IMUI_SetSize (bi_ctx->imui_ctx, P_INT (pr, 1), P_INT (pr, 2));
}

bi (IMUI_ProcessEvent)
{
	qfZoneScoped (true);
	auto res = (imui_resources_t *) _res;
	auto bi_ctx = get_imui_ctx (P_INT (pr, 0));
	auto ie_event = (struct IE_event_s *) P_GPOINTER (pr, 1);
	R_INT (pr) = IMUI_ProcessEvent (bi_ctx->imui_ctx, ie_event);
}

bi (IMUI_BeginFrame)
{
	qfZoneScoped (true);
	auto res = (imui_resources_t *) _res;
	auto bi_ctx = get_imui_ctx (P_INT (pr, 0));
	IMUI_BeginFrame (bi_ctx->imui_ctx);
}

bi (IMUI_Draw)
{
	qfZoneScoped (true);
	auto res = (imui_resources_t *) _res;
	auto bi_ctx = get_imui_ctx (P_INT (pr, 0));
	IMUI_Draw (bi_ctx->imui_ctx);
	Canvas_Draw (res->canvas_sys);
}

bi (IMUI_PushLayout)
{
	qfZoneScoped (true);
	auto res = (imui_resources_t *) _res;
	auto bi_ctx = get_imui_ctx (P_INT (pr, 0));
	R_INT (pr) = IMUI_PushLayout (bi_ctx->imui_ctx, P_INT (pr, 1));
}

bi (IMUI_PopLayout)
{
	qfZoneScoped (true);
	auto res = (imui_resources_t *) _res;
	auto bi_ctx = get_imui_ctx (P_INT (pr, 0));
	IMUI_PopLayout (bi_ctx->imui_ctx);
}

bi (IMUI_Layout_SetXSize)
{
	qfZoneScoped (true);
	auto res = (imui_resources_t *) _res;
	auto bi_ctx = get_imui_ctx (P_INT (pr, 0));
	IMUI_Layout_SetXSize (bi_ctx->imui_ctx, P_INT (pr, 1), P_INT (pr, 2));
}

bi (IMUI_Layout_SetYSize)
{
	qfZoneScoped (true);
	auto res = (imui_resources_t *) _res;
	auto bi_ctx = get_imui_ctx (P_INT (pr, 0));
	IMUI_Layout_SetYSize (bi_ctx->imui_ctx, P_INT (pr, 1), P_INT (pr, 2));
}

bi (IMUI_PushStyle)
{
	qfZoneScoped (true);
	auto res = (imui_resources_t *) _res;
	auto bi_ctx = get_imui_ctx (P_INT (pr, 0));
	R_INT (pr) = IMUI_PushStyle (bi_ctx->imui_ctx,
								 &P_PACKED (pr, imui_style_t, 1));
}

bi (IMUI_PopStyle)
{
	qfZoneScoped (true);
	auto res = (imui_resources_t *) _res;
	auto bi_ctx = get_imui_ctx (P_INT (pr, 0));
	IMUI_PopStyle (bi_ctx->imui_ctx);
}

bi (IMUI_Style_Update)
{
	qfZoneScoped (true);
	auto res = (imui_resources_t *) _res;
	auto bi_ctx = get_imui_ctx (P_INT (pr, 0));
	IMUI_Style_Update (bi_ctx->imui_ctx, (imui_style_t *) P_GPOINTER (pr, 1));
}

bi (IMUI_Style_Fetch)
{
	qfZoneScoped (true);
	auto res = (imui_resources_t *) _res;
	auto bi_ctx = get_imui_ctx (P_INT (pr, 0));
	IMUI_Style_Fetch (bi_ctx->imui_ctx, (imui_style_t *) P_GPOINTER (pr, 1));
}

bi (IMUI_SetFill)
{
	qfZoneScoped (true);
	auto res = (imui_resources_t *) _res;
	auto bi_ctx = get_imui_ctx (P_INT (pr, 0));
	IMUI_SetFill (bi_ctx->imui_ctx, P_UINT (pr, 1));
}

bi (IMUI_Label)
{
	qfZoneScoped (true);
	auto res = (imui_resources_t *) _res;
	auto bi_ctx = get_imui_ctx (P_INT (pr, 0));
	IMUI_Label (bi_ctx->imui_ctx, P_GSTRING (pr, 1));
}

bi (IMUI_Labelf)
{
	qfZoneScoped (true);
	auto res = (imui_resources_t *) _res;
	auto bi_ctx = get_imui_ctx (P_INT (pr, 0));
	dstring_clearstr (res->dstr);
	RUA_Sprintf (pr, res->dstr, "IMUI_Labelf", 1);
	IMUI_Label (bi_ctx->imui_ctx, res->dstr->str);
}

bi (IMUI_Passage)
{
	qfZoneScoped (true);
	auto res = (imui_resources_t *) _res;
	auto bi_ctx = get_imui_ctx (P_INT (pr, 0));
	auto passage = RUA_GUI_GetPassage (pr, P_INT (pr, 2));
	IMUI_Passage (bi_ctx->imui_ctx, P_GSTRING (pr, 1), passage);
}

bi (IMUI_Button)
{
	qfZoneScoped (true);
	auto res = (imui_resources_t *) _res;
	auto bi_ctx = get_imui_ctx (P_INT (pr, 0));
	IMUI_Button (bi_ctx->imui_ctx, P_GSTRING (pr, 1));
}

bi (IMUI_Checkbox)
{
	qfZoneScoped (true);
	auto res = (imui_resources_t *) _res;
	auto bi_ctx = get_imui_ctx (P_INT (pr, 0));
	bool flag = *(bool *) P_GPOINTER (pr, 1);
	IMUI_Checkbox (bi_ctx->imui_ctx, &flag, P_GSTRING (pr, 2));
	*(bool *) P_GPOINTER (pr, 1) = flag;
}

bi (IMUI_Radio)
{
	qfZoneScoped (true);
	auto res = (imui_resources_t *) _res;
	auto bi_ctx = get_imui_ctx (P_INT (pr, 0));
	IMUI_Radio (bi_ctx->imui_ctx, (int *) P_GPOINTER (pr, 1), P_INT (pr, 2),
				P_GSTRING (pr, 3));
}

bi (IMUI_Slider)
{
	qfZoneScoped (true);
	auto res = (imui_resources_t *) _res;
	auto bi_ctx = get_imui_ctx (P_INT (pr, 0));
	IMUI_Slider (bi_ctx->imui_ctx, (float *) P_GPOINTER (pr, 1),
				 P_FLOAT (pr, 2), P_FLOAT (pr, 3), P_GSTRING (pr, 4));
}

bi (IMUI_Spacer)
{
	qfZoneScoped (true);
	auto res = (imui_resources_t *) _res;
	auto bi_ctx = get_imui_ctx (P_INT (pr, 0));
	IMUI_Spacer (bi_ctx->imui_ctx, P_INT (pr, 1), P_INT (pr, 2),
				 P_INT (pr, 3), P_INT (pr, 4));
}

bi (IMUI_FlexibleSpace)
{
	qfZoneScoped (true);
	auto res = (imui_resources_t *) _res;
	auto bi_ctx = get_imui_ctx (P_INT (pr, 0));
	//IMUI_FlexibleSpace (bi_ctx->imui_ctx);
	IMUI_Spacer(bi_ctx->imui_ctx, imui_size_expand, 100, imui_size_expand, 100);
}

bi (IMUI_StartPanel)
{
	qfZoneScoped (true);
	auto res = (imui_resources_t *) _res;
	auto bi_ctx = get_imui_ctx (P_INT (pr, 0));
	R_INT (pr) = IMUI_StartPanel (bi_ctx->imui_ctx,
								  &P_PACKED (pr, imui_window_t, 1));
}

bi (IMUI_ExtendPanel)
{
	qfZoneScoped (true);
	auto res = (imui_resources_t *) _res;
	auto bi_ctx = get_imui_ctx (P_INT (pr, 0));
	R_INT (pr) = IMUI_ExtendPanel (bi_ctx->imui_ctx, P_GSTRING (pr, 1));
}

bi (IMUI_EndPanel)
{
	qfZoneScoped (true);
	auto res = (imui_resources_t *) _res;
	auto bi_ctx = get_imui_ctx (P_INT (pr, 0));
	IMUI_EndPanel (bi_ctx->imui_ctx);
}

bi (IMUI_StartMenu)
{
	qfZoneScoped (true);
	auto res = (imui_resources_t *) _res;
	auto bi_ctx = get_imui_ctx (P_INT (pr, 0));
	R_INT (pr) = IMUI_StartMenu (bi_ctx->imui_ctx,
								 &P_PACKED (pr, imui_window_t, 1),
								 P_INT (pr, 2));
}

bi (IMUI_EndMenu)
{
	qfZoneScoped (true);
	auto res = (imui_resources_t *) _res;
	auto bi_ctx = get_imui_ctx (P_INT (pr, 0));
	IMUI_EndMenu (bi_ctx->imui_ctx);
}

bi (IMUI_MenuItem)
{
	qfZoneScoped (true);
	auto res = (imui_resources_t *) _res;
	auto bi_ctx = get_imui_ctx (P_INT (pr, 0));
	R_INT (pr) = IMUI_MenuItem (bi_ctx->imui_ctx, P_GSTRING (pr, 1),
								P_INT (pr, 2));
}

bi (IMUI_StartWindow)
{
	qfZoneScoped (true);
	auto res = (imui_resources_t *) _res;
	auto bi_ctx = get_imui_ctx (P_INT (pr, 0));
	auto window =  (imui_window_t *) P_GPOINTER (pr, 1);
	R_INT (pr) = IMUI_StartWindow (bi_ctx->imui_ctx, window);
}

bi (IMUI_EndWindow)
{
	qfZoneScoped (true);
	auto res = (imui_resources_t *) _res;
	auto bi_ctx = get_imui_ctx (P_INT (pr, 0));
	IMUI_EndWindow (bi_ctx->imui_ctx);
}

bi (IMUI_StartScrollBox)
{
	qfZoneScoped (true);
	auto res = (imui_resources_t *) _res;
	auto bi_ctx = get_imui_ctx (P_INT (pr, 0));
	const char *name = P_GSTRING (pr, 1);
	R_INT (pr) = IMUI_StartScrollBox (bi_ctx->imui_ctx, name);
}

bi (IMUI_EndScrollBox)
{
	qfZoneScoped (true);
	auto res = (imui_resources_t *) _res;
	auto bi_ctx = get_imui_ctx (P_INT (pr, 0));
	IMUI_EndScrollBox (bi_ctx->imui_ctx);
}

bi (IMUI_ScrollBar)
{
	qfZoneScoped (true);
	auto res = (imui_resources_t *) _res;
	auto bi_ctx = get_imui_ctx (P_INT (pr, 0));
	const char *name = P_GSTRING (pr, 1);
	IMUI_ScrollBar (bi_ctx->imui_ctx, name);
}

#undef bi
#define bi(x,np,params...) {#x, bi_##x, -1, np, {params}}
#define p(type) PR_PARAM(type)
#define P(a, s) { .size = (s), .alignment = BITOP_LOG2 (a), }
static builtin_t builtins[] = {
	bi(IMUI_NewWindow,              1, p(string)),
	bi(IMUI_DeleteWindow,           1, p(ptr)),

	bi(IMUI_Window_SetPos,          2, p(ptr), p(ivec2)),
	bi(IMUI_Window_SetSize,         2, p(ptr), p(ivec2)),
	bi(IMUI_Window_SetOpen,         2, p(ptr), p(int)),
	bi(IMUI_Window_SetCollapsed,    2, p(ptr), p(int)),
	bi(IMUI_Window_SetNoCollapse,   2, p(ptr), p(int)),
	bi(IMUI_Window_SetAutoFit,      2, p(ptr), p(int)),
	bi(IMUI_Window_SetReference,    2, p(ptr), p(string)),
	bi(IMUI_Window_SetReferenceGravity, 2, p(ptr), p(int)),
	bi(IMUI_Window_SetAnchorGravity, 2, p(ptr), p(int)),
	bi(IMUI_Window_SetParent,       2, p(ptr), p(uint)),

	bi(IMUI_Window_GetPos,          1, p(ptr)),
	bi(IMUI_Window_GetSize,         1, p(ptr)),
	bi(IMUI_Window_IsOpen,          1, p(ptr)),
	bi(IMUI_Window_IsCollapsed,     1, p(ptr)),
	bi(IMUI_Window_GetNoCollapse,   1, p(ptr)),
	bi(IMUI_Window_GetAutoFit,      1, p(ptr)),
	bi(IMUI_Window_GetReference,    1, p(ptr)),
	bi(IMUI_Window_GetReferenceGravity, 1, p(ptr)),
	bi(IMUI_Window_GetAnchorGravity, 1, p(ptr)),
	bi(IMUI_Window_GetParent,       1, p(ptr)),

	bi(IMUI_State_SetPos,       3, p(int), p(string), p(ivec2)),
	bi(IMUI_State_SetLen,       3, p(int), p(string), p(ivec2)),
	bi(IMUI_State_GetPos,       2, p(int), p(string)),
	bi(IMUI_State_GetLen,       2, p(int), p(string)),

	bi(IMUI_NewContext,         2, p(string), p(float)),
	bi(IMUI_DestroyContext,     2, p(int)),
	bi(IMUI_SetVisible,         2, p(int), p(int)),
	bi(IMUI_SetSize,            3, p(int), p(int), p(int)),
	bi(IMUI_ProcessEvent,       2, p(int), p(ptr)),
	bi(IMUI_BeginFrame,         1, p(int)),
	bi(IMUI_Draw,               1, p(int)),
	bi(IMUI_PushLayout,         2, p(int), p(int)),
	bi(IMUI_PopLayout,          1, p(int)),
	bi(IMUI_Layout_SetXSize,    3, p(int), p(int), p(int)),
	bi(IMUI_Layout_SetYSize,    3, p(int), p(int), p(int)),
	bi(IMUI_PushStyle,          2, p(int), p(ptr)),
	bi(IMUI_PopStyle,           1, p(int)),
	bi(IMUI_Style_Update,       2, p(int), p(ptr)),
	bi(IMUI_Style_Fetch,        2, p(int), p(ptr)),
	bi(IMUI_SetFill,            2, p(int), p(uint)),
	bi(IMUI_Label,              2, p(int), p(string)),
	bi(IMUI_Labelf,             -3, p(int), p(string)),
	bi(IMUI_Passage,            3, p(int), p(string), p(int)),
	bi(IMUI_Button,             2, p(int), p(string)),
	bi(IMUI_Checkbox,           3, p(int), p(ptr), p(string)),
	bi(IMUI_Radio,              4, p(int), p(ptr), p(int), p(string)),
	bi(IMUI_Slider,             5, p(int), p(ptr), p(float), p(float), p(string)),
	bi(IMUI_Spacer,             5, p(int), p(int), p(int), p(int), p(int)),
	bi(IMUI_FlexibleSpace,      1, p(int)),
	bi(IMUI_StartPanel,         2, p(int), p(ptr)),
	bi(IMUI_ExtendPanel,        3, p(int), p(string)),
	bi(IMUI_EndPanel,           1, p(int)),
	bi(IMUI_StartMenu,          3, p(int), p(ptr), p(int)),
	bi(IMUI_EndMenu,            1, p(int)),
	bi(IMUI_MenuItem,           3, p(int), p(string), p(int)),
	bi(IMUI_StartWindow,        2, p(int), p(ptr)),
	bi(IMUI_EndWindow,          1, p(int)),
	bi(IMUI_StartScrollBox,     2, p(int), p(string)),
	bi(IMUI_EndScrollBox,       1, p(int)),
	bi(IMUI_ScrollBar,          2, p(int), p(string)),

	{0}
};

void
RUA_IMUI_Init (progs_t *pr, int secure)
{
	qfZoneScoped (true);
	imui_resources_t *res = calloc (1, sizeof (imui_resources_t));
	res->pr = pr;

	PR_Resources_Register (pr, "IMUI", res, bi_imui_clear, bi_imui_destroy);
	PR_RegisterBuiltins (pr, builtins, res);

	res->canvas_sys = RUA_GUI_GetCanvasSystem (pr);
	res->dstr = dstring_new ();
}
