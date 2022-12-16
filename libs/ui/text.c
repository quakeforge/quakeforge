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
#include "QF/ui/text.h"
#include "QF/ui/view.h"

#include "compat.h"

static void
text_features_create (void *_features)
{
	featureset_t *features = _features;
	*features = (featureset_t) DARRAY_STATIC_INIT (4);
}

static void
text_features_destroy (void *_features)
{
	featureset_t *features = _features;
	DARRAY_CLEAR (features);
}

const component_t text_components[text_comp_count] = {
	[text_passage_glyphs] = {
		.size = sizeof (glyphset_t),
		.name = "passage glyphs",
	},
	[text_glyphs] = {
		.size = sizeof (glyphref_t),
		.name = "glyphs",
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

static view_resize_f text_flow_funcs[] = {
	[text_right_down]   = view_flow_right_down,
	[text_left_down]    = view_flow_left_down,
	[text_down_right]   = view_flow_down_right,
	[text_up_right]     = view_flow_up_right,
	[text_right_up]     = view_flow_right_up,
	[text_left_up]      = view_flow_left_up,
	[text_down_left]    = view_flow_down_left,
	[text_up_left]      = view_flow_up_left,
};

view_t
Text_View (ecs_system_t viewsys, font_t *font, passage_t *passage)
{
	ecs_registry_t *reg = passage->reg;
	hierarchy_t *h = passage->hierarchy;
	psg_text_t *text_objects = h->components[passage_type_text_obj];
	glyphnode_t *glyph_nodes = 0;
	glyphnode_t **head = &glyph_nodes;

	hb_font_t  *fnt = hb_ft_font_create (font->face, 0);
	hb_buffer_t *buffer = hb_buffer_create ();
	hb_buffer_allocation_successful (buffer);

	hb_script_t psg_script = HB_SCRIPT_LATIN;
	text_dir_e psg_direction = text_right_down;
	hb_language_t psg_language = hb_language_from_string ("en", 2);
	featureset_t passage_features = DARRAY_STATIC_INIT (0);
	featureset_t *psg_features = &passage_features;

	uint32_t    glyph_count = 0;

	// passages have only the root, paragraphs, and text objects in their
	// hierarchies
	for (uint32_t i = 0; i < h->childCount[0]; i++) {
		uint32_t    paragraph = h->childIndex[0] + i;
		uint32_t    para_ent = h->ent[paragraph];

		hb_script_t para_script = psg_script;
		text_dir_e para_direction = psg_direction;
		hb_language_t para_language = psg_language;
		featureset_t *para_features = psg_features;;

		if (Ent_HasComponent (para_ent, text_script, reg)) {
			script_component_t *s = Ent_GetComponent (para_ent, text_script,
													  reg);
			para_script = s->script;
			para_language = s->language;
			para_direction = s->direction;
		}
		if (Ent_HasComponent (para_ent, text_features, reg)) {
			para_features = Ent_GetComponent (para_ent, text_features, reg);
		}
		for (uint32_t j = 0; j < h->childCount[paragraph]; j++) {
			uint32_t    textind = h->childIndex[paragraph] + j;
			uint32_t    text_ent = h->ent[textind];
			psg_text_t *textobj = &text_objects[textind];
			const char *str = passage->text + textobj->text;
			uint32_t    len = textobj->size;

			hb_script_t txt_script = para_script;
			text_dir_e txt_direction = para_direction;
			hb_language_t txt_language = para_language;
			featureset_t *txt_features = para_features;;

			if (Ent_HasComponent (text_ent, text_script, reg)) {
				script_component_t *s = Ent_GetComponent (text_ent, text_script,
														  reg);
				txt_script = s->script;
				txt_language = s->language;
				txt_direction = s->direction;
			}
			if (Ent_HasComponent (text_ent, text_features, reg)) {
				txt_features = Ent_GetComponent (text_ent, text_features, reg);
			}

			hb_buffer_reset (buffer);
			hb_buffer_set_direction (buffer, txt_direction | HB_DIRECTION_LTR);
			hb_buffer_set_script (buffer, txt_script);
			hb_buffer_set_language (buffer, txt_language);
			hb_buffer_add_utf8 (buffer, str, len, 0, len);
			hb_shape (fnt, buffer, txt_features->a, txt_features->size);

			unsigned    c;
			__auto_type glyphInfo = hb_buffer_get_glyph_infos (buffer, &c);
			__auto_type glyphPos = hb_buffer_get_glyph_positions (buffer, &c);

			*head = alloca (sizeof (glyphnode_t));
			**head = (glyphnode_t) {
				.ent = h->ent[textind],
				.count = c,
				.glyphs = alloca (c * sizeof (glyphobj_t)),
				.mins = { 0, 0 },
				.maxs = { INT32_MIN, INT32_MIN },
			};
			glyph_count += c;

			int         x = 0, y = 0;
			for (unsigned k = 0; k < c; k++) {
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
				(*head)->mins[0] = min ((*head)->mins[0], xp + x);
				(*head)->mins[1] = min ((*head)->mins[1], ym + y);
				(*head)->maxs[0] = max ((*head)->maxs[0], xm + x);
				(*head)->maxs[1] = max ((*head)->maxs[1], yp + y);

				(*head)->glyphs[k] = (glyphobj_t) {
					.glyphid = glyphid,
					.x = x + xp,
					.y = y - yp,
					.fontid = font->fontid,
				};
				x += xa;
				y += ya;
			}

			head = &(*head)->next;
		}
	}
	view_t      passage_view = View_AddToEntity (h->ent[0], viewsys, nullview);
	glyphref_t  passage_ref = {};
	glyphobj_t *glyphs = malloc (glyph_count * sizeof (glyphobj_t));
	glyphnode_t *g = glyph_nodes;
	// paragraph flow
	int         psg_vertical = !!(psg_direction & 2);
	for (uint32_t i = 0; i < h->childCount[0]; i++) {
		uint32_t    paragraph = h->childIndex[0] + i;
		view_t      paraview = View_AddToEntity (h->ent[paragraph], viewsys,
												 passage_view);
		glyphref_t  pararef = { .start = passage_ref.count };
		for (uint32_t j = 0; j < h->childCount[paragraph]; j++, g = g->next) {
			uint32_t    to = h->childIndex[paragraph] + j;
			view_t      textview = View_AddToEntity (h->ent[to], viewsys,
													 paraview);
			glyphref_t  glyph_ref = {
				.start = passage_ref.count,
				.count = g->count,
			};
			for (uint32_t k = 0; k < g->count; k++) {
				glyphobj_t *go = g->glyphs + k;
				glyphs[passage_ref.count + k] = (glyphobj_t) {
					.glyphid = go->glyphid,
					.x = go->x + g->mins[0],
					.y = go->y + g->maxs[1],
					.fontid = go->fontid,
				};
			}
			Ent_SetComponent (textview.id, text_glyphs, reg, &glyph_ref);
			View_SetPos (textview, g->mins[0], -g->mins[1]);
			View_SetLen (textview, g->maxs[0] - g->mins[0],
								   g->maxs[1] - g->mins[1]);
			View_SetGravity (textview, grav_flow);
			passage_ref.count += g->count;
		}
		pararef.count = passage_ref.count - pararef.start;
		Ent_SetComponent (paraview.id, text_glyphs, reg, &pararef);

		uint32_t    para_ent = h->ent[paragraph];
		text_dir_e para_direction = psg_direction;
		if (Ent_HasComponent (para_ent, text_script, reg)) {
			script_component_t *s = Ent_GetComponent (para_ent, text_script,
													  reg);
			para_direction = s->direction;
		}
		View_SetOnResize (paraview, text_flow_funcs[para_direction]);
		View_SetResize (paraview, !psg_vertical, psg_vertical);
		View_SetGravity (paraview, grav_northwest);
		View_Control (paraview)->flow_size = 1;
	}
	Ent_SetComponent (passage_view.id, text_glyphs, reg, &passage_ref);
	glyphset_t glyphset = {
		.glyphs = glyphs,
		.count = glyph_count,
	};
	Ent_SetComponent (passage_view.id, text_passage_glyphs, reg, &glyphset);
	return passage_view;
}

void
Text_SetScript (ecs_system_t textsys, uint32_t textid, const char *lang, hb_script_t script,
				text_dir_e dir)
{
	script_component_t scr = {
		.language = hb_language_from_string (lang, strlen (lang)),
		.script = script,
		.direction = dir,
	};
	Ent_SetComponent (textid, textsys.base + text_script, textsys.reg, &scr);
}

void
Text_SetFont (ecs_system_t textsys, uint32_t textid, font_t *font)
{
	Ent_SetComponent (textid, textsys.base + text_font, textsys.reg, &font);
}

void
Text_SetFeatures (ecs_system_t textsys, uint32_t textid, featureset_t *features)
{
	uint32_t    features_comp = textsys.base + text_features;
	if (Ent_HasComponent (textid, features_comp, textsys.reg)) {
		Ent_RemoveComponent (textid, features_comp, textsys.reg);
	}
	featureset_t *f = Ent_SetComponent (textid, features_comp, textsys.reg, 0);
	DARRAY_RESIZE (f, features->size);
	memcpy (f->a, features->a, features->size * sizeof (hb_feature_t));
}

void
Text_AddFeature (ecs_system_t textsys, uint32_t textid, hb_feature_t feature)
{
	uint32_t    features_comp = textsys.base + text_features;
	if (!Ent_HasComponent (textid, features_comp, textsys.reg)) {
		Ent_SetComponent (textid, features_comp, textsys.reg, 0);
	}
	featureset_t *f = Ent_GetComponent (textid, features_comp, textsys.reg);
	DARRAY_APPEND (f, feature);
}
