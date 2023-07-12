/*
	r_draw.c

	Renderer general draw support

	Copyright (C) 2022 Bill Currie <bill@taniwha.org>

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
#include <stdlib.h>
#include <string.h>

#include "QF/draw.h"
#include "QF/render.h"

#include "QF/plugin/vid_render.h"

VISIBLE draw_charbuffer_t *
Draw_CreateBuffer (int width, int height)
{
	size_t      size = width * height;
	draw_charbuffer_t *buffer = malloc (sizeof (draw_charbuffer_t) + size);
	buffer->width = width;
	buffer->height = height;
	buffer->chars = (char *) (buffer + 1);
	buffer->cursx = buffer->cursy = 0;
	return buffer;
}

VISIBLE void
Draw_DestroyBuffer (draw_charbuffer_t *buffer)
{
	free (buffer);
}

VISIBLE void
Draw_ClearBuffer (draw_charbuffer_t *buffer)
{
	memset (buffer->chars, ' ', buffer->height * buffer->width);
	buffer->cursx = buffer->cursy = 0;
}

VISIBLE void
Draw_ScrollBuffer (draw_charbuffer_t *buffer, int lines)
{
	int         down = 0;
	if (lines < 0) {
		lines = -lines;
		down = 1;
	}
	if (lines > buffer->height) {
		lines = buffer->height;
	}
	if (down) {
		memmove (buffer->chars + lines * buffer->width, buffer->chars,
				 (buffer->height - lines) * buffer->width);
		memset (buffer->chars, ' ', lines * buffer->width);
	} else {
		memmove (buffer->chars, buffer->chars + lines * buffer->width,
				 (buffer->height - lines) * buffer->width);
		memset (buffer->chars + (buffer->height - lines) * buffer->width, ' ',
				lines * buffer->width);
	}
}

VISIBLE void
Draw_CharBuffer (int x, int y, draw_charbuffer_t *buffer)
{
	r_funcs->Draw_CharBuffer (x, y, buffer);
}

#define TAB 8

VISIBLE int
Draw_PrintBuffer (draw_charbuffer_t *buffer, const char *str)
{
	char       *dst = buffer->chars;
	char        c;
	int         tab;
	int         lines = 0;
	dst += buffer->cursy * buffer->width + buffer->cursx;
	while ((c = *str++)) {
		switch (c) {
			case '\r':
				dst -= buffer->cursx;
				buffer->cursx = 0;
				break;
			case '\n':
				dst -= buffer->cursx;
				buffer->cursx = 0;
				dst += buffer->width;
				buffer->cursy++;
				lines++;
				break;
			case '\f':
				dst += buffer->width;
				buffer->cursy++;
				lines++;
				break;
			case '\t':
				tab = TAB - buffer->cursx % TAB;
				buffer->cursx += tab;
				dst += tab;
				break;
			default:
				*dst++ = c;
				buffer->cursx++;
				break;
		}
		if (buffer->cursx >= buffer->width) {
			buffer->cursx -= buffer->width;
			buffer->cursy++;
			lines++;
		}
		if (buffer->cursy >= buffer->height) {
			int         excess = buffer->cursy - buffer->height + 1;
			Draw_ScrollBuffer (buffer, excess);
			dst -= excess * buffer->width;
			buffer->cursy -= excess;
		}
	}
	return lines;
}

void
Draw_SetScale (int scale)
{
	if (r_funcs->Draw_SetScale) {
		r_funcs->Draw_SetScale (scale);
	}
}

int
Draw_MaxScale (void)
{
	return r_funcs->Draw_SetScale ? 20 : 1;
}
