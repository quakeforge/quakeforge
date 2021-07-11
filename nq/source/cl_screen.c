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

#include "QF/ui/view.h"

#include "sbar.h"

#include "nq/include/client.h"

static view_t  *net_view;
static view_t  *loading_view;

static void
draw_pic (view_t *view)
{
	r_funcs->Draw_Pic (view->xabs, view->yabs, view->data);
}

static void
draw_cachepic (view_t *view)
{
	qpic_t     *pic = r_funcs->Draw_CachePic (view->data, 1);
	r_funcs->Draw_Pic (view->xabs, view->yabs, pic);
}

static void
SCR_CShift (void)
{
	mleaf_t    *leaf;
	int         contents = CONTENTS_EMPTY;

	if (cls.state == ca_active && cl.worldmodel) {
		//FIXME
		leaf = Mod_PointInLeaf (&r_data->refdef->viewposition[0],
								cl.worldmodel);
		contents = leaf->contents;
	}
	V_SetContentsColor (contents);
	r_funcs->Draw_BlendScreen (r_data->vid->cshift_color);
}

static void
scr_draw_views (void)
{
	net_view->visible = (!cls.demoplayback
						 && realtime - cl.last_servermessage >= 0.3);
	loading_view->visible = cl.loading;

	view_draw (r_data->vid->conview);
}

static SCR_Func scr_funcs_normal[] = {
	0, //Draw_Crosshair,
	0, //SCR_DrawRam,
	0, //SCR_DrawTurtle,
	0, //SCR_DrawPause,
	Sbar_Draw,
	SCR_CShift,
	Sbar_DrawCenterPrint,
	scr_draw_views,
	Con_DrawConsole,
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

	if (!net_view) {
		qpic_t     *pic = r_funcs->Draw_PicFromWad ("net");
		net_view = view_new (64, 0, pic->width, pic->height, grav_northwest);
		net_view->draw = draw_pic;
		net_view->data = pic;
		net_view->visible = 0;
		view_add (r_data->scr_view, net_view);
	}

	if (!loading_view) {
		const char *name = "gfx/loading.lmp";
		qpic_t     *pic = r_funcs->Draw_CachePic (name, 1);
		loading_view = view_new (0, -24, pic->width, pic->height, grav_center);
		loading_view->draw = draw_cachepic;
		loading_view->data = (void *) name;
		loading_view->visible = 0;
		view_add (r_data->vid->conview, loading_view);
	}

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
	SCR_UpdateScreen (realtime, V_RenderView, scr_funcs[index]);
}
