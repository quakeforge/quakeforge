/*
	qf_draw.h

	glsl specific draw function

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

#ifndef __QF_GLSL_qf_draw_h
#define __QF_GLSL_qf_draw_h

struct qpic_s;
struct font_s;
struct draw_charbuffer_s;

void glsl_Draw_Init (void);
void glsl_Draw_Shutdown (void);
void glsl_Draw_CharBuffer (int x, int y, struct draw_charbuffer_s *buffer);
void glsl_Draw_SetScale (int scale);
void glsl_Draw_Character (int x, int y, unsigned ch);
void glsl_Draw_String (int x, int y, const char *str);
void glsl_Draw_nString (int x, int y, const char *str, int count);
void glsl_Draw_AltString (int x, int y, const char *str);
void glsl_Draw_ConsoleBackground (int lines, byte alpha);
void glsl_Draw_Crosshair (void);
void glsl_Draw_CrosshairAt (int ch, int x, int y);
void glsl_Draw_TileClear (int x, int y, int w, int h);
void glsl_Draw_Fill (int x, int y, int w, int h, int c);
void glsl_Draw_Line (int x0, int y0, int x1, int y1, int c);
void glsl_LineGraph (int x, int y, int *h_vals, int count, int height);
void glsl_Draw_TextBox (int x, int y, int width, int lines, byte alpha);
void glsl_Draw_FadeScreen (void);
void glsl_Draw_BlendScreen (quat_t color);
struct qpic_s *glsl_Draw_CachePic (const char *path, bool alpha);
void glsl_Draw_UncachePic (const char *path);
struct qpic_s *glsl_Draw_MakePic (int width, int height, const byte *data);
void glsl_Draw_DestroyPic (struct qpic_s *pic);
struct qpic_s *glsl_Draw_PicFromWad (const char *name);
void glsl_Draw_Pic (int x, int y, struct qpic_s *pic);
void glsl_Draw_FitPic (int x, int y, int width, int height, struct qpic_s *pic);
void glsl_Draw_Picf (float x, float y, struct qpic_s *pic);
void glsl_Draw_SubPic(int x, int y, struct qpic_s *pic,
					  int srcx, int srcy, int width, int height);
int glsl_Draw_AddFont (struct font_s *font);
void glsl_Draw_Glyph (int x, int y, int fontid, int glyphid, int c);

void GLSL_Set2D (void);
void GLSL_Set2DScaled (void);
void GLSL_End2D (void);
void GLSL_DrawReset (void);
void GLSL_FlushText (void);

#endif//__QF_GLSL_qf_draw_h
