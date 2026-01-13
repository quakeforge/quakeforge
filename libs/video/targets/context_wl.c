/*
	context_wl.c

	general wayland context layer

	copyright (c) 1996-1997  id software, inc.
	copyright (c) 2000       zephaniah e. hull <warp@whitestar.soark.net>
	copyright (c) 2000, 2001 jeff teunissen <deek@quakeforge.net>

	this program is free software; you can redistribute it and/or
	modify it under the terms of the gnu general public license
	as published by the free software foundation; either version 2
	of the license, or (at your option) any later version.

	this program is distributed in the hope that it will be useful,
	but without any warranty; without even the implied warranty of
	merchantability or fitness for a particular purpose.

	see the gnu general public license for more details.

	you should have received a copy of the gnu general public license
	along with this program; if not, write to:

		free software foundation, inc.
		59 temple place - suite 330
		boston, ma  02111-1307, usa

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
#include "libs/video/targets/xdg-shell-client-protocol.h"
#include "libs/video/targets/xdg-decoration-client-protocol.h"

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
#include "vid_internal.h"

struct wl_display *wl_disp;
struct wl_registry *wl_reg;
struct wl_compositor *wl_comp;
struct wl_surface *wl_surf;
struct wl_shm *wl_shm;
struct wl_shm_pool *wl_shm_pool;
struct xdg_wm_base *xdg_wm_base;
struct xdg_surface *xdg_surface;
struct xdg_toplevel *xdg_toplevel;
struct zxdg_decoration_manager_v1 *decoration_manager;
struct zxdg_toplevel_decoration_v1 *toplevel_decoration;

bool wl_surface_configured = false;

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

static void
toplevel_wm_capabilities (void *data, struct xdg_toplevel *toplvl,
                            struct wl_array *capabilities)
{
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
    } else if (strcmp (interface, wl_shm_interface.name) == 0) {
        wl_shm = wl_registry_bind (wl_reg, name, &wl_shm_interface, 2);
    } else if (strcmp (interface, xdg_wm_base_interface.name) == 0) {
        xdg_wm_base = wl_registry_bind (wl_reg, name, &xdg_wm_base_interface, 6);
        xdg_wm_base_add_listener(xdg_wm_base, &xdg_base_listener, nullptr);
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
        Sys_MaskPrintf(SYS_wayland, "Initializing decorations\n");

        toplevel_decoration = zxdg_decoration_manager_v1_get_toplevel_decoration (
                decoration_manager, xdg_toplevel);
        zxdg_toplevel_decoration_v1_add_listener(toplevel_decoration, &toplevel_decoration_listener, nullptr);

        zxdg_toplevel_decoration_v1_set_mode (toplevel_decoration,
                ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
    }

    wl_surface_commit (wl_surf);
}

void
WL_OpenDisplay (void)
{
    wl_disp = wl_display_connect (nullptr);
    if (!wl_disp) {
        Sys_Error("WL_OpenDisplay: Could not open display\n");
    }

    Sys_MaskPrintf(SYS_wayland, "WL_OpenDisplay: SUCCESS\n");

    wl_reg = wl_display_get_registry (wl_disp);
    if (!wl_reg) {
        Sys_Error("WL_OpenDisplay: Could not get registry for display\n");
    }

    wl_registry_add_listener (wl_reg, &registry_listener, nullptr);
    wl_display_roundtrip (wl_disp);
}

extern int wl_force_link;
static __attribute__((used)) int *context_wl_force_link = &wl_force_link;
