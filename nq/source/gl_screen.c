/*
	gl_screen.c

	@description@

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

	$Id$
*/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "QF/compat.h"
#include "QF/input.h"
#include "QF/qendian.h"
#include "vid.h"
#include "QF/sys.h"
#include "QF/mathlib.h"					// needed by: protocol.h, render.h,
										// client.h,
						// modelgen.h, glmodel.h
#include "wad.h"
#include "draw.h"
#include "QF/cvar.h"
#include "net.h"						// needed by: client.h
#include "protocol.h"					// needed by: client.h
#include "QF/keys.h"
#include "QF/cmd.h"
#include "sbar.h"
#include "QF/sound.h"
#include "screen.h"
#include "render.h"						// needed by: client.h, gl_model.h,
										// glquake.h
#include "client.h"						// need cls in this file
#include "QF/model.h"						// needed by: glquake.h
#include "QF/console.h"
#include "glquake.h"
#include "view.h"
#include "client.h"

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

extern byte *host_basepal;
extern double host_frametime;
extern double realtime;
int         glx, gly, glwidth, glheight;

// only the refresh window will be updated unless these variables are flagged 
int         scr_copytop;
int         scr_copyeverything;

float       scr_con_current;
float       scr_conlines;				// lines of console to display

float       oldscreensize, oldfov;
cvar_t     *scr_viewsize;
cvar_t     *scr_fov;					// 10 - 170
cvar_t     *scr_conspeed;
cvar_t     *scr_centertime;
cvar_t     *scr_showram;
cvar_t     *scr_showturtle;
cvar_t     *scr_showpause;
cvar_t     *scr_printspeed;
cvar_t     *scr_allowsnap;
cvar_t     *gl_triplebuffer;
extern cvar_t *crosshair;

qboolean    scr_initialized;			// ready to draw

qpic_t     *scr_ram;
qpic_t     *scr_net;
qpic_t     *scr_turtle;

int         scr_fullupdate;

int         clearconsole;
int         clearnotify;

extern int  sb_lines;

viddef_t    vid;						// global video state

vrect_t     scr_vrect;

qboolean    scr_disabled_for_loading;
qboolean    scr_drawloading;
float       scr_disabled_time;

qboolean    block_drawing;

void        SCR_ScreenShot_f (void);

/*
===============================================================================

CENTER PRINTING

===============================================================================
*/

char        scr_centerstring[1024];
float       scr_centertime_start;		// for slow victory printing
float       scr_centertime_off;
int         scr_center_lines;
int         scr_erase_lines;
int         scr_erase_center;

/*
==============
SCR_CenterPrint

Called for important messages that should stay in the center of the screen
for a few moments
==============
*/
void
SCR_CenterPrint (char *str)
{
	strncpy (scr_centerstring, str, sizeof (scr_centerstring) - 1);
	scr_centertime_off = scr_centertime->value;
	scr_centertime_start = cl.time;

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
	if (cl.intermission)
		remaining = scr_printspeed->value * (cl.time - scr_centertime_start);
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
			Draw_Character8 (x, y, start[j]);
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

	scr_centertime_off -= host_frametime;

	if (scr_centertime_off <= 0 && !cl.intermission)
		return;
	if (key_dest != key_game)
		return;

	SCR_DrawCenterString ();
}

//=============================================================================

/*
====================
CalcFov
====================
*/
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
=================
SCR_CalcRefdef

