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

/** \defgroup segtext Segmented text files
	\ingroup utils

	Based on The OpenGL Shader Wrangler:
	https://prideout.net/blog/old/blog/index.html@p=11.html
	However, nothing is assumed about any of the segments: they are all nothing
	more than chunks of mostly random (lines cannot start with --) text with
	identifying tags. Also, the identifying tag represents only the shader_key
	of OpenGL Shader Wrangler's Effect.shader_key identifier.
*/
///@{

/** Represent a single text segment

	Segments are separated by lines beginning with "--" (without the quotes).

	The first word ([A-Za-z0-9_.]+) following the double-hyphen is used as an
	identifying tag for the segment. Whitespace and any additional characters
	are ignored.

	The segment itself starts on the following line.
*/
typedef struct segchunk_s {
	struct segchunk_s *next;
	const char *tag;		///< identifying tag or null if no tag
	const char *text;		///< nul-terminated string holding the segment
	int         start_line;	///< line number for first line of the segment
} segchunk_t;

/** Container for all the segments extracted from the provided text

	The first segment has no tag and may or may not be empty.
	Segments are stored sequentially in \a chunk_list are indexed by
	identifying tag (if present) in \a tab.
	Segments that have no identifying tag are not in \a tab, but can
	be accessed by walking \a chunk_list its \a next field.
*/
typedef struct segtext_s {
	struct segtext_s *next;
	segchunk_t *chunk_list;
	struct hashtab_s *tab;
} segtext_t;

/** Parse a nul-terminated string into (optionally) tagged segments

	The segments are separated by lines beginning with a double-hyphen ("--"),
	with an optional tag following the double-hyphen on the same line. Valid
	tag characters are ASCII alpha-numerics, "." and "_". Whitespace is ignored
	and invalid characters are treated as whitespace. Only the first tag on the
	line is significant.

	\param src	nul-terminated string to be parsed into segments
	\return		Pointer to container of parsed segments. Can be freed using
				Segtext_delete().
*/
segtext_t *Segtext_new (const char *src);
/** Free a segmented text container created by Segtext_new()

	\param st	Pointer to segmented text container to be freed. Must have been
				created by Segtext_new().
*/
void Segtext_delete (segtext_t *st);
/** Find a text segment via its identifying tag

	Like OpenGL Shader Wrangler, the segment tag does not have to be a perfect
	match with the specified tag: the segment with the longest string of
	sub-tags matching the leading subtags of the specified tag is selected.
	Tags are logically broken into sub-tag by the "." character. Thus in a
	collection of segments with tags "foo" and "foo.b", "foo.bar" will find the
	segment tagged "foo". However, if a segment tagged "foo.bar" is present, it
	will be returned. A segment tagged "foo.bar.baz" will not.

	\param st	Pointer to segmented text container
	\param tag	String specifying the tag identifying the segment to be
				returned. Valid tag characters are alphanumerics, "_" and "."
				(ie, [A-Za-z0-9._]). "." is used as a sub-tag separator for
				partial matches.
	\return		Pointer to segment data block with the best matching tag,
				providing access to the segment's tag and starting line number,
				as well as the segment contents. Null if no matching tag was
				found.
*/
const segchunk_t *Segtext_FindChunk (const segtext_t *st, const char *tag);

/** Find a text segment via its identifying tag

	Like OpenGL Shader Wrangler, the segment tag does not have to be a perfect
	match with the specified tag: the segment with the longest string of
	sub-tags matching the leading subtags of the specified tag is selected.
	Tags are logically broken into sub-tag by the "." character. Thus in a
	collection of segments with tags "foo" and "foo.b", "foo.bar" will find the
	segment tagged "foo". However, if a segment tagged "foo.bar" is present, it
	will be returned. A segment tagged "foo.bar.baz" will not.

	\param st	Pointer to segmented text container
	\param tag	String specifying the tag identifying the segment to be
				returned. Valid tag characters are alphanumerics, "_" and "."
				(ie, [A-Za-z0-9._]). "." is used as a sub-tag separator for
				partial matches.
	\return		Contents of the segment with the best matching tag, or null if
				no matching tag was found.
*/
const char *Segtext_Find (const segtext_t *st, const char *tag);

///@}

#endif//__QF_segtext_h
