/*
	glsl_main.c

	GLSL rendering

	Copyright (C) 2011 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2011/12/23

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

#include "QF/image.h"
#include "QF/sys.h"

#include "QF/GLSL/defines.h"
#include "QF/GLSL/funcs.h"

#include "r_internal.h"
#include "vid_gl.h"

tex_t *
glsl_SCR_CaptureBGR (void)
{
	byte       *r, *b;
	int         count, i;
	tex_t      *tex;

	count = vid.width * vid.height;
	tex = malloc (sizeof (tex_t) + count * 3);
	tex->data = (byte *) (tex + 1);
	SYS_CHECKMEM (tex);
	tex->width = vid.width;
	tex->height = vid.height;
	tex->format = tex_rgb;
	tex->palette = 0;
	qfeglReadPixels (0, 0, vid.width, vid.height, GL_RGB,
					GL_UNSIGNED_BYTE, tex->data);
	for (i = 0, r = tex->data, b = tex->data + 2; i < count;
		 i++, r += 3, b += 3) {
		byte        t = *b;
		*b = *r;
		*r = t;
	}
	return tex;
}
