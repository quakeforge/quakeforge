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
#include "QF/quakefs.h"
#include "QF/render.h"
#include "QF/screen.h"
#include "QF/sys.h"
#include "QF/tga.h"
#include "QF/va.h"
#include "QF/GL/defines.h"
#include "QF/GL/funcs.h"
#include "QF/GL/qf_draw.h"
#include "QF/GL/qf_rmain.h"
#include "QF/GL/qf_vid.h"

#include "compat.h"
#include "r_internal.h"
#include "sbar.h"
#include "vid_gl.h"

/* SCREEN SHOTS */

tex_t *
gl_SCR_CaptureBGR (void)
{
	int         count;
	tex_t      *tex;

	count = vid.width * vid.height;
	tex = malloc (sizeof (tex_t) + count * 3);
	tex->data = (byte *) (tex + 1);
	SYS_CHECKMEM (tex);
	tex->width = vid.width;
	tex->height = vid.height;
	tex->format = tex_rgb;
	tex->palette = 0;
	qfglReadPixels (0, 0, tex->width, tex->height, GL_BGR_EXT,
					GL_UNSIGNED_BYTE, tex->data);
	return tex;
}

tex_t *
gl_SCR_ScreenShot (unsigned width, unsigned height)
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

	tex = malloc (sizeof (tex_t) + w * h);
	tex->data = (byte *) (tex + 1);
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
gl_SCR_ScreenShot_f (void)
{
	dstring_t  *pcxname = dstring_new ();

	// find a file name to save it to
	if (!QFS_NextFilename (pcxname,
						   va (0, "%s/qf", qfs_gamedir->dir.shots), ".tga")) {
		Sys_Printf ("SCR_ScreenShot_f: Couldn't create a TGA file\n");
	} else {
		tex_t      *tex;

		tex = gl_SCR_CaptureBGR ();
		WriteTGAfile (pcxname->str, tex->data, tex->width, tex->height);
		free (tex);
		Sys_Printf ("Wrote %s/%s\n", qfs_userpath, pcxname->str);
	}
	dstring_delete (pcxname);
}

static void
SCR_TileClear (void)
{
	if (r_refdef.vrect.x > 0) {
		// left
		Draw_TileClear (0, 0, r_refdef.vrect.x, vid.height - vr_data.lineadj);
		// right
		Draw_TileClear (r_refdef.vrect.x + r_refdef.vrect.width, 0,
						vid.width - r_refdef.vrect.x + r_refdef.vrect.width,
						vid.height - vr_data.lineadj);
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
						vid.height - vr_data.lineadj -
						(r_refdef.vrect.height + r_refdef.vrect.y));
	}
}

void
gl_R_RenderFrame (SCR_Func scr_3dfunc, SCR_Func *scr_funcs)
{
	double      time1 = 0, time2;
	static int  begun = 0;

	if (begun) {
		gl_ctx->end_rendering ();
		begun = 0;
	}

	vid.numpages = 2 + gl_triplebuffer->int_val;

	//FIXME forces the status bar to redraw. needed because it does not fully
	//update in sw modes but must in gl mode
	vr_data.scr_copyeverything = 1;

	begun = 1;

	if (r_speeds->int_val) {
		time1 = Sys_DoubleTime ();
		gl_c_brush_polys = 0;
		gl_c_alias_polys = 0;
	}

	// do 3D refresh drawing, and then update the screen
	scr_3dfunc ();

	SCR_SetUpToDrawConsole ();
	GL_Set2D ();
	GL_DrawReset ();

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
		Sys_MaskPrintf (SYS_DEV, "%3i ms  %4i wpoly %4i epoly %4i parts\n",
						(int) ((time2 - time1) * 1000), gl_c_brush_polys,
						gl_c_alias_polys, numparticles);
	}

	GL_FlushText ();
	qfglFlush ();

	if (gl_finish->int_val) {
		gl_ctx->end_rendering ();
		begun = 0;
	}
}
