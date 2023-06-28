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
#include "QF/plugin/general.h"
#include "QF/plugin/vid_render.h"

#include "QF/scene/scene.h"
#include "QF/scene/transform.h"
#include "QF/ui/canvas.h"

#include "r_local.h"	//FIXME for r_cache_thrash

#include "client/hud.h"
#include "client/sbar.h"
#include "client/screen.h"
#include "client/view.h"
#include "client/world.h"

//#include "nq/include/client.h"

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

view_t cl_screen_view;
uint32_t cl_canvas;
static view_t net_view;
static view_t timegraph_view;
static view_t zgraph_view;
static view_t loading_view;
static view_t ram_view;
static view_t turtle_view;
static view_t pause_view;

static viewstate_t *_vs;//FIXME ick

canvas_system_t cl_canvas_sys;

static void
SCR_CShift (void)
{
	mleaf_t    *leaf;
	int         contents = CONTENTS_EMPTY;

	if (_vs->active && cl_world.scene->worldmodel) {
		vec4f_t     origin;
		origin = Transform_GetWorldPosition (_vs->camera_transform);
		leaf = Mod_PointInLeaf (origin, &cl_world.scene->worldmodel->brush);
		contents = leaf->contents;
	}
	V_SetContentsColor (_vs, contents);
	r_funcs->Draw_BlendScreen (_vs->cshift_color);
}

static void
scr_draw_views (void)
{
	if (scr_showturtle) {
		static int  count;
		if (r_data->frametime < 0.1) {
			count = 0;
		} else {
			count++;
		}
		View_SetVisible (turtle_view, count > 2);
	}

	// turn off for screenshots
	View_SetVisible (pause_view, scr_showpause && r_data->paused);

	View_SetVisible (ram_view, scr_showram && r_cache_thrash);
	double msg_time = _vs->realtime - _vs->last_servermessage;
	View_SetVisible (net_view, (!_vs->demoplayback && msg_time >= 0.3));
	View_SetVisible (loading_view, _vs->loading);
	// FIXME cvar callbacks
	View_SetVisible (timegraph_view, r_timegraph);
	View_SetVisible (zgraph_view, r_zgraph);

	if (!_vs->intermission) {
		r_funcs->Draw_Crosshair ();//FIXME canvas_func
	}
	Con_DrawConsole ();
	Canvas_Draw (cl_canvas_sys);
	SCR_CShift ();//FIXME canvas_func
	Sbar_DrawCenterPrint ();//FIXME canvas_func
}

static SCR_Func scr_funcs[] = {
	scr_draw_views,
	0
};

static int cl_scale;
static int cl_xlen;
static int cl_ylen;

static void
cl_set_size (void)
{
	int        xlen = cl_xlen / cl_scale;
	int        ylen = cl_ylen / cl_scale;
	printf ("cl_set_size: %d %d %d\n", cl_scale, xlen, ylen);
	//View_SetLen (cl_screen_view, xlen, ylen);
	//View_UpdateHierarchy (cl_screen_view);
	Canvas_SetLen (cl_canvas_sys, (view_pos_t) { xlen, ylen });
}

static void
cl_scale_listener (void *data, const cvar_t *cvar)
{
	cl_scale = *(int *) cvar->value.value;
	cl_set_size ();
}

static void
cl_vidsize_listener (void *data, const viddef_t *vdef)
{
	cl_xlen = vdef->width;
	cl_ylen = vdef->height;
	cl_set_size ();
}

