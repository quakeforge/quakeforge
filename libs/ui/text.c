/*
	text.c

	Font text management

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

#include "QF/mathlib.h"
#include "QF/quakefs.h"
#include "QF/sys.h"

#include "QF/ui/font.h"
#include "QF/ui/passage.h"
#include "QF/ui/shaper.h"
#include "QF/ui/text.h"
#include "QF/ui/view.h"

#include "compat.h"

static void
text_passage_glyphs_destroy (void *_glyphset, ecs_registry_t *reg)
{
	glyphset_t *glyphset = _glyphset;
	free (glyphset->glyphs);
}

static void
text_features_create (void *_features)
{
	featureset_t *features = _features;
	*features = (featureset_t) DARRAY_STATIC_INIT (4);
}

static void
text_features_destroy (void *_features, ecs_registry_t *reg)
{
	featureset_t *features = _features;
	DARRAY_CLEAR (features);
}

const component_t text_components[text_comp_count] = {
	[text_passage_glyphs] = {
		.size = sizeof (glyphset_t),
		.name = "passage glyphs",
		.destroy = text_passage_glyphs_destroy,
	},
	[text_glyphs] = {
		.size = sizeof (glyphref_t),
		.name = "glyphs",
	},
	[text_color] = {
		.size = sizeof (uint32_t),
		.name = "color",
	},
	[text_script] = {
		.size = sizeof (script_component_t),
		.name = "script",
	},
	[text_font] = {
		.size = sizeof (font_t *),
		.name = "font",
	},
	[text_features] = {
		.size = sizeof (featureset_t),
		.name = "features",
		.create = text_features_create,
		.destroy = text_features_destroy,
	},
};

typedef struct glyphnode_s {
	struct glyphnode_s *next;
	uint32_t    ent;
	uint32_t    count;
	glyphobj_t *glyphs;
	int         mins[2];
	int         maxs[2];
} glyphnode_t;

static view_resize_f para_flow_funcs[] = {
	[text_right_down]   = view_flow_right_down,
	[text_left_down]    = view_flow_left_down,
	[text_down_right]   = view_flow_down_right,
	[text_up_right]     = view_flow_up_right,
	[text_right_up]     = view_flow_right_up,
	[text_left_up]      = view_flow_left_up,
	[text_down_left]    = view_flow_down_left,
	[text_up_left]      = view_flow_up_left,
};

static void
text_flow_vertical (view_t view, view_pos_t len)
{
	auto ref = View_GetRef (view);
	hierarchy_t *h = Ent_GetComponent (ref.id, ecs_hierarchy, view.reg);
	view_pos_t *vlen = h->components[view_len];
	vlen[ref.index].y = 0;
}

static void
text_flow_horizontal (view_t view, view_pos_t len)
{
	auto ref = View_GetRef (view);
	hierarchy_t *h = Ent_GetComponent (ref.id, ecs_hierarchy, view.reg);
	view_pos_t *vlen = h->components[view_len];
	vlen[ref.index].x = 0;
}

static view_resize_f text_flow_funcs[] = {
	[text_right_down]   = text_flow_vertical,
	[text_left_down]    = text_flow_vertical,
	[text_down_right]   = text_flow_horizontal,
	[text_up_right]     = text_flow_horizontal,
	[text_right_up]     = text_flow_vertical,
	[text_left_up]      = text_flow_vertical,
	[text_down_left]    = text_flow_horizontal,
	[text_up_left]      = text_flow_horizontal,
};

static void
layout_glyphs (glyphnode_t *node, font_t *font, unsigned glpyhCount,
			   const hb_glyph_info_t *glyphInfo,
			   const hb_glyph_position_t *glyphPos)
{
	int         x = 0, y = 0;
	for (unsigned k = 0; k < glpyhCount; k++) {
		uint32_t    glyphid = glyphInfo[k].codepoint;
		vec2i_t     bearing = font->glyph_bearings[glyphid];

		int         xp = glyphPos[k].x_offset / 64;
		int         yp = glyphPos[k].y_offset / 64;
		int         xa = glyphPos[k].x_advance / 64;
		int         ya = glyphPos[k].y_advance / 64;
		xp += bearing[0];
		yp += bearing[1];
		int         xm = xp + font->glyph_rects[glyphid].width;
		int         ym = yp - font->glyph_rects[glyphid].height;

		if (xm - xp == 0) {
			xm = xa;
		}
		if (ym - yp == 0) {
			ym = ya;
		}
		node->mins[0] = min (node->mins[0], xp + x);
		node->mins[1] = min (node->mins[1], ym + y);
		node->maxs[0] = max (node->maxs[0], xm + x);
		node->maxs[1] = max (node->maxs[1], yp + y);

		node->glyphs[k] = (glyphobj_t) {
			.glyphid = glyphid,
			.x = x + xp,
			.y = y - yp,
			.fontid = font->fontid,
		};
		x += xa;
		y += ya;
	}
}

static void
configure_textview (view_t textview, glyphobj_t *glyphs, glyphnode_t *node,
					uint32_t glyphref_base, uint32_t c_glyphs,
					bool vertical, font_t *font)
{
	glyphref_t  glyph_ref = {
		.start = glyphref_base,
		.count = node->count,
	};
	for (uint32_t k = 0; k < node->count; k++) {
		glyphobj_t *go = node->glyphs + k;
		glyphs[glyphref_base + k] = (glyphobj_t) {
			.glyphid = go->glyphid,
			.x = go->x + node->mins[0],
			.y = go->y + node->maxs[1],
			.fontid = go->fontid,
		};
	}
	Ent_SetComponent (textview.id, c_glyphs, textview.reg, &glyph_ref);
	View_SetPos (textview, node->mins[0], -node->mins[1]);
	View_SetLen (textview, node->maxs[0] - node->mins[0],
						   node->maxs[1] - node->mins[1]);
	View_Control (textview)->free_x = 1;
	View_Control (textview)->free_y = 1;
}

static void
set_shaping (shaping_t *shaping, uint32_t ent, text_system_t textsys)
{
	auto reg = textsys.reg;
	uint32_t c_script = textsys.text_base + text_script;
	uint32_t c_features = textsys.text_base + text_features;

	if (Ent_HasComponent (ent, c_script, reg)) {
		shaping->script = Ent_GetComponent (ent, c_script, reg);
	}
	if (Ent_HasComponent (ent, c_features, reg)) {
		shaping->features = Ent_GetComponent(ent, c_features, reg);
	}
}

view_t
Text_PassageView (text_system_t textsys, view_t parent,
				  font_t *font, passage_t *passage, text_shaper_t *shaper)
{
	auto reg = textsys.reg;
	uint32_t c_script = textsys.text_base + text_script;
	uint32_t c_glyphs = textsys.text_base + text_glyphs;
	uint32_t c_passage_glyphs = textsys.text_base + text_passage_glyphs;
	uint32_t psg_ent = passage->hierarchy;
	hierarchy_t *h = Ent_GetComponent (psg_ent, ecs_hierarchy, reg);
	psg_text_t *text_objects = h->components[passage_type_text_obj];
	glyphnode_t *glyph_nodes = 0;
	glyphnode_t **head = &glyph_nodes;

	script_component_t psg_script = {
		.script = HB_SCRIPT_LATIN,
		.direction = text_right_down,
		.language = hb_language_from_string ("en", 2),
	};
	featureset_t psg_features = DARRAY_STATIC_INIT (0);
	shaping_t   psg_shaping = {
		.script = &psg_script,
		.features = &psg_features,
		.font = font,
	};
	set_shaping (&psg_shaping, psg_ent, textsys);

	uint32_t    glyph_count = 0;

	// passages have only the root, paragraphs, and text objects in their
	// hierarchies
	for (uint32_t i = 0; i < h->childCount[0]; i++) {
		uint32_t    paragraph = h->childIndex[0] + i;
		uint32_t    para_ent = h->ent[paragraph];

		shaping_t   para_shaping = psg_shaping;
		set_shaping (&para_shaping, para_ent, textsys);

		for (uint32_t j = 0; j < h->childCount[paragraph]; j++) {
			uint32_t    textind = h->childIndex[paragraph] + j;
			uint32_t    text_ent = h->ent[textind];
			psg_text_t *textobj = &text_objects[textind];
			const char *str = passage->text + textobj->text;
			uint32_t    len = textobj->size;

			shaping_t   txt_shaping = para_shaping;
			set_shaping (&txt_shaping, text_ent, textsys);

			auto shaped_glyphs = Shaper_ShapeText (shaper, &txt_shaping,
												   str, len);
			unsigned    c = shaped_glyphs.count;
			auto glyphInfo = shaped_glyphs.glyphInfo;
			auto glyphPos = shaped_glyphs.glyphPos;

			*head = alloca (sizeof (glyphnode_t));
			**head = (glyphnode_t) {
				.ent = h->ent[textind],
				.count = c,
				.glyphs = alloca (c * sizeof (glyphobj_t)),
				.mins = { 0, 0 },
				.maxs = { INT32_MIN, INT32_MIN },
			};
			glyph_count += c;
			layout_glyphs (*head, font, c, glyphInfo, glyphPos);

			head = &(*head)->next;
		}
	}
	ecs_system_t viewsys = { reg, textsys.view_base };
	if (Ent_HasComponent (h->ent[0], viewsys.base + view_href, viewsys.reg)) {
		Ent_RemoveComponent (h->ent[0], viewsys.base + view_href, viewsys.reg);
		h = Ent_GetComponent (passage->hierarchy, ecs_hierarchy, reg);
	}
	view_t      passage_view = View_AddToEntity (h->ent[0], viewsys, parent,
												 false);
	View_SetOnResize (passage_view, text_flow_funcs[psg_script.direction]);
	h = Ent_GetComponent (passage->hierarchy, ecs_hierarchy, reg);
	glyphref_t  passage_ref = {};
	glyphobj_t *glyphs = malloc (glyph_count * sizeof (glyphobj_t));
	glyphnode_t *g = glyph_nodes;
	// paragraph flow
	for (uint32_t i = 0; i < h->childCount[0]; i++) {
		uint32_t    paragraph = h->childIndex[0] + i;
		uint32_t    para_ent = h->ent[paragraph];
		text_dir_e  para_direction = psg_script.direction;
		if (Ent_HasComponent (para_ent, c_script, reg)) {
			script_component_t *s = Ent_GetComponent (para_ent, c_script,
													  reg);
			para_direction = s->direction;
		}
		bool        para_vertical = !!(para_direction & 2);

		view_t      paraview = View_AddToEntity (h->ent[paragraph], viewsys,
												 passage_view, false);
		View_Control (paraview)->free_x = 1;
		View_Control (paraview)->free_y = 1;
		h = Ent_GetComponent (passage->hierarchy, ecs_hierarchy, reg);
		glyphref_t  pararef = { .start = passage_ref.count };
		for (uint32_t j = 0; j < h->childCount[paragraph]; j++, g = g->next) {
			uint32_t    to = h->childIndex[paragraph] + j;
			view_t      textview = View_AddToEntity (h->ent[to], viewsys,
													 paraview, false);
			h = Ent_GetComponent (passage->hierarchy, ecs_hierarchy, reg);
			configure_textview (textview, glyphs, g, passage_ref.count,
								c_glyphs, para_vertical, font);
			View_SetGravity (textview, grav_flow);
			passage_ref.count += g->count;
		}
		pararef.count = passage_ref.count - pararef.start;
		Ent_SetComponent (paraview.id, c_glyphs, reg, &pararef);

		View_SetOnResize (paraview, para_flow_funcs[para_direction]);
		View_SetResize (paraview, !para_vertical, para_vertical);
		View_SetGravity (paraview, grav_northwest);
		View_Control (paraview)->flow_size = 1;
		View_Control (paraview)->flow_parent = 1;
	}
	Ent_SetComponent (passage_view.id, c_glyphs, reg, &passage_ref);
	glyphset_t glyphset = {
		.glyphs = glyphs,
		.count = glyph_count,
	};
	if (Ent_HasComponent (passage_view.id, c_passage_glyphs, reg)) {
		// free the glyphs directly to avoid unnecessary motion in the
		// component pool (more for order preservation than it being overly
		// expensive);
		glyphset_t *gs = Ent_GetComponent (passage_view.id, c_passage_glyphs,
										   reg);
		free (gs->glyphs);
	}
	Ent_SetComponent (passage_view.id, c_passage_glyphs, reg, &glyphset);
	return passage_view;
}

view_t
Text_StringView (text_system_t textsys, view_t parent,
				 font_t *font, const char *str, uint32_t len,
				 script_component_t *sc, featureset_t *fs,
				 text_shaper_t *shaper)
{
	auto reg = textsys.reg;
	uint32_t c_glyphs = textsys.text_base + text_glyphs;
	uint32_t c_passage_glyphs = textsys.text_base + text_passage_glyphs;
	glyphnode_t *glyph_nodes = 0;
	glyphnode_t **head = &glyph_nodes;


	script_component_t script = {
		.script = sc ? sc->script : HB_SCRIPT_LATIN,
		.direction = sc ? sc->direction : text_right_down,
		.language = sc ? sc->language : hb_language_from_string ("en", 2),
	};
	bool string_vertical = !!(script.direction & 2);
	featureset_t text_features = DARRAY_STATIC_INIT (0);
	shaping_t   shaping = {
		.script = &script,
		.features = fs ? fs : &text_features,
		.font = font,
	};

	uint32_t    glyph_count = 0;

	auto shaped_glyphs = Shaper_ShapeText (shaper, &shaping, str, len);
	unsigned    c = shaped_glyphs.count;
	auto glyphInfo = shaped_glyphs.glyphInfo;
	auto glyphPos = shaped_glyphs.glyphPos;

	*head = alloca (sizeof (glyphnode_t));
	**head = (glyphnode_t) {
		.ent = nullent,
		.count = c,
		.glyphs = alloca (c * sizeof (glyphobj_t)),
		.mins = { 0, 0 },
		.maxs = { INT32_MIN, INT32_MIN },
	};
	glyph_count += c;
	layout_glyphs (*head, font, c, glyphInfo, glyphPos);

	head = &(*head)->next;

	ecs_system_t viewsys = { reg, textsys.view_base };
	view_t      stringview = View_New (viewsys, parent);
	glyphref_t  passage_ref = {};
	glyphobj_t *glyphs = malloc (glyph_count * sizeof (glyphobj_t));
	glyphnode_t *g = glyph_nodes;

	configure_textview (stringview, glyphs, g, passage_ref.count, c_glyphs,
						string_vertical, font);
	passage_ref.count += g->count;

	glyphset_t glyphset = {
		.glyphs = glyphs,
		.count = glyph_count,
	};
	Ent_SetComponent (stringview.id, c_passage_glyphs, reg, &glyphset);
	return stringview;
}

void
Text_SetScript (text_system_t textsys, uint32_t textid, const char *lang,
				hb_script_t script, text_dir_e dir)
{
	script_component_t scr = {
		.language = hb_language_from_string (lang, strlen (lang)),
		.script = script,
		.direction = dir,
	};
	Ent_SetComponent (textid, textsys.text_base + text_script, textsys.reg,
					  &scr);
}

void
Text_SetFont (text_system_t textsys, uint32_t textid, font_t *font)
{
	Ent_SetComponent (textid, textsys.text_base + text_font, textsys.reg,
					  &font);
}

void
Text_SetFeatures (text_system_t textsys, uint32_t textid,
				  featureset_t *features)
{
	uint32_t    features_comp = textsys.text_base + text_features;
	if (Ent_HasComponent (textid, features_comp, textsys.reg)) {
		Ent_RemoveComponent (textid, features_comp, textsys.reg);
	}
	featureset_t *f = Ent_SetComponent (textid, features_comp, textsys.reg, 0);
	DARRAY_RESIZE (f, features->size);
	memcpy (f->a, features->a, features->size * sizeof (hb_feature_t));
}

void
Text_AddFeature (text_system_t textsys, uint32_t textid, hb_feature_t feature)
{
	uint32_t    features_comp = textsys.text_base + text_features;
	if (!Ent_HasComponent (textid, features_comp, textsys.reg)) {
		Ent_SetComponent (textid, features_comp, textsys.reg, 0);
	}
	featureset_t *f = Ent_GetComponent (textid, features_comp, textsys.reg);
	DARRAY_APPEND (f, feature);
}