Must be called whenever vid changes
Internal use only
=================
*/
static void
SCR_CalcRefdef (void)
{
	int         size;
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

	// intermission is always full screen   
	if (cl.intermission)
		size = 120;
	else
		size = scr_viewsize->int_val;

	if (size >= 120)
		sb_lines = 0;					// no status bar at all
	else if (size >= 110)
		sb_lines = 24;					// no inventory
	else
		sb_lines = 24 + 16 + 8;

	if (scr_viewsize->int_val >= 100) {
		full = true;
		size = 100;
	} else {
		size = scr_viewsize->int_val;
	}

	if (cl.intermission) {
		full = true;
		size = 100;
		sb_lines = 0;
	}
	size /= 100.0;

	if (!cl_sbar->int_val && full)
		h = vid.height;
	else
		h = vid.height - sb_lines;

	r_refdef.vrect.width = vid.width * size;
	if (r_refdef.vrect.width < 96) {
		size = 96.0 / r_refdef.vrect.width;
		r_refdef.vrect.width = 96;		// min for icons
	}

	r_refdef.vrect.height = vid.height * size;
	if (cl_sbar->int_val || !full) {
		if (r_refdef.vrect.height > vid.height - sb_lines)
			r_refdef.vrect.height = vid.height - sb_lines;
	} else {
		if (r_refdef.vrect.height > vid.height)
			r_refdef.vrect.height = vid.height;
	}
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
=================
SCR_SizeUp_f

Keybinding command
=================
*/
void
SCR_SizeUp_f (void)
{
	Cvar_SetValue (scr_viewsize, scr_viewsize->int_val + 10);
	vid.recalc_refdef = 1;
}


/*
=================
SCR_SizeDown_f

Keybinding command
=================
*/
void
SCR_SizeDown_f (void)
{
	Cvar_SetValue (scr_viewsize, scr_viewsize->int_val - 10);
	vid.recalc_refdef = 1;
}

//============================================================================

/*
==================
SCR_Init
==================
*/
void
SCR_InitCvars (void)
{
	scr_fov = Cvar_Get ("fov", "90", CVAR_NONE, 0, "10 - 170");
	scr_viewsize = Cvar_Get ("viewsize", "100", CVAR_ARCHIVE, 0, "None");
	scr_conspeed = Cvar_Get ("scr_conspeed", "300", CVAR_NONE, 0, "None");
	scr_showram = Cvar_Get ("showram", "1", CVAR_NONE, 0, "None");
	scr_showturtle = Cvar_Get ("showturtle", "0", CVAR_NONE, 0, "None");
	scr_showpause = Cvar_Get ("showpause", "1", CVAR_NONE, 0, "None");
	scr_centertime = Cvar_Get ("scr_centertime", "2", CVAR_NONE, 0, "None");
	scr_printspeed = Cvar_Get ("scr_printspeed", "8", CVAR_NONE, 0, "None");
	scr_allowsnap = Cvar_Get ("scr_allowsnap", "1", CVAR_NONE, 0, "None");
	gl_triplebuffer = Cvar_Get ("gl_triplebuffer", "1", CVAR_ARCHIVE, 0, "None");
}

void
SCR_Init (void)
{
//
// register our commands
//
	Cmd_AddCommand ("screenshot", SCR_ScreenShot_f, "No Description");
	Cmd_AddCommand ("sizeup", SCR_SizeUp_f, "No Description");
	Cmd_AddCommand ("sizedown", SCR_SizeDown_f, "No Description");

	scr_ram = Draw_PicFromWad ("ram");
	scr_net = Draw_PicFromWad ("net");
	scr_turtle = Draw_PicFromWad ("turtle");

	scr_initialized = true;
}



/*
==============
SCR_DrawRam
==============
*/
void
SCR_DrawRam (void)
{
	if (!scr_showram->int_val)
		return;

	if (!r_cache_thrash)
		return;

	Draw_Pic (scr_vrect.x + 32, scr_vrect.y, scr_ram);
}

/*
==============
SCR_DrawTurtle
==============
*/
void
SCR_DrawTurtle (void)
{
	static int  count;

	if (!scr_showturtle->int_val)
		return;

	if (host_frametime < 0.1) {
		count = 0;
		return;
	}

	count++;
	if (count < 3)
		return;

	Draw_Pic (scr_vrect.x, scr_vrect.y, scr_turtle);
}

/*
==============
SCR_DrawFPS
==============
*/

void
SCR_DrawFPS (void)
{
	extern cvar_t *show_fps;
	static double lastframetime;
	double      t;
	extern int  fps_count;
	static int  lastfps;
	int         x, y;
	char        st[80];

	if (!show_fps->int_val)
		return;

	t = Sys_DoubleTime ();
	if ((t - lastframetime) >= 1.0) {
		lastfps = fps_count;
		fps_count = 0;
		lastframetime = t;
	}
	snprintf (st, sizeof (st), "%-3d FPS", lastfps);
	/* Misty: New trick! (for me) the ? makes this work like a if then else - 
	   IE: if cl_hudswap->int_val is not null, do first case, else (else is a 
	   : here) do second case. Deek taught me this trick */
	x = cl_hudswap->int_val ? vid.width - ((strlen (st) * 8) + 8) : 8;
	y = vid.height - sb_lines - 8;
	Draw_String8 (x, y, st);

}

/* Misty: I like to see the time */
void
SCR_DrawTime (void)
{
	extern cvar_t *show_time;
	int         x, y;
	char        st[80];
	char        local_time[120];
	time_t      systime;

	/* any cvar that can take multiple settings must be able to handle abuse. 
	 */
	if (show_time->int_val <= 0)
		return;

	/* actually find the time and set systime to it */
	time (&systime);

	if (show_time->int_val == 1) {
		/* now set local_time to 24 hour time using hours:minutes format */
		strftime (local_time, sizeof (local_time), "%k:%M",
				  localtime (&systime));
	} else if (show_time->int_val >= 2) {
		/* >= is another abuse protector */
		strftime (local_time, sizeof (local_time), "%l:%M %P",
				  localtime (&systime));
	}

	/* now actually print it to the screen directly above where show_fps is */
	snprintf (st, sizeof (st), "%s", local_time);
	x = cl_hudswap->int_val ? vid.width - ((strlen (st) * 8) + 8) : 8;
	y = vid.height - sb_lines - 16;
	Draw_String8 (x, y, st);
}

/*
==============
DrawPause
==============
*/
void
SCR_DrawPause (void)
{
	qpic_t     *pic;

	if (!scr_showpause->int_val)		// turn off for screenshots
		return;

	if (!cl.paused)
		return;

	pic = Draw_CachePic ("gfx/pause.lmp");
	Draw_Pic ((vid.width - pic->width) / 2,
			  (vid.height - 48 - pic->height) / 2, pic);
}



/*
==============
SCR_DrawLoading
==============
*/
void
SCR_DrawLoading (void)
{
	qpic_t     *pic;

	if (!scr_drawloading)
		return;

	pic = Draw_CachePic ("gfx/loading.lmp");
	Draw_Pic ((vid.width - pic->width) / 2,
			  (vid.height - 48 - pic->height) / 2, pic);
}



//=============================================================================


/*
==================
SCR_SetUpToDrawConsole
==================
*/
void
SCR_SetUpToDrawConsole (void)
{
	Con_CheckResize ();

	if (scr_drawloading)
		return;							// never a console with loading
	// plaque

// decide on the height of the console
	if (!cl.worldmodel || cls.signon != SIGNONS) {
		scr_conlines = vid.height;		// full screen
		scr_con_current = scr_conlines;
	} else if (key_dest == key_console)
		scr_conlines = vid.height / 2;	// half screen
	else
		scr_conlines = 0;				// none visible

	if (scr_conlines < scr_con_current) {
		scr_con_current -= scr_conspeed->value * host_frametime;
		if (scr_conlines > scr_con_current)
			scr_con_current = scr_conlines;

	} else if (scr_conlines > scr_con_current) {
		scr_con_current += scr_conspeed->value * host_frametime;
		if (scr_conlines < scr_con_current)
			scr_con_current = scr_conlines;
	}

	if (clearconsole++ < vid.numpages) {
		Sbar_Changed ();
	} else if (clearnotify++ < vid.numpages) {
	} else
		con_notifylines = 0;
}

/*
==================
SCR_DrawConsole
==================
*/
void
SCR_DrawConsole (void)
{
	if (scr_con_current) {
		scr_copyeverything = 1;
		Con_DrawConsole (scr_con_current);	// , true);
		clearconsole = 0;
	} else {
		if (key_dest == key_game || key_dest == key_message)
			Con_DrawNotify ();			// only draw notify in game
	}
}


/* 
============================================================================== 
 
						SCREEN SHOTS 
 
============================================================================== 
*/

typedef struct _TargaHeader {
	unsigned char id_length, colormap_type, image_type;
	unsigned short colormap_index, colormap_length;
	unsigned char colormap_size;
	unsigned short x_origin, y_origin, width, height;
	unsigned char pixel_size, attributes;
} TargaHeader;


/* 
================== 
SCR_ScreenShot_f
================== 
*/
void
SCR_ScreenShot_f (void)
{
	byte       *buffer;
	char        pcxname[80];
	char        checkname[MAX_OSPATH];
	int         i, c, temp;

// 
// find a file name to save it to 
// 
	strcpy (pcxname, "nuq000.tga");

	for (i = 0; i <= 999; i++) {
		pcxname[3] = i / 100 + '0';
		pcxname[4] = i / 10 % 10 + '0';
		pcxname[5] = i % 10 + '0';
		snprintf (checkname, sizeof (checkname), "%s/%s", com_gamedir, pcxname);
		if (Sys_FileTime (checkname) == -1)
			break;						// file doesn't exist
	}
	if (i == 1000) {
		Con_Printf ("SCR_ScreenShot_f: Couldn't create a PCX file\n");
		return;
	}


	buffer = malloc (glwidth * glheight * 3 + 18);
	memset (buffer, 0, 18);
	buffer[2] = 2;						// uncompressed type
	buffer[12] = glwidth & 255;
	buffer[13] = glwidth >> 8;
	buffer[14] = glheight & 255;
	buffer[15] = glheight >> 8;
	buffer[16] = 24;					// pixel size

	glReadPixels (glx, gly, glwidth, glheight, GL_RGB, GL_UNSIGNED_BYTE,
				  buffer + 18);

	// swap rgb to bgr
	c = 18 + glwidth * glheight * 3;
	for (i = 18; i < c; i += 3) {
		temp = buffer[i];
		buffer[i] = buffer[i + 2];
		buffer[i + 2] = temp;
	}
	COM_WriteFile (pcxname, buffer, glwidth * glheight * 3 + 18);

	free (buffer);
	Con_Printf ("Wrote %s\n", pcxname);
}


//=============================================================================


/*
===============
SCR_BeginLoadingPlaque

================
*/
void
SCR_BeginLoadingPlaque (void)
{
	S_StopAllSounds (true);

	if (cls.state != ca_connected)
		return;
	if (cls.signon != SIGNONS)
		return;

// redraw with no console and the loading plaque
	Con_ClearNotify ();
	scr_centertime_off = 0;
	scr_con_current = 0;

	scr_drawloading = true;
	scr_fullupdate = 0;
	Sbar_Changed ();
	SCR_UpdateScreen ();
	scr_drawloading = false;

	scr_disabled_for_loading = true;
	scr_disabled_time = realtime;
	scr_fullupdate = 0;
}

/*
===============
SCR_EndLoadingPlaque

================
*/
void
SCR_EndLoadingPlaque (void)
{
	scr_disabled_for_loading = false;
	scr_fullupdate = 0;
	Con_ClearNotify ();
}

//=============================================================================

char       *scr_notifystring;
qboolean    scr_drawdialog;

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
			Draw_Character8 (x, y, start[j]);

		y += 8;

		while (*start && *start != '\n')
			start++;

		if (!*start)
			break;
		start++;						// skip the \n
	} while (1);
}

