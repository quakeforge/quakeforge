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
#include "QF/quakefs.h"
#include "QF/render.h"
#include "QF/sys.h"

#include "QF/ui/view.h"

#include "r_internal.h"
#include "r_font.h"

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

typedef struct bi_charbuff_s {
	struct bi_charbuff_s *next;
	draw_charbuffer_t *buffer;
} bi_charbuff_t;

typedef struct {
	PR_RESMAP (qpic_res_t) qpic_map;
	PR_RESMAP (bi_charbuff_t) charbuff_map;
	qpic_res_t *qpics;
	hashtab_t  *pic_hash;
	bi_charbuff_t *charbuffs;
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

static qpic_res_t * __attribute__((pure))
get_qpic (progs_t *pr, draw_resources_t *res, const char *name, int qpic_handle)
{
	qpic_res_t *qp = qpic_get (res, qpic_handle);

	if (!qp)
		PR_RunError (pr, "invalid qpic handle passed to %s", name + 3);
	return qp;
}

static bi_charbuff_t *
charbuff_new (draw_resources_t *res)
{
	return PR_RESNEW (res->charbuff_map);
}

static void
charbuff_free (draw_resources_t *res, bi_charbuff_t *cbuff)
{
	Draw_DestroyBuffer (cbuff->buffer);
	PR_RESFREE (res->charbuff_map, cbuff);
}

static void
charbuff_reset (draw_resources_t *res)
{
	PR_RESRESET (res->charbuff_map);
}

static inline bi_charbuff_t *
charbuff_get (draw_resources_t *res, int index)
{
	return PR_RESGET (res->charbuff_map, index);
}

static inline int __attribute__((pure))
charbuff_index (draw_resources_t *res, bi_charbuff_t *qp)
{
	return PR_RESINDEX (res->charbuff_map, qp);
}

static bi_charbuff_t * __attribute__((pure))
get_charbuff (progs_t *pr, draw_resources_t *res, const char *name, int charbuff_handle)
{
	bi_charbuff_t *cbuff = charbuff_get (res, charbuff_handle);

	if (!cbuff)
		PR_RunError (pr, "invalid charbuff handle passed to %s", name + 3);
	return cbuff;
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
bi_Draw_Line (progs_t *pr, void *_res)
{
	int         x0 = P_INT (pr, 0);
	int         y0 = P_INT (pr, 1);
	int         x1 = P_INT (pr, 2);
	int         y1 = P_INT (pr, 3);
	int         color = P_INT (pr, 4);

	r_funcs->Draw_Line (x0, y0, x1, y1, color);
}

static void
bi_Draw_Crosshair (progs_t *pr, void *_res)
{
	int         ch = P_INT (pr, 0);
	int         x = P_INT (pr, 1);
	int         y = P_INT (pr, 2);

	r_funcs->Draw_CrosshairAt (ch, x, y);
}

static void
bi_Draw_Width (progs_t *pr, void *_res)
{
	R_INT (pr) = r_data->vid->conview->xlen;
}

static void
bi_Draw_Height (progs_t *pr, void *_res)
{
	R_INT (pr) = r_data->vid->conview->ylen;
}

static void
bi_Font_Load (progs_t *pr, void *_res)
{
	const char *font_path = P_GSTRING (pr, 0);
	int         font_size = P_INT (pr, 1);

	QFile      *font_file = QFS_FOpenFile (font_path);
	rfont_t    *font = R_FontLoad (font_file, font_size);
	r_funcs->Draw_AddFont (font);
}

static void
bi_Font_String (progs_t *pr, void *_res)
{
	int         x = P_INT (pr, 0);
	int         y = P_INT (pr, 1);
	const char *str = P_GSTRING (pr, 2);
	r_funcs->Draw_FontString (x, y, str);
}

static void
bi_Draw_CreateBuffer (progs_t *pr, void *_res)
{
	draw_resources_t *res = _res;
	int         width = P_INT (pr, 0);
	int         height = P_INT (pr, 1);
	draw_charbuffer_t *buffer;
	bi_charbuff_t *cbuff;

	buffer = Draw_CreateBuffer (width, height);
	cbuff = charbuff_new (res);
	cbuff->next = res->charbuffs;
	res->charbuffs = cbuff;
	cbuff->buffer = buffer;
	R_INT (pr) = charbuff_index (res, cbuff);
}

static void
bi_Draw_DestroyBuffer (progs_t *pr, void *_res)
{
	draw_resources_t *res = _res;
	pr_ptr_t    cbuff_handle = P_POINTER (pr, 0);
	bi_charbuff_t *cbuff = get_charbuff (pr, res, __FUNCTION__, cbuff_handle);

	charbuff_free (res, cbuff);
}

static void
bi_Draw_ClearBuffer (progs_t *pr, void *_res)
{
	draw_resources_t *res = _res;
	pr_ptr_t    cbuff_handle = P_POINTER (pr, 0);
	bi_charbuff_t *cbuff = get_charbuff (pr, res, __FUNCTION__, cbuff_handle);

	Draw_ClearBuffer (cbuff->buffer);
}

static void
bi_Draw_ScrollBuffer (progs_t *pr, void *_res)
{
	draw_resources_t *res = _res;
	pr_ptr_t    cbuff_handle = P_POINTER (pr, 0);
	bi_charbuff_t *cbuff = get_charbuff (pr, res, __FUNCTION__, cbuff_handle);
	int         lines = P_INT (pr, 1);

	Draw_ScrollBuffer (cbuff->buffer, lines);
}

static void
bi_Draw_CharBuffer (progs_t *pr, void *_res)
{
	draw_resources_t *res = _res;
	int         x = P_INT (pr, 0);
	int         y = P_INT (pr, 1);
	pr_ptr_t    cbuff_handle = P_POINTER (pr, 2);
	bi_charbuff_t *cbuff = get_charbuff (pr, res, __FUNCTION__, cbuff_handle);

	Draw_CharBuffer (x, y, cbuff->buffer);
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
	bi_charbuff_t *cbuff;

	for (qp = res->qpics; qp; qp = qp->next) {
		bi_draw_free_qpic (qp);
	}
	res->qpics = 0;

	for (cbuff = res->charbuffs; cbuff; cbuff = cbuff->next) {
		Draw_DestroyBuffer (cbuff->buffer);
	}
	res->charbuffs = 0;

	qpic_reset (res);
	charbuff_reset (res);
	Hash_FlushTable (res->pic_hash);
}

static void
bi_draw_destroy (progs_t *pr, void *_res)
{
	draw_resources_t *res = (draw_resources_t *) _res;
	Hash_DelTable (res->pic_hash);
}

#define bi(x,np,params...) {#x, bi_##x, -1, np, {params}}
#define p(type) PR_PARAM(type)
#define P(a, s) { .size = (s), .alignment = BITOP_LOG2 (a), }
static builtin_t builtins[] = {
	bi(Draw_Width,      0),
	bi(Draw_Height,     0),
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
	bi(Draw_Line,       5, p(int), p(int), p(int), p(int), p(int)),
	bi(Draw_Crosshair,  5, p(int), p(int), p(int), p(int)),

	bi(Font_Load,       2, p(string), p(int)),
	bi(Font_String,     3, p(int), p(int), p(string)),

	bi(Draw_CreateBuffer,   2, p(int), p(int)),
	bi(Draw_DestroyBuffer,  1, p(ptr)),
	bi(Draw_ClearBuffer,    1, p(ptr)),
	bi(Draw_ScrollBuffer,   2, p(ptr), p(int)),
	bi(Draw_CharBuffer,     3, p(int), p(int), p(ptr)),
	{0}
};

void
R_Progs_Init (progs_t *pr)
{
	draw_resources_t *res = calloc (1, sizeof (draw_resources_t));
	res->pic_hash = Hash_NewTable (61, bi_draw_get_key, 0, 0, pr->hashctx);

	PR_Resources_Register (pr, "Draw", res, bi_draw_clear, bi_draw_destroy);
	PR_RegisterBuiltins (pr, builtins, res);
}
