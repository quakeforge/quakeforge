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

#include "QF/console.h"
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
#include "r_dynamic.h"
#include "r_local.h"
#include "r_screen.h"
#include "sbar.h"
#include "view.h"

static void
SCR_ApplyBlend (void)		// Used to be V_UpdatePalette
{
	int         r, g, b, i;
	byte       *basepal, *newpal;
	byte        pal[768];

	switch(r_pixbytes) {
	case 1:
	{
		basepal = vid.basepal;
		newpal = pal;

		for (i = 0; i < 256; i++) {
			r = basepal[0];
			g = basepal[1];
			b = basepal[2];
			basepal += 3;

			r += (int) (v_blend[3] * (v_blend[0] * 256 - r));
			g += (int) (v_blend[3] * (v_blend[1] * 256 - g));
			b += (int) (v_blend[3] * (v_blend[2] * 256 -g));

			newpal[0] = gammatable[r];
			newpal[1] = gammatable[g];
			newpal[2] = gammatable[b];
			newpal += 3;
		}
		VID_ShiftPalette (pal);
	}
	break;
	case 2:
	{
		unsigned int g1, g2, x, y;
		unsigned short rramp[32], gramp[64], bramp[32], *temp;
		for (i = 0; i < 32; i++) {
			r = i << 3;
			g1 = i << 3;
			g2 = g1 + 4;
			b = i << 3;

			r += (int) (v_blend[3] * (v_blend[0] * 256 - r));
			g1 += (int) (v_blend[3] * (v_blend[1] - g1));
			g2 += (int) (v_blend[3] * (v_blend[1] - g2));
			b += (int) (v_blend[3] * (v_blend[2] - b));

			rramp[i] = (gammatable[r] << 8) & 0xF800;
			gramp[i*2+0] = (gammatable[g1] << 3) & 0x07E0;
			gramp[i*2+1] = (gammatable[g2] << 3) & 0x07E0;
			bramp[i] = (gammatable[b] >> 3) & 0x001F;
		}
		temp = vid.buffer;
		for (y = 0;y < vid.height;y++, temp += (vid.rowbytes >> 1))
			for (x = 0;x < vid.width;x++)
				temp[x] = rramp[(temp[x] & 0xF800) >> 11]
					+ gramp[(temp[x] & 0x07E0) >> 5] + bramp[temp[x] & 0x001F];
	}
	break;
	case 4:
	{
		unsigned int x, y;
		byte ramp[256][4], *temp;
		for (i = 0; i < 256; i++) {
			r = i;
			g = i;
			b = i;

			r += (int) (v_blend[3] * (v_blend[0] * 256 - r));
			g += (int) (v_blend[3] * (v_blend[1] * 256 - g));
			b += (int) (v_blend[3] * (v_blend[2] * 256 - b));

			ramp[i][0] = gammatable[r];
			ramp[i][1] = gammatable[g];
			ramp[i][2] = gammatable[b];
			ramp[i][3] = 0;
		}
		temp = vid.buffer;
		for (y = 0;y < vid.height;y++, temp += vid.rowbytes)
		{
			for (x = 0;x < vid.width;x++)
			{
				temp[x*4+0] = ramp[temp[x*4+0]][0];
				temp[x*4+1] = ramp[temp[x*4+1]][1];
				temp[x*4+2] = ramp[temp[x*4+2]][2];
				temp[x*4+3] = 0;
			}
		}
	}
	break;
	default:
		Sys_Error("V_UpdatePalette: unsupported r_pixbytes %i", r_pixbytes);
	}
}

/* SCREEN SHOTS */

VISIBLE tex_t *
SCR_ScreenShot (int width, int height)
{
	return 0;
}

void
SCR_ScreenShot_f (void)
{
	dstring_t  *pcxname = dstring_new ();
	pcx_t      *pcx = 0;
	int         pcx_len;

	// find a file name to save it to 
	if (!QFS_NextFilename (pcxname,
						   va ("%s/qf", qfs_gamedir->dir.def), ".pcx")) {
		Con_Printf ("SCR_ScreenShot_f: Couldn't create a PCX");
	} else {
		// enable direct drawing of console to back buffer
		D_EnableBackBufferAccess ();

		// save the pcx file
		switch(r_pixbytes) {
		case 1:
			pcx = EncodePCX (vid.buffer, vid.width, vid.height, vid.rowbytes,
							 vid.basepal, false, &pcx_len);
			break;
		case 2:
			Con_Printf("SCR_ScreenShot_f: FIXME - add 16bit support\n");
			break;
		case 4:
			Con_Printf("SCR_ScreenShot_f: FIXME - add 32bit support\n");
			break;
		default:
			Sys_Error("SCR_ScreenShot_f: unsupported r_pixbytes %i", r_pixbytes);
		}

		// for adapters that can't stay mapped in for linear writes all the time
		D_DisableBackBufferAccess ();

		if (pcx) {
			QFS_WriteFile (pcxname->str, pcx, pcx_len);
			Con_Printf ("Wrote %s/%s\n", qfs_userpath, pcxname->str);
		}
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
VISIBLE void
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
