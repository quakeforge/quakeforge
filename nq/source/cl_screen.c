/*
	cl_screen.c

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

#include <time.h>

#include "QF/console.h"
#include "QF/cvar.h"
#include "QF/draw.h"
#include "QF/image.h"
#include "QF/pcx.h"
#include "QF/screen.h"

#include "QF/plugin/vid_render.h"

#include "sbar.h"

#include "nq/include/client.h"

static qpic_t  *scr_net;

static void
SCR_DrawNet (void)
{
	if (realtime - cl.last_received_message < 0.3)
		return;
	if (cls.demoplayback)
		return;

	if (!scr_net)
		scr_net = r_funcs->Draw_PicFromWad ("net");

	r_funcs->Draw_Pic (r_data->scr_vrect->x + 64, r_data->scr_vrect->y,
					   scr_net);
}

static void
SCR_DrawLoading (void)
{
	qpic_t     *pic;

	if (!cl.loading)
		return;
	pic = r_funcs->Draw_CachePic ("gfx/loading.lmp", 1);
	r_funcs->Draw_Pic ((r_data->vid->conwidth - pic->width) / 2,
					   (r_data->vid->conheight - 48 - pic->height) / 2, pic);
}

static void
SCR_CShift (void)
{
	mleaf_t    *leaf;
	int         contents = CONTENTS_EMPTY;

	if (cls.state == ca_active && cl.worldmodel) {
		leaf = Mod_PointInLeaf (r_data->refdef->vieworg, cl.worldmodel);
		contents = leaf->contents;
	}
	V_SetContentsColor (contents);
	r_funcs->Draw_BlendScreen (r_data->vid->cshift_color);
}

static SCR_Func scr_funcs_normal[] = {
	0, //Draw_Crosshair,
	0, //SCR_DrawRam,
	0, //SCR_DrawTurtle,
	0, //SCR_DrawPause,
	SCR_DrawNet,
	Sbar_Draw,
	SCR_CShift,
	Sbar_DrawCenterPrint,
	Con_DrawConsole,
	SCR_DrawLoading,
	0
};

static SCR_Func scr_funcs_intermission[] = {
	Sbar_IntermissionOverlay,
	Con_DrawConsole,
	0
};

static SCR_Func scr_funcs_finale[] = {
	Sbar_FinaleOverlay,
	Con_DrawConsole,
	0,
};

static SCR_Func *scr_funcs[] = {
	scr_funcs_normal,
	scr_funcs_intermission,
	scr_funcs_finale,
};

void
CL_UpdateScreen (double realtime)
{
	unsigned    index = cl.intermission;

	if (index >= sizeof (scr_funcs) / sizeof (scr_funcs[0]))
		index = 0;

	//FIXME not every time
	if (cls.state == ca_active) {
		if (cl.watervis)
			r_data->min_wateralpha = 0.0;
		else
			r_data->min_wateralpha = 1.0;
	}
	scr_funcs_normal[0] = r_funcs->Draw_Crosshair;
	scr_funcs_normal[1] = r_funcs->SCR_DrawRam;
	scr_funcs_normal[2] = r_funcs->SCR_DrawTurtle;
	scr_funcs_normal[3] = r_funcs->SCR_DrawPause;

	V_PrepBlend ();
	r_funcs->SCR_UpdateScreen (realtime, V_RenderView, scr_funcs[index]);
}
