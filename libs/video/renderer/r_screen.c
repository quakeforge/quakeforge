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

#include "QF/cmd.h"
#include "QF/cvar.h"
#include "QF/draw.h"
#include "QF/image.h"
#include "QF/pcx.h"
#include "QF/render.h"
#include "QF/screen.h"
#include "QF/sys.h"

#include "QF/scene/transform.h"
#include "QF/ui/view.h"

#include "compat.h"
#include "r_internal.h"
#include "sbar.h"

// only the refresh window will be updated unless these variables are flagged
int         scr_copytop;
byte       *draw_chars;			// 8*8 graphic characters FIXME location
qboolean    r_cache_thrash;		// set if surface cache is thrashing

qboolean    scr_skipupdate;
static qboolean scr_initialized;// ready to draw
static qpic_t *scr_ram;
static qpic_t *scr_turtle;

void
R_SetVrect (const vrect_t *vrectin, vrect_t *vrect, int lineadj)
{
	float       size;
	int         h;

	// intermission is always full screen
	size = min (r_viewsize, 100);
	if (r_data->force_fullscreen) {
		size = 100.0;
		lineadj = 0;
	}
	size /= 100.0;

	h = vrectin->height - lineadj;

	vrect->width = vrectin->width * size + 0.5;
	if (vrect->width < 96) {
		size = 96.0 / vrectin->width;
		vrect->width = 96;				// min for icons
	}
	vrect->width &= ~7;

	vrect->height = vrectin->height * size + 0.5;
	if (vrect->height > h)
		vrect->height = h;
	vrect->height &= ~1;

	vrect->x = (vrectin->width - vrect->width) / 2;
	vrect->y = (h - vrect->height) / 2;
}

static float __attribute__((pure))
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

static void
SCR_CalcRefdef (void)
{
	vrect_t     vrect;
	refdef_t   *refdef = r_data->refdef;

	// force a background redraw
	r_data->scr_fullupdate = 0;
	r_data->vid->recalc_refdef = 0;

	vrect.x = 0;
	vrect.y = 0;
	vrect.width = r_data->vid->width;
	vrect.height = r_data->vid->height;

	R_SetVrect (&vrect, &refdef->vrect, r_data->lineadj);

	view_setgeometry (r_data->scr_view, refdef->vrect.x, refdef->vrect.y,
					  refdef->vrect.width, refdef->vrect.height);

	// bound field of view
	Cvar_SetValue (scr_fov, bound (1, scr_fov->value, 170));

	refdef->fov_y = CalcFov (refdef->fov_x, refdef->vrect.width,
							 refdef->vrect.height);

	// notify the refresh of the change
	r_funcs->R_ViewChanged ();
}

/*
	SCR_UpdateScreen

	This is called every frame, and can also be called explicitly to flush
	text to the screen.

	WARNING: be very careful calling this from elsewhere, because the refresh
	needs almost the entire 256k of stack space!
*/
void
SCR_UpdateScreen (transform_t *camera, double realtime, SCR_Func *scr_funcs)
{
	if (scr_skipupdate || !scr_initialized) {
		return;
	}

	if (camera) {
		r_data->refdef->viewposition = Transform_GetWorldPosition (camera);
		r_data->refdef->viewrotation = Transform_GetWorldRotation (camera);
	} else {
		r_data->refdef->viewposition = (vec4f_t) { 0, 0, 0, 1 };
		r_data->refdef->viewrotation = (vec4f_t) { 0, 0, 0, 1 };
	}

	r_data->realtime = realtime;
	scr_copytop = r_data->scr_copyeverything = 0;

	if (r_data->vid->recalc_refdef) {
		SCR_CalcRefdef ();
	}

	r_funcs->R_RenderFrame (scr_funcs);
}

void
SCR_SetFOV (float fov)
{
	refdef_t   *refdef = r_data->refdef;
	refdef->fov_x = fov;
	r_data->vid->recalc_refdef = 1;
}

static void
ScreenShot_f (void)
{
	r_funcs->SCR_ScreenShot_f ();
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
		r_data->vid->recalc_refdef = 1;
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
	r_data->vid->recalc_refdef = 1;
}

void
SCR_DrawRam (void)
{
	if (!scr_showram->int_val)
		return;

	if (!r_cache_thrash)
		return;

	//FIXME view
	r_funcs->Draw_Pic (r_data->scr_view->xpos + 32, r_data->scr_view->ypos,
					   scr_ram);
}

void
SCR_DrawTurtle (void)
{
	static int  count;

	if (!scr_showturtle->int_val)
		return;

	if (r_data->frametime < 0.1) {
		count = 0;
		return;
	}

	count++;
	if (count < 3)
		return;

	//FIXME view
	r_funcs->Draw_Pic (r_data->scr_view->xpos, r_data->scr_view->ypos,
					   scr_turtle);
}

void
SCR_DrawPause (void)
{
	qpic_t     *pic;

	if (!scr_showpause->int_val)		// turn off for screenshots
		return;

	if (!r_data->paused)
		return;

	//FIXME view conwidth
	pic = r_funcs->Draw_CachePic ("gfx/pause.lmp", true);
	r_funcs->Draw_Pic ((r_data->vid->conview->xlen - pic->width) / 2,
					   (r_data->vid->conview->ylen - 48 - pic->height) / 2,
					   pic);
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
		r1 = r_data->vid->palette[j] - r;
		g1 = r_data->vid->palette[j + 1] - g;
		b1 = r_data->vid->palette[j + 2] - b;
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
SCR_DrawCharToSnap (int num, byte *dest, int width)
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
	Cmd_AddCommand ("screenshot", ScreenShot_f, "Take a screenshot, "
					"saves as qfxxx.pcx in the current directory");
	Cmd_AddCommand ("sizeup", SCR_SizeUp_f, "Increases the screen size");
	Cmd_AddCommand ("sizedown", SCR_SizeDown_f, "Decreases the screen size");

	scr_ram = r_funcs->Draw_PicFromWad ("ram");
	scr_turtle = r_funcs->Draw_PicFromWad ("turtle");

	scr_initialized = true;
}
