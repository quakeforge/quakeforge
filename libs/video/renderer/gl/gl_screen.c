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
#include "QF/quakefs.h"     // MAX_OSPATH
#include "QF/render.h"  // r_refdef
#include "QF/screen.h"
#include "QF/sys.h"
#include "QF/texture.h"
#include "QF/tga.h"
#include "QF/GL/defines.h"
#include "QF/GL/funcs.h"
#include "QF/GL/qf_rmain.h"
#include "QF/GL/qf_vid.h"

#include "compat.h"
#include "r_cvar.h"
#include "r_dynamic.h"
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

int         glx, gly, glwidth, glheight;

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

vrect_t     scr_vrect;

qboolean    scr_disabled_for_loading;
float       scr_disabled_time;

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

void
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

static float
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
	SCR_CalcRefdef

	Must be called whenever vid changes
	Internal use only
*/
static void
SCR_CalcRefdef (void)
{
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
	if (r_refdef.vrect.height > vid.height - r_lineadj)
		r_refdef.vrect.height = vid.height - r_lineadj;
	r_refdef.vrect.x = (vid.width - r_refdef.vrect.width) / 2;
	if (full)
		r_refdef.vrect.y = 0;
	else
		r_refdef.vrect.y = (h - r_refdef.vrect.height) / 2;

	r_refdef.fov_x = scr_fov->value;
	r_refdef.fov_y =
		CalcFov (r_refdef.fov_x, r_refdef.vrect.width, r_refdef.vrect.height);

	scr_vrect = r_refdef.vrect;
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
	char          st[80];
	double        t;
	static double lastframetime;
	int           i, x, y;
	static int    lastfps;

	extern int    fps_count;	//FIXME

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
	char		st[80];
	char	   *timefmt = NULL;
	int 		x, y;
	struct tm  *local = NULL;
	time_t		utc = 0;

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

/* SCREEN SHOTS */

tex_t *
SCR_ScreenShot (int width, int height)
{
	unsigned char *src, *dest, *snap;
	float          fracw, frach;
	int            count, dex, dey, dx, dy, nx, r, g, b, x, y, w, h;
	tex_t         *tex;

	snap = Hunk_TempAlloc (vid.width * vid.height * 3);

	qfglReadPixels (0, 0, vid.width, vid.height, GL_RGB, GL_UNSIGNED_BYTE,
				    snap);

	w = (vid.width < width) ? vid.width : width;
	h = (vid.height < height) ? vid.height : height;

	fracw = (float) vid.width / (float) w;
	frach = (float) vid.height / (float) h;

	tex = malloc (field_offset (tex_t, data[w * h]));
	if (!tex)
		return 0;

	tex->width = w;
	tex->height = h;
	tex->palette = vid.palette;

	for (y = 0; y < h; y++) {
		dest = snap + (w * 3 * y);

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
				src = snap + (vid.width * 3 * dy) + dx * 3;
				for (nx = dx; nx < dex; nx++) {
					r += *src++;
					g += *src++;
					b += *src++;
					count++;
				}
			}
			r /= count;
			g /= count;
			b /= count;
			*dest++ = r;
			*dest++ = g;
			*dest++ = b;
		}
	}

	// convert to eight bit
	for (y = 0; y < h; y++) {
		src = snap + (w * 3 * y);
		dest = tex->data + (w * y);

		for (x = 0; x < w; x++) {
			*dest++ = MipColor (src[0], src[1], src[2]);
			src += 3;
		}
	}

	return tex;
}

void
SCR_ScreenShot_f (void)
{
	byte       *buffer;
	char        pcxname[MAX_OSPATH];

	// find a file name to save it to 
	if (!COM_NextFilename (pcxname, "qf", ".tga")) {
		Con_Printf ("SCR_ScreenShot_f: Couldn't create a TGA file\n");
		return;
	}
	buffer = malloc (glwidth * glheight * 3);
	SYS_CHECKMEM (buffer);
	qfglReadPixels (glx, gly, glwidth, glheight, GL_BGR_EXT, GL_UNSIGNED_BYTE,
				  buffer);
	WriteTGAfile (pcxname, buffer, glwidth, glheight);
	free (buffer);
	Con_Printf ("Wrote %s\n", pcxname);
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
		static int	j;
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

// in gl_draw.c

void
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
	int          width = tex->width;

	dest = buf + ((y * width) + x);

	p = (const unsigned char *) s;
	while (*p) {
		SCR_DrawCharToSnap (*p++, dest, width);
		dest += 8;
	}
}

char       *scr_notifystring;

