/*
	r_font.h

	Renderer font management

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

#ifndef __r_font_h
#define __r_font_h

#include <ft2build.h>
#include FT_FREETYPE_H

#include "QF/ui/vrect.h"
#include "QF/simd/types.h"

#include "r_scrap.h"

typedef struct fontent_s {
	uint32_t    id;
} fontent_t;

typedef struct rfont_s {
	void       *font_resource;
	FT_Face     face;
	rscrap_t    scrap;
	byte       *scrap_bitmap;
	FT_Long     num_glyphs;
	vrect_t    *glyph_rects;
	vec2i_t    *glyph_bearings;
} rfont_t;

void R_FontInit (void);
void R_FontFree (rfont_t *font);
rfont_t *R_FontLoad (QFile *font_file, int size);

#endif//__r_font_h
