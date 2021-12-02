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

#define NH_DEFINE
#include "namehack.h"

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
sw32_SCR_CaptureBGR (void)
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
	sw32_D_EnableBackBufferAccess ();
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
	sw32_D_DisableBackBufferAccess ();
	return tex;
}

__attribute__((const)) tex_t *
sw32_SCR_ScreenShot (unsigned width, unsigned height)
{
	return 0;
}

void
sw32_SCR_ScreenShot_f (void)
{
	dstring_t  *pcxname = dstring_new ();
	pcx_t      *pcx = 0;
	int         pcx_len;

	// find a file name to save it to
	if (!QFS_NextFilename (pcxname, va (0, "%s/qf",
										qfs_gamedir->dir.shots), ".pcx")) {
		Sys_Printf ("SCR_ScreenShot_f: Couldn't create a PCX");
	} else {
		// enable direct drawing of console to back buffer
		sw32_D_EnableBackBufferAccess ();

		// save the pcx file
		switch(sw32_ctx->pixbytes) {
		case 1:
			pcx = EncodePCX (vid.buffer, vid.width, vid.height, vid.rowbytes,
							 vid.basepal, false, &pcx_len);
			break;
		case 2:
			Sys_Printf("SCR_ScreenShot_f: FIXME - add 16bit support\n");
			break;
		case 4:
			Sys_Printf("SCR_ScreenShot_f: FIXME - add 32bit support\n");
			break;
		default:
			Sys_Error("SCR_ScreenShot_f: unsupported r_pixbytes %i", sw32_ctx->pixbytes);
		}

		// for adapters that can't stay mapped in for linear writes all the time
		sw32_D_DisableBackBufferAccess ();

		if (pcx) {
			QFS_WriteFile (pcxname->str, pcx, pcx_len);
			Sys_Printf ("Wrote %s/%s\n", qfs_userpath, pcxname->str);
		}
	}
	dstring_delete (pcxname);
}

void
sw32_R_RenderFrame (SCR_Func *scr_funcs)
{
	vrect_t     vrect;

	// do 3D refresh drawing, and then update the screen
	sw32_D_EnableBackBufferAccess ();		// of all overlay stuff if drawing
										// directly

	if (vr_data.scr_fullupdate++ < vid.numpages) {	// clear the entire screen
		vr_data.scr_copyeverything = 1;
		sw32_Draw_TileClear (0, 0, vid.width, vid.height);
	}

	pconupdate = NULL;

	SCR_SetUpToDrawConsole ();

	sw32_D_DisableBackBufferAccess ();		// for adapters that can't stay mapped
										// in for linear writes all the time
	VID_LockBuffer ();
	sw32_R_RenderView ();
	VID_UnlockBuffer ();

	sw32_D_EnableBackBufferAccess ();		// of all overlay stuff if drawing
										// directly

	view_draw (vr_data.scr_view);
	while (*scr_funcs) {
		(*scr_funcs)();
		scr_funcs++;
	}

	sw32_D_DisableBackBufferAccess ();		// for adapters that can't stay mapped
										// in for linear writes all the time
	if (pconupdate) {
		sw32_D_UpdateRects (pconupdate);
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
	sw32_ctx->update (sw32_ctx, &vrect);
}
