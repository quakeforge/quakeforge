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
#include "QF/pcx.h"
#include "QF/render.h"
#include "QF/screen.h"
#include "QF/sys.h"
#include "QF/texture.h"

#include "compat.h"
#include "r_cvar.h"
#include "r_local.h"
#include "r_screen.h"
#include "sbar.h"

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

int         oldscreensize;
float       oldfov;
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

qboolean    scr_skipupdate;

qboolean    block_drawing;

/* CENTER PRINTING */

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

static void
SCR_DrawCenterString (void)
{
	char       *start;
	int         remaining, j, l, x, y;

	// the finale prints the characters one at a time
	if (r_force_fullscreen /* FIXME: better test */)
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

	if (scr_centertime_off <= 0 && !r_force_fullscreen /*FIXME: better test*/)
		return;
	if (key_dest != key_game)
		return;

	SCR_DrawCenterString ();
}

float
CalcFov (float fov_x, float width, float height)
{
	float       a, x;

	if (fov_x < 1 || fov_x > 179)
		Sys_Error ("Bad fov: %f", fov_x);

	x = width / tan (fov_x * (M_PI / 360));

	a = (x == 0) ? 90 : atan (height / x);	// 0 shouldn't happen

	a = a * (360 / M_PI);

	return a;
}

/*
	SCR_SizeUp_f

	Keybinding command
*/
static void
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
static void
SCR_SizeDown_f (void)
{
	Cvar_SetValue (scr_viewsize, scr_viewsize->int_val - 10);
	vid.recalc_refdef = 1;
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

extern int    fps_count;	//FIXME

void
SCR_DrawFPS (void)
{
	char          st[80];
	double        t;
	static double lastframetime;
	int           i, x, y;
	static int    lastfps;

	if (!show_fps->int_val)
		return;

	t = Sys_DoubleTime ();
	if ((t - lastframetime) >= 1.0) {
		lastfps = fps_count;
		fps_count = 0;
		lastframetime = t;
	}
	snprintf (st, sizeof (st), "%3d FPS", lastfps);

	// FIXME! This is evil. -- Deek
	// calculate the location of the clock
	if (show_time->int_val <= 0) {
		i = 8;
	} else if (show_time->int_val == 1) {
		i = 56;
	} else {
		i = 80;
	}

	x = hudswap ? vid.width - ((strlen (st) * 8) + i) : i;
	y = vid.height - sb_lines - 8;
	Draw_String (x, y, st);
}

/*
	SCR_DrawTime

	Draw a clock on the screen
	Written by Misty, rewritten by Deek.
*/
void
SCR_DrawTime (void)
{
	char        st[80];
	const char *timefmt = NULL;
	int         x, y;
	struct tm  *local = NULL;
	time_t      utc = 0;

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
	strftime (st, sizeof (st), timefmt, local);

	// Print it at far left/right of screen
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

	if (scr_con_current >= vid.height - sb_lines)
		scr_copyeverything = 1;
	if (scr_conlines < scr_con_current) {
		scr_con_current -= scr_conspeed->value * r_frametime;
		if (scr_conlines > scr_con_current)
			scr_con_current = scr_conlines;

	} else if (scr_conlines > scr_con_current) {
		scr_con_current += scr_conspeed->value * r_frametime;
		if (scr_conlines < scr_con_current)
			scr_con_current = scr_conlines;
	}
	if (scr_con_current >= vid.height - sb_lines)
		scr_copyeverything = 1;

	if (clearconsole++ < vid.numpages)
		Sbar_Changed ();
}

void
SCR_DrawConsole (void)
{
	Con_DrawConsole (scr_con_current);
}

/*
	Find closest color in the palette for named color
*/
int
MipColor (int r, int g, int b)
{
	float       bestdist, dist;
	int         r1, g1, b1, i;
	int         best = 0;
	static int  lastbest;
	static int  lr = -1, lg = -1, lb = -1;

	if (r == lr && g == lg && b == lb)
		return lastbest;

	bestdist = 256 * 256 * 3;

	for (i = 0; i < 256; i++) {
		int         j;
		j = i * 3;
		r1 = vid.palette[j] - r;
		g1 = vid.palette[j + 1] - g;
		b1 = vid.palette[j + 2] - b;
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

static void
SCR_DrawCharToSnap (int num, byte * dest, int width)
{
	byte       *source;
	int         col, row, drawline, x;

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
		dest -= width;
	}

}

void
SCR_DrawStringToSnap (const char *s, tex_t *tex, int x, int y)
{
	byte       *dest;
	byte       *buf = tex->data;
	const unsigned char *p;
	int         width = tex->width;

	dest = buf + ((y * width) + x);

	p = (const unsigned char *) s;
	while (*p) {
		SCR_DrawCharToSnap (*p++, dest, width);
		dest += 8;
	}
}

void
SCR_Init (void)
{
	// register our commands
	Cmd_AddCommand ("screenshot", SCR_ScreenShot_f, "Take a screenshot, "
					"saves as qfxxx.pcx in the current directory");
	Cmd_AddCommand ("sizeup", SCR_SizeUp_f, "Increases the screen size");
	Cmd_AddCommand ("sizedown", SCR_SizeDown_f, "Decreases the screen size");

	scr_ram = Draw_PicFromWad ("ram");
	scr_net = Draw_PicFromWad ("net");
	scr_turtle = Draw_PicFromWad ("turtle");

	scr_initialized = true;
}
