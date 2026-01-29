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
#include "libs/video/targets/xdg-shell.h"
#include "libs/video/targets/xdg-decoration-unstable-v1.h"
#include "libs/video/targets/relative-pointer-unstable-v1.h"
#include "libs/video/targets/pointer-constraints-unstable-v1.h"
#include "libs/video/targets/cursor-shape-v1.h"
#include "libs/video/targets/text-input-unstable-v3.h"

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

#define WL_IFACE(iface) struct iface *iface
	WL_ITER_IFACES ();
#undef WL_IFACE

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

static bool
match_interface (const char *name, uint32_t iface_id, const struct wl_interface *iface,
				 uint32_t version, void **global)
{
	if (strcmp (name, iface->name) != 0) {
		return false;
	}

	// FIXME: Handle unsupported verisons more gracefully (currently just segfaults)
	*global = wl_registry_bind (wl_registry, iface_id, iface, version);

	if (*global == nullptr) {
		Sys_Error ("Failed to bind registry interface %s", iface->name);
	}

	Sys_MaskPrintf (SYS_wayland, "Successfully bound interface %s\n", iface->name);

	return *global != nullptr;
}

static void
registry_handle_global (void *data, struct wl_registry *reg, uint32_t name,
                        const char *interface, uint32_t version)
{
#define MATCH_BEGIN() if (false) {}
#define MATCH_IFACE(iface, v) else if (match_interface (interface, name,\
									   &iface##_interface, v, (void **)&iface))
#define MATCH_END() else { Sys_MaskPrintf(SYS_wayland, "wl_interface: %s, v%d\n", interface, version); }

	MATCH_BEGIN ()
	MATCH_IFACE (wl_compositor, 6) {}
	MATCH_IFACE (wl_seat, 9) { IN_WL_RegisterSeat (); }
	MATCH_IFACE (wl_shm, 2) {}
	MATCH_IFACE (xdg_wm_base, 6) {
        xdg_wm_base_add_listener (xdg_wm_base, &xdg_base_listener, nullptr);
	}
	MATCH_IFACE (zxdg_decoration_manager_v1, 1) {}
	MATCH_IFACE (zwp_relative_pointer_manager_v1, 1) {}
	MATCH_IFACE (zwp_pointer_constraints_v1, 1) {}
	MATCH_IFACE (wp_cursor_shape_manager_v1, 1) {}
	MATCH_IFACE (zwp_text_input_manager_v3, 1) {}
    MATCH_END ()

#undef MATCH_BEGIN
#undef MATCH_IFACE
#undef MATCH_END
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
    wl_display_dispatch_timeout (wl_display, &ts);
}

static void
wl_destroy_window (void)
{
    xdg_toplevel_destroy (xdg_toplevel);
    xdg_surface_destroy (xdg_surface);
    wl_surface_destroy (wl_surface);
}

void
WL_CloseDisplay (void)
{
    Sys_MaskPrintf (SYS_wayland, "WL_CloseDisplay\n");

    wl_destroy_window ();

    // TODO: Remove registry globals

    wl_registry_destroy (wl_registry);
    wl_display_disconnect (wl_display);
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
	wl_surface = wl_compositor_create_surface (wl_compositor);
	xdg_surface = xdg_wm_base_get_xdg_surface (xdg_wm_base, wl_surface);
	xdg_surface_add_listener (xdg_surface, &surface_listener, nullptr);

	xdg_toplevel = xdg_surface_get_toplevel (xdg_surface);
	xdg_toplevel_add_listener (xdg_toplevel, &toplevel_listener, nullptr);
	xdg_toplevel_set_title (xdg_toplevel, "Hello");

	if (zxdg_decoration_manager_v1) {
		Sys_MaskPrintf (SYS_wayland, "Initializing decorations\n");

		zxdg_toplevel_decoration_v1 =
			zxdg_decoration_manager_v1_get_toplevel_decoration (
				zxdg_decoration_manager_v1, xdg_toplevel);
		zxdg_toplevel_decoration_v1_add_listener (zxdg_toplevel_decoration_v1,
				&toplevel_decoration_listener, nullptr);

		zxdg_toplevel_decoration_v1_set_mode (zxdg_toplevel_decoration_v1,
			ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
	}

	wl_surface_commit (wl_surface);
	wl_display_roundtrip (wl_display);

	WL_SetCaption (va ("%s", PACKAGE_STRING));
}

void
WL_OpenDisplay (void)
{
	wl_display = wl_display_connect (nullptr);
	if (!wl_display) {
		Sys_Error ("WL_OpenDisplay: Could not open display\n");
	}

	wl_registry = wl_display_get_registry (wl_display);
	if (!wl_registry) {
		Sys_Error ("WL_OpenDisplay: Could not get registry for display\n");
	}

	wl_registry_add_listener (wl_registry, &registry_listener, nullptr);
	wl_display_roundtrip (wl_display);
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
