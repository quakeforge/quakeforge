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
#include "QF/ui/view.h"

#include "QF/plugin/vid_render.h"

#include "rua_internal.h"

typedef struct {
} gui_resources_t;

#define bi(x) static void bi_##x (progs_t *pr, void *_res)

bi (Font_Load)
{
	const char *font_path = P_GSTRING (pr, 0);
	int         font_size = P_INT (pr, 1);

	QFile      *font_file = QFS_FOpenFile (font_path);
	font_t     *font = Font_Load (font_file, font_size);
	R_INT (pr) = font->fontid;
}

bi (Font_String)
{
	int         x = P_INT (pr, 0);
	int         y = P_INT (pr, 1);
	int         fontid = P_INT (pr, 2);
	const char *str = P_GSTRING (pr, 3);
	r_funcs->Draw_FontString (x, y, fontid, str);
}

#undef bi
#define bi(x,np,params...) {#x, bi_##x, -1, np, {params}}
#define p(type) PR_PARAM(type)
#define P(a, s) { .size = (s), .alignment = BITOP_LOG2 (a), }
static builtin_t builtins[] = {
	bi(Font_Load,       2, p(string), p(int)),
	bi(Font_String,     4, p(int), p(int), p(int), p(string)),

	{0}
};

static void
bi_gui_clear (progs_t *pr, void *_res)
{
}

static void
bi_gui_destroy (progs_t *pr, void *_res)
{
	gui_resources_t *res = _res;
	free (res);
}

void
RUA_GUI_Init (progs_t *pr, int secure)
{
	gui_resources_t *res = calloc (1, sizeof (gui_resources_t));

	PR_Resources_Register (pr, "Draw", res, bi_gui_clear, bi_gui_destroy);
	PR_RegisterBuiltins (pr, builtins, res);
}
