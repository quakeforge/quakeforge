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
	buffer->line_head = 1;
	buffer->line_tail = 0;
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
	con_line_t *cur_line = &buf->lines[(buf->line_head - 1 + buf->max_lines)
									   % buf->max_lines];
	con_line_t *tail_line = &buf->lines[buf->line_tail];
	uint32_t    text_head = (cur_line->text + cur_line->len) % buf->buffer_size;
	byte        c;

	while ((c = *text++)) {
		cur_line->len++;
		buf->buffer[text_head++] = c;
		if (text_head >= buf->buffer_size) {
			text_head -= buf->buffer_size;
		}

		if (text_head == tail_line->text) {
			tail_line->len--;
			buf->buffer[tail_line->text++] = 0;
			if (tail_line->text >= buf->buffer_size) {
				tail_line->text -= buf->buffer_size;
			}
			if (!tail_line->len) {
				buf->line_tail++;
				buf->line_tail %= buf->max_lines;
				tail_line = &buf->lines[buf->line_tail];
			}
		}
		if (c == '\n') {
			cur_line++;
			if ((uint32_t) (cur_line - buf->lines) >= buf->max_lines) {
				cur_line -= buf->max_lines;
			}
			cur_line->text = text_head;
			cur_line->len = 0;

			buf->line_head++;
			if (buf->line_head >= buf->max_lines) {
				buf->line_head -= buf->max_lines;
			}

			if (buf->line_head == buf->line_tail) {
				buf->lines[buf->line_tail].text = 0;
				buf->lines[buf->line_tail].len = 0;
				buf->line_tail++;
				if (buf->line_tail >= buf->max_lines) {
					buf->line_tail -= buf->max_lines;
				}
			}
		}
	}
}
