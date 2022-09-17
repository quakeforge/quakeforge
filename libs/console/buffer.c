/*
	buffer.c

	console buffer support

	Copyright (C) 2001 BillCurrie <bill@taniwha.org>

	Author: BillCurrie <bill@taniwha.org>
	Date: 2001/09/28

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
# include "string.h"
#endif
#ifdef HAVE_STRINGS_H
# include "strings.h"
#endif

#include <stdlib.h>

#include "QF/console.h"

VISIBLE con_buffer_t *
Con_CreateBuffer (size_t buffer_size, int max_lines)
{
	con_buffer_t *buffer;

	if (!(buffer = malloc (sizeof (con_buffer_t))))
		return 0;
	if (!(buffer->buffer = malloc (buffer_size)))
		goto err;
	buffer->buffer_size = buffer_size;
	if (!(buffer->lines = calloc (max_lines, sizeof (con_line_t))))
		goto err;
	buffer->max_lines = max_lines;
	buffer->num_lines = 1;
	buffer->cur_line = 0;
	buffer->lines[0].text = buffer->buffer;
	return buffer;
err:
	if (buffer->buffer)
		free (buffer->buffer);
	free (buffer);
	return 0;
}

VISIBLE void
Con_DestroyBuffer (con_buffer_t *buffer)
{
	free (buffer->buffer);
	free (buffer->lines);
	free (buffer);
}

VISIBLE void
Con_BufferAddText (con_buffer_t *buf, const char *text)
{
	con_line_t *cur_line = &buf->lines[buf->cur_line];
	size_t		len = strlen (text);
	// Point to the oldest line in the buffer (first to be dropped when
	// the buffer overflows).
	con_line_t *tail_line = buf->lines + (buf->cur_line + 1 - buf->num_lines
										  + buf->max_lines) % buf->max_lines;

	byte       *pos = cur_line->text + cur_line->len;
	if (pos >= buf->buffer + buf->buffer_size)
		pos -= buf->buffer_size;

	// Limit the appended text to the size of the buffer. The entire buffer
	// may be overwritten.
	if (len > buf->buffer_size) {
		text += len - buf->buffer_size;
		len = buf->buffer_size;
	}
	while (len--) {
		// Drop the oldest line if it will be overwritten by the next
		// character to be inserted. However, if there is only one line,
		// then tail_line is effectively invalid and should not be touched.
		// This happens when the buffer is filled with a single line of text.
		if (buf->num_lines > 1 && pos == tail_line->text) {
			buf->num_lines--;
			tail_line->text = 0;
			tail_line->len = 0;
			tail_line++;
			if (tail_line - buf->lines >= buf->max_lines)
				tail_line = buf->lines;
		}

		// Copy character into the buffer, updating the current line length
		// (trailing \n is part of the line).
		byte        c = *pos++ = *text++;
		if (pos >= buf->buffer + buf->buffer_size)
			pos = buf->buffer;
		cur_line->len++;

		if (c == '\n') {
			if (buf->num_lines < buf->max_lines)
				buf->num_lines++;
			cur_line++;
			buf->cur_line++;
			if (cur_line - buf->lines >= buf->max_lines)
				cur_line = buf->lines;
			cur_line->text = pos;
			cur_line->len = 0;
		}
	}
	buf->cur_line %= buf->max_lines;
}
