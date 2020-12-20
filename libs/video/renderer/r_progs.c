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

#include "r_internal.h"

typedef struct {
	pr_int_t    width;
	pr_int_t    height;
	pr_int_t    pic_handle;
} bi_qpic_t;

typedef struct qpic_res_s {
	struct qpic_res_s *next;
	struct qpic_res_s **prev;
	bi_qpic_t  *bq;
	qpic_t     *pic;
	int         cached;
	char       *name;
} qpic_res_t;

typedef struct {
	PR_RESMAP (qpic_res_t) qpic_map;
	qpic_res_t *qpics;
	hashtab_t  *pic_hash;
} draw_resources_t;

static qpic_res_t *
qpic_new (draw_resources_t *res)
{
	PR_RESNEW (qpic_res_t, res->qpic_map);
}

static void
bi_draw_free_qpic (qpic_res_t *qp)
{
	if (qp->cached)
		r_funcs->Draw_UncachePic (qp->name);
	else
		r_funcs->Draw_DestroyPic (qp->pic);
	if (qp->name)
		free (qp->name);
}

static void
qpic_free (draw_resources_t *res, qpic_res_t *qp)
{
	bi_draw_free_qpic (qp);
	PR_RESFREE (qpic_res_t, res->qpic_map, qp);
}

static void
qpic_reset (draw_resources_t *res)
{
	PR_RESRESET (qpic_res_t, res->qpic_map);
}

static inline qpic_res_t *
qpic_get (draw_resources_t *res, int index)
{
	PR_RESGET (res->qpic_map, index);
}

static inline int __attribute__((pure))
qpic_index (draw_resources_t *res, qpic_res_t *qp)
{
	PR_RESINDEX (res->qpic_map, qp);
}

static qpic_res_t *
get_qpic (progs_t *pr, const char *name, int qpic_handle)
{
	draw_resources_t *res = PR_Resources_Find (pr, "Draw");
	qpic_res_t *qp = qpic_get (res, qpic_handle);

	if (!qp)
		PR_RunError (pr, "invalid qpic handle passed to %s", name + 3);
	return qp;
}

static void
bi_Draw_FreePic (progs_t *pr)
{
	draw_resources_t *res = PR_Resources_Find (pr, "Draw");
	bi_qpic_t  *bq = &P_STRUCT (pr, bi_qpic_t, 0);
	qpic_res_t *qp = get_qpic (pr, __FUNCTION__, bq->pic_handle);

	PR_Zone_Free (pr, qp->bq);
	qpic_free (res, qp);
}

static void
bi_Draw_MakePic (progs_t *pr)
{
	draw_resources_t *res = PR_Resources_Find (pr, "Draw");
	int         width = P_INT (pr, 0);
	int         height = P_INT (pr, 1);
	byte       *data = (byte *) P_GSTRING (pr, 2);
	qpic_t     *pic;
	qpic_res_t *qp;
	bi_qpic_t  *bq;

	pic = r_funcs->Draw_MakePic (width, height, data);
	qp = qpic_new (res);
	qp->name = 0;
	qp->pic = pic;
	qp->cached = 1;
	bq = PR_Zone_Malloc (pr, sizeof (bi_qpic_t));
	bq->width = pic->width;
	bq->height = pic->height;
	bq->pic_handle = qpic_index (res, qp);
	qp->bq = bq;
	RETURN_POINTER (pr, qp->bq);
}

static void
bi_Draw_CachePic (progs_t *pr)
{
	draw_resources_t *res = PR_Resources_Find (pr, "Draw");
	const char *name = P_GSTRING (pr, 0);
	int         alpha = P_INT (pr, 1);
	qpic_t     *pic;
	qpic_res_t *qp;
	bi_qpic_t  *bq;

	pic = r_funcs->Draw_CachePic (name, alpha);
	qp = Hash_Find (res->pic_hash, name);
	if (qp) {
		RETURN_POINTER (pr, qp->bq);
		return;
	}
	qp = qpic_new (res);
	qp->name = strdup (name);
	qp->pic = pic;
	qp->cached = 1;
	bq = PR_Zone_Malloc (pr, sizeof (bi_qpic_t));
	bq->width = pic->width;
	bq->height = pic->height;
	bq->pic_handle = qpic_index (res, qp);
	qp->bq = bq;
	Hash_Add (res->pic_hash, qp);
	RETURN_POINTER (pr, qp->bq);
}

