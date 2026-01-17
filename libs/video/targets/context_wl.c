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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include <ctype.h>
#include <sys/time.h>

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#include <locale.h>

#include <wayland-client.h>
#include "libs/video/targets/xdg-shell-client-protocol.hinc"
#include "libs/video/targets/xdg-decoration-client-protocol.hinc"
#include "libs/video/targets/relative-pointer-client-protocol.hinc"
#include "libs/video/targets/pointer-constraints-client-protocol.hinc"
#include "libs/video/targets/cursor-shape-client-protocol.hinc"

#include "QF/cmd.h"
#include "QF/cvar.h"
#include "QF/input.h"
#include "QF/qargs.h"
#include "QF/qtypes.h"
#include "QF/sys.h"
#include "QF/va.h"
#include "QF/vid.h"

#include "QF/input/event.h"

#include "context_wl.h"
#include "in_wl.h"
#include "vid_internal.h"

struct wl_display *wl_disp;
struct wl_registry *wl_reg;
struct wl_compositor *wl_comp;
struct wl_surface *wl_surf;
struct wl_seat *wl_seat;
struct wl_pointer *wl_pointer;
struct wl_keyboard *wl_keyboard;
struct wl_shm *wl_shm;
struct wl_shm_pool *wl_shm_pool;
struct xdg_wm_base *xdg_wm_base;
struct xdg_surface *xdg_surface;
struct xdg_toplevel *xdg_toplevel;
struct zxdg_decoration_manager_v1 *decoration_manager;
struct zxdg_toplevel_decoration_v1 *toplevel_decoration;
struct zwp_relative_pointer_manager_v1 *wl_relative_pointer_manager;
struct zwp_relative_pointer_v1 *wl_relative_pointer;
struct zwp_pointer_constraints_v1 *zwp_pointer_constraints_v1;
struct wp_cursor_shape_manager_v1 *wp_cursor_shape_manager_v1;
struct wp_cursor_shape_device_v1 *wp_cursor_shape_device_v1;

bool wl_surface_configured = false;

static bool wl_is_fullscreen = false;
static bool wl_fullscreen_supported = false;

static void
zxdg_toplevel_decoration_configure (void *data,
			                        struct zxdg_toplevel_decoration_v1 *toplevel_decoration,
			                        uint32_t mode)
{
    if (mode == ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE) {
        Sys_Error ("Wayland: Compositor expects client-side decorations, which we don't support.");
    }

    Sys_MaskPrintf (SYS_wayland, "Wayland compositor will give server-side decorations.\n");
}

static const struct zxdg_toplevel_decoration_v1_listener toplevel_decoration_listener = {
    .configure = zxdg_toplevel_decoration_configure
};

static void
toplevel_configure (void *data, struct xdg_toplevel *toplvl,
                    int32_t width, int32_t height, struct wl_array *states)
{
    if (width == 0 || height == 0) {
        return;
    }

    Sys_MaskPrintf (SYS_wayland, "toplevel_configure: width = %d, height = %d\n",
            width, height);

    VID_SetWindow (0, 0, width, height);
}

static void
toplevel_close (void *data, struct xdg_toplevel *toplvl)
{
    Sys_Quit ();
}

static void
toplevel_configure_bounds (void *data, struct xdg_toplevel *toplvl,
                            int32_t width, int32_t height)
{
}

static const char *
toplevel_wm_capability_name (enum xdg_toplevel_wm_capabilities cap)
{
	switch (cap) {
		case XDG_TOPLEVEL_WM_CAPABILITIES_WINDOW_MENU: return "XDG_TOPLEVEL_WM_CAPABILITIES_WINDOW_MENU";
		case XDG_TOPLEVEL_WM_CAPABILITIES_MAXIMIZE: return "XDG_TOPLEVEL_WM_CAPABILITIES_MAXIMIZE";
		case XDG_TOPLEVEL_WM_CAPABILITIES_FULLSCREEN: return "XDG_TOPLEVEL_WM_CAPABILITIES_FULLSCREEN";
		case XDG_TOPLEVEL_WM_CAPABILITIES_MINIMIZE: return "XDG_TOPLEVEL_WM_CAPABILITIES_MINIMIZE";
		default:
			Sys_Error ("toplevel_wm_capability_name: Unknown capability %d", cap);
	}
}

