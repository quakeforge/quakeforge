/*
	screen.h

	(description)

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
// screen.h

#ifndef __QF_screen_h
#define __QF_screen_h

struct scene_s;
struct transform_s;
struct tex_s;

void SCR_Init (void);

typedef void (*SCR_Func)(void);
// scr_funcs is a null terminated array
void SCR_UpdateScreen (struct transform_s camera, double realtime,
					   SCR_Func *scr_funcs);
void SCR_SetFOV (float fov);
// control whether the 3d viewport is user-controlled or always fullscreen
void SCR_SetFullscreen (qboolean fullscreen);
void SCR_SetBottomMargin (int lines);

void SCR_NewScene (struct scene_s *scene);

extern int hud_fps;
extern int hud_time;
extern int r_timegraph;
extern int r_zgraph;
extern int         scr_copytop;
extern qboolean    scr_skipupdate;

struct view_s;
void R_TimeGraph (struct view_s *view);
void R_ZGraph (struct view_s *view);

#endif//__QF_screen_h
