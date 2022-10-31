/*
	r_text.h

	Renderer text management

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

#ifndef __r_text_h
#define __r_text_h

#include <hb.h>
#include <hb-ft.h>

#include "QF/darray.h"

#include "QF/ecs/component.h"

typedef struct script_component_s {
	hb_language_t language;
	hb_script_t script;
	hb_direction_t direction;
} script_component_t;

typedef struct rtext_s {
	const char *text;
	const char *language;
	hb_script_t script;
	hb_direction_t direction;
} rtext_t;

typedef struct r_hb_featureset_s DARRAY_TYPE (hb_feature_t) r_hb_featureset_t;

struct rfont_s;
struct rglyph_s;
typedef void rtext_render_t (uint32_t glyphid, int x, int y, void *data);

typedef struct rshaper_s {
	struct rfont_s *rfont;
	hb_font_t  *font;
	hb_buffer_t *buffer;
	r_hb_featureset_t features;
} rshaper_t;

extern hb_feature_t LigatureOff;
extern hb_feature_t LigatureOn;
extern hb_feature_t KerningOff;
extern hb_feature_t KerningOn;
extern hb_feature_t CligOff;
extern hb_feature_t CligOn;

rshaper_t *RText_NewShaper (struct rfont_s *font);
void RText_DeleteShaper (rshaper_t *shaper);
void RText_AddFeature (rshaper_t *shaper, hb_feature_t feature);
void RText_ShapeText (rshaper_t *shaper, rtext_t *text);
void RText_RenderText (rshaper_t *shaper, rtext_t *text, int x, int y,
					   rtext_render_t *render, void *data);

#endif//__r_text_h
