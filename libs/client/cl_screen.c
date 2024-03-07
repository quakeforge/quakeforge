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
static view_t crosshair_view;
static view_t cshift_view;
static view_t centerprint_view;

static viewstate_t *_vs;//FIXME ick

canvas_system_t cl_canvas_sys;

static ecs_system_t cl_view_system;

static view_t
clscr_view (int x, int y, int w, int h, grav_t gravity)
{
	auto view = View_New (cl_view_system, cl_screen_view);
	View_SetPos (view, x, y);
	View_SetLen (view, w, h);
	View_SetGravity (view, gravity);
	View_SetVisible (view, 0);
	return view;
}

static void
clscr_set_pic (view_t view, qpic_t *pic)
{
	Ent_SetComponent (view.id, cl_canvas_sys.base + canvas_pic, view.reg, &pic);
}

static void
clscr_set_cachepic (view_t view, const char *name)
{
	Ent_SetComponent (view.id, cl_canvas_sys.base + canvas_cachepic, view.reg, &name);
}

static void
clscr_set_canvas_func (view_t view, canvas_func_f func)
{
	Ent_SetComponent (view.id, cl_canvas_sys.base + canvas_func, view.reg,
					  &(canvas_func_t) { .func = func });
}

static void
cl_draw_crosshair (view_pos_t abs, view_pos_t len, void *data)
{
	r_funcs->Draw_Crosshair ();
}

static void
cl_draw_centerprint (view_pos_t abs, view_pos_t len, void *data)
{
	Sbar_DrawCenterPrint ();
}

static void
SCR_CShift (view_pos_t abs, view_pos_t len, void *data)
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
	qfZoneNamed (zone, true);
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
	View_SetVisible (crosshair_view, !_vs->intermission);
	// FIXME cvar callbacks
	View_SetVisible (timegraph_view, r_timegraph);
	View_SetVisible (zgraph_view, r_zgraph);

	Con_DrawConsole ();
	Canvas_Draw (cl_canvas_sys);
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
	//printf ("cl_set_size: %d %d %d\n", cl_scale, xlen, ylen);
	Canvas_SetLen (cl_canvas_sys, cl_canvas, (view_pos_t) { xlen, ylen });
	Canvas_SetLen (cl_canvas_sys, hud_canvas, (view_pos_t) { xlen, ylen });
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

static void
cl_timegraph (view_pos_t abs, view_pos_t len, void *data)
{
	R_TimeGraph (abs, len);
}

static void
cl_zgraph (view_pos_t abs, view_pos_t len, void *data)
{
	R_ZGraph (abs, len);
}

static void
cl_create_views (void)
{
	qpic_t     *pic;
	const char *name;
	auto vid = r_data->vid;

	pic = r_funcs->Draw_PicFromWad ("ram");
	ram_view = clscr_view (32, 0, pic->width, pic->height, grav_northwest);
	clscr_set_pic (ram_view, pic);

	pic = r_funcs->Draw_PicFromWad ("turtle");
	turtle_view = clscr_view (32, 0, pic->width, pic->height, grav_northwest);
	clscr_set_pic (turtle_view, pic);

	pic = r_funcs->Draw_PicFromWad ("net");
	net_view = clscr_view (64, 0, pic->width, pic->height, grav_northwest);
	clscr_set_pic (net_view, pic);

	timegraph_view = clscr_view (0, 0, vid->width, 100, grav_southwest);
	clscr_set_canvas_func (timegraph_view, cl_timegraph);
	View_SetVisible (timegraph_view, r_timegraph);

	zgraph_view = clscr_view (0, 0, vid->width, 100, grav_southwest);
	clscr_set_canvas_func (zgraph_view, cl_zgraph);
	View_SetVisible (zgraph_view, r_zgraph);

	name = "gfx/loading.lmp";
	pic = r_funcs->Draw_CachePic (name, 1);
	loading_view = clscr_view (0, -24, pic->width, pic->height, grav_center);
	clscr_set_cachepic (loading_view, name);

	name = "gfx/pause.lmp";
	pic = r_funcs->Draw_CachePic (name, 1);
	pause_view = clscr_view (0, -24, pic->width, pic->height, grav_center);
	clscr_set_cachepic (pause_view, name);

	crosshair_view = clscr_view (0, 0, vid->width, vid->height, grav_northwest);
	clscr_set_canvas_func (crosshair_view, cl_draw_crosshair);

	cshift_view = clscr_view (0, 0, vid->width, vid->height, grav_northwest);
	clscr_set_canvas_func (cshift_view, SCR_CShift);
	View_SetVisible (cshift_view, 1);

	centerprint_view = clscr_view (0, 0, vid->width, vid->height,
								   grav_northwest);
	clscr_set_canvas_func (centerprint_view, cl_draw_centerprint);
}

static void
CL_Shutdown_Screen (void *data)
{
	ECS_DelRegistry (cl_canvas_sys.reg);
	cl_canvas_sys.reg = 0;
}

void
CL_Init_Screen (void)
{
	qfZoneScoped (true);
	Sys_RegisterShutdown (CL_Shutdown_Screen, 0);
	Con_Load ("client");

	__auto_type reg = ECS_NewRegistry ("cl screen");
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

	cl_view_system = (ecs_system_t) {
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

	Cvar_Register (&scr_showpause_cvar, 0, 0);
	Cvar_Register (&scr_showram_cvar, 0, 0);
	Cvar_Register (&scr_showturtle_cvar, 0, 0);

	cl_create_views ();

	cvar_t     *con_scale = Cvar_FindVar ("con_scale");
	Cvar_AddListener (con_scale, cl_scale_listener, 0);
	cl_scale_listener (0, con_scale);
	VID_OnVidResize_AddListener (cl_vidsize_listener, 0);
}

void
CL_UpdateScreen (viewstate_t *vs)
{
	qfZoneNamedN (us_zone, "CL_UpdateScreen", true);
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
	uint32_t c_animation = cl_world.scene->base + scene_animation;
	uint32_t c_renderer = cl_world.scene->base + scene_renderer;
	auto reg = cl_world.scene->reg;
	auto animpool = reg->comp_pools + c_animation;
	auto rendpool = reg->comp_pools + c_renderer;
	Anim_Update (cl_realtime, animpool, rendpool);
	SCR_UpdateScreen (vs->camera_transform, vs->time, scr_funcs);
}
