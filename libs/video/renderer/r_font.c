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

static uintptr_t
glyph_get_hash (const void *_glyph, void *unused)
{
	__auto_type glyph = (const rglyph_t *) _glyph;
	return glyph->charcode;
}

static int
glyph_compare (const void *_a, const void *_b, void *unused)
{
	__auto_type a = (const rglyph_t *) _a;
	__auto_type b = (const rglyph_t *) _b;
	return a->charcode == b->charcode;
}

static rglyph_t *
alloc_glyph (rfont_t *font)
{
	return PR_RESNEW(font->glyphs);
}

static void
free_glyph (rfont_t *font, rglyph_t *glyph)
{
	PR_RESFREE (font->glyphs, glyph);
}

static void
copy_glypn (rglyph_t *glyph, FT_GlyphSlot src_glyph)
{
	int         dst_pitch = glyph->font->scrap.width;
	byte       *dst = glyph->font->scrap_bitmap + glyph->rect->x + glyph->rect->y * dst_pitch;
	int         src_pitch = src_glyph->bitmap.pitch;
	byte       *src = src_glyph->bitmap.buffer;

	for (unsigned i = 0; i < src_glyph->bitmap.rows; i++) {
		memcpy (dst, src, src_glyph->bitmap.width);
		dst += dst_pitch;
		src += src_pitch;
	}
}

static void
glyphmap_free_glyph (void *_g, void *_f)
{
	rglyph_t   *glyph = _g;
	rfont_t    *font = _f;
	free_glyph (font, glyph);
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
	if (font->glyphmap) {
		Hash_DelTable (font->glyphmap);
	}
	if (font->scrap.rects || font->scrap.free_rects) {
		R_ScrapDelete (&font->scrap);
	}
	free (font->scrap_bitmap);
	free (font->font_resource);
	free (font);
}

VISIBLE rfont_t *
R_FontLoad (QFile *font_file, int size, const int *preload)
{
	byte       *font_data = QFS_LoadFile (font_file, 0);
	if (!font_data) {
		return 0;
	}
	size_t      font_size = Qfilesize (font_file);;
	rfont_t    *font = calloc (1, sizeof (rfont_t));
	font->font_resource = font_data;
	if (FT_New_Memory_Face (ft, font_data, font_size, 0, &font->face)) {
		R_FontFree (font);
		return 0;
	}

	font->glyphmap = Hash_NewTable (0x10000, 0, glyphmap_free_glyph, font, 0);
	Hash_SetHashCompare (font->glyphmap, glyph_get_hash, glyph_compare);

	FT_Set_Pixel_Sizes(font->face, 0, size);
	int         pixels = 0;
	for (const int *c = preload; *c; c++) {
		rglyph_t    search = { .charcode = *c };
		rglyph_t   *glyph = Hash_FindElement (font->glyphmap, &search);
		if (glyph) {
			continue;
		}
		if (FT_Load_Char(font->face, *c, FT_LOAD_DEFAULT)) {
			continue;
		}
		__auto_type g = font->face->glyph;
		pixels += g->bitmap.width * g->bitmap.rows;

		glyph = alloc_glyph (font);
		glyph->charcode = *c;
		Hash_AddElement (font->glyphmap, glyph);
	}
	Hash_FlushTable (font->glyphmap);
	pixels = sqrt (2 * pixels);
	pixels = BITOP_RUP (pixels);
	R_ScrapInit (&font->scrap, pixels, pixels);
	font->scrap_bitmap = calloc (1, pixels * pixels);

	for (const int *c = preload; *c; c++) {
		rglyph_t    search = { .charcode = *c };
		rglyph_t   *glyph = Hash_FindElement (font->glyphmap, &search);
		if (glyph) {
			continue;
		}
		if (FT_Load_Char(font->face, *c, FT_LOAD_RENDER)) {
			continue;
		}
		__auto_type g = font->face->glyph;
		int     width = g->bitmap.width;
		int     height = g->bitmap.rows;
		glyph = alloc_glyph (font);
		glyph->font = font;
		glyph->rect = R_ScrapAlloc (&font->scrap, width, height);
		glyph->bearing = (vec2i_t) { g->bitmap_left, g->bitmap_top };
		glyph->advance = g->advance.x;
		glyph->charcode = *c;
		Hash_AddElement (font->glyphmap, glyph);

		copy_glypn (glyph, g);
	}

	return font;
}
