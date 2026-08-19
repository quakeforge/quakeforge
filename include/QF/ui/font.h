/*
	font.h

	Font management

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

#ifndef __QF_ui_font_h
#define __QF_ui_font_h

#include <ft2build.h>
#include FT_FREETYPE_H

#include "QF/ui/vrect.h"
#include "QF/simd/types.h"

#include "r_scrap.h"//FIXME

typedef struct QFile_s QFile;

typedef struct fontent_s {
	uint32_t    id;
} fontent_t;

typedef struct font_s {
	void       *font_resource;
	FT_Face     face;
	rscrap_t    scrap;
	byte       *scrap_bitmap;
	FT_Long     num_glyphs;
	vrect_t    *glyph_rects;
	vec2i_t    *glyph_bearings;
	uint32_t    fontid;
} font_t;

typedef union glyphkey_s {
	uint64_t    raw_key;
	struct {
		uint64_t    glyphid:16;
		uint64_t    fontid:10;
		uint64_t    ptsize:14;	// 8.6 fixed-point
		uint64_t    subpixel_x:2;
		uint64_t    subpixel_y:2;
		uint64_t    render_mode:2;
		uint64_t    _reserved:18;
	};
} glyphkey_t;

static_assert(sizeof(glyphkey_t) == sizeof(uint64_t));

typedef struct glyphobj_s {
	glyphkey_t  key;
	uint64_t    glyphid:16;
	int64_t     x:24;			// 18.6 fixed-point
	int64_t     y:24;			// 18.6 fixed-point
} glyphobj_t;

void Font_Init (void);
void Font_Free (font_t *font);
font_t *Font_Load (QFile *font_file, int index, int size);

typedef struct fontspec_s {
	char       *path; // free the string. null if no font found
	int         index;
} fontspec_t;

fontspec_t Font_SystemFont (const char *font_pattern);

#endif//__QF_ui_font_h
