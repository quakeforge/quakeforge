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

#include "QF/ui/view.h"
#include "QF/ui/passage.h"

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
add_text_view (view_t *paragraph_view, psg_text_t *text_object, int suppress)
{
	view_t     *text_view = view_new (0, 0, 0, 0, grav_flow);
	text_view->data = text_object;
	text_view->bol_suppress = suppress;
	view_add (paragraph_view, text_view);
}

VISIBLE passage_t *
Passage_ParseText (const char *text)
{
	passage_t  *passage = malloc (sizeof (passage_t));
	passage->text = text;
	passage->num_text_objects = 0;
	passage->view = view_new (0, 0, 10, 10, grav_northwest);
	passage->text_objects = 0;

	if (!*text) {
		return passage;
	}

	unsigned    num_paragraphs = 1;
	int         parsing_space = Passage_IsSpace (text);
	passage->num_text_objects = 1;
	for (const char *c = text; *c; c++) {
		int         size;
		if ((size = Passage_IsSpace (c))) {
			if (!parsing_space) {
				passage->num_text_objects++;
			}
			parsing_space = 1;
			c += size - 1;
		} else if (*c == '\n') {
			if (c[1]) {
				num_paragraphs++;
				passage->num_text_objects += !Passage_IsSpace (c + 1);
			}
		} else {
			if (parsing_space) {
				passage->num_text_objects++;
			}
			parsing_space = 0;
		}
	}
#if 0
	printf ("num_paragraphs %d, num_text_objects %d\n", num_paragraphs,
			passage->num_text_objects);
#endif
	passage->text_objects = malloc (passage->num_text_objects
									* sizeof (psg_text_t));
	for (unsigned i = 0; i < num_paragraphs; i++) {
		view_t     *view = view_new (0, 0, 10, 10, grav_northwest);
		view->flow_size = 1;
		view_add (passage->view, view);
	}

	num_paragraphs = 0;
	parsing_space = Passage_IsSpace (text);
	psg_text_t *text_object = passage->text_objects;
	text_object->text = 0;
	text_object->size = 0;
	view_t     *paragraph_view = passage->view->children[num_paragraphs++];
	add_text_view (paragraph_view, text_object, parsing_space);
	for (const char *c = text; *c; c++) {
		int         size;
		if ((size = Passage_IsSpace (c))) {
			if (!parsing_space) {
				text_object->size = c - text - text_object->text;
				(++text_object)->text = c - text;
				add_text_view (paragraph_view, text_object, 1);
			}
			parsing_space = 1;
			c += size - 1;
		} else if (*c == '\n') {
			text_object->size = c - text - text_object->text;
			if (c[1]) {
				(++text_object)->text = c + 1 - text;
				paragraph_view = passage->view->children[num_paragraphs++];
				add_text_view (paragraph_view, text_object, 0);
				parsing_space = Passage_IsSpace (c + 1);
			}
		} else {
			if (parsing_space) {
				text_object->size = c - text - text_object->text;
				(++text_object)->text = c - text;
				add_text_view (paragraph_view, text_object, 0);
			}
			parsing_space = 0;
			if (!c[1]) {
				text_object->size = c + 1 - text - text_object->text;
			}
		}
	}
#if 0
	for (int i = 0; i < passage->view->num_children; i++) {
		paragraph_view = passage->view->children[i];
		for (int j = 0; j < paragraph_view->num_children; j++) {
			view_t     *text_view = paragraph_view->children[j];
			psg_text_t *to = text_view->data;
			printf ("%3d %3d %d %4d %4d '%.*s'\n", i, j,
					text_view->bol_suppress,
					to->text, to->size, to->size, text + to->text);
		}
	}
#endif
	return passage;
}

VISIBLE void
Passage_Delete (passage_t *passage)
{
	if (passage->view) {
		view_delete (passage->view);
	}
	free (passage->text_objects);
	free (passage);
}
