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
	D_EnableBackBufferAccess ();
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
	D_DisableBackBufferAccess ();
	return tex;
}

tex_t *
SCR_ScreenShot (unsigned width, unsigned height)
{
	unsigned char *src, *dest;
	float          fracw, frach;
	int            count, dex, dey, dx, dy, nx, r, g, b, x, y, w, h;
	tex_t         *tex;

	// enable direct drawing of console to back buffer
	D_EnableBackBufferAccess ();

	w = (vid.width < width) ? vid.width : width;
	h = (vid.height < height) ? vid.height : height;

	fracw = (float) vid.width / (float) w;
	frach = (float) vid.height / (float) h;

	tex = malloc (sizeof (tex_t) + w * h);
	tex->data = (byte *) (tex + 1);
	if (!tex)
		return 0;

	tex->width = w;
	tex->height = h;
	tex->palette = vid.palette;

	for (y = 0; y < h; y++) {
		dest = tex->data + (w * (h - y - 1));

		for (x = 0; x < w; x++) {
			r = g = b = 0;

			dx = x * fracw;
			dex = (x + 1) * fracw;
			if (dex == dx)
				dex++;					// at least one
			dy = y * frach;
			dey = (y + 1) * frach;
			if (dey == dy)
				dey++;					// at least one

			count = 0;
			for (; dy < dey; dy++) {
				src = ((byte*)vid.buffer) + (vid.rowbytes * dy) + dx;
				for (nx = dx; nx < dex; nx++) {
					r += vid.basepal[*src * 3];
					g += vid.basepal[*src * 3 + 1];
					b += vid.basepal[*src * 3 + 2];
					src++;
					count++;
				}
			}
			r /= count;
			g /= count;
			b /= count;
			*dest++ = MipColor (r, g, b);
		}
	}
	// for adapters that can't stay mapped in for linear writes all the time
	D_DisableBackBufferAccess ();

	return tex;
}

void
SCR_ScreenShot_f (void)
{
	dstring_t  *pcxname = dstring_new ();
	pcx_t      *pcx;
	int         pcx_len;

	// find a file name to save it to
	if (!QFS_NextFilename (pcxname,
						   va (0, "%s/qf", qfs_gamedir->dir.shots), ".pcx")) {
		Sys_Printf ("SCR_ScreenShot_f: Couldn't create a PCX");
	} else {
		// enable direct drawing of console to back buffer
		D_EnableBackBufferAccess ();

		// save the pcx file
		pcx = EncodePCX (vid.buffer, vid.width, vid.height, vid.rowbytes,
						 vid.basepal, false, &pcx_len);
		QFS_WriteFile (pcxname->str, pcx, pcx_len);


		// for adapters that can't stay mapped in for linear writes all the
		// time
		D_DisableBackBufferAccess ();

		Sys_Printf ("Wrote %s/%s\n", qfs_userpath, pcxname->str);
	}
	dstring_delete (pcxname);
}

void
R_RenderFrame (SCR_Func *scr_funcs)
{
	vrect_t     vrect;

	if (scr_skipupdate)
		return;

	// do 3D refresh drawing, and then update the screen
	D_EnableBackBufferAccess ();		// of all overlay stuff if drawing
										// directly

	if (vr_data.scr_fullupdate++ < vid.numpages) {	// clear the entire screen
		vr_data.scr_copyeverything = 1;
		Draw_TileClear (0, 0, vid.width, vid.height);
	}

	pconupdate = NULL;

	SCR_SetUpToDrawConsole ();

	D_DisableBackBufferAccess ();		// for adapters that can't stay mapped
										// in for linear writes all the time
	VID_LockBuffer ();
	R_RenderView ();
	VID_UnlockBuffer ();

	D_EnableBackBufferAccess ();		// of all overlay stuff if drawing
										// directly

	view_draw (vr_data.scr_view);
	while (*scr_funcs) {
		(*scr_funcs)();
		scr_funcs++;
	}

	D_DisableBackBufferAccess ();		// for adapters that can't stay mapped
										// in for linear writes all the time
	if (pconupdate) {
		D_UpdateRects (pconupdate);
	}

	// update one of three areas
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
