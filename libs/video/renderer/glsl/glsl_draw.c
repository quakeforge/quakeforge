/*
	glsl_draw.c

	2D drawing support for GLSL

	Copyright (C) 2011 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2011/12/23

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

static __attribute__ ((used)) const char rcsid[] = "$Id$";

#include "QF/draw.h"

VISIBLE byte *draw_chars;

VISIBLE qpic_t *
Draw_PicFromWad (const char *name)
{
	return 0;
}

VISIBLE qpic_t *
Draw_CachePic (const char *path, qboolean alpha)
{
	return 0;
}

VISIBLE void
Draw_TextBox (int x, int y, int width, int lines, byte alpha)
{
}

VISIBLE void
Draw_Init (void)
{
}

VISIBLE void
Draw_Character (int x, int y, unsigned int chr)
{
}

VISIBLE void
Draw_String (int x, int y, const char *str)
{
}

VISIBLE void
Draw_nString (int x, int y, const char *str, int count)
{
}

void
Draw_AltString (int x, int y, const char *str)
{
}

VISIBLE void
Draw_Crosshair (void)
{
}

void
Draw_CrosshairAt (int ch, int x, int y)
{
}

VISIBLE void
Draw_Pic (int x, int y, qpic_t *pic)
{
}

VISIBLE void
Draw_SubPic (int x, int y, qpic_t *pic, int srcx, int srcy, int width,
			 int height)
{
}

VISIBLE void
Draw_ConsoleBackground (int lines, byte alpha)
{
}

VISIBLE void
Draw_TileClear (int x, int y, int w, int h)
{
}

VISIBLE void
Draw_Fill (int x, int y, int w, int h, int c)
{
}

VISIBLE void
Draw_FadeScreen (void)
{
}