/*
==================
SCR_ModalMessage

Displays a text string in the center of the screen and waits for a Y or N
keypress.  
==================
*/
int
SCR_ModalMessage (char *text)
{
	if (cls.state == ca_dedicated)
		return true;

	scr_notifystring = text;

// draw a fresh screen
	scr_fullupdate = 0;
	scr_drawdialog = true;
	SCR_UpdateScreen ();
	scr_drawdialog = false;

	S_ClearBuffer ();					// so dma doesn't loop current sound

	do {
		// key_count = -1;      // wait for a key down and up
		IN_SendKeyEvents ();
	} while (key_lastpress != 'y' && key_lastpress != 'n'
			 && key_lastpress != K_ESCAPE);

	scr_fullupdate = 0;
	SCR_UpdateScreen ();

	return key_lastpress == 'y';
}


//=============================================================================

/*
===============
SCR_BringDownConsole

Brings the console down and fades the palettes back to normal
================
*/
void
SCR_BringDownConsole (void)
{
	int         i;

	scr_centertime_off = 0;

	for (i = 0; i < 20 && scr_conlines != scr_con_current; i++)
		SCR_UpdateScreen ();

	cl.cshifts[0].percent = 0;			// no area contents palette on next
	// frame
	VID_SetPalette (host_basepal);
}

