/*
	txtbuffer.c

	Text buffer

	Copyright (C) 2020  Bill Currie <bill@taniwha.org>

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

#include "QF/ui/txtbuffer.h"

#include "compat.h"

static txtbuffer_t *txtbuffers_freelist;

static char *
txtbuffer_open_gap (txtbuffer_t *buffer, size_t offset, size_t length)
{
	size_t      len;
	char       *dst;
	char       *src;

	if (offset == buffer->gapOffset && length <= buffer->gapSize) {
		return buffer->text + buffer->gapOffset;
	}
	if (length <= buffer->gapSize) {
		// no resize needed
		if (offset < buffer->gapOffset) {
			len = buffer->gapOffset - offset;
			dst = buffer->text + buffer->gapOffset + buffer->gapSize - len;
			src = buffer->text + offset;
		} else {
			len = offset - buffer->gapOffset;
			dst = buffer->text + buffer->gapOffset;
			src = buffer->text + buffer->gapOffset + buffer->gapSize;
		}
		memmove (dst, src, len);
	} else {
		// need to resize the buffer
		// grow the buffer by the difference in lengths rounded up to the
		// next multiple of TXTBUFFER_GROW
		size_t      new_size = buffer->bufferSize + length - buffer->gapSize;
		new_size = (new_size + TXTBUFFER_GROW - 1) & ~(TXTBUFFER_GROW - 1);
		char       *new_text = realloc (buffer->text, new_size);

		if (!new_text) {
			return 0;
		}
		buffer->text = new_text;

		if (offset < buffer->gapOffset) {
			// move the old post-gap to the end of the new buffer
			len = buffer->bufferSize - (buffer->gapOffset + buffer->gapSize);
			dst = buffer->text + new_size - len;
			src = buffer->text + buffer->gapOffset + buffer->gapSize;
			memmove (dst, src, len);
			// move the remaining chunk to after the end of the new gap or
			// just before the location of the previous move
			len = buffer->gapOffset - offset;
			dst -= len;
			src = buffer->text + offset;
			memmove (dst, src, len);
		} else if (offset > buffer->gapOffset) {
			// move the old post-offset to the end of the new buffer
			len = buffer->bufferSize - (offset + buffer->gapSize);
			dst = buffer->text + new_size - len;
			src = buffer->text + offset + buffer->gapSize;
			memmove (dst, src, len);
			// move the old post-gap to offset block to before the new gap
			len = offset - buffer->gapOffset;
			dst = buffer->text + buffer->gapOffset;
			src = buffer->text + buffer->gapOffset + buffer->gapSize;
			memmove (dst, src, len);
		} else {
			// the gap only grew, did not move
			len = buffer->bufferSize - (offset + buffer->gapSize);
			dst = buffer->text + new_size - len;
			src = buffer->text + buffer->gapOffset + buffer->gapSize;
			memmove (dst, src, len);
		}
		buffer->gapSize += new_size - buffer->bufferSize;
		buffer->bufferSize = new_size;
	}
	buffer->gapOffset = offset;
	return buffer->text + buffer->gapOffset;
}

static char *
txtbuffer_delete_text (txtbuffer_t *buffer, size_t offset, size_t length)
{
	size_t      len;
	char       *dst;
	char       *src;

	if (offset > buffer->gapOffset) {
		len = offset - buffer->gapOffset;
		dst = buffer->text + buffer->gapOffset;
		src = buffer->text + buffer->gapOffset + buffer->gapSize;
		memmove (dst, src, len);
	} else if (offset + length < buffer->gapOffset) {
		len = buffer->gapOffset - (offset + length);
		dst = buffer->text + buffer->gapSize + (offset + length);
		src = buffer->text + (offset + length);
		memmove (dst, src, len);
	}
	// don't need to do any copying when offset <= gapOffset &&
	// offset + length >= offset
	buffer->gapOffset = offset;
	buffer->gapSize += length;
	buffer->textSize -= length;
	return buffer->text + buffer->gapOffset;
}

VISIBLE txtbuffer_t *
TextBuffer_Create (void)
{
	txtbuffer_t *buffer;
	ALLOC (16, txtbuffer_t, txtbuffers, buffer);
	return buffer;
}

VISIBLE void
TextBuffer_Destroy (txtbuffer_t *buffer)
{
	if (buffer->text) {
		free (buffer->text);
	}
	FREE (txtbuffers, buffer);
}

VISIBLE char *
TextBuffer_OpenGap (txtbuffer_t *buffer, size_t offset, size_t text_len)
{
	char       *dst;

	if (offset > buffer->textSize) {
		return 0;
	}
	dst = txtbuffer_open_gap (buffer, offset, text_len);
	if (!dst) {
		return 0;
	}
	return dst;
}

VISIBLE int
TextBuffer_InsertAt (txtbuffer_t *buffer, size_t offset,
					 const char *text, size_t text_len)
{
	char       *dst;

	if (offset > buffer->textSize) {
		return 0;
	}
	dst = txtbuffer_open_gap (buffer, offset, text_len);
	if (!dst) {
		return 0;
	}
	memcpy (dst, text, text_len);
	buffer->textSize += text_len;
	buffer->gapOffset += text_len;
	buffer->gapSize -= text_len;
	return 1;
}

VISIBLE int
TextBuffer_DeleteAt (txtbuffer_t *buffer, size_t offset, size_t len)
{
	if (offset > buffer->textSize) {
		return 0;
	}
	// clamp len to the amount of text beyond offset
	if (len > buffer->textSize - offset) {
		len = buffer->textSize - offset;
	}
	txtbuffer_delete_text (buffer, offset, len);
	return 1;
}
