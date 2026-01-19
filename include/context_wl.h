/*
	context_wl.h

	General Wayland Context Layer

	Copyright (C) 2021 Bill Currie <bill@taniwha.org>
	Copyright (C) 2026 Peter Nilsson <peter8nilsson@live.se>
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

#define WL_IFACE(...)
#define WL_ITER_IFACES() \
	WL_IFACE (wl_display); \
	WL_IFACE (wl_registry); \
	WL_IFACE (wl_compositor); \
	WL_IFACE (wl_surface); \
	WL_IFACE (wl_seat); \
	WL_IFACE (wl_pointer); \
	WL_IFACE (wl_keyboard); \
	WL_IFACE (wl_shm); \
	WL_IFACE (wl_shm_pool); \
	WL_IFACE (xdg_wm_base); \
	WL_IFACE (xdg_surface); \
	WL_IFACE (xdg_toplevel); \
	WL_IFACE (zxdg_decoration_manager_v1); \
	WL_IFACE (zxdg_toplevel_decoration_v1); \
	WL_IFACE (zwp_relative_pointer_manager_v1); \
	WL_IFACE (zwp_relative_pointer_v1); \
	WL_IFACE (zwp_pointer_constraints_v1); \
	WL_IFACE (wp_cursor_shape_manager_v1); \
	WL_IFACE (wp_cursor_shape_device_v1)
#undef WL_IFACE

#define WL_IFACE(iface) extern struct iface *iface
	WL_ITER_IFACES ();
#undef WL_IFACE

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