void
SCR_TileClear (void)
{
#if 0
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
#endif
}

float       oldsbar = 0;
extern void R_ForceLightUpdate ();
qboolean    lighthalf;
extern cvar_t *gl_lightmode;

/*
==================
SCR_UpdateScreen

This is called every frame, and can also be called explicitly to flush
text to the screen.

WARNING: be very careful calling this from elsewhere, because the refresh
needs almost the entire 256k of stack space!
==================
*/
void
SCR_UpdateScreen (void)
{
	double      time1 = 0, time2;
	float       f;

	if (block_drawing)
		return;

	vid.numpages = 2 + gl_triplebuffer->int_val;

	scr_copytop = 0;
	scr_copyeverything = 0;

	if (scr_disabled_for_loading) {
		if (realtime - scr_disabled_time > 60) {
			scr_disabled_for_loading = false;
			Con_Printf ("load failed.\n");
		} else {
			return;
		}
	}

	if (!scr_initialized || !con_initialized)
		return;							// not initialized yet

	if (oldsbar != cl_sbar->value) {
		oldsbar = cl_sbar->value;
		vid.recalc_refdef = true;
	}

	GL_BeginRendering (&glx, &gly, &glwidth, &glheight);

	if (r_speeds->int_val) {
		time1 = Sys_DoubleTime ();
		c_brush_polys = 0;
		c_alias_polys = 0;
	}
	// 
	// determine size of refresh window
	// 
	if (oldfov != scr_fov->value) {
		oldfov = scr_fov->value;
		vid.recalc_refdef = true;
	}

	if (vid.recalc_refdef)
		SCR_CalcRefdef ();

//
// do 3D refresh drawing, and then update the screen
//

	// LordHavoc: set lighthalf based on gl_lightmode cvar
	if (lighthalf != (gl_lightmode->int_val != 0)) {
		lighthalf = gl_lightmode->int_val != 0;
		R_ForceLightUpdate ();
	}

	SCR_SetUpToDrawConsole ();

	V_RenderView ();

	GL_Set2D ();

	// 
	// draw any areas not covered by the refresh
	// 
	SCR_TileClear ();

	if (scr_drawdialog) {
		Sbar_Draw ();
		Draw_FadeScreen ();
		SCR_DrawNotifyString ();
		scr_copyeverything = true;
	} else {
		if (scr_drawloading) {
			SCR_DrawLoading ();
			Sbar_Draw ();
		} else {
			if (cl.intermission == 1 && key_dest == key_game) {
				Sbar_IntermissionOverlay ();
			} else {
				if (cl.intermission == 2 && key_dest == key_game) {
					Sbar_FinaleOverlay ();
					SCR_CheckDrawCenterString ();
				} else {
					if (crosshair->int_val)
						Draw_Crosshair ();

					SCR_DrawRam ();
					SCR_DrawFPS ();
					SCR_DrawTime ();
					SCR_DrawTurtle ();
					SCR_DrawPause ();
					SCR_CheckDrawCenterString ();
					Sbar_Draw ();
					SCR_DrawConsole ();
					// FIXME: MENUCODE
//					M_Draw ();
				}
			}
		}
	}

	// LordHavoc: adjustable brightness and contrast,
	// also makes polyblend apply to whole screen
	glDisable (GL_TEXTURE_2D);
	glEnable (GL_BLEND);
	Cvar_SetValue (brightness, bound (1, brightness->value, 5));
	if (lighthalf)						// LordHavoc: render was done at half 
										// 
		// 
		// brightness
		f = brightness->value * 2;
	else
		f = brightness->value;
	if (f > 1.0) {
		glBlendFunc (GL_DST_COLOR, GL_ONE);
		glBegin (GL_QUADS);
		while (f > 1.0) {
			if (f >= 2.0)
				glColor3f (1, 1, 1);
			else
				glColor3f (f - 1, f - 1, f - 1);
			glVertex2f (0, 0);
			glVertex2f (vid.width, 0);
			glVertex2f (vid.width, vid.height);
			glVertex2f (0, vid.height);
			f *= 0.5;
		}
		glEnd ();
	}
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	Cvar_SetValue (contrast, bound (0.2, contrast->value, 1.0));
	if ((gl_polyblend->int_val && v_blend[3]) || contrast->value < 1) {
		glBegin (GL_QUADS);
		if (contrast->value < 1) {
			glColor4f (1, 1, 1, 1 - contrast->value);
			glVertex2f (0, 0);
			glVertex2f (vid.width, 0);
			glVertex2f (vid.width, vid.height);
			glVertex2f (0, vid.height);
		}
		if (gl_polyblend->int_val && v_blend[3]) {
			glColor4fv (v_blend);
			glVertex2f (0, 0);
			glVertex2f (vid.width, 0);
			glVertex2f (vid.width, vid.height);
			glVertex2f (0, vid.height);
		}
		glEnd ();
	}

	glDisable (GL_BLEND);
	glEnable (GL_TEXTURE_2D);

	V_UpdatePalette ();

	if (r_speeds->int_val) {
//      glFinish ();
		time2 = Sys_DoubleTime ();
		Con_Printf ("%3i ms  %4i wpoly %4i epoly\n",
					(int) ((time2 - time1) * 1000), c_brush_polys,
					c_alias_polys);
	}

	glFinish ();
	GL_EndRendering ();
}
