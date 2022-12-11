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

static const component_t hud_components[hud_comp_count] = {
	[hud_href] = {
		.size = sizeof (hierref_t),
		.name = "href",
	},
	[hud_update] = {
		.size = sizeof (hud_update_f),
		.name = "update",
	},
	[hud_updateonce] = {
		.size = sizeof (hud_update_f),
		.name = "updateonce",
	},
	[hud_tile] = {
		.size = sizeof (byte),
		.name = "pic",
	},
	[hud_pic] = {
		.size = sizeof (qpic_t *),
		.name = "pic",
	},
	[hud_subpic] = {
		.size = sizeof (hud_subpic_t),
		.name = "subpic",
	},
	[hud_cachepic] = {
		.size = sizeof (const char *),
		.name = "cachepic",
	},
	[hud_fill] = {
		.size = sizeof (byte),
		.name = "fill",
	},
	[hud_charbuff] = {
		.size = sizeof (draw_charbuffer_t *),
		.name = "charbuffer",
	},
	[hud_func] = {
		.size = sizeof (hud_func_f),
		.name = "func",
	},
	[hud_outline] = {
		.size = sizeof (byte),
		.name = "outline",
	},
};

ecs_registry_t *hud_registry;
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
grav_t hud_scoreboard_gravity;
static cvar_t hud_scoreboard_gravity_cvar = {
	.name = "hud_scoreboard_gravity",
	.description =
		"control placement of scoreboard overlay: center, northwest, north, "
		"northeast, west, east, southwest, south, southeast",
	.default_value = "center",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &grav_t_type, .value = &hud_scoreboard_gravity },
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

view_t sbar_view;
view_t sbar_inventory_view;
view_t sbar_frags_view;

view_t hud_view;

view_t hud_overlay_view;
view_t hud_stuff_view;
view_t hud_time_view;
view_t hud_fps_view;
view_t hud_ping_view;
view_t hud_pl_view;

static void
hud_add_outlines (view_t view)
{
	byte        color = 0x6f;
	Ent_SetComponent (view.id, hud_outline, view.reg, &color);
	uint32_t    count = View_ChildCount (view);
	for (uint32_t i = 0; i < count; i++) {
		hud_add_outlines (View_GetChild (view, i));
	}
}

static void
hud_remove_outlines (view_t view)
{
	Ent_RemoveComponent (view.id, hud_outline, view.reg);
	uint32_t    count = View_ChildCount (view);
	for (uint32_t i = 0; i < count; i++) {
		hud_remove_outlines (View_GetChild (view, i));
	}
}

