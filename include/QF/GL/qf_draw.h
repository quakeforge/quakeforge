/*
	qf_draw.h

	gl specific draw function

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

#ifndef __gl_draw_h
#define __gl_draw_h

struct qpic_s;
struct rfont_s;

void gl_Draw_Init (void);
void gl_Draw_Shutdown (void);
void gl_Draw_Character (int x, int y, unsigned ch);
void gl_Draw_String (int x, int y, const char *str);
void gl_Draw_nString (int x, int y, const char *str, int count);
void gl_Draw_AltString (int x, int y, const char *str);
void gl_Draw_ConsoleBackground (int lines, byte alpha);
void gl_Draw_Crosshair (void);
void gl_Draw_CrosshairAt (int ch, int x, int y);
void gl_Draw_TileClear (int x, int y, int w, int h);
void gl_Draw_Fill (int x, int y, int w, int h, int c);
void gl_Draw_Line (int x0, int y0, int x1, int y1, int c);
void gl_Draw_TextBox (int x, int y, int width, int lines, byte alpha);
void gl_Draw_FadeScreen (void);
void gl_Draw_BlendScreen (quat_t color);
struct qpic_s *gl_Draw_CachePic (const char *path, qboolean alpha);
void gl_Draw_UncachePic (const char *path);
struct qpic_s *gl_Draw_MakePic (int width, int height, const byte *data);
void gl_Draw_DestroyPic (struct qpic_s *pic);
struct qpic_s *gl_Draw_PicFromWad (const char *name);
void gl_Draw_Pic (int x, int y, struct qpic_s *pic);
void gl_Draw_Picf (float x, float y, struct qpic_s *pic);
void gl_Draw_SubPic(int x, int y, struct qpic_s *pic,
					  int srcx, int srcy, int width, int height);
void gl_Draw_AddFont (struct rfont_s *font);

void GL_Set2D (void);
void GL_Set2DScaled (void);
void GL_End2D (void);
void GL_DrawReset (void);
void GL_FlushText (void);

#endif//__gl_draw_h
