/*
	passage.c

	Text passage formatting.

	Copyright (C) 2022  Bill Currie <bill@taniwha.org>

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

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/alloc.h"
#include "QF/qtypes.h"
#include "QF/sys.h"

#include "QF/ecs.h"

#include "QF/ui/passage.h"
#include "QF/ui/view.h"

const component_t passage_components[passage_comp_count] = {
	[passage_href] = {
		.size = sizeof (hierref_t),
		.name = "passage href",
		.destroy = Hierref_DestroyComponent,
	},
};

static const component_t passage_type_components[passage_type_count] = {
	[passage_type_text_obj] = {
		.size = sizeof (psg_text_t),
		.name = "Text",
	},
};

static const hierarchy_type_t passage_type = {
	.num_components = passage_type_count,
	.components = passage_type_components,
};

VISIBLE int
Passage_IsSpace (const char *text)
{
	if (text[0] == ' ') {
		return 1;
	}
	// 2002;EN SPACE;Zs;0;WS;<compat> 0020;;;;N;;;;;
	// 2003;EM SPACE;Zs;0;WS;<compat> 0020;;;;N;;;;;
	// 2004;THREE-PER-EM SPACE;Zs;0;WS;<compat> 0020;;;;N;;;;;
	// 2005;FOUR-PER-EM SPACE;Zs;0;WS;<compat> 0020;;;;N;;;;;
	// 2006;SIX-PER-EM SPACE;Zs;0;WS;<compat> 0020;;;;N;;;;;
	// 2008;PUNCTUATION SPACE;Zs;0;WS;<compat> 0020;;;;N;;;;;
	// 2009;THIN SPACE;Zs;0;WS;<compat> 0020;;;;N;;;;;
	// 200A;HAIR SPACE;Zs;0;WS;<compat> 0020;;;;N;;;;;
	if ((byte)text[0] == 0xe2 && (byte)text[1] == 0x80
		&& ((byte)text[2] >= 0x80 && (byte)text[2] < 0x90
			&& ((1 << (text[2] & 0xf)) & 0x077c))) {
		return 3;
	}
	// 205F;MEDIUM MATHEMATICAL SPACE;Zs;0;WS;<compat> 0020;;;;N;;;;;
	if ((byte)text[0] == 0xe2 && (byte)text[1] == 0x81
		&& (byte)text[2] == 0x9f) {
		return 3;
	}
	return 0;
}

static void
add_entity (uint32_t hent, uint32_t parent, ecs_registry_t *reg)
{
	hierarchy_t *h = Ent_GetComponent (hent, ecs_hierarchy, reg);
	auto ref = Hierref_InsertHierarchy ((hierref_t) { hent, parent},
										(hierref_t) { nullent, nullindex },
										reg);
	h->ent[ref.index] = ECS_NewEntity (reg);
	h->own[ref.index] = true;
	Ent_SetComponent (h->ent[ref.index], h->href_comp, reg, &ref);
}

VISIBLE void
Passage_ParseText (passage_t *passage, const char *text)
{
	auto reg = passage->reg;
	//FIXME just empty hierarchy
	ECS_DelEntity (reg, passage->hierarchy);
	if (!*text) {
		return;
	}
	passage->text = text;

	unsigned    num_paragraphs = 1;
	unsigned    num_text_objects = 1;
	psg_text_t  root_text = {};
	int         parsing_space = Passage_IsSpace (text);
	for (const char *c = text; *c; c++) {
		int         size;
		if ((size = Passage_IsSpace (c))) {
			if (!parsing_space) {
				num_text_objects++;
			}
			parsing_space = 1;
			c += size - 1;
		} else if (*c == '\n') {
			if (c[1]) {
				num_paragraphs++;
				num_text_objects += !Passage_IsSpace (c + 1);
			}
		} else {
			if (parsing_space) {
				num_text_objects++;
			}
			parsing_space = 0;
		}
		root_text.size = c - text;
	}
	passage->hierarchy = Hierarchy_New (passage->reg,
										passage->comp_base + passage_href,
										&passage_type, 0);
	hierarchy_t *h = Ent_GetComponent (passage->hierarchy, ecs_hierarchy, reg);
	Hierarchy_Reserve (h, 1 + num_paragraphs + num_text_objects);
#if 0
	printf ("num_paragraphs %d, num_text_objects %d\n", num_paragraphs,
			passage->num_text_objects);
#endif
	add_entity (passage->hierarchy, nullent, reg);
	for (unsigned i = 0; i < num_paragraphs; i++) {
		add_entity (passage->hierarchy, 0, reg);
	}

	num_paragraphs = 0;
	psg_text_t *passage_obj = h->components[passage_type_text_obj];
	psg_text_t *par_obj = &passage_obj[h->childIndex[0]];
	psg_text_t *text_obj = &passage_obj[h->childIndex[1]];
	*par_obj = *text_obj = (psg_text_t) { };

	*passage_obj = root_text;
	add_entity (passage->hierarchy, par_obj - passage_obj, reg);

	parsing_space = Passage_IsSpace (text);
	for (const char *c = text; *c; c++) {
		int         size;
		if ((size = Passage_IsSpace (c))) {
			if (!parsing_space) {
				add_entity (passage->hierarchy, par_obj - passage_obj, reg);
				text_obj->size = c - text - text_obj->text;
				(++text_obj)->text = c - text;
			}
			parsing_space = 1;
			c += size - 1;
		} else if (*c == '\n') {
			text_obj->size = c - text - text_obj->text;
			par_obj->size = c - text - par_obj->text;
			if (c[1]) {
				(++par_obj)->text = c + 1 - text;
				add_entity (passage->hierarchy, par_obj - passage_obj, reg);
				(++text_obj)->text = c + 1 - text;
				parsing_space = Passage_IsSpace (c + 1);
			}
		} else {
			if (parsing_space) {
				add_entity (passage->hierarchy, par_obj - passage_obj, reg);
				text_obj->size = c - text - text_obj->text;
				(++text_obj)->text = c - text;
			}
			parsing_space = 0;
			if (!c[1]) {
				text_obj->size = c + 1 - text - text_obj->text;
				par_obj->size = c + 1 - text - par_obj->text;
				passage_obj->size = c + 1 - text - passage_obj->text;
			}
		}
	}
#if 0
	for (uint32_t i = 0; i < h->num_objects; i++) {
		psg_text_t *to = &passage_obj[i];
		uint32_t    ent = h->ent[i];
		hierref_t  *ref = Ent_GetComponent (ent, h->href_comp, reg);
		printf ("%3d %8x %3d %4d %4d '%.*s'\n", i, ent, ref->index,
				to->text, to->size, to->size, text + to->text);
	}
#endif
}

VISIBLE passage_t *
Passage_New (ecs_system_t passage_sys)
{
	passage_t  *passage = malloc (sizeof (passage_t));
	*passage = (passage_t) {
		.reg = passage_sys.reg,
		.comp_base = passage_sys.base,
		.hierarchy = nullent,
	};
	return passage;
}

VISIBLE void
Passage_Delete (passage_t *passage)
{
	auto reg = passage->reg;
	ECS_DelEntity (reg, passage->hierarchy);
	free (passage);
}