static void
hud_debug_f (void *data, const cvar_t *cvar)
{
	if (!View_Valid (hud_view)) {
		return;
	}
	if (hud_debug) {
		hud_add_outlines (hud_view);
		hud_add_outlines (hud_overlay_view);
	} else {
		hud_remove_outlines (hud_view);
		hud_remove_outlines (hud_overlay_view);
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
		view_insert (hud_main_view, sbar_view, 0);
	} else {
		view_remove (hud_main_view, hud_main_view->children[0]);
		view_insert (hud_main_view, hud_view, 0);
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

static void
hud_scoreboard_gravity_f (void *data, const cvar_t *cvar)
{
	if (View_Valid (hud_overlay_view)) {
		View_SetGravity (hud_overlay_view, hud_scoreboard_gravity);
	}
}

void
HUD_Init (void)
{
	hud_registry = ECS_NewRegistry ();
	ECS_RegisterComponents (hud_registry, hud_components, hud_comp_count);
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
	Cvar_Register (&hud_scoreboard_gravity_cvar, hud_scoreboard_gravity_f, 0);
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
#endif
}

static void
draw_update (ecs_pool_t *pool)
{
	uint32_t    count = pool->count;
	uint32_t   *ent = pool->dense;
	hud_update_f *func = pool->data;
	while (count-- > 0) {
		view_t      view = { .id = *ent++, .reg = hud_registry };
		(*func++) (view);
	}
}

static void
draw_updateonce (ecs_pool_t *pool)
{
	draw_update (pool);
	pool->count = 0;
}

static void
draw_tile_views (ecs_pool_t *pool)
{
	uint32_t    count = pool->count;
	uint32_t   *ent = pool->dense;
	while (count-- > 0) {
		view_t      view = { .id = *ent++, .reg = hud_registry };
		if (View_GetVisible (view)) {
			view_pos_t  pos = View_GetAbs (view);
			view_pos_t  len = View_GetLen (view);
			r_funcs->Draw_TileClear (pos.x, pos.y, len.x, len.y);
		}
	}
}

static void
draw_pic_views (ecs_pool_t *pool)
{
	uint32_t    count = pool->count;
	uint32_t   *ent = pool->dense;
	qpic_t    **pic = pool->data;
	while (count-- > 0) {
		view_t      view = { .id = *ent++, .reg = hud_registry };
		if (View_GetVisible (view)) {
			view_pos_t  pos = View_GetAbs (view);
			r_funcs->Draw_Pic (pos.x, pos.y, *pic);
		}
		pic++;
	}
}

static void
draw_subpic_views (ecs_pool_t *pool)
{
	uint32_t    count = pool->count;
	uint32_t   *ent = pool->dense;
	hud_subpic_t *subpic = pool->data;
	while (count-- > 0) {
		view_t      view = { .id = *ent++, .reg = hud_registry };
		if (View_GetVisible (view)) {
			view_pos_t  pos = View_GetAbs (view);
			r_funcs->Draw_SubPic (pos.x, pos.y, subpic->pic,
								  subpic->x, subpic->y, subpic->w, subpic->h);
		}
		subpic++;
	}
}

static void
draw_cachepic_views (ecs_pool_t *pool)
{
	uint32_t    count = pool->count;
	uint32_t   *ent = pool->dense;
	const char **name = pool->data;
	while (count-- > 0) {
		view_t      view = { .id = *ent++, .reg = hud_registry };
		if (View_GetVisible (view)) {
			view_pos_t  pos = View_GetAbs (view);
			qpic_t     *pic = r_funcs->Draw_CachePic (*name, 1);
			r_funcs->Draw_Pic (pos.x, pos.y, pic);
		}
		name++;
	}
}

static void
draw_fill_views (ecs_pool_t *pool)
{
	uint32_t    count = pool->count;
	uint32_t   *ent = pool->dense;
	byte       *fill = pool->data;
	while (count-- > 0) {
		view_t      view = { .id = *ent++, .reg = hud_registry };
		if (View_GetVisible (view)) {
			view_pos_t  pos = View_GetAbs (view);
			view_pos_t  len = View_GetLen (view);
			r_funcs->Draw_Fill (pos.x, pos.y, len.x, len.y, *fill);
		}
		fill++;
	}
}

static void
draw_charbuff_views (ecs_pool_t *pool)
{
	uint32_t    count = pool->count;
	uint32_t   *ent = pool->dense;
	draw_charbuffer_t **charbuff = pool->data;
	while (count-- > 0) {
		view_t      view = { .id = *ent++, .reg = hud_registry };
		if (View_GetVisible (view)) {
			view_pos_t  pos = View_GetAbs (view);
			r_funcs->Draw_CharBuffer (pos.x, pos.y, *charbuff);
		}
		charbuff++;
	}
}

static void
draw_func_views (ecs_pool_t *pool)
{
	uint32_t    count = pool->count;
	uint32_t   *ent = pool->dense;
	hud_func_f *func = pool->data;
	while (count-- > 0) {
		view_t      view = { .id = *ent++, .reg = hud_registry };
		if (View_GetVisible (view)) {
			view_pos_t  pos = View_GetAbs (view);
			view_pos_t  len = View_GetLen (view);
			(*func) (pos, len);
		}
		func++;
	}
}

static void
draw_outline_views (ecs_pool_t *pool)
{
	uint32_t    count = pool->count;
	uint32_t   *ent = pool->dense;
	byte       *col = pool->data;
	__auto_type line = r_funcs->Draw_Line;
	while (count-- > 0) {
		view_t      view = { .id = *ent++, .reg = hud_registry };
		byte        c = *col++;
		if (View_GetVisible (view)) {
			view_pos_t  p = View_GetAbs (view);
			view_pos_t  l = View_GetLen (view);
			view_pos_t  q = { p.x + l.x - 1, p.y + l.y - 1 };
			line (p.x, p.y, q.x, p.y, c);
			line (p.x, q.y, q.x, q.y, c);
			line (p.x, p.y, p.x, q.y, c);
			line (q.x, p.y, q.x, q.y, c);
		}
	}
}

void
HUD_Draw_Views (void)
{
	static void (*draw_func[hud_comp_count]) (ecs_pool_t *) = {
		[hud_update]     = draw_update,
		[hud_updateonce] = draw_updateonce,
		[hud_tile]       = draw_tile_views,
		[hud_pic]        = draw_pic_views,
		[hud_subpic]     = draw_subpic_views,
		[hud_cachepic]   = draw_cachepic_views,
		[hud_fill]       = draw_fill_views,
		[hud_charbuff]   = draw_charbuff_views,
		[hud_func]       = draw_func_views,
		[hud_outline]    = draw_outline_views,
	};
	for (int i = 0; i < hud_comp_count; i++) {
		if (draw_func[i]) {
			draw_func[i] (&hud_registry->comp_pools[i]);
		}
	}
}
