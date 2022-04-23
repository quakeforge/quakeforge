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

extern int hud_sb_lines;

extern int hud_sbar;
extern char *hud_scoreboard_gravity;
extern int hud_swap;

extern struct view_s *sbar_view;
extern struct view_s *sbar_inventory_view;
extern struct view_s *sbar_frags_view;

extern struct view_s *hud_view;
extern struct view_s *hud_inventory_view;
extern struct view_s *hud_armament_view;
extern struct view_s *hud_frags_view;

extern struct view_s *hud_overlay_view;
extern struct view_s *hud_stuff_view;
extern struct view_s *hud_main_view;

void HUD_Init_Cvars (void);
void HUD_Calc_sb_lines (int view_size);

#endif//__client_hud_h
