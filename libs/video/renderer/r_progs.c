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
static const char rcsid[] = 
	"$Id$";

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "QF/console.h"
#include "QF/draw.h"
#include "QF/progs.h"
#include "QF/render.h"

static qpic_t *
get_qpic (progs_t *pr, int arg, const char *func)
{
	qpic_t     *pic;

	if (arg <= ((pr_type_t *) pr->zone - pr->pr_globals)
		|| arg >= (pr->zone_size / sizeof (pr_type_t)))
		PR_RunError (pr, "%s: Invalid qpic_t", func);

	memcpy (&pic, ((qpic_t *)(pr->pr_globals + arg))->data, sizeof (qpic_t *));
	return pic;
}

static void
bi_Draw_CachePic (progs_t *pr)
{
	const char *path = G_STRING (pr, OFS_PARM0);
	int         alpha = G_INT (pr, OFS_PARM1);
	qpic_t     *pic = Draw_CachePic (path, alpha);
	qpic_t     *qpic;

	if (!pic) {
		Con_DPrintf ("can't load %s\n", path);
		G_INT (pr, OFS_RETURN) = 0;
		return;
	}
	qpic = PR_Zone_Malloc (pr, sizeof (qpic_t) - 4 + sizeof (qpic_t *));
	qpic->width = pic->width;
	qpic->height = pic->height;
	memcpy (qpic->data, &pic, sizeof (qpic_t *));
	G_INT (pr, OFS_RETURN) = (pr_type_t *)qpic - pr->pr_globals;
}

static void
bi_Draw_Pic (progs_t *pr)
{
	int         x = G_INT (pr, OFS_PARM0);
	int         y = G_INT (pr, OFS_PARM1);
	qpic_t     *pic = get_qpic (pr, G_INT (pr, OFS_PARM2), "Draw_Pic");

	Draw_Pic (x, y, pic);
}

static void
bi_Draw_CenterPic (progs_t *pr)
{
	int         x = G_INT (pr, OFS_PARM0);
	int         y = G_INT (pr, OFS_PARM1);
	qpic_t     *pic = get_qpic (pr, G_INT (pr, OFS_PARM2), "Draw_CenterPic");

	Draw_Pic (x - pic->width / 2, y, pic);
}

static void
bi_Draw_Character (progs_t *pr)
{
	int         x = G_INT (pr, OFS_PARM0);
	int         y = G_INT (pr, OFS_PARM1);
	int         c = G_INT (pr, OFS_PARM2);

	Draw_Character (x, y, c);
}

static void
bi_Draw_String (progs_t *pr)
{
	int         x = G_INT (pr, OFS_PARM0);
	int         y = G_INT (pr, OFS_PARM1);
	const char *text = G_STRING (pr, OFS_PARM2);

	Draw_String (x, y, text);
}

static void
bi_Draw_nString (progs_t *pr)
{
	int         x = G_INT (pr, OFS_PARM0);
	int         y = G_INT (pr, OFS_PARM1);
	const char *text = G_STRING (pr, OFS_PARM2);
	int         n = G_INT (pr, OFS_PARM3);

	Draw_nString (x, y, text, n);
}

static void
bi_Draw_AltString (progs_t *pr)
{
	int         x = G_INT (pr, OFS_PARM0);
	int         y = G_INT (pr, OFS_PARM1);
	const char *text = G_STRING (pr, OFS_PARM2);

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
	int         x = G_INT (pr, OFS_PARM0);
	int         y = G_INT (pr, OFS_PARM1);
	int         w = G_INT (pr, OFS_PARM2);
	int         h = G_INT (pr, OFS_PARM3);
	int         color = G_INT (pr, OFS_PARM4);

	Draw_Fill (x, y, w, h, color);
}

void
R_Progs_Init (progs_t *pr)
{
	PR_AddBuiltin (pr, "Draw_CachePic", bi_Draw_CachePic, -1);
	PR_AddBuiltin (pr, "Draw_Pic", bi_Draw_Pic, -1);
	PR_AddBuiltin (pr, "Draw_CenterPic", bi_Draw_CenterPic, -1);
	PR_AddBuiltin (pr, "Draw_Character", bi_Draw_Character, -1);
	PR_AddBuiltin (pr, "Draw_String", bi_Draw_String, -1);
	PR_AddBuiltin (pr, "Draw_nString", bi_Draw_nString, -1);
	PR_AddBuiltin (pr, "Draw_AltString", bi_Draw_AltString, -1);
	PR_AddBuiltin (pr, "Draw_Fill", bi_Draw_Fill, -1);
}
