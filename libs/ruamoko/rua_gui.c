/*
	r_gui.c

	Ruamoko GUI support functions

	Copyright (C) 2022       Bill Currie <bill@taniwha.org>

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

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#include <stdlib.h>

#include "QF/draw.h"
#include "QF/progs.h"
#include "QF/quakefs.h"
#include "QF/render.h"

#include "QF/ui/font.h"
#include "QF/ui/passage.h"
#include "QF/ui/text.h"
#include "QF/ui/view.h"

#include "QF/plugin/vid_render.h"

#include "rua_internal.h"

typedef struct rua_passage_s {
	struct rua_passage_s *next;
	struct rua_passage_s **prev;
	passage_t  *passage;
} rua_passage_t;

typedef struct rua_font_s {
	struct rua_font_s *next;
	struct rua_font_s **prev;
	font_t     *font;
} rua_font_t;

typedef struct {
	progs_t    *pr;
	PR_RESMAP (rua_passage_t) passage_map;
	rua_passage_t *passages;
	PR_RESMAP (rua_font_t) font_map;
	rua_font_t *fonts;
} gui_resources_t;

static rua_passage_t *
passage_new (gui_resources_t *res)
{
	return PR_RESNEW (res->passage_map);
}

static void
passage_free (gui_resources_t *res, rua_passage_t *passage)
{
	PR_RESFREE (res->passage_map, passage);
}

static void
passage_reset (gui_resources_t *res)
{
	PR_RESRESET (res->passage_map);
}

static inline rua_passage_t *
passage_get (gui_resources_t *res, int index)
{
	return PR_RESGET(res->passage_map, index);
}

static inline int __attribute__((pure))
passage_index (gui_resources_t *res, rua_passage_t *passage)
{
	return PR_RESINDEX(res->passage_map, passage);
}

static int
alloc_passage (gui_resources_t *res, passage_t *passage)
{
	rua_passage_t *psg = passage_new (res);

	psg->next = res->passages;
	psg->prev = &res->passages;
	if (res->passages) {
		res->passages->prev = &psg->next;
	}
	res->passages = psg;
	psg->passage = passage;
	return passage_index (res, psg);
}

static rua_passage_t *
_get_passage (gui_resources_t *res, int handle, const char *func)
{
	rua_passage_t *psg = passage_get (res, handle);
	if (!psg) {
		PR_RunError (res->pr, "invalid passage handle passed to %s", func);
	}
	return psg;
}
#define get_passage(res, handle) _get_passage (res, handle, __FUNCTION__ + 3)

static rua_font_t *
font_new (gui_resources_t *res)
{
	return PR_RESNEW (res->font_map);
}
/*
static void
font_free (gui_resources_t *res, rua_font_t *font)
{
	PR_RESFREE (res->font_map, font);
}
*/
static void
font_reset (gui_resources_t *res)
{
	PR_RESRESET (res->font_map);
}

static inline rua_font_t *
font_get (gui_resources_t *res, int index)
{
	return PR_RESGET(res->font_map, index);
}

static inline int __attribute__((pure))
font_index (gui_resources_t *res, rua_font_t *font)
{
	return PR_RESINDEX(res->font_map, font);
}

static int
alloc_font (gui_resources_t *res, font_t *font)
{
	rua_font_t *psg = font_new (res);

	psg->next = res->fonts;
	psg->prev = &res->fonts;
	if (res->fonts) {
		res->fonts->prev = &psg->next;
	}
	res->fonts = psg;
	psg->font = font;
	return font_index (res, psg);
}

static rua_font_t *
_get_font (gui_resources_t *res, int handle, const char *func)
{
	rua_font_t *psg = font_get (res, handle);
	if (!psg) {
		PR_RunError (res->pr, "invalid font handle passed to %s", func);
	}
	return psg;
}
#define get_font(res, handle) _get_font (res, handle, __FUNCTION__ + 3)

static void
bi_gui_clear (progs_t *pr, void *_res)
{
	gui_resources_t *res = _res;

	rua_passage_t *psg;
	for (psg = res->passages; psg; psg = psg->next) {
		Passage_Delete (psg->passage);
	}
	res->passages = 0;
	passage_reset (res);

	rua_font_t *font;
	for (font = res->fonts; font; font = font->next) {
		//FIXME can't unload fonts
	}
	res->fonts = 0;
	font_reset (res);
}

static void
bi_gui_destroy (progs_t *pr, void *_res)
{
	gui_resources_t *res = _res;
	free (res);
}

#define bi(x) static void bi_##x (progs_t *pr, void *_res)

bi (Font_Load)
{
	gui_resources_t *res = _res;
	const char *font_path = P_GSTRING (pr, 0);
	int         font_size = P_INT (pr, 1);

	QFile      *font_file = QFS_FOpenFile (font_path);
	font_t     *font = Font_Load (font_file, font_size);
	R_INT (pr) = alloc_font (res, font);
}

bi (Passage_New)
{
	gui_resources_t *res = _res;
	passage_t  *passage = Passage_New (text_reg);
	R_INT (pr) = alloc_passage (res, passage);
}

bi (Passage_ParseText)
{
	gui_resources_t *res = _res;
	int         handle = P_INT (pr, 0);
	rua_passage_t *psg = get_passage (res, handle);
	const char *text = P_GSTRING (pr, 1);
	Passage_ParseText (psg->passage, text);
}

bi (Passage_Delete)
{
	gui_resources_t *res = _res;
	int         handle = P_INT (pr, 0);
	rua_passage_t *psg = get_passage (res, handle);
	Passage_Delete (psg->passage);
	passage_free (res, psg);
}

bi (Text_View)
{
	gui_resources_t *res = _res;
	rua_font_t *font = get_font (res, P_INT (pr, 0));
	rua_passage_t *psg = get_passage (res, P_INT (pr, 1));
	view_t      view = Text_View (font->font, psg->passage);
	R_INT (pr) = view.id;//FIXME
}

#undef bi
#define bi(x,np,params...) {#x, bi_##x, -1, np, {params}}
#define p(type) PR_PARAM(type)
#define P(a, s) { .size = (s), .alignment = BITOP_LOG2 (a), }
static builtin_t builtins[] = {
	bi(Font_Load,           2, p(string), p(int)),
	bi(Passage_New,         0),
	bi(Passage_ParseText,   2, p(ptr), p(string)),
	bi(Passage_Delete,      1, p(ptr)),

	bi(Text_View,           2, p(ptr), p(string)),

	{0}
};

void
RUA_GUI_Init (progs_t *pr, int secure)
{
	gui_resources_t *res = calloc (1, sizeof (gui_resources_t));
	res->pr = pr;

	PR_Resources_Register (pr, "Draw", res, bi_gui_clear, bi_gui_destroy);
	PR_RegisterBuiltins (pr, builtins, res);
}
