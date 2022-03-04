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

#include "QF/scene/transform.h"
#include "QF/ui/view.h"

#include "sbar.h"

#include "client/hud.h"
#include "client/view.h"
#include "client/world.h"

#include "qw/include/client.h"
#include "qw/include/cl_parse.h"

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

	if (cls.state == ca_active && cl_world.worldmodel) {
		vec4f_t     origin;
		origin = Transform_GetWorldPosition (cl.viewstate.camera_transform);
		//FIXME
		leaf = Mod_PointInLeaf (&origin[0], cl_world.worldmodel);
		contents = leaf->contents;
	}
	V_SetContentsColor (&cl.viewstate, contents);
	r_funcs->Draw_BlendScreen (cl.viewstate.cshift_color);
}

static void
scr_draw_views (void)
{
	net_view->visible = (!cls.demoplayback
						 && (cls.netchan.outgoing_sequence
							 - cls.netchan.incoming_acknowledged)
						    >= UPDATE_BACKUP - 1);
	loading_view->visible = cl.loading;
	cl_netgraph_view->visible = cl_netgraph->int_val != 0;

	//FIXME don't do every frame
	view_move (cl_netgraph_view, cl_netgraph_view->xpos, sb_lines);
	view_setgravity (cl_netgraph_view,
					 hud_swap->int_val ? grav_southeast : grav_southwest);

	view_draw (r_data->vid->conview);
}

static SCR_Func scr_funcs_normal[] = {
	0, //Draw_Crosshair,
	0, //SCR_DrawRam,
	0, //SCR_DrawTurtle,
	0, //SCR_DrawPause,
	//CL_NetGraph,FIXME
	Sbar_Draw,
	SCR_CShift,
	scr_draw_views,
	Sbar_DrawCenterPrint,
	Con_DrawConsole,
	0
};

static SCR_Func scr_funcs_intermission[] = {
	Sbar_IntermissionOverlay,
	Con_DrawConsole,
	scr_draw_views,
	0
};

static SCR_Func scr_funcs_finale[] = {
	Sbar_FinaleOverlay,
	Con_DrawConsole,
	scr_draw_views,
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

	if (!cl_netgraph_view) {
		cl_netgraph_view = view_new (0, sb_lines,
									 NET_TIMINGS + 16,
									 cl_netgraph_height->int_val + 25,
									 grav_southwest);
		cl_netgraph_view->draw = CL_NetGraph;
		cl_netgraph_view->visible = 0;
		view_add (r_data->vid->conview, cl_netgraph_view);
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

	if (cl.viewstate.flags & VF_GIB) {
		cl.viewstate.height = 8;			// gib view height
	} else if (cl.viewstate.flags & VF_DEAD) {
		cl.viewstate.height = -16;			// corpse view height
	} else {
		cl.viewstate.height = DEFAULT_VIEWHEIGHT;	// view height
		if (cl.stdver)
			cl.viewstate.height = cl.stats[STAT_VIEWHEIGHT];
	}

	cl.viewstate.intermission = cl.intermission != 0;
	V_PrepBlend (&cl.viewstate);
	int         seq = (cls.netchan.outgoing_sequence - 1) & UPDATE_MASK;
	frame_t    *frame = &cl.frames[seq];
	cl.viewstate.movecmd[FORWARD] = frame->cmd.forwardmove;
	V_RenderView (&cl.viewstate);
	SCR_UpdateScreen (cl.viewstate.camera_transform,
					  realtime, scr_funcs[index]);
}
