/*
	segtext.h

	Segmented text file handling

	Copyright (C) 2013 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2013/02/26

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

#ifndef __QF_segtext_h
#define __QF_segtext_h

/** \defgroup segtext Segmented text files.
	\ingroup utils

	Based on The OpenGL Shader Wrangler: http://prideout.net/blog/?p=11
*/

typedef struct segchunk_s {
	struct segchunk_s *next;
	const char *tag;
	const char *text;
	int         start_line;
} segchunk_t;

typedef struct segtext_s {
	struct segtext_s *next;
	segchunk_t *chunk_list;
	struct hashtab_s *tab;
} segtext_t;

segtext_t *Segtext_new (const char *src);
void Segtext_delete (segtext_t *st);
const segchunk_t *Segtext_FindChunk (const segtext_t *st, const char *tag);
const char *Segtext_Find (const segtext_t *st, const char *tag);

#endif//__QF_segtext_h
