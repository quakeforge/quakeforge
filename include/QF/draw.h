/*
	draw.h

	Video buffer handling definitions and prototypes

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

	$Id$
*/

#ifndef _DRAW_H
#define _DRAW_H

#include "QF/wad.h"

extern byte *draw_chars;
extern qpic_t *draw_disc; // also used on sbar

void Draw_Init (void);
void Draw_InitText (void);
void Draw_Init_Cvars (void);
void Draw_Character (int x, int y, unsigned int num);
void Draw_Pic (int x, int y, qpic_t *pic);
void Draw_SubPic(int x, int y, qpic_t *pic, int srcx, int srcy, int width, int height);
void Draw_TextBox (int x, int y, int width, int lines, byte alpha);
void Draw_TransPicTranslate (int x, int y, qpic_t *pic, byte *translation);
void Draw_ConsoleBackground (int lines, byte alpha);
void Draw_Crosshair (void);
void Draw_CrosshairAt (int ch, int x, int y);
void Draw_BeginDisc (void);
void Draw_EndDisc (void);
void Draw_TileClear (int x, int y, int w, int h);
void Draw_Fill (int x, int y, int w, int h, int c);
void Draw_FadeScreen (void);
void Draw_String (int x, int y, const char *str);
void Draw_nString (int x, int y, const char *str, int count);
void Draw_AltString (int x, int y, const char *str);
qpic_t *Draw_PicFromWad (const char *name);
qpic_t *Draw_CachePic (const char *path, qboolean alpha);

void Draw_ClearCache (void);

void GL_Set2D (void);
void GL_DrawReset (void);
void GL_FlushText (void);

#endif // _DRAW_H
