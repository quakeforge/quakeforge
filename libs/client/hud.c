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
#include "QF/screen.h"
#include "QF/render.h"
#include "QF/plugin/vid_render.h"
#include "QF/ui/view.h"

#include "compat.h"

#include "client/hud.h"

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
char *hud_scoreboard_gravity;
static cvar_t hud_scoreboard_gravity_cvar = {
	.name = "hud_scoreboard_gravity",
	.description =
		"control placement of scoreboard overlay: center, northwest, north, "
		"northeast, west, east, southwest, south, southeast",
	.default_value = "center",
	.flags = CVAR_ARCHIVE,
	.value = { .type = 0/* not used */, .value = &hud_scoreboard_gravity },
};
int hud_swap;
static cvar_t hud_swap_cvar = {
	.name = "hud_swap",
	.description =
		"new HUD on left side?",
	.default_value = "0",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_int, .value = &hud_swap },
};

view_t *sbar_view;
view_t *sbar_inventory_view;
view_t *sbar_frags_view;

view_t *hud_view;
view_t *hud_inventory_view;
view_t *hud_armament_view;
view_t *hud_frags_view;

view_t *hud_overlay_view;
view_t *hud_stuff_view;
view_t *hud_main_view;

static void
hud_sbar_f (void *data, const cvar_t *cvar)
{
	HUD_Calc_sb_lines (*r_data->scr_viewsize);
	SCR_SetBottomMargin (hud_sbar ? hud_sb_lines : 0);
	if (hud_sbar) {
		view_remove (hud_main_view, hud_main_view->children[0]);
		view_insert (hud_main_view, sbar_view, 0);
	} else {
		view_remove (hud_main_view, hud_main_view->children[0]);
		view_insert (hud_main_view, hud_view, 0);
	}
}

static void
hud_swap_f (void *data, const cvar_t *cvar)
{
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
}

static void
hud_scoreboard_gravity_f (void *data, const cvar_t *cvar)
{
	grav_t      grav;

	if (strequal (hud_scoreboard_gravity, "center"))
		grav = grav_center;
	else if (strequal (hud_scoreboard_gravity, "northwest"))
		grav = grav_northwest;
	else if (strequal (hud_scoreboard_gravity, "north"))
		grav = grav_north;
	else if (strequal (hud_scoreboard_gravity, "northeast"))
		grav = grav_northeast;
	else if (strequal (hud_scoreboard_gravity, "west"))
		grav = grav_west;
	else if (strequal (hud_scoreboard_gravity, "east"))
		grav = grav_east;
	else if (strequal (hud_scoreboard_gravity, "southwest"))
		grav = grav_southwest;
	else if (strequal (hud_scoreboard_gravity, "south"))
		grav = grav_south;
	else if (strequal (hud_scoreboard_gravity, "southeast"))
		grav = grav_southeast;
	else
		grav = grav_center;
	view_setgravity (hud_overlay_view, grav);
}

void
HUD_Init_Cvars (void)
{
	Cvar_Register (&hud_sbar_cvar, hud_sbar_f, 0);
	Cvar_Register (&hud_swap_cvar, hud_swap_f, 0);
	Cvar_Register (&hud_scoreboard_gravity_cvar, hud_scoreboard_gravity_f, 0);
}

void
HUD_Calc_sb_lines (int view_size)
{
	int        stuff_y;

	if (view_size >= 120) {
		hud_sb_lines = 0;
		stuff_y = 0;
	} else if (view_size >= 110) {
		hud_sb_lines = 24;
		sbar_inventory_view->visible = 0;
		hud_inventory_view->visible = 0;
		hud_armament_view->visible = 0;
		stuff_y = 32;
	} else {
		hud_sb_lines = 48;
		sbar_inventory_view->visible = 1;
		hud_inventory_view->visible = 1;
		hud_armament_view->visible = 1;
		stuff_y = 48;
	}
	if (hud_sb_lines) {
		sbar_view->visible = 1;
		hud_view->visible = 1;
		view_resize (sbar_view, sbar_view->xlen, hud_sb_lines);
		view_resize (hud_view, hud_view->xlen, hud_sb_lines);
	} else {
		sbar_view->visible = 0;
		hud_view->visible = 0;
	}
	view_move (hud_stuff_view, hud_stuff_view->xpos, stuff_y);
}
