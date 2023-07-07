/*
	shaper.h

	Text shaping system

	Copyright (C) 2023 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2023/07/07

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

#ifndef __QF_ui_shaper_h
#define __QF_ui_shaper_h

#include "QF/ui/text.h"

typedef struct text_shaper_s text_shaper_t;

typedef struct shaped_glyphs_s {
	const hb_glyph_info_t *glyphInfo;
	const hb_glyph_position_t *glyphPos;
	unsigned    count;
} shaped_glyphs_t;

typedef struct shaping_s {
	const script_component_t *script;
	const featureset_t *features;
	const struct font_s *font;
} shaping_t;

text_shaper_t *Shaper_New (void);
void Shaper_Delete (text_shaper_t *shaper);
void Shaper_FlushUnused (text_shaper_t *shaper);

shaped_glyphs_t Shaper_ShapeText (text_shaper_t *shaper,
								  const shaping_t *control,
								  const char *text, size_t text_len);

#endif//__QF_ui_shaper_h
