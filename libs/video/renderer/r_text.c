/*
	r_text.c

	Renderer font text management

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

#include "QF/ui/font.h"

#include "r_text.h"

#include "compat.h"

rshaper_t *
RText_NewShaper (font_t *font)
{
	rshaper_t  *shaper = malloc (sizeof (rshaper_t));
	shaper->rfont = font;
	shaper->font = hb_ft_font_create (font->face, 0);
	hb_ft_font_set_funcs (shaper->font);
	shaper->buffer = hb_buffer_create ();
	shaper->features = (r_hb_featureset_t) DARRAY_STATIC_INIT (4);

	hb_buffer_allocation_successful (shaper->buffer);
	return shaper;
}

void
RText_DeleteShaper (rshaper_t *shaper)
{
	hb_buffer_destroy (shaper->buffer);
	hb_font_destroy (shaper->font);
	DARRAY_CLEAR (&shaper->features);
	free (shaper);
}

void
RText_AddFeature (rshaper_t *shaper, hb_feature_t feature)
{
	DARRAY_APPEND (&shaper->features, feature);
}

void
RText_ShapeText (rshaper_t *shaper, rtext_t *text)
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
RText_RenderText (rshaper_t *shaper, rtext_t *text, int x, int y,
				  rtext_render_t *render, void *data)
{
	RText_ShapeText (shaper, text);

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