void
CL_Init_Screen (void)
{
	qpic_t     *pic;

	__auto_type reg = ECS_NewRegistry ();
	Canvas_InitSys (&cl_canvas_sys, reg);
	if (con_module) {
		__auto_type cd = con_module->data->console;
		cd->component_base = ECS_RegisterComponents (reg, cd->components,
													 cd->num_components);
		cd->canvas_sys = &cl_canvas_sys;
	}
	HUD_Init (reg);
	ECS_CreateComponentPools (reg);

	cl_xlen = viddef.width;
	cl_ylen = viddef.height;

	ecs_system_t vsys = {
		.reg = reg,
		.base = cl_canvas_sys.view_base,
	};

	HUD_CreateCanvas (cl_canvas_sys);

	cl_canvas = Canvas_New (cl_canvas_sys);
	cl_screen_view = Canvas_GetRootView (cl_canvas_sys, cl_canvas);

	View_SetPos (cl_screen_view, 0, 0);
	View_SetLen (cl_screen_view, cl_xlen, cl_ylen);
	View_SetGravity (cl_screen_view, grav_northwest);
	View_SetVisible (cl_screen_view, 1);

	pic = r_funcs->Draw_PicFromWad ("ram");
	ram_view = View_New (vsys, cl_screen_view);
	View_SetPos (ram_view, 32, 0);
	View_SetLen (ram_view, pic->width, pic->height);
	View_SetGravity (ram_view, grav_northwest);
	Ent_SetComponent (ram_view.id, canvas_pic, ram_view.reg, &pic);
	View_SetVisible (ram_view, 0);

	pic = r_funcs->Draw_PicFromWad ("turtle");
	turtle_view = View_New (vsys, cl_screen_view);
	View_SetPos (turtle_view, 32, 0);
	View_SetLen (turtle_view, pic->width, pic->height);
	View_SetGravity (turtle_view, grav_northwest);
	Ent_SetComponent (turtle_view.id, canvas_pic, turtle_view.reg, &pic);
	View_SetVisible (turtle_view, 0);

	Cvar_Register (&scr_showpause_cvar, 0, 0);
	Cvar_Register (&scr_showram_cvar, 0, 0);
	Cvar_Register (&scr_showturtle_cvar, 0, 0);

	pic = r_funcs->Draw_PicFromWad ("net");
	net_view = View_New (vsys, cl_screen_view);
	View_SetPos (net_view, 64, 0);
	View_SetLen (net_view, pic->width, pic->height);
	View_SetGravity (net_view, grav_northwest);
	Ent_SetComponent (net_view.id, canvas_pic, net_view.reg, &pic);
	View_SetVisible (net_view, 0);

	timegraph_view = View_New (vsys, cl_screen_view);
	View_SetPos (timegraph_view, 0, 0);
	View_SetLen (timegraph_view, r_data->vid->width, 100);
	View_SetGravity (timegraph_view, grav_southwest);
	void       *rtg = R_TimeGraph;
	Ent_SetComponent (timegraph_view.id, canvas_func, timegraph_view.reg, &rtg);
	View_SetVisible (timegraph_view, r_timegraph);

	zgraph_view = View_New (vsys, cl_screen_view);
	View_SetPos (zgraph_view, 0, 0);
	View_SetLen (zgraph_view, r_data->vid->width, 100);
	View_SetGravity (zgraph_view, grav_southwest);
	void       *rzg = R_ZGraph;
	Ent_SetComponent (zgraph_view.id, canvas_func, zgraph_view.reg, &rzg);
	View_SetVisible (zgraph_view, r_zgraph);

	const char *name = "gfx/loading.lmp";
	pic = r_funcs->Draw_CachePic (name, 1);
	loading_view = View_New (vsys, cl_screen_view);
	View_SetPos (loading_view, 0, -24);
	View_SetLen (loading_view, pic->width, pic->height);
	View_SetGravity (loading_view, grav_center);
	Ent_SetComponent (loading_view.id, canvas_cachepic,
					  loading_view.reg, &name);
	View_SetVisible (loading_view, 0);

	name = "gfx/pause.lmp";
	pic = r_funcs->Draw_CachePic (name, 1);
	pause_view = View_New (vsys, cl_screen_view);
	View_SetPos (pause_view, 0, -24);
	View_SetLen (pause_view, pic->width, pic->height);
	View_SetGravity (pause_view, grav_center);
	Ent_SetComponent (pause_view.id, canvas_cachepic, pause_view.reg, &name);
	View_SetVisible (pause_view, 0);

	cvar_t     *con_scale = Cvar_FindVar ("con_scale");
	Cvar_AddListener (con_scale, cl_scale_listener, 0);
	cl_scale_listener (0, con_scale);
	VID_OnVidResize_AddListener (cl_vidsize_listener, 0);
}

void
CL_UpdateScreen (viewstate_t *vs)
{
	_vs = vs;

	//FIXME not every time
	if (vs->active) {
		if (vs->watervis)
			r_data->min_wateralpha = 0.0;
		else
			r_data->min_wateralpha = 1.0;
	}

	V_PrepBlend (vs);
	V_RenderView (vs);
	SCR_UpdateScreen (vs->camera_transform, vs->time, scr_funcs);
}
