/*
	screen.c

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

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include <time.h>

#include "QF/cvar.h"
#include "QF/draw.h"
#include "QF/dstring.h"
#include "QF/image.h"
#include "QF/pcx.h"
#include "QF/quakefs.h"
#include "QF/render.h"
#include "QF/screen.h"
#include "QF/sys.h"
#include "QF/va.h"
#include "QF/ui/view.h"

#include "compat.h"
#include "r_internal.h"
#include "vid_internal.h"
#include "vid_sw.h"

/* SCREEN SHOTS */

tex_t *
SCR_CaptureBGR (void)
{
	int         count, x, y;
	tex_t      *tex;
	const byte *src;
	byte       *dst;

	count = vid.width * vid.height;
	tex = malloc (sizeof (tex_t) + count * 3);
	tex->data = (byte *) (tex + 1);
	SYS_CHECKMEM (tex);
	tex->width = vid.width;
	tex->height = vid.height;
	tex->format = tex_rgb;
	tex->palette = 0;
	src = vid.buffer;
	for (y = 0; y < tex->height; y++) {
		dst = tex->data + (tex->height - 1 - y) * tex->width * 3;
		for (x = 0; x < tex->width; x++) {
			*dst++ = vid.basepal[*src * 3 + 2]; // blue
			*dst++ = vid.basepal[*src * 3 + 1]; // green
			*dst++ = vid.basepal[*src * 3 + 0];	// red
			src++;
		}
	}
	return tex;
}

void
R_RenderFrame (SCR_Func *scr_funcs)
{
	// do 3D refresh drawing, and then update the screen
	if (vr_data.scr_fullupdate++ < vid.numpages) {	// clear the entire screen
		vr_data.scr_copyeverything = 1;
		Draw_TileClear (0, 0, vid.width, vid.height);
	}

	R_RenderView ();

	view_draw (vr_data.scr_view);
	while (*scr_funcs) {
		(*scr_funcs)();
		scr_funcs++;
	}

	// update one of three areas
	vrect_t     vrect;
	if (vr_data.scr_copyeverything) {
		vrect.x = 0;
		vrect.y = 0;
		vrect.width = vid.width;
		vrect.height = vid.height;
		vrect.next = 0;
	} else if (scr_copytop) {
		vrect.x = 0;
		vrect.y = 0;
		vrect.width = vid.width;
		vrect.height = vid.height - vr_data.lineadj;
		vrect.next = 0;
	} else {
		vrect.x = vr_data.scr_view->xpos;
		vrect.y = vr_data.scr_view->ypos;
		vrect.width = vr_data.scr_view->xlen;
		vrect.height = vr_data.scr_view->ylen;
		vrect.next = 0;
	}
	sw_ctx->update (sw_ctx, &vrect);
}
