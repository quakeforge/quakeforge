/*
	context_win.h

	(description)

	Copyright (C) 1996-1997  Id Software, Inc.
	Copyright (C) 1999,2000  contributors of the QuakeForge project
	Please see the file "AUTHORS" for a list of contributors

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

#ifndef __context_win_h
#define __context_win_h

#include "QF/qtypes.h"
#include "winquake.h"

extern HWND     win_mainwindow;
extern HDC      win_maindc;
extern HDC      win_dib_section;
extern int      win_using_ddraw;
extern int      win_palettized;
extern int      win_canalttab;
extern DEVMODE  win_gdevmode;
extern LPDIRECTDRAWSURFACE win_dd_frontbuffer;
extern LPDIRECTDRAWSURFACE win_dd_backbuffer;
extern RECT     win_src_rect;
extern RECT     win_dst_rect;
extern RECT     win_window_rect;
extern HDC      win_gdi;
extern struct sw_ctx_s *win_sw_context;

void Win_UnloadAllDrivers (void);
void Win_CreateDriver (void);
void Win_OpenDisplay (void);
void Win_CloseDisplay (void);
void Win_SetVidMode (int width, int height, const byte *palette);
void Win_Init_Cvars (void);
void Win_UpdateWindowStatus (int x, int y);
void Win_SetCaption (const char *text);
qboolean Win_SetGamma (double gamma);

struct gl_ctx_s *Win_GL_Context (void);
void Win_GL_Init_Cvars (void);

struct sw_ctx_s *Win_SW_Context (void);
void Win_SW_Init_Cvars (void);

struct vulkan_ctx_s *Win_Vulkan_Context (void);
void Win_Vulkan_Init_Cvars (void);

void IN_UpdateClipCursor (void);
void IN_ShowMouse (void);
void IN_HideMouse (void);
void IN_ActivateMouse (void);
void IN_DeactivateMouse (void);

#endif	// __context_win_h
