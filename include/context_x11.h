/*
	context_x11.h

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

#ifndef __context_x11_h_
#define __context_x11_h_

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <QF/qtypes.h>


#define X11_WINDOW_MASK (VisibilityChangeMask | StructureNotifyMask \
						 | ExposureMask)
#define X11_KEY_MASK (KeyPressMask | KeyReleaseMask)
#define X11_MOUSE_MASK (ButtonPressMask | ButtonReleaseMask \
						| PointerMotionMask)
#define X11_FOCUS_MASK (FocusChangeMask | EnterWindowMask | LeaveWindowMask)
#define X11_INPUT_MASK (X11_KEY_MASK | X11_MOUSE_MASK | X11_FOCUS_MASK)
#define X11_MASK (X11_WINDOW_MASK | X11_FOCUS_MASK | PointerMotionMask)

extern Display	*x_disp;
extern Visual	*x_vis;
extern Window	x_root;
extern Window	x_win;
extern Colormap x_cmap;
extern XVisualInfo *x_visinfo;
extern int		x_screen;
extern int		x_shmeventtype;
extern Time		x_time;
extern Time		x_mouse_time;
extern qboolean oktodraw;
extern qboolean x_have_focus;

qboolean X11_AddEvent (int event, void (*event_handler)(XEvent *));
qboolean X11_RemoveEvent (int event, void (*event_handler)(XEvent *));
qboolean X11_SetGamma (double);
void X11_CloseDisplay (void);
void X11_CreateNullCursor (void);
void X11_CreateWindow (int, int);
void X11_ForceViewPort (void);
void X11_Init_Cvars (void);
void X11_OpenDisplay (void);
void X11_ProcessEvent (void);
void X11_ProcessEvents (void);
void X11_RestoreGamma (void);
void X11_RestoreVidMode (void);
void X11_SetCaption (const char *);
void X11_SetVidMode (int, int);
void X11_SaveMouseAcceleration (void);
void X11_RemoveMouseAcceleration (void);
void X11_RestoreMouseAcceleration (void);

struct gl_ctx_s *X11_GL_Context (void);
void X11_GL_Init_Cvars (void);

struct sw_ctx_s *X11_SW_Context (void);
struct sw_ctx_s *X11_SW32_Context (void);
void X11_SW_Init_Cvars (void);	// sw and sw32 cvars shared

struct vulkan_ctx_s *X11_Vulkan_Context (void);
void X11_Vulkan_Init_Cvars (void);

#endif	// __context_x11_h_
