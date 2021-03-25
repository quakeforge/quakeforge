/*
	r_screen.h

	shared screen declarations

	Copyright (C) 2002 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2002/12/11

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

#ifndef __r_screen_h
#define __r_screen_h

#include "QF/screen.h"
#include "QF/vid.h"

void SCR_UpdateScreen (double realtime, SCR_Func scr_3dfunc,
					   SCR_Func *scr_funcs);
void SCR_DrawRam (void);
void SCR_DrawFPS (void);
void SCR_DrawTime (void);
void SCR_DrawTurtle (void);
void SCR_DrawPause (void);
struct tex_s *SCR_CaptureBGR (void);
struct tex_s *SCR_ScreenShot (unsigned width, unsigned height);
void SCR_DrawStringToSnap (const char *s, struct tex_s *tex, int x, int y);


extern int         scr_copytop;

extern float       scr_con_current;
extern float       scr_conlines;				// lines of console to display

extern int         oldscreensize;
extern int         oldsbar;

extern qboolean    scr_initialized;			// ready to draw

extern struct qpic_s *scr_ram;
extern struct qpic_s *scr_net;
extern struct qpic_s *scr_turtle;

extern int         clearconsole;
extern int         clearnotify;

extern vrect_t    *pconupdate;
extern vrect_t     scr_vrect;

extern qboolean    scr_skipupdate;

void SCR_SetFOV (float fov);
void SCR_SetUpToDrawConsole (void);
void SCR_ScreenShot_f (void);

#endif//__r_screen_h
