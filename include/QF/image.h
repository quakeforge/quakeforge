/*
	image.h

	General image handling

	Copyright (C) 2003 Harry Roberts

	Author: Harry Roberts
	Date: Sep 4 2003

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
#ifndef __QF_image_h
#define __QF_image_h

#include "QF/qtypes.h"

typedef enum QFFormat {
	tex_palette = 0,
	tex_l = 0x1909, //GL_LUMINANCE
	tex_a = 0x1906, //GL_ALPHA
	tex_la = 2,
	tex_rgb = 3,
	tex_rgba = 4,
} QFFormat;

// could not use texture_t as that is used for models.
typedef struct tex_s {
	int         width;
	int         height;
	QFFormat    format;
	byte       *palette;					// 0 = 32 bit, otherwise 8
	byte        data[4];					// variable length
} tex_t;


tex_t *LoadImage (const char *imageFile);

#endif//__QF_image_h
