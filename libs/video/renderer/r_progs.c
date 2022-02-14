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
	return PR_RESNEW (res->qpic_map);
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
	PR_RESFREE (res->qpic_map, qp);
}

static void
qpic_reset (draw_resources_t *res)
{
	PR_RESRESET (res->qpic_map);
}

static inline qpic_res_t *
qpic_get (draw_resources_t *res, int index)
{
	return PR_RESGET (res->qpic_map, index);
}

static inline int __attribute__((pure))
qpic_index (draw_resources_t *res, qpic_res_t *qp)
{
	return PR_RESINDEX (res->qpic_map, qp);
}

static qpic_res_t *
get_qpic (progs_t *pr, draw_resources_t *res, const char *name, int qpic_handle)
{
	qpic_res_t *qp = qpic_get (res, qpic_handle);

	if (!qp)
		PR_RunError (pr, "invalid qpic handle passed to %s", name + 3);
	return qp;
}

static void
bi_Draw_FreePic (progs_t *pr, void *_res)
{
	draw_resources_t *res = _res;
	bi_qpic_t  *bq = &P_STRUCT (pr, bi_qpic_t, 0);
	qpic_res_t *qp = get_qpic (pr, res, __FUNCTION__, bq->pic_handle);

	PR_Zone_Free (pr, qp->bq);
	qpic_free (res, qp);
}

static void
bi_Draw_MakePic (progs_t *pr, void *_res)
{
	draw_resources_t *res = _res;
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
bi_Draw_CachePic (progs_t *pr, void *_res)
{
	draw_resources_t *res = _res;
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
bi_Draw_Pic (progs_t *pr, void *_res)
{
	draw_resources_t *res = _res;
	int         x = P_INT (pr, 0);
	int         y = P_INT (pr, 1);
	bi_qpic_t  *bq = &P_STRUCT (pr, bi_qpic_t, 2);
	qpic_res_t *qp = get_qpic (pr, res, __FUNCTION__, bq->pic_handle);
	qpic_t     *pic = qp->pic;

	r_funcs->Draw_Pic (x, y, pic);
}

static void
bi_Draw_Picf (progs_t *pr, void *_res)
{
	draw_resources_t *res = _res;
	float       x = P_FLOAT (pr, 0);
	float       y = P_FLOAT (pr, 1);
	bi_qpic_t  *bq = &P_STRUCT (pr, bi_qpic_t, 2);
	qpic_res_t *qp = get_qpic (pr, res, __FUNCTION__, bq->pic_handle);
	qpic_t     *pic = qp->pic;

	r_funcs->Draw_Picf (x, y, pic);
}

static void
bi_Draw_SubPic (progs_t *pr, void *_res)
{
	draw_resources_t *res = _res;
	int         x = P_INT (pr, 0);
	int         y = P_INT (pr, 1);
	bi_qpic_t  *bq = &P_STRUCT (pr, bi_qpic_t, 2);
	qpic_res_t *qp = get_qpic (pr, res, __FUNCTION__, bq->pic_handle);
	qpic_t     *pic = qp->pic;
	int         srcx = P_INT (pr, 3);
	int         srcy = P_INT (pr, 4);
	int         width = P_INT (pr, 5);
	int         height = P_INT (pr, 6);

	r_funcs->Draw_SubPic (x, y, pic, srcx, srcy, width, height);
}

static void
bi_Draw_CenterPic (progs_t *pr, void *_res)
{
	draw_resources_t *res = _res;
	int         x = P_INT (pr, 0);
	int         y = P_INT (pr, 1);
	bi_qpic_t  *bq = &P_STRUCT (pr, bi_qpic_t, 2);
	qpic_res_t *qp = get_qpic (pr, res, __FUNCTION__, bq->pic_handle);
	qpic_t     *pic = qp->pic;

	r_funcs->Draw_Pic (x - pic->width / 2, y, pic);
}

static void
bi_Draw_Character (progs_t *pr, void *_res)
{
	int         x = P_INT (pr, 0);
	int         y = P_INT (pr, 1);
	int         c = P_INT (pr, 2);

	r_funcs->Draw_Character (x, y, c);
}

static void
bi_Draw_String (progs_t *pr, void *_res)
{
	int         x = P_INT (pr, 0);
	int         y = P_INT (pr, 1);
	const char *text = P_GSTRING (pr, 2);

	r_funcs->Draw_String (x, y, text);
}

static void
bi_Draw_nString (progs_t *pr, void *_res)
{
	int         x = P_INT (pr, 0);
	int         y = P_INT (pr, 1);
	const char *text = P_GSTRING (pr, 2);
	int         n = P_INT (pr, 3);

	r_funcs->Draw_nString (x, y, text, n);
}

static void
bi_Draw_AltString (progs_t *pr, void *_res)
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
bi_Draw_Fill (progs_t *pr, void *_res)
{
	int         x = P_INT (pr, 0);
	int         y = P_INT (pr, 1);
	int         w = P_INT (pr, 2);
	int         h = P_INT (pr, 3);
	int         color = P_INT (pr, 4);

	r_funcs->Draw_Fill (x, y, w, h, color);
}

static void
bi_Draw_Crosshair (progs_t *pr, void *_res)
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
bi_draw_clear (progs_t *pr, void *_res)
{
	draw_resources_t *res = (draw_resources_t *) _res;
	qpic_res_t *qp;

	for (qp = res->qpics; qp; qp = qp->next) {
		bi_draw_free_qpic (qp);
	}
	res->qpics = 0;

	qpic_reset (res);
	Hash_FlushTable (res->pic_hash);
}

#define bi(x,np,params...) {#x, bi_##x, -1, np, {params}}
#define p(type) PR_PARAM(type)
#define P(a, s) { .size = (s), .alignment = BITOP_LOG2 (a), }
static builtin_t builtins[] = {
	bi(Draw_FreePic,    1, p(ptr)),
	bi(Draw_MakePic,    3, p(int), p(int), p(string)),
	bi(Draw_CachePic,   2, p(string), p(int)),
	bi(Draw_Pic,        3, p(int), p(int), p(ptr)),
	bi(Draw_Picf,       3, p(float), p(float), p(ptr)),
	bi(Draw_SubPic,     7, p(int), p(int), p(ptr),
	                       p(int), p(int), p(int), p(int)),
	bi(Draw_CenterPic,  3, p(int), p(int), p(ptr)),
	bi(Draw_Character,  3, p(int), p(int), p(int)),
	bi(Draw_String,     3, p(int), p(int), p(string)),
	bi(Draw_nString,    4, p(int), p(int), p(string), p(int)),
	bi(Draw_AltString,  3, p(int), p(int), p(string)),
	bi(Draw_Fill,       5, p(int), p(int), p(int), p(int), p(int)),
	bi(Draw_Crosshair,  5, p(int), p(int), p(int), p(int)),
	{0}
};

void
R_Progs_Init (progs_t *pr)
{
	draw_resources_t *res = calloc (1, sizeof (draw_resources_t));
	res->pic_hash = Hash_NewTable (61, bi_draw_get_key, 0, 0,
								   pr->hashlink_freelist);

	PR_Resources_Register (pr, "Draw", res, bi_draw_clear);
	PR_RegisterBuiltins (pr, builtins, res);
}
