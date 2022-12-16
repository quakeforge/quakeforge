/*
	hud.h

	Heads-up display

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
#ifndef __client_hud_h
#define __client_hud_h

struct view_s;

enum {
	hud_update,
	hud_updateonce,
	hud_tile,
	hud_pic,
	hud_subpic,
	hud_cachepic,
	hud_fill,
	hud_charbuff,
	hud_func,
	hud_outline,

	hud_comp_count
};

typedef void (*hud_update_f) (struct view_s);
typedef void (*hud_func_f) (struct view_pos_s, struct view_pos_s);

typedef struct hud_subpic_s {
	struct qpic_s *pic;
	uint32_t    x, y;
	uint32_t    w, h;
} hud_subpic_t;

extern struct ecs_system_s hud_system;
extern struct ecs_system_s hud_viewsys;
extern struct ecs_system_s hud_psgsys;

extern int hud_sb_lines;

extern int hud_sbar;
extern int hud_swap;
extern int hud_fps;
extern int hud_pl;
extern int hud_ping;
extern int hud_time;

extern struct view_s hud_view;

extern struct view_s hud_overlay_view;
extern struct view_s hud_stuff_view;
extern struct view_s hud_time_view;
extern struct view_s hud_fps_view;
extern struct view_s hud_ping_view;
extern struct view_s hud_pl_view;

void HUD_Init (void);
void HUD_Init_Cvars (void);
void HUD_Calc_sb_lines (int view_size);
void HUD_Draw_Views (void);

#endif//__client_hud_h
