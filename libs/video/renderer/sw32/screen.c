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
static const char rcsid[] = 
	"$Id$";

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

#include "QF/console.h"
#include "QF/cvar.h"
#include "QF/draw.h"
#include "QF/pcx.h"
#include "QF/quakefs.h"
#include "QF/render.h"
#include "QF/screen.h"
#include "QF/sys.h"
#include "QF/texture.h"

#include "compat.h"
#include "r_cvar.h"
#include "r_dynamic.h"
#include "r_local.h"
#include "r_screen.h"
#include "sbar.h"
#include "view.h"

/*
	SCR_CalcRefdef

	Must be called whenever vid changes
	Internal use only
*/
static void
SCR_CalcRefdef (void)
{
	vrect_t     vrect;
	float       size;
	int         h;
	qboolean    full = false;

	scr_fullupdate = 0;					// force a background redraw
	vid.recalc_refdef = 0;

	// force the status bar to redraw
	Sbar_Changed ();

	// bound viewsize
	Cvar_SetValue (scr_viewsize, bound (30, scr_viewsize->int_val, 120));

	// bound field of view
	Cvar_SetValue (scr_fov, bound (1, scr_fov->value, 170));

	if (scr_viewsize->int_val >= 120)
		sb_lines = 0;					// no status bar at all
	else if (scr_viewsize->int_val >= 110)
		sb_lines = 24;					// no inventory
	else
		sb_lines = 24 + 16 + 8;

	if (scr_viewsize->int_val >= 100) {
		full = true;
		size = 100.0;
	} else {
		size = scr_viewsize->int_val;
	}
	// intermission is always full screen
	if (r_force_fullscreen /* FIXME: better test */) {
		full = true;
		size = 100.0;
		sb_lines = 0;
	}
	size /= 100.0;

	h = vid.height - r_lineadj;

	r_refdef.vrect.width = vid.width * size + 0.5;
	if (r_refdef.vrect.width < 96) {
		size = 96.0 / r_refdef.vrect.width;
		r_refdef.vrect.width = 96;		// min for icons
	}

	r_refdef.vrect.height = vid.height * size + 0.5;
	if (r_refdef.vrect.height > h)
		r_refdef.vrect.height = h;
	r_refdef.vrect.x = (vid.width - r_refdef.vrect.width) / 2;
	if (full)
		r_refdef.vrect.y = 0;
	else
		r_refdef.vrect.y = (h - r_refdef.vrect.height) / 2;

	r_refdef.fov_x = scr_fov->value;
	r_refdef.fov_y =
		CalcFov (r_refdef.fov_x, r_refdef.vrect.width, r_refdef.vrect.height);

	scr_vrect = r_refdef.vrect;

	// these calculations mirror those in R_Init() for r_refdef, but take no
	// account of water warping
	vrect.x = 0;
	vrect.y = 0;
	vrect.width = vid.width;
	vrect.height = vid.height;

	R_SetVrect (&vrect, &scr_vrect, r_lineadj);

	// guard against going from one mode to another that's less than half the
	// vertical resolution
	if (scr_con_current > vid.height)
		scr_con_current = vid.height;

	// notify the refresh of the change
	R_ViewChanged (&vrect, r_lineadj, vid.aspect);
}


void
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
		int g1, g2, x, y;
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
		int x, y;
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

tex_t *
SCR_ScreenShot (int width, int height)
{
	return 0;
}

void
SCR_ScreenShot_f (void)
{
	char        pcxname[MAX_OSPATH];
	pcx_t      *pcx;
	int         pcx_len;

	// find a file name to save it to 
	if (!COM_NextFilename (pcxname, "qf", ".pcx")) {
		Con_Printf ("SCR_ScreenShot_f: Couldn't create a PCX");
		return;
	}

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

	Con_Printf ("Wrote %s\n", pcxname);
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
	static int  oldviewsize;
	vrect_t     vrect;

	if (scr_skipupdate || block_drawing)
		return;

	r_realtime = realtime;

	scr_copytop = 0;
	scr_copyeverything = 0;

	if (!scr_initialized)
		return;							// not initialized yet

	if (oldviewsize != scr_viewsize->int_val) {
		oldviewsize = scr_viewsize->int_val;
		vid.recalc_refdef = true;
	}

	if (oldfov != scr_fov->value) {		// determine size of refresh window
		oldfov = scr_fov->value;
		vid.recalc_refdef = true;
	}

	if (oldscreensize != scr_viewsize->int_val) {
		oldscreensize = scr_viewsize->int_val;
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

	if (r_force_fullscreen /*FIXME*/ == 1 && key_dest == key_game) {
		Sbar_IntermissionOverlay ();
	} else if (r_force_fullscreen /*FIXME*/ == 2 && key_dest == key_game) {
		Sbar_FinaleOverlay ();
		SCR_CheckDrawCenterString ();
	} else {
		while (*scr_funcs) {
			(*scr_funcs)();
			scr_funcs++;
		}
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
		vrect.height = vid.height - sb_lines;
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
