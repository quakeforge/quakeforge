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
#include "QF/quakefs.h"
#include "QF/render.h"
#include "QF/screen.h"
#include "QF/sys.h"
#include "QF/tga.h"
#include "QF/va.h"
#include "QF/GL/defines.h"
#include "QF/GL/funcs.h"
#include "QF/GL/qf_rmain.h"
#include "QF/GL/qf_vid.h"

#include "compat.h"
#include "gl_draw.h"
#include "r_cvar.h"
#include "r_dynamic.h"
#include "r_local.h"
#include "r_screen.h"
#include "sbar.h"
#include "view.h"

/* SCREEN SHOTS */

VISIBLE tex_t *
SCR_ScreenShot (int width, int height)
{
	unsigned char *src, *dest, *snap;
	float          fracw, frach;
	int            count, dex, dey, dx, dy, nx, r, g, b, x, y, w, h;
	tex_t         *tex;

	snap = Hunk_TempAlloc (vid.width * vid.height * 3);

	qfglReadPixels (0, 0, vid.width, vid.height, GL_RGB, GL_UNSIGNED_BYTE,
				    snap);

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
			*dest++ = MipColor (r, g, b);
		}
	}

	return tex;
}

void
SCR_ScreenShot_f (void)
{
	byte       *buffer;
	dstring_t  *pcxname = dstring_new ();

	// find a file name to save it to 
	if (!QFS_NextFilename (pcxname,
						   va ("%s/qf", qfs_gamedir->dir.def), ".tga")) {
		Sys_Printf ("SCR_ScreenShot_f: Couldn't create a TGA file\n");
	} else {
		buffer = malloc (vid.width * vid.height * 3);
		SYS_CHECKMEM (buffer);
		qfglReadPixels (0, 0, vid.width, vid.height, GL_BGR_EXT,
						GL_UNSIGNED_BYTE, buffer);
		WriteTGAfile (pcxname->str, buffer, vid.width, vid.height);
		free (buffer);
		Sys_Printf ("Wrote %s/%s\n", qfs_userpath, pcxname->str);
	}
	dstring_delete (pcxname);
}

static void
SCR_TileClear (void)
{
	if (r_refdef.vrect.x > 0) {
		// left
		Draw_TileClear (0, 0, r_refdef.vrect.x, vid.height - r_lineadj);
		// right
		Draw_TileClear (r_refdef.vrect.x + r_refdef.vrect.width, 0,
						vid.width - r_refdef.vrect.x + r_refdef.vrect.width,
						vid.height - r_lineadj);
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
						vid.height - r_lineadj -
						(r_refdef.vrect.height + r_refdef.vrect.y));
	}
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
	double      time1 = 0, time2;
	static int  begun = 0;

	if (scr_skipupdate)
		return;

	if (begun)
		GL_EndRendering ();

	r_realtime = realtime;

	vid.numpages = 2 + gl_triplebuffer->int_val;

	scr_copytop = 0;
	scr_copyeverything = 0;

	if (!scr_initialized)
		return;							// not initialized yet

	begun = 1;

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
	if (!r_worldentity.model)
		GL_Set2D ();
	GL_DrawReset ();

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

	GL_Set2DScaled ();

	while (*scr_funcs) {
		(*scr_funcs)();
		scr_funcs++;
	}

	if (r_speeds->int_val) {
//		qfglFinish ();
		time2 = Sys_DoubleTime ();
		Sys_DPrintf ("%3i ms  %4i wpoly %4i epoly %4i parts\n",
					 (int) ((time2 - time1) * 1000), c_brush_polys,
					 c_alias_polys, numparticles);
	}

	GL_FlushText ();
	qfglFlush ();

	if (gl_finish->int_val) {
		GL_EndRendering ();
		begun = 0;
	}
}