static void
bi_Draw_Pic (progs_t *pr)
{
	int         x = P_INT (pr, 0);
	int         y = P_INT (pr, 1);
	bi_qpic_t  *bq = &P_STRUCT (pr, bi_qpic_t, 2);
	qpic_res_t *qp = get_qpic (pr, __FUNCTION__, bq->pic_handle);
	qpic_t     *pic = qp->pic;

	r_funcs->Draw_Pic (x, y, pic);
}

static void
bi_Draw_Picf (progs_t *pr)
{
	float       x = P_FLOAT (pr, 0);
	float       y = P_FLOAT (pr, 1);
	bi_qpic_t  *bq = &P_STRUCT (pr, bi_qpic_t, 2);
	qpic_res_t *qp = get_qpic (pr, __FUNCTION__, bq->pic_handle);
	qpic_t     *pic = qp->pic;

	r_funcs->Draw_Picf (x, y, pic);
}

static void
bi_Draw_SubPic (progs_t *pr)
{
	int         x = P_INT (pr, 0);
	int         y = P_INT (pr, 1);
	bi_qpic_t  *bq = &P_STRUCT (pr, bi_qpic_t, 2);
	qpic_res_t *qp = get_qpic (pr, __FUNCTION__, bq->pic_handle);
	qpic_t     *pic = qp->pic;
	int         srcx = P_INT (pr, 3);
	int         srcy = P_INT (pr, 4);
	int         width = P_INT (pr, 5);
	int         height = P_INT (pr, 6);

	r_funcs->Draw_SubPic (x, y, pic, srcx, srcy, width, height);
}

static void
bi_Draw_CenterPic (progs_t *pr)
{
	int         x = P_INT (pr, 0);
	int         y = P_INT (pr, 1);
	bi_qpic_t  *bq = &P_STRUCT (pr, bi_qpic_t, 2);
	qpic_res_t *qp = get_qpic (pr, __FUNCTION__, bq->pic_handle);
	qpic_t     *pic = qp->pic;

	r_funcs->Draw_Pic (x - pic->width / 2, y, pic);
}

static void
bi_Draw_Character (progs_t *pr)
{
	int         x = P_INT (pr, 0);
	int         y = P_INT (pr, 1);
	int         c = P_INT (pr, 2);

	r_funcs->Draw_Character (x, y, c);
}

static void
bi_Draw_String (progs_t *pr)
{
	int         x = P_INT (pr, 0);
	int         y = P_INT (pr, 1);
	const char *text = P_GSTRING (pr, 2);

	r_funcs->Draw_String (x, y, text);
}

static void
bi_Draw_nString (progs_t *pr)
{
	int         x = P_INT (pr, 0);
	int         y = P_INT (pr, 1);
	const char *text = P_GSTRING (pr, 2);
	int         n = P_INT (pr, 3);

	r_funcs->Draw_nString (x, y, text, n);
}

static void
bi_Draw_AltString (progs_t *pr)
{
	int         x = P_INT (pr, 0);
	int         y = P_INT (pr, 1);
	const char *text = P_GSTRING (pr, 2);

	r_funcs->Draw_AltString (x, y, text);
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

	r_funcs->Draw_Fill (x, y, w, h, color);
}

static void
bi_Draw_Crosshair (progs_t *pr)
{
	int         ch = P_INT (pr, 0);
	int         x = P_INT (pr, 1);
	int         y = P_INT (pr, 2);

	r_funcs->Draw_CrosshairAt (ch, x, y);
}

static const char *
bi_draw_get_key (const void *p, void *unused)
{
	return ((qpic_res_t *) p)->name;
}

static void
bi_draw_clear (progs_t *pr, void *data)
{
	draw_resources_t *res = (draw_resources_t *) data;
	qpic_res_t *qp;

	for (qp = res->qpics; qp; qp = qp->next) {
		bi_draw_free_qpic (qp);
	}
	res->qpics = 0;

	qpic_reset (res);
	Hash_FlushTable (res->pic_hash);
}

static builtin_t builtins[] = {
	{"Draw_FreePic",	bi_Draw_FreePic,	-1},
	{"Draw_MakePic",	bi_Draw_MakePic,	-1},
	{"Draw_CachePic",	bi_Draw_CachePic,	-1},
	{"Draw_Pic",		bi_Draw_Pic,		-1},
	{"Draw_Picf",		bi_Draw_Picf,		-1},
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
	draw_resources_t *res = calloc (1, sizeof (draw_resources_t));
	res->pic_hash = Hash_NewTable (61, bi_draw_get_key, 0, 0,
								   pr->hashlink_freelist);

	PR_Resources_Register (pr, "Draw", res, bi_draw_clear);
	PR_RegisterBuiltins (pr, builtins);
}
