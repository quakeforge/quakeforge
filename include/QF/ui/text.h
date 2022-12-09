/*
	text.h

	Text management

	Copyright (C) 2022 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2022/9/4

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

#ifndef __QF_ui_text_h
#define __QF_ui_text_h

#include <hb.h>
#include <hb-ft.h>

#include "QF/darray.h"

#include "QF/ecs/component.h"

typedef struct script_component_s {
	hb_language_t language;
	hb_script_t script;
	hb_direction_t direction;
} script_component_t;

typedef struct glyphobj_s {
	int         glyphid;
	int         x, y;
	int         fontid;
} glyphobj_t;

typedef struct glyphref_s {
	uint32_t    start;
	uint32_t    count;
} glyphref_t;

typedef struct glyphset_s {
	glyphobj_t *glyphs;
	uint32_t    count;
} glyphset_t;

typedef struct text_s {
	const char *text;
	const char *language;
	hb_script_t script;
	hb_direction_t direction;
} text_t;

enum {
	// covers both view and passage text object hierarcies
	text_href,
	// all the glyhphs in a passage
	text_passage,
	text_glyphs,
	text_script,
	text_font,
	text_features,

	text_type_count
};

typedef struct featureset_s DARRAY_TYPE (hb_feature_t) featureset_t;

struct font_s;
struct rglyph_s;
typedef void text_render_t (uint32_t glyphid, int x, int y, void *data);

typedef struct shaper_s {
	struct font_s *rfont;
	hb_font_t  *font;
	hb_buffer_t *buffer;
	featureset_t features;
} shaper_t;

extern hb_feature_t LigatureOff;
extern hb_feature_t LigatureOn;
extern hb_feature_t KerningOff;
extern hb_feature_t KerningOn;
extern hb_feature_t CligOff;
extern hb_feature_t CligOn;

struct font_s;
struct passage_s;
extern struct ecs_registry_s *text_reg;

shaper_t *Text_NewShaper (struct font_s *font);
void Text_DeleteShaper (shaper_t *shaper);
void Text_AddFeature (shaper_t *shaper, hb_feature_t feature);
void Text_ShapeText (shaper_t *shaper, text_t *text);
void Text_RenderText (shaper_t *shaper, text_t *text, int x, int y,
					  text_render_t *render, void *data);
void Text_Init (void);
struct view_s Text_View (struct font_s *font, struct passage_s *passage);

#endif//__QF_ui_text_h
