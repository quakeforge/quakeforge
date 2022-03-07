/*
	gl_screen.c

	master for refresh, status bar, console, chat, notify, etc

	Copyright (C) 1996-1997  Id Software, Inc.

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

#include "QF/GL/defines.h"
#include "QF/GL/funcs.h"
#include "QF/GL/qf_rmain.h"

#include "r_internal.h"
#include "vid_gl.h"

/* SCREEN SHOTS */

tex_t *
gl_SCR_CaptureBGR (void)
{
	int         count;
	tex_t      *tex;

	count = vid.width * vid.height;
	tex = malloc (sizeof (tex_t) + count * 3);
	tex->data = (byte *) (tex + 1);
	SYS_CHECKMEM (tex);
	tex->width = vid.width;
	tex->height = vid.height;
	tex->format = tex_rgb;
	tex->palette = 0;
	qfglReadPixels (0, 0, tex->width, tex->height, GL_BGR_EXT,
					GL_UNSIGNED_BYTE, tex->data);
	return tex;
}
