/*
	qf_draw.h

	vulkan specific draw function

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

#ifndef __QF_Vulkan_qf_draw_h
#define __QF_Vulkan_qf_draw_h

struct vulkan_ctx_s;
struct qfv_renderframe_s;
struct qpic_s;
struct rfont_s;
struct draw_charbuffer_s;

void Vulkan_Draw_CharBuffer (int x, int y, struct draw_charbuffer_s *buffer,
							 struct vulkan_ctx_s *ctx);
void Vulkan_Draw_Init (struct vulkan_ctx_s *ctx);
void Vulkan_Draw_Shutdown (struct vulkan_ctx_s *ctx);
void Vulkan_Draw_Character (int x, int y, unsigned ch,
							struct vulkan_ctx_s *ctx);
void Vulkan_Draw_String (int x, int y, const char *str,
						 struct vulkan_ctx_s *ctx);
void Vulkan_Draw_nString (int x, int y, const char *str, int count,
						  struct vulkan_ctx_s *ctx);
void Vulkan_Draw_AltString (int x, int y, const char *str,
							struct vulkan_ctx_s *ctx);
void Vulkan_Draw_ConsoleBackground (int lines, byte alpha,
									struct vulkan_ctx_s *ctx);
void Vulkan_Draw_Crosshair (struct vulkan_ctx_s *ctx);
void Vulkan_Draw_CrosshairAt (int ch, int x, int y, struct vulkan_ctx_s *ctx);
void Vulkan_Draw_TileClear (int x, int y, int w, int h,
							struct vulkan_ctx_s *ctx);
void Vulkan_Draw_Fill (int x, int y, int w, int h, int c,
					   struct vulkan_ctx_s *ctx);
void Vulkan_Draw_Line (int x0, int y0, int x1, int y1, int c,
					   struct vulkan_ctx_s *ctx);
void Vulkan_Draw_TextBox (int x, int y, int width, int lines, byte alpha,
						  struct vulkan_ctx_s *ctx);
void Vulkan_Draw_FadeScreen (struct vulkan_ctx_s *ctx);
void Vulkan_Draw_BlendScreen (quat_t color, struct vulkan_ctx_s *ctx);
struct qpic_s *Vulkan_Draw_CachePic (const char *path, qboolean alpha,
									 struct vulkan_ctx_s *ctx);
void Vulkan_Draw_UncachePic (const char *path, struct vulkan_ctx_s *ctx);
struct qpic_s *Vulkan_Draw_MakePic (int width, int height, const byte *data,
									struct vulkan_ctx_s *ctx);
void Vulkan_Draw_DestroyPic (struct qpic_s *pic, struct vulkan_ctx_s *ctx);
struct qpic_s *Vulkan_Draw_PicFromWad (const char *name,
									   struct vulkan_ctx_s *ctx);
void Vulkan_Draw_Pic (int x, int y, struct qpic_s *pic,
					  struct vulkan_ctx_s *ctx);
void Vulkan_Draw_Picf (float x, float y, struct qpic_s *pic,
					   struct vulkan_ctx_s *ctx);
void Vulkan_Draw_SubPic(int x, int y, struct qpic_s *pic,
						int srcx, int srcy, int width, int height,
						struct vulkan_ctx_s *ctx);
void Vulkan_Draw_AddFont (struct rfont_s *font, struct vulkan_ctx_s *ctx);
void Vulkan_Draw_FontString (int x, int y, const char *str,
							 struct vulkan_ctx_s *ctx);

void Vulkan_Set2D (struct vulkan_ctx_s *ctx);
void Vulkan_Set2DScaled (struct vulkan_ctx_s *ctx);
void Vulkan_End2D (struct vulkan_ctx_s *ctx);
void Vulkan_DrawReset (struct vulkan_ctx_s *ctx);
void Vulkan_FlushText (struct qfv_renderframe_s *rFrame);

#endif//__QF_Vulkan_qf_draw_h
