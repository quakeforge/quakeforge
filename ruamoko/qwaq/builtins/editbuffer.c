/*
	editbuffer.c

	High level wrapper for TextBuffer_Destroy

	Copyright (C) 2020 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2020/03/22

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
#include <stdlib.h>
#include <string.h>

#include "QF/progs.h"
#include "QF/ruamoko.h"
#include "QF/quakeio.h"

#include "QF/ui/txtbuffer.h"

#include "ruamoko/qwaq/qwaq.h"
#include "ruamoko/qwaq/editor/editbuffer.h"

#define always_inline inline __attribute__((__always_inline__))

typedef struct editbuffer_s {
	struct editbuffer_s *next;
	struct editbuffer_s **prev;
	txtbuffer_t *txtbuffer;
	int         modified;
	int         tabSize;
} editbuffer_t;

typedef struct qwaq_ebresources_s {
	progs_t    *pr;
	PR_RESMAP (editbuffer_t) buffers;
	editbuffer_t *buffer_list;
} qwaq_ebresources_t;

static editbuffer_t *
editbuffer_new (qwaq_ebresources_t *res)
{
	editbuffer_t *buffer = PR_RESNEW (res->buffers);
	buffer->next = res->buffer_list;
	buffer->prev = &res->buffer_list;
	if (res->buffer_list) {
		res->buffer_list->prev = &buffer->next;
	}
	res->buffer_list = buffer;
	return buffer;
}

static void
editbuffer_free (qwaq_ebresources_t *res, editbuffer_t *buffer)
{
	if (buffer->next) {
		buffer->next->prev = buffer->prev;
	}
	*buffer->prev = buffer->next;
	PR_RESFREE (res->buffers, buffer);
}

static void
editbuffer_reset (qwaq_ebresources_t *res)
{
	PR_RESRESET (res->buffers);
}

static inline editbuffer_t *
editbuffer_get (qwaq_ebresources_t *res, unsigned index)
{
	return PR_RESGET (res->buffers, index);
}

static inline int __attribute__((pure))
editbuffer_index (qwaq_ebresources_t *res, editbuffer_t *buffer)
{
	return PR_RESINDEX (res->buffers, buffer);
}

static always_inline editbuffer_t * __attribute__((pure))
get_editbuffer (qwaq_ebresources_t *res, const char *name, int handle)
{
	editbuffer_t *buffer = editbuffer_get (res, handle);

	if (!buffer || !buffer->txtbuffer) {
		PR_RunError (res->pr, "invalid edit buffer passed to %s", name + 3);
	}
	return buffer;
}

static always_inline int
isword (unsigned char c)
{
	return c >= 128 || c == '_' || isalnum(c);
}

static always_inline unsigned
spanGap (txtbuffer_t *buffer, unsigned ptr)
{
	return ptr < buffer->gapOffset ? ptr : ptr + buffer->gapSize;
}

static always_inline char
getChar (txtbuffer_t *buffer, unsigned ptr)
{
	return buffer->text[spanGap (buffer, ptr)];
}

static always_inline unsigned
nextChar (txtbuffer_t *buffer, unsigned ptr)
{
	if (ptr < buffer->textSize && getChar (buffer, ptr) != '\n') {
		return ptr + 1;
	}
	return ptr;
}

static always_inline unsigned
prevChar (txtbuffer_t *buffer, unsigned ptr)
{
	if (ptr > 0 && getChar (buffer, ptr - 1) != '\n') {
		return ptr - 1;
	}
	return ptr;
}

static always_inline unsigned __attribute__((pure))
nextNonSpace (txtbuffer_t *buffer, unsigned ptr)
{
	while (ptr < buffer->textSize) {
		char    c = getChar (buffer, ptr);
		if (!isspace (c) || c == '\n') {
			break;
		}
		ptr++;
	}
	return ptr;
}

static always_inline unsigned
prevNonSpace (txtbuffer_t *buffer, unsigned ptr)
{
	while (ptr > 0) {
		char    c = getChar (buffer, ptr - 1);
		if (!isspace (c) || c == '\n') {
			break;
		}
		ptr--;
	}
	return ptr;
}

static always_inline unsigned __attribute__((pure))
nextWord (txtbuffer_t *buffer, unsigned ptr)
{
	if (buffer->textSize && ptr < buffer->textSize - 1) {
		while (ptr < buffer->textSize) {
			char        c = getChar (buffer, ptr);
			if (!isword (c)) {
				break;
			}
			ptr++;
		}
		while (ptr < buffer->textSize) {
			char        c = getChar (buffer, ptr);
			if (c == '\n') {
				return ptr;
			}
			if (isword (c)) {
				break;
			}
			ptr++;
		}
	}
	return ptr;
}

static always_inline unsigned
prevWord (txtbuffer_t *buffer, unsigned ptr)
{
	if (ptr > 0) {
		while (ptr > 0) {
			char        c = getChar (buffer, ptr - 1);
			if (c == '\n') {
				return ptr;
			}
			if (isword (c)) {
				break;
			}
			ptr--;
		}
		while (ptr > 0) {
			char        c = getChar (buffer, ptr - 1);
			if (c == '\n') {
				return ptr;
			}
			if (!isword (c)) {
				break;
			}
			ptr--;
		}
	}
	return ptr;
}

static always_inline unsigned __attribute__((pure))
nextLine (txtbuffer_t *buffer, unsigned ptr)
{
	unsigned    oldptr = ptr;
	while (ptr < buffer->textSize && getChar (buffer, ptr++) != '\n') {
	}
	if (ptr == buffer->textSize && ptr > 0
		&& getChar (buffer, ptr - 1) != '\n') {
		return oldptr;
	}
	return ptr;
}

static always_inline unsigned
prevLine (txtbuffer_t *buffer, unsigned ptr)
{
	if (ptr) {
		ptr--;
		while (ptr > 0 && getChar (buffer, ptr - 1) != '\n') {
			ptr--;
		}
	}
	return ptr;
}

static always_inline unsigned
charPos (txtbuffer_t *buffer, unsigned ptr, unsigned target, int tabSize)
{
	unsigned    pos = 0;

	while (ptr < target) {
		if (getChar (buffer, ptr) == '\t') {
			pos += tabSize - (pos % tabSize) - 1;	// -1 for ++
		}
		pos++;
		ptr++;
	}
	return pos;
}

static always_inline unsigned __attribute__((pure))
charPtr (txtbuffer_t *buffer, unsigned ptr, unsigned target, int tabSize)
{
	unsigned    pos = 0;
	while (pos < target && ptr < buffer->textSize
		   && getChar (buffer, ptr) != '\n') {
		if (getChar (buffer, ptr) == '\t') {
			pos += tabSize - (pos % tabSize) - 1;	// -1 for ++
		}
		pos++;
		ptr++;
	}
	if (pos > target) {
		ptr--;
	}
	return ptr;
}

static always_inline unsigned __attribute__((pure))
getEOW (txtbuffer_t *buffer, unsigned ptr)
{
	while (ptr < buffer->textSize) {
		char        c = getChar (buffer, ptr);
		if (!isword (c)) {
			break;
		}
		ptr++;
	}
	return ptr;
}

static always_inline unsigned
getBOW (txtbuffer_t *buffer, unsigned ptr)
{
	while (ptr > 0) {
		char        c = getChar (buffer, ptr - 1);
		if (!isword (c)) {
			break;
		}
		ptr--;
	}
	return ptr;
}

static always_inline unsigned __attribute__((pure))
getEOL (txtbuffer_t *buffer, unsigned ptr)
{
	while (ptr < buffer->textSize) {
		char        c = getChar (buffer, ptr);
		if (c == '\n') {
			break;
		}
		ptr++;
	}
	return ptr;
}

static always_inline void
readString (txtbuffer_t *buffer, eb_sel_t *sel, char *str)
{
	unsigned    start = sel->start;
	unsigned    length = sel->length;
	unsigned    end = sel->start + sel->length;
	const char *ptr = buffer->text + spanGap (buffer, start);
	if ((start < buffer->gapOffset && end <= buffer->gapOffset)
		|| start > buffer->gapOffset) {
		memcpy (str, ptr, length);
	} else {
		length = buffer->gapOffset - start;
		memcpy (str, ptr, length);
		str += length;
		ptr += length + buffer->gapOffset;
		memcpy (str, ptr, sel->length - length);
	}
}

static always_inline unsigned
getBOL (txtbuffer_t *buffer, unsigned ptr)
{
	while (ptr > 0) {
		char        c = getChar (buffer, ptr - 1);
		if (c == '\n') {
			break;
		}
		ptr--;
	}
	return ptr;
}

static always_inline unsigned
_countLines (txtbuffer_t *buffer, unsigned start, unsigned len)
{
	unsigned    count = 0;
	char       *ptr = buffer->text + start;

	while (len-- > 0) {
		count += *ptr++ == '\n';
	}
	return count;
}

static always_inline unsigned
countLines (txtbuffer_t *buffer, unsigned start, unsigned len)
{
	if (start < buffer->gapOffset) {
		if (start + len <= buffer->gapOffset) {
			return _countLines (buffer, start, len);
		} else {
			return _countLines (buffer, start, buffer->gapOffset - start)
				 + _countLines (buffer, buffer->gapOffset + buffer->gapSize,
								len - (buffer->gapOffset - start));
		}
	} else {
		return _countLines (buffer, start + buffer->gapOffset, len);
	}
}

static const char *
_search (txtbuffer_t *buffer, const char *ptr, unsigned len,
		 const char *str, unsigned slen, int dir,
		 int (*cmp)(const char *, const char *, size_t))
{
	while (len-- > 0) {
		if (*ptr == *str) {
			unsigned    offset = ptr - buffer->text;
			if (offset > buffer->gapOffset
				|| offset + slen < buffer->gapOffset) {
				// search does not span gap
				if (cmp (ptr, str, slen) == 0) {
					return ptr;
				}
			} else {
				// search spans gap
				unsigned    l = buffer->gapOffset - offset;
				if (cmp (ptr, str, l) == 0
					&& cmp (ptr + l + buffer->gapSize,
							str + l, slen - l) == 0) {
					return ptr;
				}
			}
		} else {
			ptr += dir;
		}
	}
	return 0;
}

static int
search (txtbuffer_t *buffer, const eb_sel_t *sel, const char *str, int dir,
		eb_sel_t *find, int (*cmp)(const char *, const char *, size_t))
{
	unsigned    start = sel->start;
	unsigned    slen = strlen (str);
	unsigned    end = start + sel->length - slen;
	const char *found = 0;

	find->start = 0;
	find->length = 0;

	if (sel->length >= slen) {
		if (dir < 0) {
			const char *s = buffer->text + spanGap (buffer, end);
			if ((start < buffer->gapOffset && end <= buffer->gapOffset)
				|| start > buffer->gapOffset) {
				found = _search (buffer, s, end - start, str, slen, -1, cmp);
			} else {
				unsigned    l = end - (buffer->gapOffset + buffer->gapSize);
				found = _search (buffer, s, l, str, slen, 1, cmp);
				if (!found) {
					s -= l + buffer->gapSize;
					l = buffer->gapOffset - start;
					found = _search (buffer, s, l, str, slen, 1, cmp);
				}
			}
		} else {
			const char *s = buffer->text + spanGap (buffer, start);
			if ((start < buffer->gapOffset && end <= buffer->gapOffset)
				|| start > buffer->gapOffset) {
				found = _search (buffer, s, end - start, str, slen, 1, cmp);
			} else {
				unsigned    l = buffer->gapOffset - start;
				found = _search (buffer, s, l, str, slen, 1, cmp);
				if (!found) {
					s += l + buffer->gapSize;
					l = end - start - l;
					found = _search (buffer, s, l, str, slen, 1, cmp);
				}
			}
		}
	}
	if (found) {
		unsigned    offset = found - buffer->text;
		if (offset > buffer->gapOffset) {
			offset -= buffer->gapSize;
		}
		find->start = offset;
		find->length = slen;
		return 1;
	}
	return 0;
}

static int
saveFile (txtbuffer_t *buffer, const char *filename)
{
	QFile       *file = Qopen (filename, "wt");

	if (file) {
		unsigned offset = buffer->gapOffset + buffer->gapSize;
		Qwrite (file, buffer->text, buffer->gapOffset);
		Qwrite (file, buffer->text + offset, buffer->textSize - offset);
		Qclose (file);
		return 1;
	}
	return 0;
}

static int
loadFile (txtbuffer_t *buffer, const char *filename)
{
	QFile      *file = Qopen (filename, "rtz");
	char       *dst;
	unsigned    len;

	if (file) {
		len = Qfilesize (file);
		// ensure buffer is empty
		TextBuffer_DeleteAt (buffer, 0, buffer->textSize);
		dst = TextBuffer_OpenGap (buffer, 0, len);
		Qread (file, dst, len);

		buffer->textSize += len;
		buffer->gapOffset += len;
		buffer->gapSize -= len;

		Qclose (file);
		return 1;
	}
	return 0;
}

static unsigned
formatLine (txtbuffer_t *buffer, unsigned linePtr, unsigned xpos,
			int *dst, unsigned length, eb_sel_t *selection, eb_color_t *colors,
			int tabSize)
{
	unsigned    pos = 0;
	unsigned    ptr = linePtr;
	unsigned    sels = selection->start;
	unsigned    sele = selection->start + selection->length;
	int         coln = (colors->normal & ~0xff);
	int         cols = (colors->selected & ~0xff);
	int         col;
	byte        c = 0;
	int         count;
	int        *startdst = dst;
	int         startlen = length;

	while (pos < xpos && ptr < buffer->textSize) {
		c = getChar (buffer, ptr);
		if (c == '\n') {
			break;
		}
		if (c == '\t') {
			pos += tabSize - (pos % tabSize) - 1;	// -1 for ++
		}
		pos++;
		ptr++;
	}
	col = ptr >= sels && ptr < sele ? cols : coln;
	while (xpos < pos && length > 0) {
		*dst++ = col | ' ';
		length--;
		xpos++;
	}
	while (length > 0 && ptr < buffer->textSize) {
		col = ptr >= sels && ptr < sele ? cols : coln;
		c = getChar (buffer, ptr++);
		if (c == '\n') {
			break;
		}
		count = 1;
		if (c == '\t') {
			c = ' ';
			count = tabSize - (pos % tabSize);
		}
		while (length > 0 && count-- > 0) {
			*dst++ = col | c;
			pos++;
			length--;
		}
	}
	while (c != '\n' && ptr < buffer->textSize) {
		c = getChar (buffer, ptr++);
	}
	while (length-- > 0) {
		*dst++ = col | ' ';
	}
	if (dst - startdst > startlen) {
		Sys_Error ("formatLine wrote too much: %zd %u %d",
				   dst - startdst, startlen, length);
	}
	return ptr;
}

//===

static void
bi__i_EditBuffer__init (progs_t *pr)
{
	qwaq_ebresources_t *res = PR_Resources_Find (pr, "qwaq-editbuffer");
	__auto_type self = &P_STRUCT (pr, qwaq_editbuffer_t, 0);
	RUA_obj_increment_retaincount (pr);// [super init];
	txtbuffer_t *txtbuffer = TextBuffer_Create ();
	editbuffer_t *buffer = editbuffer_new (res);

	buffer->txtbuffer = txtbuffer;
	buffer->tabSize = 4; // FIXME

	self->buffer = editbuffer_index (res, buffer);
	R_INT (pr) = PR_SetPointer (pr, self);
}

static void
bi__i_EditBuffer__initWithFile_ (progs_t *pr)
{
	qwaq_ebresources_t *res = PR_Resources_Find (pr, "qwaq-editbuffer");
	__auto_type self = &P_STRUCT (pr, qwaq_editbuffer_t, 0);
	RUA_obj_increment_retaincount (pr);// [super init];
	const char *filename = P_GSTRING (pr, 2);
	txtbuffer_t *txtbuffer = TextBuffer_Create ();
	editbuffer_t *buffer = editbuffer_new (res);

	buffer->txtbuffer = txtbuffer;
	buffer->tabSize = 4; // FIXME

	loadFile (buffer->txtbuffer, filename);

	self->buffer = editbuffer_index (res, buffer);
	R_INT (pr) = PR_SetPointer (pr, self);
}

static void
bi__i_EditBuffer__dealloc (progs_t *pr)
{
	qwaq_ebresources_t *res = PR_Resources_Find (pr, "qwaq-editbuffer");
	__auto_type self = &P_STRUCT (pr, qwaq_editbuffer_t, 0);
	int         buffer_id = self->buffer;
	editbuffer_t *buffer = get_editbuffer (res, __FUNCTION__, buffer_id);

	TextBuffer_Destroy (buffer->txtbuffer);
	editbuffer_free (res, buffer);
}

static void
bi__i_EditBuffer__nextChar_ (progs_t *pr)
{
	qwaq_ebresources_t *res = PR_Resources_Find (pr, "qwaq-editbuffer");
	int         buffer_id = P_STRUCT (pr, qwaq_editbuffer_t, 0).buffer;
	editbuffer_t *buffer = get_editbuffer (res, __FUNCTION__, buffer_id);
	unsigned    ptr = P_UINT (pr, 2);

	R_INT (pr) = nextChar (buffer->txtbuffer, ptr);
}

static void
bi__i_EditBuffer__prevChar_ (progs_t *pr)
{
	qwaq_ebresources_t *res = PR_Resources_Find (pr, "qwaq-editbuffer");
	int         buffer_id = P_STRUCT (pr, qwaq_editbuffer_t, 0).buffer;
	editbuffer_t *buffer = get_editbuffer (res, __FUNCTION__, buffer_id);
	unsigned    ptr = P_UINT (pr, 2);

	R_INT (pr) = prevChar (buffer->txtbuffer, ptr);
}

static void
bi__i_EditBuffer__nextNonSpace_ (progs_t *pr)
{
	qwaq_ebresources_t *res = PR_Resources_Find (pr, "qwaq-editbuffer");
	int         buffer_id = P_STRUCT (pr, qwaq_editbuffer_t, 0).buffer;
	editbuffer_t *buffer = get_editbuffer (res, __FUNCTION__, buffer_id);
	unsigned    ptr = P_UINT (pr, 2);

	R_INT (pr) = nextNonSpace (buffer->txtbuffer, ptr);
}

static void
bi__i_EditBuffer__prevNonSpace_ (progs_t *pr)
{
	qwaq_ebresources_t *res = PR_Resources_Find (pr, "qwaq-editbuffer");
	int         buffer_id = P_STRUCT (pr, qwaq_editbuffer_t, 0).buffer;
	editbuffer_t *buffer = get_editbuffer (res, __FUNCTION__, buffer_id);
	unsigned    ptr = P_UINT (pr, 2);

	R_INT (pr) = prevNonSpace (buffer->txtbuffer, ptr);
}

static void
bi__i_EditBuffer__isWord_ (progs_t *pr)
{
	qwaq_ebresources_t *res = PR_Resources_Find (pr, "qwaq-editbuffer");
	int         buffer_id = P_STRUCT (pr, qwaq_editbuffer_t, 0).buffer;
	editbuffer_t *buffer = get_editbuffer (res, __FUNCTION__, buffer_id);
	unsigned    ptr = P_UINT (pr, 2);

	R_INT (pr) = isword (getChar (buffer->txtbuffer, ptr));
}

static void
bi__i_EditBuffer__nextWord_ (progs_t *pr)
{
	qwaq_ebresources_t *res = PR_Resources_Find (pr, "qwaq-editbuffer");
	int         buffer_id = P_STRUCT (pr, qwaq_editbuffer_t, 0).buffer;
	editbuffer_t *buffer = get_editbuffer (res, __FUNCTION__, buffer_id);
	unsigned    ptr = P_UINT (pr, 2);

	R_INT (pr) = nextWord (buffer->txtbuffer, ptr);
}

static void
bi__i_EditBuffer__prevWord_ (progs_t *pr)
{
	qwaq_ebresources_t *res = PR_Resources_Find (pr, "qwaq-editbuffer");
	int         buffer_id = P_STRUCT (pr, qwaq_editbuffer_t, 0).buffer;
	editbuffer_t *buffer = get_editbuffer (res, __FUNCTION__, buffer_id);
	unsigned    ptr = P_UINT (pr, 2);

	R_INT (pr) = prevWord (buffer->txtbuffer, ptr);
}

static void
bi__i_EditBuffer__nextLine_ (progs_t *pr)
{
	qwaq_ebresources_t *res = PR_Resources_Find (pr, "qwaq-editbuffer");
	int         buffer_id = P_STRUCT (pr, qwaq_editbuffer_t, 0).buffer;
	editbuffer_t *buffer = get_editbuffer (res, __FUNCTION__, buffer_id);
	unsigned    ptr = P_UINT (pr, 2);

	R_INT (pr) = nextLine (buffer->txtbuffer, ptr);
}

static void
bi__i_EditBuffer__prevLine_ (progs_t *pr)
{
	qwaq_ebresources_t *res = PR_Resources_Find (pr, "qwaq-editbuffer");
	int         buffer_id = P_STRUCT (pr, qwaq_editbuffer_t, 0).buffer;
	editbuffer_t *buffer = get_editbuffer (res, __FUNCTION__, buffer_id);
	unsigned    ptr = P_UINT (pr, 2);

	R_INT (pr) = prevLine (buffer->txtbuffer, ptr);
}

static void
bi__i_EditBuffer__nextLine__ (progs_t *pr)
{
	qwaq_ebresources_t *res = PR_Resources_Find (pr, "qwaq-editbuffer");
	int         buffer_id = P_STRUCT (pr, qwaq_editbuffer_t, 0).buffer;
	editbuffer_t *buffer = get_editbuffer (res, __FUNCTION__, buffer_id);
	unsigned    ptr = P_UINT (pr, 2);
	unsigned    count = P_UINT (pr, 3);

	while (count-- > 0) {
		unsigned    oldptr = ptr;
		ptr = nextLine (buffer->txtbuffer, ptr);
		if (ptr == buffer->txtbuffer->textSize && ptr > 0
			&& getChar (buffer->txtbuffer, ptr - 1) != '\n') {
			ptr = oldptr;
			break;
		}
	}
	R_INT (pr) = ptr;
}

static void
bi__i_EditBuffer__prevLine__ (progs_t *pr)
{
	qwaq_ebresources_t *res = PR_Resources_Find (pr, "qwaq-editbuffer");
	int         buffer_id = P_STRUCT (pr, qwaq_editbuffer_t, 0).buffer;
	editbuffer_t *buffer = get_editbuffer (res, __FUNCTION__, buffer_id);
	unsigned    ptr = P_UINT (pr, 2);
	unsigned    count = P_UINT (pr, 3);

	while (ptr && count-- > 0) {
		ptr = prevLine (buffer->txtbuffer, ptr);
	}
	R_INT (pr) = ptr;
}

static void
bi__i_EditBuffer__charPos_at_ (progs_t *pr)
{
	qwaq_ebresources_t *res = PR_Resources_Find (pr, "qwaq-editbuffer");
	int         buffer_id = P_STRUCT (pr, qwaq_editbuffer_t, 0).buffer;
	editbuffer_t *buffer = get_editbuffer (res, __FUNCTION__, buffer_id);
	unsigned    ptr = P_UINT (pr, 2);
	unsigned    target = P_UINT (pr, 3);
	int         tabSize = buffer->tabSize;

	R_INT (pr) = charPos (buffer->txtbuffer, ptr, target, tabSize);
}

static void
bi__i_EditBuffer__charPtr_at_ (progs_t *pr)
{
	qwaq_ebresources_t *res = PR_Resources_Find (pr, "qwaq-editbuffer");
	int         buffer_id = P_STRUCT (pr, qwaq_editbuffer_t, 0).buffer;
	editbuffer_t *buffer = get_editbuffer (res, __FUNCTION__, buffer_id);
	unsigned    ptr = P_UINT (pr, 2);
	unsigned    target = P_UINT (pr, 3);
	int         tabSize = buffer->tabSize;

	R_INT (pr) = charPtr (buffer->txtbuffer, ptr, target, tabSize);
}

static void
bi__i_EditBuffer__getWord_ (progs_t *pr)
{
	qwaq_ebresources_t *res = PR_Resources_Find (pr, "qwaq-editbuffer");
	int         buffer_id = P_STRUCT (pr, qwaq_editbuffer_t, 0).buffer;
	editbuffer_t *buffer = get_editbuffer (res, __FUNCTION__, buffer_id);
	unsigned    ptr = P_UINT (pr, 2);
	unsigned    s = getBOW (buffer->txtbuffer, ptr);
	unsigned    e = getEOW (buffer->txtbuffer, ptr);

	R_PACKED (pr, eb_sel_t).start = s;
	R_PACKED (pr, eb_sel_t).length = e - s;
}

static void
bi__i_EditBuffer__getLine_ (progs_t *pr)
{
	qwaq_ebresources_t *res = PR_Resources_Find (pr, "qwaq-editbuffer");
	int         buffer_id = P_STRUCT (pr, qwaq_editbuffer_t, 0).buffer;
	editbuffer_t *buffer = get_editbuffer (res, __FUNCTION__, buffer_id);
	unsigned    ptr = P_UINT (pr, 2);
	unsigned    s = getBOL (buffer->txtbuffer, ptr);
	unsigned    e = getEOL (buffer->txtbuffer, ptr);

	R_PACKED (pr, eb_sel_t).start = s;
	R_PACKED (pr, eb_sel_t).length = e - s;
}

static void
bi__i_EditBuffer__getBOL_ (progs_t *pr)
{
	qwaq_ebresources_t *res = PR_Resources_Find (pr, "qwaq-editbuffer");
	int         buffer_id = P_STRUCT (pr, qwaq_editbuffer_t, 0).buffer;
	editbuffer_t *buffer = get_editbuffer (res, __FUNCTION__, buffer_id);
	unsigned    ptr = P_UINT (pr, 2);

	R_INT (pr) = getBOL (buffer->txtbuffer, ptr);
}

static void
bi__i_EditBuffer__getEOL_ (progs_t *pr)
{
	qwaq_ebresources_t *res = PR_Resources_Find (pr, "qwaq-editbuffer");
	int         buffer_id = P_STRUCT (pr, qwaq_editbuffer_t, 0).buffer;
	editbuffer_t *buffer = get_editbuffer (res, __FUNCTION__, buffer_id);
	unsigned    ptr = P_UINT (pr, 2);

	R_INT (pr) = getEOL (buffer->txtbuffer, ptr);
}

static void
bi__i_EditBuffer__getBOT (progs_t *pr)
{
	//qwaq_ebresources_t *res = PR_Resources_Find (pr, "qwaq-editbuffer");
	//int         buffer_id = P_STRUCT (pr, qwaq_editbuffer_t, 0).buffer;
	//editbuffer_t *buffer = get_editbuffer (res, __FUNCTION__, buffer_id);

	R_INT (pr) = 0;
}

static void
bi__i_EditBuffer__getEOT (progs_t *pr)
{
	qwaq_ebresources_t *res = PR_Resources_Find (pr, "qwaq-editbuffer");
	int         buffer_id = P_STRUCT (pr, qwaq_editbuffer_t, 0).buffer;
	editbuffer_t *buffer = get_editbuffer (res, __FUNCTION__, buffer_id);

	R_INT (pr) = buffer->txtbuffer->textSize;
}

static void
bi__i_EditBuffer__readString_ (progs_t *pr)
{
	qwaq_ebresources_t *res = PR_Resources_Find (pr, "qwaq-editbuffer");
	int         buffer_id = P_STRUCT (pr, qwaq_editbuffer_t, 0).buffer;
	editbuffer_t *buffer = get_editbuffer (res, __FUNCTION__, buffer_id);
	__auto_type selection = &P_PACKED (pr, eb_sel_t, 2);

	char       *str = alloca (selection->length + 1);
	str[selection->length] = 0;
	readString (buffer->txtbuffer, selection, str);

	RETURN_STRING (pr, str);
}

static void
bi__i_EditBuffer__getChar_ (progs_t *pr)
{
	qwaq_ebresources_t *res = PR_Resources_Find (pr, "qwaq-editbuffer");
	int         buffer_id = P_STRUCT (pr, qwaq_editbuffer_t, 0).buffer;
	editbuffer_t *buffer = get_editbuffer (res, __FUNCTION__, buffer_id);
	unsigned    ptr = P_UINT (pr, 2);

	if (ptr >= buffer->txtbuffer->textSize) {
		PR_RunError (pr, "EditBuffer: character index out of bounds\n");
	}
	R_INT (pr) = (byte) getChar (buffer->txtbuffer, ptr);
}

static void
bi__i_EditBuffer__putChar_at_ (progs_t *pr)
{
	qwaq_ebresources_t *res = PR_Resources_Find (pr, "qwaq-editbuffer");
	int         buffer_id = P_STRUCT (pr, qwaq_editbuffer_t, 0).buffer;
	editbuffer_t *buffer = get_editbuffer (res, __FUNCTION__, buffer_id);
	int         chr = P_UINT (pr, 2);
	unsigned    ptr = P_UINT (pr, 3);

	if (ptr >= buffer->txtbuffer->textSize) {
		PR_RunError (pr, "EditBuffer: character index out of bounds\n");
	}
	buffer->txtbuffer->text[spanGap (buffer->txtbuffer, ptr)] = chr;
}

static void
bi__i_EditBuffer__insertChar_at_ (progs_t *pr)
{
	qwaq_ebresources_t *res = PR_Resources_Find (pr, "qwaq-editbuffer");
	int         buffer_id = P_STRUCT (pr, qwaq_editbuffer_t, 0).buffer;
	editbuffer_t *buffer = get_editbuffer (res, __FUNCTION__, buffer_id);
	char        chr = P_UINT (pr, 2);
	unsigned    ptr = P_UINT (pr, 3);

	if (ptr > buffer->txtbuffer->textSize) {
		PR_RunError (pr, "EditBuffer: character index out of bounds\n");
	}
	TextBuffer_InsertAt (buffer->txtbuffer, ptr, &chr, 1);
}

static void
bi__i_EditBuffer__countLines_ (progs_t *pr)
{
	qwaq_ebresources_t *res = PR_Resources_Find (pr, "qwaq-editbuffer");
	int         buffer_id = P_STRUCT (pr, qwaq_editbuffer_t, 0).buffer;
	editbuffer_t *buffer = get_editbuffer (res, __FUNCTION__, buffer_id);
	__auto_type selection = &P_PACKED (pr, eb_sel_t, 2);

	R_INT (pr) = countLines (buffer->txtbuffer,
							 selection->start, selection->length);
}

static void
bi__i_EditBuffer__search_for_direction_ (progs_t *pr)
{
	qwaq_ebresources_t *res = PR_Resources_Find (pr, "qwaq-editbuffer");
	int         buffer_id = P_STRUCT (pr, qwaq_editbuffer_t, 0).buffer;
	editbuffer_t *buffer = get_editbuffer (res, __FUNCTION__, buffer_id);
	__auto_type selection = &P_PACKED (pr, eb_sel_t, 2);
	const char *str = P_GSTRING (pr, 3);
	int         dir = P_INT (pr, 4);

	search (buffer->txtbuffer, selection, str, dir, &R_PACKED (pr, eb_sel_t),
			strncmp);
}

static void
bi__i_EditBuffer__isearch_for_direction_ (progs_t *pr)
{
	qwaq_ebresources_t *res = PR_Resources_Find (pr, "qwaq-editbuffer");
	int         buffer_id = P_STRUCT (pr, qwaq_editbuffer_t, 0).buffer;
	editbuffer_t *buffer = get_editbuffer (res, __FUNCTION__, buffer_id);
	__auto_type selection = &P_PACKED (pr, eb_sel_t, 2);
	const char *str = P_GSTRING (pr, 3);
	int         dir = P_INT (pr, 4);

	search (buffer->txtbuffer, selection, str, dir, &R_PACKED (pr, eb_sel_t),
			strncasecmp);
}

static void
bi__i_EditBuffer__formatLine_from_into_width_highlight_colors_ (progs_t *pr)
{
	qwaq_ebresources_t *res = PR_Resources_Find (pr, "qwaq-editbuffer");
	int         buffer_id = P_STRUCT (pr, qwaq_editbuffer_t, 0).buffer;
	editbuffer_t *buffer = get_editbuffer (res, __FUNCTION__, buffer_id);
	unsigned    linePtr = P_UINT (pr, 2);
	unsigned    xpos = P_UINT (pr, 3);
	int        *dst = (int *) P_GPOINTER (pr, 4);
	unsigned    length = P_UINT (pr, 5);
	__auto_type selection = &P_PACKED (pr, eb_sel_t, 6);
	__auto_type colors = &P_PACKED (pr, eb_color_t, 7);
	int         tabSize = buffer->tabSize;
	unsigned    ptr;

	ptr = formatLine (buffer->txtbuffer, linePtr, xpos, dst, length, selection,
					  colors, tabSize);
	R_INT (pr) = ptr;
}

static void
bi__i_EditBuffer__modified (progs_t *pr)
{
	qwaq_ebresources_t *res = PR_Resources_Find (pr, "qwaq-editbuffer");
	int         buffer_id = P_STRUCT (pr, qwaq_editbuffer_t, 0).buffer;
	editbuffer_t *buffer = get_editbuffer (res, __FUNCTION__, buffer_id);
	R_INT (pr) = buffer->modified;
}

static void
bi__i_EditBuffer__textSize (progs_t *pr)
{
	qwaq_ebresources_t *res = PR_Resources_Find (pr, "qwaq-editbuffer");
	int         buffer_id = P_STRUCT (pr, qwaq_editbuffer_t, 0).buffer;
	editbuffer_t *buffer = get_editbuffer (res, __FUNCTION__, buffer_id);
	R_INT (pr) = buffer->txtbuffer->textSize;
}

static void
bi__i_EditBuffer__saveFile_ (progs_t *pr)
{
	qwaq_ebresources_t *res = PR_Resources_Find (pr, "qwaq-editbuffer");
	int         buffer_id = P_STRUCT (pr, qwaq_editbuffer_t, 0).buffer;
	editbuffer_t *buffer = get_editbuffer (res, __FUNCTION__, buffer_id);
	const char *filename = P_GSTRING (pr, 2);

	if (saveFile (buffer->txtbuffer, filename)) {
		buffer->modified = 0;
	}
}

static void
qwaq_ebresources_clear (progs_t *pr, void *data)
{
	__auto_type res = (qwaq_ebresources_t *) data;

	for (editbuffer_t *buffer = res->buffer_list; buffer;
		 buffer = buffer->next) {
		TextBuffer_Destroy (buffer->txtbuffer);
		buffer->txtbuffer = 0;
	}
	editbuffer_reset (res);
}

#define bi(x,n,np,params...) {#x, bi_##x, n, np, {params}}
#define p(type) PR_PARAM(type)
#define P(a, s) { .size = (s), .alignment = BITOP_LOG2 (a), }
static builtin_t builtins[] = {
	bi(_i_EditBuffer__init,           -1, 2, p(ptr), p(ptr)),
	bi(_i_EditBuffer__initWithFile_,  -1, 3, p(ptr), p(ptr), p(string)),
	bi(_i_EditBuffer__dealloc,        -1, 2, p(ptr), p(ptr)),
	bi(_i_EditBuffer__nextChar_,      -1, 3, p(ptr), p(ptr), p(uint)),
	bi(_i_EditBuffer__prevChar_,      -1, 3, p(ptr), p(ptr), p(uint)),
	bi(_i_EditBuffer__nextNonSpace_,  -1, 3, p(ptr), p(ptr), p(uint)),
	bi(_i_EditBuffer__prevNonSpace_,  -1, 3, p(ptr), p(ptr), p(uint)),
	bi(_i_EditBuffer__isWord_,        -1, 3, p(ptr), p(ptr), p(uint)),
	bi(_i_EditBuffer__nextWord_,      -1, 3, p(ptr), p(ptr), p(uint)),
	bi(_i_EditBuffer__prevWord_,      -1, 3, p(ptr), p(ptr), p(uint)),
	bi(_i_EditBuffer__nextLine_,      -1, 3, p(ptr), p(ptr), p(uint)),
	bi(_i_EditBuffer__prevLine_,      -1, 3, p(ptr), p(ptr), p(uint)),
	bi(_i_EditBuffer__nextLine__,     -1, 4, p(ptr), p(ptr), p(uint), p(uint)),
	bi(_i_EditBuffer__prevLine__,     -1, 4, p(ptr), p(ptr), p(uint), p(uint)),
	bi(_i_EditBuffer__charPos_at_,    -1, 4, p(ptr), p(ptr), p(uint), p(uint)),
	bi(_i_EditBuffer__charPtr_at_,    -1, 4, p(ptr), p(ptr), p(uint), p(uint)),
	bi(_i_EditBuffer__getWord_,       -1, 3, p(ptr), p(ptr), p(uint)),
	bi(_i_EditBuffer__getLine_,       -1, 3, p(ptr), p(ptr), p(uint)),
	bi(_i_EditBuffer__getBOL_,        -1, 3, p(ptr), p(ptr), p(uint)),
	bi(_i_EditBuffer__getEOL_,        -1, 3, p(ptr), p(ptr), p(uint)),
	bi(_i_EditBuffer__getBOT,         -1, 2, p(ptr), p(ptr)),
	bi(_i_EditBuffer__getEOT,         -1, 2, p(ptr), p(ptr)),
	bi(_i_EditBuffer__readString_,    -1, 3, p(ptr), p(ptr), P(1, 2)),
	bi(_i_EditBuffer__getChar_,       -1, 3, p(ptr), p(ptr), p(uint)),
	bi(_i_EditBuffer__putChar_at_,    -1, 4, p(ptr), p(ptr), p(int), p(uint)),
	bi(_i_EditBuffer__insertChar_at_, -1, 4, p(ptr), p(ptr), p(int), p(uint)),
	bi(_i_EditBuffer__countLines_,    -1, 3, p(ptr), p(ptr), P(1, 2)),
	bi(_i_EditBuffer__search_for_direction_,	-1,
						5, p(ptr), p(ptr), P(1, 2), p(string), p(int)),
	bi(_i_EditBuffer__isearch_for_direction_,-1,
						5, p(ptr), p(ptr), P(1, 2), p(string), p(int)),
	bi(_i_EditBuffer__formatLine_from_into_width_highlight_colors_, -1,
						8, p(ptr), p(ptr),
						p(uint), p(uint), p(ptr), p(uint), P(1, 2), P(1, 2)),
	bi(_i_EditBuffer__modified,       -1, 2, p(ptr), p(ptr)),
	bi(_i_EditBuffer__textSize,       -1, 2, p(ptr), p(ptr)),
	bi(_i_EditBuffer__saveFile_,      -1, 3, p(ptr), p(ptr), p(string)),
	{}
};

void
QWAQ_EditBuffer_Init (progs_t *pr)
{
	qwaq_ebresources_t *res = calloc (sizeof (*res), 1);
	res->pr = pr;

	PR_Resources_Register (pr, "qwaq-editbuffer", res, qwaq_ebresources_clear);
	PR_RegisterBuiltins (pr, builtins);
}
