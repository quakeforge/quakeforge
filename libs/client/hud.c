/*
	hud.c

	Heads-up display bar

	Copyright (C) 1996-1997  Id Software, Inc.
	Copyright (C) 2022 Bill Currie <bill@taniwha.org>

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

#include <string.h>

#include "QF/cvar.h"
#include "QF/gib.h"
#include "QF/screen.h"
#include "QF/render.h"
#include "QF/plugin/vid_render.h"
#include "QF/ui/canvas.h"
#include "QF/ui/passage.h"
#include "QF/ui/view.h"

#include "compat.h"

#include "client/hud.h"
#include "client/screen.h"

ecs_system_t hud_psgsys;
uint32_t    hud_canvas;
int hud_sb_lines;

int hud_sbar;
static cvar_t hud_sbar_cvar = {
	.name = "hud_sbar",
	.description =
		"status bar mode: 0 = hud, 1 = oldstyle",
	.default_value = "0",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_int, .value = &hud_sbar },
};
grav_t hud_scoreboard_gravity = grav_center;
#if 0
static cvar_t hud_scoreboard_gravity_cvar = {
	.name = "hud_scoreboard_gravity",
	.description =
		"control placement of scoreboard overlay: center, northwest, north, "
		"northeast, west, east, southwest, south, southeast",
	.default_value = "center",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &grav_t_type, .value = &hud_scoreboard_gravity },
};
#endif
int hud_swap;
static cvar_t hud_swap_cvar = {
	.name = "hud_swap",
	.description =
		"new HUD on left side?",
	.default_value = "0",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_int, .value = &hud_swap },
};
int hud_fps;
static cvar_t hud_fps_cvar = {
	.name = "hud_fps",
	.description =
		"display realtime frames per second",
	.default_value = "0",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_int, .value = &hud_fps },
};
int hud_ping;
static cvar_t hud_ping_cvar = {
	.name = "hud_ping",
	.description =
		"display current ping to server",
	.default_value = "0",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_int, .value = &hud_ping },
};
int hud_pl;
static cvar_t hud_pl_cvar = {
	.name = "hud_pl",
	.description =
		"display current packet loss to server",
	.default_value = "0",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_int, .value = &hud_pl },
};
int hud_time;
static cvar_t hud_time_cvar = {
	.name = "hud_time",
	.description =
		"display the current time",
	.default_value = "0",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_int, .value = &hud_time },
};
int hud_debug;
static cvar_t hud_debug_cvar = {
	.name = "hud_debug",
	.description =
		"display hud view outlines for debugging",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &hud_debug },
};

view_t hud_canvas_view;

static void
hud_add_outlines (view_t view, byte color)
{
	Ent_SetComponent (view.id, canvas_outline, view.reg, &color);
	uint32_t    count = View_ChildCount (view);
	for (uint32_t i = 0; i < count; i++) {
		hud_add_outlines (View_GetChild (view, i), color);
	}
}

static void
hud_remove_outlines (view_t view)
{
	Ent_RemoveComponent (view.id, canvas_outline, view.reg);
	uint32_t    count = View_ChildCount (view);
	for (uint32_t i = 0; i < count; i++) {
		hud_remove_outlines (View_GetChild (view, i));
	}
}

static void
hud_debug_f (void *data, const cvar_t *cvar)
{
	if (!View_Valid (hud_canvas_view)) {
		return;
	}
	if (hud_debug) {
		hud_add_outlines (cl_screen_view, 0xfb);
		hud_add_outlines (hud_canvas_view, 0x6f);
	} else {
		hud_remove_outlines (hud_canvas_view);
		hud_remove_outlines (cl_screen_view);
	}
}

static void
hud_sbar_f (void *data, const cvar_t *cvar)
{
	HUD_Calc_sb_lines (*r_data->scr_viewsize);
	SCR_SetBottomMargin (hud_sbar ? hud_sb_lines : 0);
#if 0//XXX
	if (hud_sbar) {
		view_remove (hud_main_view, hud_main_view->children[0]);
	} else {
		view_remove (hud_main_view, hud_main_view->children[0]);
		view_insert (hud_main_view, hud_canvas_view, 0);
	}
#endif
}

static void
hud_swap_f (void *data, const cvar_t *cvar)
{
#if 0//XXX
	if (hud_swap) {
		//FIXME why is this needed for nq but not for qw?
		hud_armament_view->children[0]->gravity = grav_northwest;
		hud_armament_view->children[1]->gravity = grav_southeast;
		view_setgravity (hud_armament_view, grav_southwest);
		view_setgravity (hud_stuff_view, grav_southeast);
	} else {
		//FIXME why is this needed for nq but not for qw?
		hud_armament_view->children[0]->gravity = grav_northeast;
		hud_armament_view->children[1]->gravity = grav_southwest;
		view_setgravity (hud_armament_view, grav_southeast);
		view_setgravity (hud_stuff_view, grav_southwest);
	}
	view_move (hud_armament_view, hud_armament_view->xpos,
			   hud_armament_view->ypos);
	view_move (hud_stuff_view, hud_stuff_view->xpos, hud_stuff_view->ypos);
#endif
}
#if 0
static void
hud_scoreboard_gravity_f (void *data, const cvar_t *cvar)
{
	if (View_Valid (hud_overlay_view)) {
		View_SetGravity (hud_overlay_view, hud_scoreboard_gravity);
	}
}
#endif
static void
C_GIB_HUD_Enable_f (void)
{
	//hud_canvas_view->visible = 1;
}

static void
C_GIB_HUD_Disable_f (void)
{
	//hud_canvas_view->visible = 0;
}

void
HUD_Init (ecs_registry_t *reg)
{
	hud_psgsys = (ecs_system_t) {
		.reg = reg,
		.base = ECS_RegisterComponents (reg, passage_components,
										passage_comp_count),
	};

	// register GIB builtins
	GIB_Builtin_Add ("HUD::enable", C_GIB_HUD_Enable_f);
	GIB_Builtin_Add ("HUD::disable", C_GIB_HUD_Disable_f);
}

void
HUD_Init_Cvars (void)
{
	Cvar_Register (&hud_fps_cvar, 0, 0);
	Cvar_MakeAlias ("show_fps", &hud_fps_cvar);
	Cvar_Register (&hud_ping_cvar, 0, 0);
	Cvar_Register (&hud_pl_cvar, 0, 0);
	Cvar_Register (&hud_time_cvar, 0, 0);
	Cvar_Register (&hud_debug_cvar, hud_debug_f, 0);

	Cvar_Register (&hud_sbar_cvar, hud_sbar_f, 0);
	Cvar_Register (&hud_swap_cvar, hud_swap_f, 0);
#if 0
	Cvar_Register (&hud_scoreboard_gravity_cvar, hud_scoreboard_gravity_f, 0);
#endif
}

void
HUD_CreateCanvas (canvas_system_t canvas_sys)
{
	hud_canvas = Canvas_New (canvas_sys);
	hud_canvas_view = Canvas_GetRootView (canvas_sys, hud_canvas);
	View_SetPos (hud_canvas_view, 0, 0);
	View_SetLen (hud_canvas_view, viddef.width, viddef.height);
	View_SetGravity (hud_canvas_view, grav_northwest);
	View_SetVisible (hud_canvas_view, 1);
}

void
HUD_Calc_sb_lines (int view_size)
{
#if 0//XXX
	int        stuff_y;

	if (view_size >= 120) {
		hud_sb_lines = 0;
		stuff_y = 0;
	} else if (view_size >= 110) {
		hud_sb_lines = 24;
		hud_inventory_view->visible = 0;
		hud_armament_view->visible = 0;
		stuff_y = 32;
	} else {
		hud_sb_lines = 48;
		hud_inventory_view->visible = 1;
		hud_armament_view->visible = 1;
		stuff_y = 48;
	}
	if (hud_sb_lines) {
		hud_canvas_view->visible = 1;
		view_resize (hud_canvas_view, hud_canvas_view->xlen, hud_sb_lines);
	} else {
		hud_canvas_view->visible = 0;
	}
	view_move (hud_stuff_view, hud_stuff_view->xpos, stuff_y);
#endif
}
