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

extern struct ecs_system_s hud_psgsys;

extern uint32_t hud_canvas;
extern int hud_sb_lines;

extern int hud_sbar;
extern int hud_swap;
extern int hud_fps;
extern int hud_pl;
extern int hud_ping;
extern int hud_time;

//root view of the hud canvas
extern struct view_s hud_canvas_view;

struct ecs_registry_s;
struct canvas_system_s;

void HUD_Init (struct ecs_registry_s *reg);
void HUD_CreateCanvas (struct canvas_system_s canvas_sys);

extern bool sbar_showscores;

struct player_info_s;
void Sbar_Init (int *stats, float *item_gettime);
void Sbar_SetPlayers (struct player_info_s *players, int maxplayers);
void Sbar_SetLevelName (const char *levelname, const char *servername);
void Sbar_SetPlayerNum (int playernum, int spectator);
void Sbar_SetAutotrack (int autotrack);
void Sbar_SetViewEntity (int viewentity);
void Sbar_SetTeamplay (int teamplay);
void Sbar_SetGameType (int gametype);
void Sbar_SetActive (int active);

void Sbar_Update (double time);
void Sbar_UpdatePings (void);
void Sbar_UpdatePL (int pl);
void Sbar_UpdateFrags (int playernum);
void Sbar_UpdateInfo (int playernum);
void Sbar_UpdateStats (int stat);
void Sbar_Damage (double time);

void Sbar_Intermission (int mode, double completed_time);

void Sbar_DrawCenterPrint (void);
void Sbar_CenterPrint (const char *str);

void Sbar_LogFrags (double time);

#endif//__client_hud_h
