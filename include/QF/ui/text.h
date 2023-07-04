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

#include "QF/ecs.h"

// These can be converted to hb_direction_t simply by oring with 4
typedef enum {
	text_right_down,	// left to right, then down horizontal text
	text_left_down,		// right to left, then down horizontal text
	text_down_right,	// top to bottom, then right vertical text
	text_up_right,		// bottom to top, then right vertical text
	text_right_up,		// left to right, then up horizontal text
	text_left_up,		// right to left, then up horizontal text
	text_down_left,		// top to bottom, then left vertical text
	text_up_left,		// bottom to top, then left vertical text
} text_dir_e;

typedef struct script_component_s {
	hb_language_t language;
	hb_script_t script;
	text_dir_e  direction;
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

enum {
	// all the glyphs in a passage. Always on only the root view of the passage.
	text_passage_glyphs,
	// glyphs for a single text object
	text_glyphs,
	// tint color for glyphs
	text_color,
	// text_script, text_font and text_features on the passage root object set
	// the defaults for all text objects in the passage. The settings can be
	// overridden at the paragraph level or individual text object level by
	// adding the appropriate component to that text object.
	// script settings for the text object
	text_script,
	// font id for the text object
	text_font,
	// harfbuzz font features for the text object
	text_features,

	text_comp_count
};

extern const component_t text_components[text_comp_count];

typedef struct featureset_s DARRAY_TYPE (hb_feature_t) featureset_t;

extern hb_feature_t LigatureOff;
extern hb_feature_t LigatureOn;
extern hb_feature_t KerningOff;
extern hb_feature_t KerningOn;
extern hb_feature_t CligOff;
extern hb_feature_t CligOn;

struct font_s;
struct passage_s;

typedef struct text_system_s {
	ecs_registry_t *reg;
	uint32_t    view_base;
	uint32_t    text_base;
} text_system_t;

struct view_s Text_PassageView (text_system_t textsys,
								struct font_s *font, struct passage_s *passage);
struct view_s Text_StringView (text_system_t textsys, struct view_s parent,
							   struct font_s *font,
							   const char *str, uint32_t len,
							   script_component_t *sc, featureset_t *fs);
void Text_SetScript (text_system_t textsys, uint32_t textid,
					 const char *lang, hb_script_t script, text_dir_e dir);
void Text_SetFont (text_system_t textsys, uint32_t textid,
				   struct font_s *font);
void Text_SetFeatures (text_system_t textsys, uint32_t textid,
					   featureset_t *features);
void Text_AddFeature (text_system_t textsys, uint32_t textid,
					  hb_feature_t feature);

#endif//__QF_ui_text_h
