/*
	r_font.c

	Renderer font management management

	Copyright (C) 2022 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2022/8/26

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
#include <math.h>

#include "QF/quakefs.h"
#include "QF/sys.h"
#include "QF/math/bitop.h"

#include "r_font.h"

#include "compat.h"

static FT_Library ft;

static void
copy_glyph (vrect_t *rect, FT_GlyphSlot src_glyph, rfont_t *font)
{
	int         dst_pitch = font->scrap.width;
	byte       *dst = font->scrap_bitmap + rect->x + rect->y * dst_pitch;
	int         src_pitch = src_glyph->bitmap.pitch;
	byte       *src = src_glyph->bitmap.buffer;

	for (unsigned i = 0; i < src_glyph->bitmap.rows; i++) {
		memcpy (dst, src, src_glyph->bitmap.width);
		dst += dst_pitch;
		src += src_pitch;
	}
}

VISIBLE void
R_FontInit (void)
{
	if (FT_Init_FreeType (&ft)) {
		Sys_Error ("Could not init FreeType library");
	}
}

VISIBLE void
R_FontFree (rfont_t *font)
{
	if (font->face) {
		FT_Done_Face (font->face);
	}
	if (font->scrap.rects || font->scrap.free_rects) {
		R_ScrapDelete (&font->scrap);
	}
	free (font->glyph_rects);
	free (font->glyph_bearings);
	free (font->scrap_bitmap);
	free (font->font_resource);
	free (font);
}

VISIBLE rfont_t *
R_FontLoad (QFile *font_file, int size)
{
	byte       *font_data = QFS_LoadFile (font_file, 0);
	if (!font_data) {
		return 0;
	}
	size_t      font_size = qfs_filesize;
	rfont_t    *font = calloc (1, sizeof (rfont_t));
	font->font_resource = font_data;
	if (FT_New_Memory_Face (ft, font_data, font_size, 0, &font->face)) {
		R_FontFree (font);
		return 0;
	}

	FT_Set_Pixel_Sizes(font->face, 0, size);
	int         pixels = 0;
	for (FT_Long gind = 0; gind < font->face->num_glyphs; gind++) {
		FT_Load_Glyph (font->face, gind, FT_LOAD_DEFAULT);

		__auto_type g = font->face->glyph;
		pixels += g->bitmap.width * g->bitmap.rows;
	}
	pixels = sqrt (5 * pixels / 4);
	pixels = BITOP_RUP (pixels);
	R_ScrapInit (&font->scrap, pixels, pixels);
	font->scrap_bitmap = calloc (1, pixels * pixels);
	font->num_glyphs = font->face->num_glyphs;
	font->glyph_rects = malloc (font->num_glyphs * sizeof (vrect_t));
	font->glyph_bearings = malloc (font->num_glyphs * sizeof (vec2i_t));

	for (FT_Long gind = 0; gind < font->face->num_glyphs; gind++) {
		vrect_t    *rect = &font->glyph_rects[gind];
		vec2i_t    *bearing = &font->glyph_bearings[gind];
		FT_Load_Glyph (font->face, gind, FT_LOAD_DEFAULT);
		__auto_type slot = font->face->glyph;
		FT_Render_Glyph (slot, FT_RENDER_MODE_NORMAL);
		int     width = slot->bitmap.width;
		int     height = slot->bitmap.rows;
		*rect = *R_ScrapAlloc (&font->scrap, width, height);
		*bearing = (vec2i_t) { slot->bitmap_left, slot->bitmap_top };

		copy_glyph (rect, slot, font);
	}

	return font;
}
