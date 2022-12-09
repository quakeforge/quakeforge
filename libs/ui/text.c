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

static const component_t text_components[text_type_count] = {
	[text_href] = {
		.size = sizeof (hierref_t),
		.name = "href",
	},
	[text_passage] = {
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
	},
};
ecs_registry_t *text_reg;

void
Text_Init (void)
{
	text_reg = ECS_NewRegistry ();
	ECS_RegisterComponents (text_reg, text_components, text_type_count);
	text_reg->href_comp = text_href;
}

typedef struct glyphnode_s {
	struct glyphnode_s *next;
	uint32_t    ent;
	uint32_t    count;
	glyphobj_t *glyphs;
	int         mins[2];
	int         maxs[2];
} glyphnode_t;

view_t
Text_View (font_t *font, passage_t *passage)
{
	hierarchy_t *h = passage->hierarchy;
	psg_text_t *text_objects = h->components[passage_type_text_obj];
	glyphnode_t *glyph_nodes = 0;
	glyphnode_t **head = &glyph_nodes;

	hb_font_t  *fnt = hb_ft_font_create (font->face, 0);
	hb_buffer_t *buffer = hb_buffer_create ();
	hb_buffer_allocation_successful (buffer);

	/*XXX*/hb_script_t script = HB_SCRIPT_LATIN;
	/*XXX*/hb_direction_t direction = HB_DIRECTION_LTR;
	/*XXX*/hb_language_t language = hb_language_from_string ("en", 2);

	uint32_t    glyph_count = 0;

	// passages have only the root, paragraphs, and text objects in their
	// hierarchies
	for (uint32_t i = 0; i < h->childCount[0]; i++) {
		uint32_t    paragraph = h->childIndex[0] + i;
		for (uint32_t j = 0; j < h->childCount[paragraph]; j++) {
			uint32_t    textind = h->childIndex[paragraph] + j;
			psg_text_t *textobj = &text_objects[textind];
			const char *str = passage->text + textobj->text;
			uint32_t    len = textobj->size;

			hb_buffer_reset (buffer);
			hb_buffer_set_direction (buffer, direction);
			hb_buffer_set_script (buffer, script);
			hb_buffer_set_language (buffer, language);
			hb_buffer_add_utf8 (buffer, str, len, 0, len);
			hb_shape (fnt, buffer, 0, 0);

			unsigned    c;
			__auto_type glyphInfo = hb_buffer_get_glyph_infos (buffer, &c);
			__auto_type glyphPos = hb_buffer_get_glyph_positions (buffer, &c);

			*head = alloca (sizeof (glyphnode_t));
			**head = (glyphnode_t) {
				.ent = h->ent[textind],
				.count = c,
				.glyphs = alloca (c * sizeof (glyphobj_t)),
			};
			glyph_count += c;

			int         x = 0, y = 0;
			for (unsigned k = 0; k < c; k++) {
				uint32_t    glyphid = glyphInfo[k].codepoint;
				vec2i_t     bearing = font->glyph_bearings[glyphid];

				int         xp = x + glyphPos[k].x_offset / 64;
				int         yp = y + glyphPos[k].y_offset / 64;
				int         xa = glyphPos[k].x_advance / 64;
				int         ya = glyphPos[k].y_advance / 64;
				xp += bearing[0];
				yp -= bearing[1];
				int         xm = xp + font->glyph_rects[glyphid].width;
				int         ym = yp + font->glyph_rects[glyphid].height;

				(*head)->mins[0] = min ((*head)->mins[0], xp);
				(*head)->mins[1] = min ((*head)->mins[1], yp);
				(*head)->maxs[0] = max ((*head)->maxs[0], xm);
				(*head)->maxs[1] = max ((*head)->maxs[1], ym);

				(*head)->glyphs[k] = (glyphobj_t) {
					.glyphid = glyphid,
					.x = xp + bearing[0],
					.y = yp - bearing[1],
					.fontid = font->fontid,
				};
				x += xa;
				y += ya;
			}

			head = &(*head)->next;
		}
	}
	view_t      passage_view = View_New (text_reg, nullview);
	glyphref_t  passage_ref = {};
	glyphobj_t *glyphs = malloc (glyph_count * sizeof (glyphobj_t));
	glyphnode_t *g = glyph_nodes;
	for (uint32_t i = 0; i < h->childCount[0]; i++) {
		uint32_t    paragraph = h->childIndex[0] + i;
		view_t      paraview = View_New (text_reg, passage_view);
		glyphref_t  pararef = { .start = passage_ref.count };
		for (uint32_t j = 0; j < h->childCount[paragraph]; j++, g = g->next) {
			view_t      textview = View_New (text_reg, paraview);
			glyphref_t  glyph_ref = {
				.start = passage_ref.count,
				.count = g->count,
			};
			memcpy (glyphs + passage_ref.count, g->glyphs,
					g->count * sizeof (glyphobj_t));
			Ent_SetComponent (textview.id, text_glyphs, text_reg, &glyph_ref);
			View_SetPos (textview, g->mins[0], g->mins[1]);
			View_SetLen (textview, g->maxs[0] - g->mins[0],
								   g->maxs[1] - g->mins[1]);
			View_SetGravity (textview, grav_flow);
			passage_ref.count += g->count;
		}
		pararef.count = passage_ref.count - pararef.start;
		Ent_SetComponent (paraview.id, text_glyphs, text_reg, &pararef);
	}
	Ent_SetComponent (passage_view.id, text_glyphs, text_reg, &passage_ref);
	glyphset_t glyphset = {
		.glyphs = glyphs,
		.count = glyph_count,
	};
	Ent_SetComponent (passage_view.id, text_passage, text_reg, &glyphset);
	return passage_view;
}

