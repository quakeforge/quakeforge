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

#include "QF/cmd.h"
#include "QF/console.h"
#include "QF/cvar.h"
#include "QF/draw.h"
#include "QF/keys.h"
#include "QF/pcx.h"
#include "QF/render.h"
#include "QF/screen.h"
#include "QF/sys.h"
#include "QF/texture.h"
#include "QF/vfs.h"
#include "QF/vid.h"

#include "compat.h"
#include "d_iface.h"
#include "r_cvar.h"
#include "r_local.h"
#include "sbar.h"
#include "view.h"

/*
  background clear
  rendering
  turtle/net/ram icons
  sbar
  centerprint / slow centerprint
  notify lines
  intermission / finale overlay
  loading plaque
  console
  menu

  required background clears
  required update regions

  syncronous draw mode or async
  One off screen buffer, with updates either copied or xblited
  Need to double buffer?

  async draw will require the refresh area to be cleared, because it will be
  xblited, but sync draw can just ignore it.

  sync
  draw

  CenterPrint ()
  SlowPrint ()
  Screen_Update ();
  Con_Printf ();

  net
  turn off messages option

  the refresh is always rendered, unless the console is full screen

  console is:
	notify lines
	half
	full
*/

// only the refresh window will be updated unless these variables are flagged 
int         scr_copytop;
int         scr_copyeverything;

float       scr_con_current;
float       scr_conlines;				// lines of console to display

int         oldscreensize, oldfov;
int         oldsbar;

qboolean    scr_initialized;			// ready to draw

qpic_t     *scr_ram;
qpic_t     *scr_net;
qpic_t     *scr_turtle;

int         scr_fullupdate;

int         clearconsole;
int         clearnotify;


viddef_t    vid;						// global video state

vrect_t    *pconupdate;
vrect_t     scr_vrect;

qboolean    scr_disabled_for_loading;

qboolean    scr_skipupdate;

qboolean    block_drawing;

void        SCR_ScreenShot_f (void);

/*
	CENTER PRINTING
*/

char        scr_centerstring[1024];
float       scr_centertime_start;		// for slow victory printing
float       scr_centertime_off;
int         scr_center_lines;
int         scr_erase_lines;
int         scr_erase_center;


/*
	SCR_CenterPrint

	Called for important messages that should stay in the center of the screen
	for a few moments
*/
void
SCR_CenterPrint (const char *str)
{
	strncpy (scr_centerstring, str, sizeof (scr_centerstring) - 1);
	scr_centertime_off = scr_centertime->value;
	scr_centertime_start = r_realtime;

	// count the number of lines for centering
	scr_center_lines = 1;
	while (*str) {
		if (*str == '\n')
			scr_center_lines++;
		str++;
	}
}

void
SCR_DrawCenterString (void)
{
	char       *start;
	int         l;
	int         j;
	int         x, y;
	int         remaining;

	// the finale prints the characters one at a time
	if (r_force_fullscreen /*FIXME*/)
		remaining = scr_printspeed->value * (r_realtime -
											 scr_centertime_start);
	else
		remaining = 9999;

	scr_erase_center = 0;
	start = scr_centerstring;

	if (scr_center_lines <= 4)
		y = vid.height * 0.35;
	else
		y = 48;

	do {
		// scan the width of the line
		for (l = 0; l < 40; l++)
			if (start[l] == '\n' || !start[l])
				break;
		x = (vid.width - l * 8) / 2;
		for (j = 0; j < l; j++, x += 8) {
			Draw_Character (x, y, start[j]);
			if (!remaining--)
				return;
		}

		y += 8;

		while (*start && *start != '\n')
			start++;

		if (!*start)
			break;
		start++;						// skip the \n
	} while (1);
}

void
SCR_CheckDrawCenterString (void)
{
	scr_copytop = 1;
	if (scr_center_lines > scr_erase_lines)
		scr_erase_lines = scr_center_lines;

	scr_centertime_off -= r_frametime;

	if (scr_centertime_off <= 0 && !r_force_fullscreen /*FIXME*/)
		return;
	if (key_dest != key_game)
		return;

	SCR_DrawCenterString ();
}

//=============================================================================

float
CalcFov (float fov_x, float width, float height)
{
	float       a;
	float       x;

	if (fov_x < 1 || fov_x > 179)
		Sys_Error ("Bad fov: %f", fov_x);

	x = width / tan (fov_x / 360 * M_PI);

	a = (x == 0) ? 90 : atan (height / x);	// 0 shouldn't happen

	a = a * 360 / M_PI;

	return a;
}

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

