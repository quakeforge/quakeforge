/*
	passage.h

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

#ifndef __QF_ui_passage_h
#define __QF_ui_passage_h

#include <stdint.h>

/** \defgroup passage Text passages
	\ingroup utils
*/
///@{

enum {
	passage_type_text_obj,

	passage_type_count
};

typedef struct psg_text_s {
	/// beginning of text for this segment relative to passage_t.text
	uint32_t    text;
	/// length of text segment in bytes rather than chars as text may be utf-8
	uint32_t    size;
} psg_text_t;

typedef struct passage_s {
	const char *text;			///< Not owned by passage

	struct ecs_registry_s *reg;	///< Owning ECS registry
	struct hierarchy_s *hierarchy;	///< hierarchy of text objects
} passage_t;

void Passage_ParseText (passage_t *passage, const char *text);
passage_t *Passage_New (struct ecs_registry_s *reg);
void Passage_Delete (passage_t *passage);
int Passage_IsSpace (const char *text) __attribute__((pure));

///@}

#endif//__QF_ui_passage_h