shaper_t *
Text_NewShaper (font_t *font)
{
	shaper_t   *shaper = malloc (sizeof (shaper_t));
	shaper->rfont = font;
	shaper->font = hb_ft_font_create (font->face, 0);
	hb_ft_font_set_funcs (shaper->font);
	shaper->buffer = hb_buffer_create ();
	shaper->features = (featureset_t) DARRAY_STATIC_INIT (4);

	hb_buffer_allocation_successful (shaper->buffer);
	return shaper;
}

void
Text_DeleteShaper (shaper_t *shaper)
{
	hb_buffer_destroy (shaper->buffer);
	hb_font_destroy (shaper->font);
	DARRAY_CLEAR (&shaper->features);
	free (shaper);
}

void
Text_AddFeature (shaper_t *shaper, hb_feature_t feature)
{
	DARRAY_APPEND (&shaper->features, feature);
}

void
Text_ShapeText (shaper_t *shaper, text_t *text)
{
	hb_buffer_reset (shaper->buffer);
	hb_buffer_set_direction (shaper->buffer, text->direction);
	hb_buffer_set_script (shaper->buffer, text->script);
	const char *lang = text->language;
	hb_buffer_set_language (shaper->buffer,
							hb_language_from_string (lang, strlen (lang)));
	size_t      length = strlen (text->text);
	hb_buffer_add_utf8 (shaper->buffer, text->text, length, 0, length);

	hb_shape (shaper->font, shaper->buffer,
			  shaper->features.a, shaper->features.size);
}

void
Text_RenderText (shaper_t *shaper, text_t *text, int x, int y,
				 text_render_t *render, void *data)
{
	Text_ShapeText (shaper, text);

	unsigned    count;
	__auto_type glyphInfo = hb_buffer_get_glyph_infos (shaper->buffer, &count);
	__auto_type glyphPos = hb_buffer_get_glyph_positions (shaper->buffer,
														  &count);

	font_t     *font = shaper->rfont;
	for (unsigned i = 0; i < count; i++) {
		vec2i_t     bearing = font->glyph_bearings[glyphInfo[i].codepoint];

		int         xp = x + glyphPos[i].x_offset / 64;
		int         yp = y + glyphPos[i].y_offset / 64;
		int         xa = glyphPos[i].x_advance / 64;
		int         ya = glyphPos[i].y_advance / 64;

		xp += bearing[0];
		yp -= bearing[1];
		render (glyphInfo[i].codepoint, xp, yp, data);
		x += xa;
		y += ya;
	}
}
