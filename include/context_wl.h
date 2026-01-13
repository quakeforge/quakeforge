/*
	context_wl.h

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

#ifndef __context_wl_h_
#define __context_wl_h_

#include <QF/qtypes.h>

extern struct wl_display *wl_disp;
extern struct wl_registry *wl_reg;
extern struct wl_compositor *wl_comp;
extern struct wl_shm *wl_shm;
extern struct wl_shm_pool *wl_shm_pool;
extern struct wl_surface *wl_surf;
extern struct xdg_wm_base *xdg_wm_base;
extern struct xdg_surface *xdg_surface;
extern struct xdg_toplevel *xdg_toplevel;
extern struct zxdg_decoration_manager_v1 *decoration_manager;
extern struct zxdg_toplevel_decoration_v1 *toplevel_decoration;

extern bool wl_surface_configured;

bool WL_SetGamma (double);
void WL_SetScreenSaver (void);
void WL_CloseDisplay (void);
void WL_CreateNullCursor (void);
void WL_CreateWindow (int, int);
void WL_ForceViewPort (void);
void WL_UpdateFullscreen (int fullscreen);
void WL_Init_Cvars (void);
void WL_OpenDisplay (void);
void WL_ProcessEvent (void);
void WL_ProcessEvents (void);
void WL_RestoreGamma (void);
void WL_RestoreScreenSaver (void);
void WL_RestoreVidMode (void);
void WL_SetCaption (const char *);
void WL_SetVidMode (int, int);
void WL_SaveMouseAcceleration (void);
void WL_RemoveMouseAcceleration (void);
void WL_RestoreMouseAcceleration (void);

struct vid_internal_s;
struct gl_ctx_s *WL_GL_Context (struct vid_internal_s *);
void WL_GL_Init_Cvars (void);

struct sw_ctx_s *WL_SW_Context (struct vid_internal_s *);
void WL_SW_Init_Cvars (void);

struct vulkan_ctx_s *WL_Vulkan_Context (struct vid_internal_s *);
void WL_Vulkan_Init_Cvars (void);

#endif	// __context_wl_h_
