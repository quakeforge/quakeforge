/*
	r_progs.c

	QC interface to the renderer

	Copyright (C) 1996-1997  Id Software, Inc.

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

static __attribute__ ((used)) const char rcsid[] = 
	"$Id$";

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#include <stdlib.h>

#include "QF/draw.h"
#include "QF/hash.h"
#include "QF/progs.h"
#include "QF/render.h"
#include "QF/sys.h"

typedef struct {
	int         width;
	int         height;
	qpic_t     *pic;
} bi_qpic_t;

typedef struct qpic_res_s {
	char       *name;
	bi_qpic_t  *pic;
} qpic_res_t;

typedef struct {
	hashtab_t  *pic_hash;
} draw_resources_t;

static qpic_t *
get_qpic (progs_t *pr, int arg, const char *func)
{
	bi_qpic_t  *pic;

	if (arg <= ((pr_type_t *) pr->zone - pr->pr_globals)
		|| arg >= pr->globals_size)
		PR_RunError (pr, "%s: Invalid qpic_t: %d %d", func, arg, pr->globals_size);

	pic = (bi_qpic_t *)(pr->pr_globals + arg);
	return pic->pic;
}

static void
bi_Draw_CachePic (progs_t *pr)
{
	draw_resources_t *res = PR_Resources_Find (pr, "Draw");
	const char *path = P_GSTRING (pr, 0);
	int         alpha = P_INT (pr, 1);
	qpic_t     *pic = Draw_CachePic (path, alpha);
	bi_qpic_t  *qpic;
	qpic_res_t *rpic = Hash_Find (res->pic_hash, path);

	if (!pic) {
		Sys_DPrintf ("can't load %s\n", path);
		R_INT (pr) = 0;
		return;
	}
	if (rpic) {
		qpic = rpic->pic;
		qpic->pic = pic;
		R_INT (pr) = (pr_type_t *)qpic - pr->pr_globals;
		return;
	}
	qpic = PR_Zone_Malloc (pr, sizeof (bi_qpic_t));
	qpic->width = pic->width;
	qpic->height = pic->height;
	qpic->pic = pic;
	R_INT (pr) = (pr_type_t *)qpic - pr->pr_globals;
	rpic = malloc (sizeof (qpic_res_t));
	rpic->name = strdup (path);
	rpic->pic = qpic;
	Hash_Add (res->pic_hash, rpic);
}

static void
bi_Draw_Pic (progs_t *pr)
{
	int         x = P_INT (pr, 0);
	int         y = P_INT (pr, 1);
	qpic_t     *pic = get_qpic (pr, P_INT (pr, 2), "Draw_Pic");

	Draw_Pic (x, y, pic);
}

static void
bi_Draw_SubPic (progs_t *pr)
{
	int         x = P_INT (pr, 0);
	int         y = P_INT (pr, 1);
	qpic_t     *pic = get_qpic (pr, P_INT (pr, 2), "Draw_SubPic");
	int         srcx = P_INT (pr, 3);
	int         srcy = P_INT (pr, 4);
	int         width = P_INT (pr, 5);
	int         height = P_INT (pr, 6);

	Draw_SubPic (x, y, pic, srcx, srcy, width, height);
}

static void
bi_Draw_CenterPic (progs_t *pr)
{
	int         x = P_INT (pr, 0);
	int         y = P_INT (pr, 1);
	qpic_t     *pic = get_qpic (pr, P_INT (pr, 2), "Draw_CenterPic");

	Draw_Pic (x - pic->width / 2, y, pic);
}

static void
bi_Draw_Character (progs_t *pr)
{
	int         x = P_INT (pr, 0);
	int         y = P_INT (pr, 1);
	int         c = P_INT (pr, 2);

	Draw_Character (x, y, c);
}

static void
bi_Draw_String (progs_t *pr)
{
	int         x = P_INT (pr, 0);
	int         y = P_INT (pr, 1);
	const char *text = P_GSTRING (pr, 2);

	Draw_String (x, y, text);
}

static void
bi_Draw_nString (progs_t *pr)
{
	int         x = P_INT (pr, 0);
	int         y = P_INT (pr, 1);
	const char *text = P_GSTRING (pr, 2);
	int         n = P_INT (pr, 3);

	Draw_nString (x, y, text, n);
}

static void
bi_Draw_AltString (progs_t *pr)
{
	int         x = P_INT (pr, 0);
	int         y = P_INT (pr, 1);
	const char *text = P_GSTRING (pr, 2);

	Draw_AltString (x, y, text);
}

/*
	bi_Draw_Fill

	Draws a filled area with a color
	(only a wrapper function to original qf-api)
*/
static void
bi_Draw_Fill (progs_t *pr)
{
	int         x = P_INT (pr, 0);
	int         y = P_INT (pr, 1);
	int         w = P_INT (pr, 2);
	int         h = P_INT (pr, 3);
	int         color = P_INT (pr, 4);

	Draw_Fill (x, y, w, h, color);
}

static void
bi_Draw_Crosshair (progs_t *pr)
{
	int         ch = P_INT (pr, 0);
	int         x = P_INT (pr, 1);
	int         y = P_INT (pr, 2);

	Draw_CrosshairAt (ch, x, y);
}

static const char *
bi_draw_get_key (void *p, void *unused)
{
	return ((qpic_res_t *) p)->name;
}

static void
bi_draw_free (void *_p, void *unused)
{
	qpic_res_t *p = (qpic_res_t *) _p;

	free (p->name);
	free (p);
}

static void
bi_draw_clear (progs_t *pr, void *data)
{
	draw_resources_t *res = (draw_resources_t *) data;

	Hash_FlushTable (res->pic_hash);
}

static builtin_t builtins[] = {
	{"Draw_CachePic",	bi_Draw_CachePic,	-1},
	{"Draw_Pic",		bi_Draw_Pic,		-1},
	{"Draw_SubPic",		bi_Draw_SubPic,		-1},
	{"Draw_CenterPic",	bi_Draw_CenterPic,	-1},
	{"Draw_Character",	bi_Draw_Character,	-1},
	{"Draw_String",		bi_Draw_String,		-1},
	{"Draw_nString",	bi_Draw_nString,	-1},
	{"Draw_AltString",	bi_Draw_AltString,	-1},
	{"Draw_Fill",		bi_Draw_Fill,		-1},
	{"Draw_Crosshair",	bi_Draw_Crosshair,	-1},
	{0}
};

void
R_Progs_Init (progs_t *pr)
{
	draw_resources_t *res = malloc (sizeof (draw_resources_t));
	res->pic_hash = Hash_NewTable (61, bi_draw_get_key, bi_draw_free, 0);

	PR_Resources_Register (pr, "Draw", res, bi_draw_clear);
	PR_RegisterBuiltins (pr, builtins);
}
