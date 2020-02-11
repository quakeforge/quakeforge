/*
	vulkan_draw.c

	2D drawing support for Vulkan

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

#define NH_DEFINE
#include "namehack.h"

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/cvar.h"
#include "QF/draw.h"
#include "QF/dstring.h"
#include "QF/hash.h"
#include "QF/quakefs.h"
#include "QF/sys.h"
#include "QF/vid.h"

#include "QF/Vulkan/qf_draw.h"
#include "QF/Vulkan/qf_vid.h"

#include "r_internal.h"

static qpic_t *
pic_data (const char *name, int w, int h, const byte *data)
{
	qpic_t     *pic;

	pic = malloc (field_offset (qpic_t, data[w * h]));
	pic->width = w;
	pic->height = h;
	memcpy (pic->data, data, pic->width * pic->height);
	return pic;
}

qpic_t *
vulkan_Draw_MakePic (int width, int height, const byte *data)
{
	return pic_data (0, width, height, data);
}

void
vulkan_Draw_DestroyPic (qpic_t *pic)
{
}

qpic_t *
vulkan_Draw_PicFromWad (const char *name)
{
	return pic_data (0, 1, 1, (const byte *)"");
}

qpic_t *
vulkan_Draw_CachePic (const char *path, qboolean alpha)
{
	return pic_data (0, 1, 1, (const byte *)"");
}

void
vulkan_Draw_UncachePic (const char *path)
{
}

void
vulkan_Draw_TextBox (int x, int y, int width, int lines, byte alpha)
{
}

void
vulkan_Draw_Init (void)
{
}

void
vulkan_Draw_Character (int x, int y, unsigned int chr)
{
}

void
vulkan_Draw_String (int x, int y, const char *str)
{
}

void
vulkan_Draw_nString (int x, int y, const char *str, int count)
{
}

void
vulkan_Draw_AltString (int x, int y, const char *str)
{
}

void
vulkan_Draw_CrosshairAt (int ch, int x, int y)
{
}

void
vulkan_Draw_Crosshair (void)
{
}

void
vulkan_Draw_Pic (int x, int y, qpic_t *pic)
{
}

void
vulkan_Draw_Picf (float x, float y, qpic_t *pic)
{
}

void
vulkan_Draw_SubPic (int x, int y, qpic_t *pic, int srcx, int srcy, int width,
				  int height)
{
}

void
vulkan_Draw_ConsoleBackground (int lines, byte alpha)
{
}

void
vulkan_Draw_TileClear (int x, int y, int w, int h)
{
}

void
vulkan_Draw_Fill (int x, int y, int w, int h, int c)
{
}

void
vulkan_Draw_FadeScreen (void)
{
}

void
Vulkan_Set2D (void)
{
}

void
Vulkan_Set2DScaled (void)
{
}

void
Vulkan_End2D (void)
{
}

void
Vulkan_DrawReset (void)
{
}

void
Vulkan_FlushText (void)
{
}

void
vulkan_Draw_BlendScreen (quat_t color)
{
}
