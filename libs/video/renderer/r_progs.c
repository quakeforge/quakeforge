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

static void
bi_Draw_Pic (progs_t *pr)
{
	int         x = G_INT (pr, OFS_PARM0);
	int         y = G_INT (pr, OFS_PARM1);
	const char *path = G_STRING (pr, OFS_PARM2);
	qpic_t     *pic = Draw_CachePic (path, 1);

	if (!pic) {
		Con_DPrintf ("can't load %s\n", path);
		return;
	}
	Draw_Pic (x, y, pic);
}

static void
bi_Draw_String (progs_t *pr)
{
	int         x = G_INT (pr, OFS_PARM0);
	int         y = G_INT (pr, OFS_PARM1);
	const char *text = G_STRING (pr, OFS_PARM2);

	Draw_String (x, y, text);
}

void
R_Progs_Init (progs_t *pr)
{
	PR_AddBuiltin (pr, "Draw_Pic", bi_Draw_Pic, -1);
	PR_AddBuiltin (pr, "Draw_String", bi_Draw_String, -1);
}
