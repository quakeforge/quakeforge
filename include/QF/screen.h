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

	$Id$
*/
// screen.h

#ifndef __screen_h
#define __screen_h

#include "QF/qtypes.h"

void SCR_Init_Cvars (void);
void SCR_Init (void);

typedef void (*SCR_Func)(int);
// scr_funcs is a null terminated array
void SCR_UpdateScreen (double realtime, SCR_Func *scr_funcs, int swap);
void SCR_UpdateWholeScreen (void);

void SCR_SizeUp (void);
void SCR_SizeDown (void);
void SCR_BringDownConsole (void);
void SCR_CenterPrint (const char *str);

void SCR_BeginLoadingPlaque (void);
void SCR_EndLoadingPlaque (void);

void SCR_DrawRam (int swap);
void SCR_DrawFPS (int swap);
void SCR_DrawTime (int swap);
void SCR_DrawTurtle (int swap);
void SCR_DrawPause (int swap);
void SCR_CheckDrawCenterString (int swap);
void SCR_DrawConsole (int swap);

struct tex_s *SCR_ScreenShot (int width, int height);
void SCR_DrawStringToSnap (const char *s, struct tex_s *tex, int x, int y);
int MipColor (int r, int g, int b);
int SCR_ModalMessage (const char *text);

extern	float		scr_con_current;
extern	float		scr_conlines;		// lines of console to display

extern	int			scr_fullupdate;	// set to 0 to force full redraw
extern	int			sb_lines;

extern	int			clearnotify;	// set to 0 whenever notify text is drawn
extern	qboolean	scr_disabled_for_loading;
extern	qboolean	scr_skipupdate;

extern struct cvar_s		*scr_viewsize;
extern struct cvar_s		*scr_fov;
extern struct cvar_s		*scr_viewsize;
extern struct cvar_s		*scr_consize;

// only the refresh window will be updated unless these variables are flagged 
extern	int			scr_copytop;
extern	int			scr_copyeverything;

extern qboolean        block_drawing;

extern struct qpic_s *scr_ram;
extern struct qpic_s *scr_net;
extern struct qpic_s *scr_turtle;

#endif // __screen_h