static void
toplevel_wm_capabilities (void *data, struct xdg_toplevel *toplvl,
                            struct wl_array *caps)
{
	Sys_MaskPrintf (SYS_wayland, "Supported Wayland Capabilities:\n");

	enum xdg_toplevel_wm_capabilities* cap;
	wl_array_for_each (cap, caps) {
		Sys_MaskPrintf (SYS_wayland, "\t- %s\n", toplevel_wm_capability_name(*cap));
		switch (*cap) {
			case XDG_TOPLEVEL_WM_CAPABILITIES_FULLSCREEN:
				wl_fullscreen_supported = true;
				break;
			default:
				break;
		}
	};
}

static const struct xdg_toplevel_listener toplevel_listener = {
    .configure = toplevel_configure,
    .close = toplevel_close,
    .configure_bounds = toplevel_configure_bounds,
    .wm_capabilities = toplevel_wm_capabilities,
};

static void
xdg_surface_configure (void *data, struct xdg_surface *surf, uint32_t serial)
{
    xdg_surface_ack_configure (surf, serial);
    wl_surface_configured = true;
}

static const struct xdg_surface_listener surface_listener = {
    .configure = xdg_surface_configure
};

static void
xdg_wm_base_ping (void *data, struct xdg_wm_base *base, uint32_t serial)
{
    xdg_wm_base_pong (base, serial);
}

static const struct xdg_wm_base_listener xdg_base_listener = {
    .ping = xdg_wm_base_ping
};

static void
registry_handle_global (void *data, struct wl_registry *reg, uint32_t name,
                        const char *interface, uint32_t version)
{
    /*Sys_MaskPrintf (SYS_wayland, "Interface '%s', version: %d, name: %d\n",
                    interface, version, name);*/
 
    if (strcmp (interface, wl_compositor_interface.name) == 0) {
        wl_comp = wl_registry_bind (wl_reg, name, &wl_compositor_interface, 6);
    } else if (strcmp (interface, wl_seat_interface.name) == 0) {
        wl_seat = wl_registry_bind (wl_reg, name, &wl_seat_interface, 7);
		IN_WL_RegisterSeat ();
	} else if (strcmp (interface, zwp_relative_pointer_manager_v1_interface.name) == 0) {
		wl_relative_pointer_manager = wl_registry_bind (wl_reg, name,
						&zwp_relative_pointer_manager_v1_interface, 1);
	} else if (strcmp (interface, zwp_pointer_constraints_v1_interface.name) == 0) {
        zwp_pointer_constraints_v1 = wl_registry_bind (wl_reg, name,
						&zwp_pointer_constraints_v1_interface, 1);
	} else if (strcmp (interface, wp_cursor_shape_manager_v1_interface.name) == 0) {
		wp_cursor_shape_manager_v1 = wl_registry_bind (wl_reg, name,
						&wp_cursor_shape_manager_v1_interface, 1);
	} else if (strcmp (interface, wl_shm_interface.name) == 0) {
        wl_shm = wl_registry_bind (wl_reg, name, &wl_shm_interface, 2);
    } else if (strcmp (interface, xdg_wm_base_interface.name) == 0) {
        xdg_wm_base = wl_registry_bind (wl_reg, name, &xdg_wm_base_interface, 6);
        xdg_wm_base_add_listener (xdg_wm_base, &xdg_base_listener, nullptr);
    } else if (strcmp (interface, zxdg_decoration_manager_v1_interface.name) == 0) {
        decoration_manager = wl_registry_bind (wl_reg, name, &zxdg_decoration_manager_v1_interface, 1);
    }
}

