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

#include "QF/plugin/console.h"
#include "QF/plugin/vid_render.h"

#include "QF/scene/scene.h"
#include "QF/scene/transform.h"
#include "QF/ui/view.h"

#include "sbar.h"

#include "r_local.h"	//FIXME for r_cache_thrash

#include "client/world.h"

#include "nq/include/client.h"

int scr_showpause;
static cvar_t scr_showpause_cvar = {
	.name = "showpause",
	.description =
		"Toggles display of pause graphic",
	.default_value = "1",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &scr_showpause },
};
int scr_showram;
static cvar_t scr_showram_cvar = {
	.name = "showram",
	.description =
		"Show RAM icon if game is running low on memory",
	.default_value = "1",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &scr_showram },
};
int scr_showturtle;
static cvar_t scr_showturtle_cvar = {
	.name = "showturtle",
	.description =
		"Show a turtle icon if your fps is below 10",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &scr_showturtle },
};

view_t *cl_screen_view;
static view_t  *net_view;
static view_t  *timegraph_view;
static view_t  *zgraph_view;
static view_t  *loading_view;

static qpic_t *scr_ram;
static qpic_t *scr_turtle;

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

	if (cls.state == ca_active && cl_world.scene->worldmodel) {
		vec4f_t     origin;
		origin = Transform_GetWorldPosition (cl.viewstate.camera_transform);
		leaf = Mod_PointInLeaf (origin, cl_world.scene->worldmodel);
		contents = leaf->contents;
	}
	V_SetContentsColor (&cl.viewstate, contents);
	r_funcs->Draw_BlendScreen (cl.viewstate.cshift_color);
}

static void
SCR_DrawRam (void)
{
	if (!scr_showram)
		return;

	if (!r_cache_thrash)
		return;

	//FIXME view
	r_funcs->Draw_Pic (cl_screen_view->xpos + 32, cl_screen_view->ypos,
					   scr_ram);
}

static void
SCR_DrawTurtle (void)
{
	static int  count;

	if (!scr_showturtle)
		return;

	if (r_data->frametime < 0.1) {
		count = 0;
		return;
	}

	count++;
	if (count < 3)
		return;

	//FIXME view
	r_funcs->Draw_Pic (cl_screen_view->xpos, cl_screen_view->ypos,
					   scr_turtle);
}

static void
SCR_DrawPause (void)
{
	qpic_t     *pic;

	if (!scr_showpause)		// turn off for screenshots
		return;

	if (!r_data->paused)
		return;

	//FIXME view conwidth
	pic = r_funcs->Draw_CachePic ("gfx/pause.lmp", true);
	r_funcs->Draw_Pic ((cl_screen_view->xlen - pic->width) / 2,
					   (cl_screen_view->ylen - 48 - pic->height) / 2,
					   pic);
}

static void
scr_draw_views (void)
{
	net_view->visible = (!cls.demoplayback
						 && realtime - cl.last_servermessage >= 0.3);
	loading_view->visible = cl.loading;
	timegraph_view->visible = r_timegraph;
	zgraph_view->visible = r_zgraph;

	view_draw (cl_screen_view);
}

static SCR_Func scr_funcs_normal[] = {
	0, //Draw_Crosshair,
	SCR_DrawRam,
	SCR_DrawTurtle,
	SCR_DrawPause,
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
CL_Init_Screen (void)
{
	qpic_t     *pic;

	cl_screen_view = r_data->scr_view;
	con_module->data->console->screen_view = cl_screen_view;

	scr_ram = r_funcs->Draw_PicFromWad ("ram");
	scr_turtle = r_funcs->Draw_PicFromWad ("turtle");

	Cvar_Register (&scr_showpause_cvar, 0, 0);
	Cvar_Register (&scr_showram_cvar, 0, 0);
	Cvar_Register (&scr_showturtle_cvar, 0, 0);

	pic = r_funcs->Draw_PicFromWad ("net");
	net_view = view_new (64, 0, pic->width, pic->height, grav_northwest);
	net_view->draw = draw_pic;
	net_view->data = pic;
	net_view->visible = 0;
	view_add (cl_screen_view, net_view);

	timegraph_view = view_new (0, 0, cl_screen_view->xlen, 100, grav_southwest);
	timegraph_view->draw = R_TimeGraph;
	timegraph_view->visible = 0;
	view_add (cl_screen_view, timegraph_view);

	zgraph_view = view_new (0, 0, cl_screen_view->xlen, 100, grav_southwest);
	zgraph_view->draw = R_ZGraph;
	zgraph_view->visible = 0;
	view_add (cl_screen_view, zgraph_view);

	const char *name = "gfx/loading.lmp";
	pic = r_funcs->Draw_CachePic (name, 1);
	loading_view = view_new (0, -24, pic->width, pic->height, grav_center);
	loading_view->draw = draw_cachepic;
	loading_view->data = (void *) name;
	loading_view->visible = 0;
	view_add (cl_screen_view, loading_view);
}

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

	cl.viewstate.intermission = cl.intermission != 0;
	V_PrepBlend (&cl.viewstate);
	V_RenderView (&cl.viewstate);
	SCR_UpdateScreen (cl.viewstate.camera_transform,
					  realtime, scr_funcs[index]);
}
