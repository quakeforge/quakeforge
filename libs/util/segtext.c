/*
	segtext.c

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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <ctype.h>

#include "QF/alloc.h"
#include "QF/hash.h"
#include "QF/qtypes.h"
#include "QF/segtext.h"

ALLOC_STATE (segchunk_t, chunks);
ALLOC_STATE (segtext_t, texts);

static segchunk_t *
new_chunk (void)
{
	segchunk_t *chunk;
	ALLOC (64, segchunk_t, chunks, chunk);
	return chunk;
}

static void
delete_chunk (segchunk_t *chunk)
{
	FREE (chunks, chunk);
}

static segtext_t *
new_text (void)
{
	segtext_t  *text;
	ALLOC (64, segtext_t, texts, text);
	return text;
}

static void
delete_text (segtext_t *text)
{
	FREE (texts, text);
}

static const char *
segtext_getkey (const void *sc, void *unused)
{
	segchunk_t *chunk = (segchunk_t *) sc;
	if (!chunk->tag)
		return "";
	return chunk->tag;
}

static char *
next_line (char *src, int *line)
{
	while (*src && *src != '\n')
		src++;
	if (*src == '\n') {
		*line += 1;
		src++;
	}
	return src;
}

static char *
next_chunk (char *src, int *line)
{
	while (*src) {
		// chunks begin on a line that starts with --
		if (src[0] == '-' && src[1] == '-')
			return src;
		src = next_line (src, line);
	}
	// no next chunk
	return 0;
}

static int
is_tag_char (char ch)
{
	return isalnum ((byte) ch) || ch == '.' || ch == '_';
}

static char *
find_tag (const char *src)
{
	const char *end;
	char       *tag;

	while (*src && *src != '\n' && !is_tag_char (*src))
		src++;

	for (end = src; is_tag_char (*end); end++)
		;
	if (end == src)
		return 0;

	tag = malloc (end - src + 1);
	strncpy (tag, src, end - src);
	tag[end - src] = 0;
	return tag;
}

VISIBLE segtext_t *
Segtext_new (const char *source_string)
{
	segchunk_t **chunk;
	segtext_t  *text;
	char       *src;
	int         line = 1;

	if (!source_string)
		return 0;
	text = new_text ();
	text->tab = Hash_NewTable (61, segtext_getkey, 0, 0, 0);

	src = strdup (source_string);
	// The first chunk is special in that it holds the pointer to the copied
	// string data. The chunk itself has no name and may be empty.
	chunk = &text->chunk_list;
	*chunk = new_chunk ();
	(*chunk)->tag = 0;
	(*chunk)->text = src;
	(*chunk)->start_line = line;

	chunk = &(*chunk)->next;

	while ((src = next_chunk (src, &line))) {
		*src++ = 0;							// terminate the previous chunk
		*chunk = new_chunk ();
		(*chunk)->tag = find_tag (src);
		src = next_line (src, &line);
		(*chunk)->text = src;
		(*chunk)->start_line = line;
		// If tags are duplicated, the first one takes precedence
		if ((*chunk)->tag && !Hash_Find (text->tab, (*chunk)->tag))
			Hash_Add (text->tab, *chunk);
		chunk = &(*chunk)->next;
	}
	return text;
}

VISIBLE void
Segtext_delete (segtext_t *st)
{
	segchunk_t *chunk;

	// the first chunk holds the pointer to the block holding all chunk data;
	free ((char *) st->chunk_list->text);
	while (st->chunk_list) {
		chunk = st->chunk_list;
		st->chunk_list = chunk->next;
		if (chunk->tag) {
			free ((char *) chunk->tag);
		}
		delete_chunk (chunk);
	}
	Hash_DelTable (st->tab);
	delete_text (st);
}

const segchunk_t *
Segtext_FindChunk (const segtext_t *st, const char *tag)
{
	char       *sub;
	char       *dot;
	segchunk_t *chunk;

	sub = alloca (strlen (tag) + 1);
	strcpy (sub, tag);
	do {
		chunk = Hash_Find (st->tab, sub);
		if (chunk)
			return chunk;
		dot = strrchr (sub, '.');
		if (dot)
			*dot = 0;
	} while (dot);
	return 0;
}

const char *
Segtext_Find (const segtext_t *st, const char *tag)
{
	const segchunk_t *chunk = Segtext_FindChunk (st, tag);
	if (chunk)
		return chunk->text;
	return 0;
}

static void
segtext_shutdown (void *data)
{
	ALLOC_FREE_BLOCKS (chunks);
	ALLOC_FREE_BLOCKS (texts);
}

static void __attribute__((constructor))
segtext_init (void)
{
	Sys_RegisterShutdown (segtext_shutdown, 0);
}