static void
registry_handle_global_remove (void *data, struct wl_registry *reg, uint32_t name)
{
}

static const struct wl_registry_listener registry_listener = {
    .global = registry_handle_global,
    .global_remove = registry_handle_global_remove,
};

void
WL_ProcessEvents (void)
{
    struct timespec ts = {};
    wl_display_dispatch_timeout (wl_disp, &ts);
}

static void
wl_destroy_window (void)
{
    xdg_toplevel_destroy (xdg_toplevel);
    xdg_surface_destroy (xdg_surface);
    wl_surface_destroy (wl_surf);
}

void
WL_CloseDisplay (void)
{
    Sys_MaskPrintf (SYS_wayland, "WL_CloseDisplay\n");

    wl_destroy_window ();

    // TODO: Remove registry globals

    wl_registry_destroy (wl_reg);
    wl_display_disconnect (wl_disp);
}

void
WL_UpdateFullscreen (int fullscreen)
{
	if (!xdg_toplevel) {
		return;
	}

	if (!wl_fullscreen_supported) {
		Sys_MaskPrintf (SYS_wayland, "WL_UpdateFullscreen: changing fullscreen "
									"state not supported by compositor.\n");
		return;
	}

	if (fullscreen && !wl_is_fullscreen) {
		xdg_toplevel_set_fullscreen (xdg_toplevel, nullptr);
		wl_is_fullscreen = true;
	} else if (wl_is_fullscreen) {
		xdg_toplevel_unset_fullscreen (xdg_toplevel);
		wl_is_fullscreen = false;
	}
}

void
WL_Init_Cvars (void)
{
}

void
WL_CreateWindow (int width, int height)
{
	wl_surf = wl_compositor_create_surface (wl_comp);
	xdg_surface = xdg_wm_base_get_xdg_surface (xdg_wm_base, wl_surf);
	xdg_surface_add_listener (xdg_surface, &surface_listener, nullptr);

	xdg_toplevel = xdg_surface_get_toplevel (xdg_surface);
	xdg_toplevel_add_listener (xdg_toplevel, &toplevel_listener, nullptr);
	xdg_toplevel_set_title (xdg_toplevel, "Hello");

	if (decoration_manager) {
		Sys_MaskPrintf (SYS_wayland, "Initializing decorations\n");

		toplevel_decoration = zxdg_decoration_manager_v1_get_toplevel_decoration (
				decoration_manager, xdg_toplevel);
		zxdg_toplevel_decoration_v1_add_listener (toplevel_decoration,
				&toplevel_decoration_listener, nullptr);

		zxdg_toplevel_decoration_v1_set_mode (toplevel_decoration,
			ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
	}

	wl_surface_commit (wl_surf);
	wl_display_roundtrip (wl_disp);

	WL_SetCaption (va ("%s", PACKAGE_STRING));
}

void
WL_OpenDisplay (void)
{
	wl_disp = wl_display_connect (nullptr);
	if (!wl_disp) {
		Sys_Error ("WL_OpenDisplay: Could not open display\n");
	}

	wl_reg = wl_display_get_registry (wl_disp);
	if (!wl_reg) {
		Sys_Error ("WL_OpenDisplay: Could not get registry for display\n");
	}

	wl_registry_add_listener (wl_reg, &registry_listener, nullptr);
	wl_display_roundtrip (wl_disp);
}

void
WL_SetCaption (const char *text)
{
	if (!xdg_toplevel) {
		return;
	}

	Sys_MaskPrintf(SYS_wayland, "WL_SetCaption: %s\n", text);

	xdg_toplevel_set_title(xdg_toplevel, text);
}

extern int wl_force_link;
static __attribute__((used)) int *context_wl_force_link = &wl_force_link;
