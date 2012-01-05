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

static __attribute__ ((used)) const char rcsid[] = 
	"$Id$";

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

#include "compat.h"
#include "r_cvar.h"
#include "r_local.h"
#include "r_screen.h"
#include "sbar.h"
#include "clview.h"	//FIXME

static void
SCR_ApplyBlend (void)		// Used to be V_UpdatePalette
{
	int         r, g, b, i;
	byte       *basepal, *newpal;
	byte        pal[768];
	basepal = vid.basepal;
	newpal = pal;

	for (i = 0; i < 256; i++) {
		r = basepal[0];
		g = basepal[1];
		b = basepal[2];
		basepal += 3;

		r += (int) (v_blend[3] * (v_blend[0] * 256 - r));
		g += (int) (v_blend[3] * (v_blend[1] * 256 - g));
		b += (int) (v_blend[3] * (v_blend[2] * 256 - b));

		newpal[0] = gammatable[r];
		newpal[1] = gammatable[g];
		newpal[2] = gammatable[b];
		newpal += 3;
	}
	VID_ShiftPalette (pal);
}

/* SCREEN SHOTS */

tex_t *
SCR_ScreenShot (int width, int height)
{
	unsigned char *src, *dest;
	float          fracw, frach;
	int            count, dex, dey, dx, dy, nx, r, g, b, x, y, w, h;
	tex_t         *tex;

	// enable direct drawing of console to back buffer
	D_EnableBackBufferAccess ();

	w = (vid.width < (unsigned int) width) ? vid.width
										   : (unsigned int) width;
	h = (vid.height < (unsigned int) height) ? vid.height
											 : (unsigned int) height;

	fracw = (float) vid.width / (float) w;
	frach = (float) vid.height / (float) h;

	tex = malloc (field_offset (tex_t, data[w * h]));
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
						   va ("%s/qf", qfs_gamedir->dir.shots), ".pcx")) {
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

/*
	SCR_UpdateScreen

	This is called every frame, and can also be called explicitly to flush
	text to the screen.

	WARNING: be very careful calling this from elsewhere, because the refresh
	needs almost the entire 256k of stack space!
*/
void
SCR_UpdateScreen (double realtime, SCR_Func *scr_funcs)
{
	vrect_t     vrect;

	if (scr_skipupdate)
		return;

	r_realtime = realtime;

	scr_copytop = 0;
	scr_copyeverything = 0;

	if (!scr_initialized)
		return;							// not initialized yet

	if (oldfov != scr_fov->value) {		// determine size of refresh window
		oldfov = scr_fov->value;
		vid.recalc_refdef = true;
	}

	if (vid.recalc_refdef)
		SCR_CalcRefdef ();

	// do 3D refresh drawing, and then update the screen
	D_EnableBackBufferAccess ();		// of all overlay stuff if drawing
										// directly

	if (scr_fullupdate++ < vid.numpages) {	// clear the entire screen
		scr_copyeverything = 1;
		Draw_TileClear (0, 0, vid.width, vid.height);
		Sbar_Changed ();
	}

	pconupdate = NULL;

	SCR_SetUpToDrawConsole ();

	D_DisableBackBufferAccess ();		// for adapters that can't stay mapped
										// in for linear writes all the time
	VID_LockBuffer ();
	V_RenderView ();
	VID_UnlockBuffer ();

	D_EnableBackBufferAccess ();		// of all overlay stuff if drawing
										// directly

	while (*scr_funcs) {
		(*scr_funcs)();
		scr_funcs++;
	}

	D_DisableBackBufferAccess ();		// for adapters that can't stay mapped
										// in for linear writes all the time
	if (pconupdate) {
		D_UpdateRects (pconupdate);
	}

	SCR_ApplyBlend ();

	// update one of three areas
	if (scr_copyeverything) {
		vrect.x = 0;
		vrect.y = 0;
		vrect.width = vid.width;
		vrect.height = vid.height;
		vrect.pnext = 0;

		VID_Update (&vrect);
	} else if (scr_copytop) {
		vrect.x = 0;
		vrect.y = 0;
		vrect.width = vid.width;
		vrect.height = vid.height - r_lineadj;
		vrect.pnext = 0;

		VID_Update (&vrect);
	} else {
		vrect.x = scr_vrect.x;
		vrect.y = scr_vrect.y;
		vrect.width = scr_vrect.width;
		vrect.height = scr_vrect.height;
		vrect.pnext = 0;

		VID_Update (&vrect);
	}
}