//========================================

	// bound viewsize
	Cvar_SetValue (scr_viewsize, bound (30, scr_viewsize->int_val, 120));

	// bound field of view
	Cvar_SetValue (scr_fov, bound (10, scr_fov->value, 170));

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
	if (r_force_fullscreen /*FIXME*/) {
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

	r_refdef.fov_x = scr_fov->int_val;
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
	basepal = vid_basepal;
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

/*
	SCR_SizeUp_f

	Keybinding command
*/
void
SCR_SizeUp_f (void)
{
	if (scr_viewsize->int_val < 120) {
		Cvar_SetValue (scr_viewsize, scr_viewsize->int_val + 10);
		vid.recalc_refdef = 1;
	}
}

/*
	SCR_SizeDown_f

	Keybinding command
*/
void
SCR_SizeDown_f (void)
{
	Cvar_SetValue (scr_viewsize, scr_viewsize->int_val - 10);
	vid.recalc_refdef = 1;
}

//============================================================================

void
SCR_Init (void)
{
	// register our commands
	Cmd_AddCommand ("screenshot", SCR_ScreenShot_f, "Take a screenshot and "
					"write it as qfxxx.tga in the current directory");
	Cmd_AddCommand ("sizeup", SCR_SizeUp_f, "Increase the size of the screen");
	Cmd_AddCommand ("sizedown", SCR_SizeDown_f, "Decrease the size of the "
					"screen");

	scr_ram = Draw_PicFromWad ("ram");
	scr_net = Draw_PicFromWad ("net");
	scr_turtle = Draw_PicFromWad ("turtle");

	scr_initialized = true;
}

void
SCR_DrawRam (void)
{
	if (!scr_showram->int_val)
		return;

	if (!r_cache_thrash)
		return;

	Draw_Pic (scr_vrect.x + 32, scr_vrect.y, scr_ram);
}

void
SCR_DrawTurtle (void)
{
	static int  count;

	if (!scr_showturtle->int_val)
		return;

	if (r_frametime < 0.1) {
		count = 0;
		return;
	}

	count++;
	if (count < 3)
		return;

	Draw_Pic (scr_vrect.x, scr_vrect.y, scr_turtle);
}


void
SCR_DrawFPS (void)
{
	static double lastframetime;
	double      t;
	extern int  fps_count;
	static int  lastfps;
	int         i, x, y;
	char        st[80];

	if (!show_fps->int_val)
		return;

	t = Sys_DoubleTime ();
	if ((t - lastframetime) >= 1.0) {
		lastfps = fps_count;
		fps_count = 0;
		lastframetime = t;
	}
	snprintf (st, sizeof (st), "%3d FPS", lastfps);
	if (show_time->int_val <= 0) {
		i = 8;
	} else if (show_time->int_val == 1) {
		i = 56;
	} else {
		i = 80;
	}
	x = hudswap ? vid.width - ((strlen (st) * 8) + i) : i;
	y = vid.height - (sb_lines + 8);
	Draw_String (x, y, st);
}

/*
	SCR_DrawTime

	Draw a clock on the screen
	Written by Misty, rewritten by Deek
*/
void
SCR_DrawTime (void)
{
	int 		x, y;
	char		st[80];

	time_t		utc = 0;
	struct tm	*local = NULL;
	char		*timefmt = NULL;

	// any cvar that can take multiple settings must be able to handle abuse. 
	if (show_time->int_val <= 0)
		return;

	// Get local time
	utc = time (NULL);
	local = localtime (&utc);

	if (show_time->int_val == 1) {	// Use international format
		timefmt = "%k:%M";
	} else if (show_time->int_val >= 2) {	// US AM/PM display
		timefmt = "%l:%M %P";
	}

	// Print it next to the fps meter
	strftime (st, sizeof (st), timefmt, local);
	x = hudswap ? (vid.width - ((strlen (st) * 8) + 8)) : 8;
	y = vid.height - (sb_lines + 8);
	Draw_String (x, y, st);
}

void
SCR_DrawPause (void)
{
	qpic_t     *pic;

	if (!scr_showpause->int_val)		// turn off for screenshots
		return;

	if (!r_paused)
		return;

	pic = Draw_CachePic ("gfx/pause.lmp", true);
	Draw_Pic ((vid.width - pic->width) / 2,
			  (vid.height - 48 - pic->height) / 2, pic);
}

//=============================================================================

void
SCR_SetUpToDrawConsole (void)
{
	Con_CheckResize ();

	// decide on the height of the console
	if (!r_active) {
		scr_conlines = vid.height;		// full screen
		scr_con_current = scr_conlines;
	} else if (key_dest == key_console)
		scr_conlines = vid.height * bound (0.2, scr_consize->value, 1);
	else
		scr_conlines = 0;				// none visible

	if (scr_conlines < scr_con_current) {
		scr_con_current -= scr_conspeed->value * r_frametime;
		if (scr_conlines > scr_con_current)
			scr_con_current = scr_conlines;

	} else if (scr_conlines > scr_con_current) {
		scr_con_current += scr_conspeed->value * r_frametime;
		if (scr_conlines < scr_con_current)
			scr_con_current = scr_conlines;
	}

	if (clearconsole++ < vid.numpages) {
		Sbar_Changed ();
	} else if (clearnotify++ < vid.numpages) {
	} else
		con_notifylines = 0;
}

void
SCR_DrawConsole (void)
{
	if (scr_con_current) {
		scr_copyeverything = 1;
		Con_DrawConsole (scr_con_current);
		Con_DrawDownload (scr_con_current);
		clearconsole = 0;
	} else {
		if (key_dest == key_game || key_dest == key_message)
			Con_DrawNotify ();			// only draw notify in game
	}
}

/*
	SCREEN SHOTS
*/

tex_t *
SCR_ScreenShot (int width, int height)
{
	int         x, y;
	unsigned char *src, *dest;
	int         w, h;
	int         dx, dy, dex, dey, nx;
	int         r, b, g;
	int         count;
	float       fracw, frach;
	tex_t      *tex;

	// enable direct drawing of console to back buffer
	D_EnableBackBufferAccess ();

	w = (vid.width < width) ? vid.width : width;
	h = (vid.height < height) ? vid.height : height;

	fracw = (float) vid.width / (float) w;
	frach = (float) vid.height / (float) h;

	tex = Hunk_TempAlloc (field_offset (tex_t, data[w * h]));
	if (!tex)
		return 0;

	for (y = 0; y < h; y++) {
		dest = tex->data + (w * y);

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
			for ( /* */ ; dy < dey; dy++) {
				src = vid.buffer + (vid.rowbytes * dy) + dx;
				for (nx = dx; nx < dex; nx++) {
					r += vid_basepal[*src * 3];
					g += vid_basepal[*src * 3 + 1];
					b += vid_basepal[*src * 3 + 2];
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
	pcx = EncodePCX (vid.buffer, vid.width, vid.height, vid.rowbytes,
					 vid_basepal, false, &pcx_len);
	COM_WriteFile (pcxname, pcx, pcx_len);


	// for adapters that can't stay mapped in for linear writes all the time
	D_DisableBackBufferAccess ();

	Con_Printf ("Wrote %s\n", pcxname);
}

/*
	Find closest color in the palette for named color
*/
int
MipColor (int r, int g, int b)
{
	int         i;
	float       dist;
	int         best = 0;
	float       bestdist;
	int         r1, g1, b1;
	static int  lr = -1, lg = -1, lb = -1;
	static int  lastbest;

	if (r == lr && g == lg && b == lb)
		return lastbest;

	bestdist = 256 * 256 * 3;

	for (i = 0; i < 256; i++) {
		r1 = vid_basepal[i * 3] - r;
		g1 = vid_basepal[i * 3 + 1] - g;
		b1 = vid_basepal[i * 3 + 2] - b;
		dist = r1 * r1 + g1 * g1 + b1 * b1;
		if (dist < bestdist) {
			bestdist = dist;
			best = i;
		}
	}
	lr = r;
	lg = g;
	lb = b;
	lastbest = best;
	return best;
}

// in draw.c

void
SCR_DrawCharToSnap (int num, byte * dest, int width)
{
	int         row, col;
	byte       *source;
	int         drawline;
	int         x;

	row = num >> 4;
	col = num & 15;
	source = draw_chars + (row << 10) + (col << 3);

	drawline = 8;

	while (drawline--) {
		for (x = 0; x < 8; x++)
			if (source[x])
				dest[x] = source[x];
			else
				dest[x] = 98;
		source += 128;
		dest += width;
	}

}

void
SCR_DrawStringToSnap (const char *s, tex_t *tex, int x, int y)
{
	byte       *buf = tex->data;
	byte       *dest;
	const unsigned char *p;
	int width = tex->width;

	dest = buf + ((y * width) + x);

	p = (const unsigned char *) s;
	while (*p) {
		SCR_DrawCharToSnap (*p++, dest, width);
		dest += 8;
	}
}

//=============================================================================

char       *scr_notifystring;

void
SCR_DrawNotifyString (void)
{
	char       *start;
	int         l;
	int         j;
	int         x, y;

	start = scr_notifystring;

	y = vid.height * 0.35;

	do {
		// scan the width of the line
		for (l = 0; l < 40; l++)
			if (start[l] == '\n' || !start[l])
				break;
		x = (vid.width - l * 8) / 2;
		for (j = 0; j < l; j++, x += 8)
			Draw_Character (x, y, start[j]);

		y += 8;

		while (*start && *start != '\n')
			start++;

		if (!*start)
			break;
		start++;						// skip the \n
	} while (1);
}

//=============================================================================

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
	static int  oldscr_viewsize;
	vrect_t     vrect;

	if (scr_skipupdate || block_drawing)
		return;

	if (scr_disabled_for_loading)
		return;

	r_realtime = realtime;

	scr_copytop = 0;
	scr_copyeverything = 0;

	if (!scr_initialized || !con_initialized)
		return;							// not initialized yet

	if (scr_viewsize->int_val != oldscr_viewsize) {
		oldscr_viewsize = scr_viewsize->int_val;
		vid.recalc_refdef = 1;
	}

	// check for vid changes
	if (oldfov != scr_fov->int_val) {
		oldfov = scr_fov->int_val;
		vid.recalc_refdef = true;
	}

	if (oldscreensize != scr_viewsize->int_val) {
		oldscreensize = scr_viewsize->int_val;
		vid.recalc_refdef = true;
	}

	if (vid.recalc_refdef) {
		// something changed, so reorder the screen
		SCR_CalcRefdef ();
	}

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