void
SCR_DrawNotifyString (void)
{
	char       *start;
	int         l, x, y;

	start = scr_notifystring;

	y = vid.height * 0.35;

	do {
		// scan the width of the line
		for (l = 0; l < 40; l++)
			if (start[l] == '\n' || !start[l])
				break;
		x = (vid.width - l * 8) / 2;
		Draw_nString (x, y, start, l);

		y += 8;

		while (*start && *start != '\n')
			start++;

		if (!*start)
			break;
		start++;						// skip the \n
	} while (1);
}

void
SCR_TileClear (void)
{
	if (r_refdef.vrect.x > 0) {
		// left
		Draw_TileClear (0, 0, r_refdef.vrect.x, vid.height - sb_lines);
		// right
		Draw_TileClear (r_refdef.vrect.x + r_refdef.vrect.width, 0,
						vid.width - r_refdef.vrect.x + r_refdef.vrect.width,
						vid.height - sb_lines);
	}
	if (r_refdef.vrect.y > 0) {
		// top
		Draw_TileClear (r_refdef.vrect.x, 0,
						r_refdef.vrect.x + r_refdef.vrect.width,
						r_refdef.vrect.y);
		// bottom
		Draw_TileClear (r_refdef.vrect.x,
						r_refdef.vrect.y + r_refdef.vrect.height,
						r_refdef.vrect.width,
						vid.height - sb_lines -
						(r_refdef.vrect.height + r_refdef.vrect.y));
	}
}

int		oldviewsize = 0;

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
	double      time1 = 0, time2;

	if (block_drawing)
		return;

	GL_EndRendering ();

	r_realtime = realtime;

	vid.numpages = 2 + gl_triplebuffer->int_val;

	scr_copytop = 0;
	scr_copyeverything = 0;

	if (scr_disabled_for_loading) {
		if (r_realtime - scr_disabled_time > 60) {
			scr_disabled_for_loading = false;
			Con_Printf ("load failed.\n");
		} else {
			return;
		}
	}

	if (!scr_initialized)
		return;							// not initialized yet

	if (oldviewsize != scr_viewsize->int_val) {
		oldviewsize = scr_viewsize->int_val;
		vid.recalc_refdef = true;
	}

	GL_BeginRendering (&glx, &gly, &glwidth, &glheight);

	if (r_speeds->int_val) {
		time1 = Sys_DoubleTime ();
		c_brush_polys = 0;
		c_alias_polys = 0;
	}

	if (oldfov != scr_fov->value) {		// determine size of refresh window
		oldfov = scr_fov->value;
		vid.recalc_refdef = true;
	}

	if (vid.recalc_refdef)
		SCR_CalcRefdef ();

	// do 3D refresh drawing, and then update the screen
	V_RenderView ();

	SCR_SetUpToDrawConsole ();
	GL_Set2D ();

	// also makes polyblend apply to whole screen
	if (v_blend[3]) {
		qfglDisable (GL_TEXTURE_2D);
		qfglBegin (GL_QUADS);

		qfglColor4fv (v_blend);
		qfglVertex2f (0, 0);
		qfglVertex2f (vid.width, 0);
		qfglVertex2f (vid.width, vid.height);
		qfglVertex2f (0, vid.height);

		qfglEnd ();
		qfglColor3ubv (color_white);
		qfglEnable (GL_TEXTURE_2D);
	}

	// draw any areas not covered by the refresh
	SCR_TileClear ();

	if (r_force_fullscreen /*FIXME better test*/ == 1 && key_dest ==
		key_game) {
		Sbar_IntermissionOverlay ();
	} else if (r_force_fullscreen /*FIXME better test*/ == 2 &&
			   key_dest == key_game) {
		Sbar_FinaleOverlay ();
		SCR_CheckDrawCenterString ();
	} else {
		while (*scr_funcs) {
			(*scr_funcs)();
			scr_funcs++;
		}
	}

	if (r_speeds->int_val) {
//		qfglFinish ();
		time2 = Sys_DoubleTime ();
		Con_Printf ("%3i ms  %4i wpoly %4i epoly %4i parts\n",
					(int) ((time2 - time1) * 1000), c_brush_polys,
					c_alias_polys, numparticles);
	}

	GL_FlushText ();
	qfglFlush ();
}

void
SCR_Init (void)
{
	// register our commands
	Cmd_AddCommand ("screenshot", SCR_ScreenShot_f, "Take a screenshot, "
					"saves as qfxxx.tga in the current directory");
	Cmd_AddCommand ("sizeup", SCR_SizeUp_f, "Increases the screen size");
	Cmd_AddCommand ("sizedown", SCR_SizeDown_f, "Decreases the screen size");

	scr_ram = Draw_PicFromWad ("ram");
	scr_net = Draw_PicFromWad ("net");
	scr_turtle = Draw_PicFromWad ("turtle");

	scr_initialized = true;
}
