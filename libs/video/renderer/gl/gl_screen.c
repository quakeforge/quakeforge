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
#include "QF/ui/view.h"

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

static void
R_Clear (void)
{
	if (gl_clear->int_val)
		qfglClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	else
		qfglClear (GL_DEPTH_BUFFER_BIT);
}

void
gl_R_RenderFrame (SCR_Func *scr_funcs)
{
	double      time1 = 0, time2;
	static int  begun = 0;

	if (begun) {
		gl_ctx->end_rendering ();
		begun = 0;
	}

	//FIXME forces the status bar to redraw. needed because it does not fully
	//update in sw modes but must in gl mode
	vr_data.scr_copyeverything = 1;

	R_Clear ();

	begun = 1;

	if (r_speeds->int_val) {
		time1 = Sys_DoubleTime ();
	}

	// do 3D refresh drawing, and then update the screen
	gl_R_RenderView ();

	GL_Set2D ();
	GL_DrawReset ();

	// draw any areas not covered by the refresh
	SCR_TileClear ();

	GL_Set2DScaled ();

	view_draw (vr_data.scr_view);
	while (*scr_funcs) {
		(*scr_funcs)();
		scr_funcs++;
	}

	if (r_speeds->int_val) {
//		qfglFinish ();
		time2 = Sys_DoubleTime ();
		Sys_MaskPrintf (SYS_dev, "%3i ms  %4i wpoly %4i epoly %4i parts\n",
						(int) ((time2 - time1) * 1000), gl_c_brush_polys,
						gl_c_alias_polys, r_psystem.numparticles);
	}

	GL_FlushText ();
	qfglFlush ();

	if (gl_finish->int_val) {
		gl_ctx->end_rendering ();
		begun = 0;
	}
}
