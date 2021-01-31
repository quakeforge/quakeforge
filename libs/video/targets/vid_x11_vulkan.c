/*
	vid_x11_vulkan.c

	Vulkan X11 video driver

	Copyright (C) 1996-1997  Id Software, Inc.
	Copyright (C) 1999-2000  contributors of the QuakeForge project
	Copyright (C) 2000       Marcus Sundberg [mackan@stacken.kth.se]
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include <dlfcn.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#define VK_NO_PROTOTYPES
#define VK_USE_PLATFORM_XLIB_KHR
#include <vulkan/vulkan.h>

#include "QF/cvar.h"
#include "QF/set.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "QF/Vulkan/instance.h"

#include "context_x11.h"
#include "vid_internal.h"
#include "vid_vulkan.h"

static cvar_t *vulkan_library_name;

typedef struct vulkan_presentation_s {
#define PRESENTATION_VULKAN_FUNCTION_FROM_EXTENSION(name,ext) PFN_##name name;
#include "QF/Vulkan/funclist.h"

	Display    *display;
	Window      window;
	int         num_visuals;
	XVisualInfo *visinfo;
	set_t      *usable_visuals;
} vulkan_presentation_t;

static const char *required_extensions[] = {
	VK_KHR_XLIB_SURFACE_EXTENSION_NAME,
	0
};

static void *vulkan_library;

static void
load_vulkan_library (vulkan_ctx_t *ctx)
{
	vulkan_library = dlopen (vulkan_library_name->string,
							 RTLD_DEEPBIND | RTLD_NOW);
	if (!vulkan_library) {
		Sys_Error ("Couldn't load vulkan library %s: %s",
				   vulkan_library_name->name, dlerror ());
	}

	#define EXPORTED_VULKAN_FUNCTION(name) \
	ctx->name = (PFN_##name) dlsym (vulkan_library, #name); \
	if (!ctx->name) { \
		Sys_Error ("Couldn't find exported vulkan function %s", #name); \
	}

	#define GLOBAL_LEVEL_VULKAN_FUNCTION(name) \
	ctx->name = (PFN_##name) ctx->vkGetInstanceProcAddr (0, #name); \
	if (!ctx->name) { \
		Sys_Error ("Couldn't find global-level function %s", #name); \
	}

	#include "QF/Vulkan/funclist.h"
}

static void
unload_vulkan_library (vulkan_ctx_t *ctx)
{
	dlclose (vulkan_library);
	vulkan_library = 0;
}

static void
x11_vulkan_init_presentation (vulkan_ctx_t *ctx)
{
	ctx->presentation = calloc (1, sizeof (vulkan_presentation_t));
	vulkan_presentation_t *pres = ctx->presentation;
	qfv_instance_t *instance = ctx->instance;
	VkInstance  inst = instance->instance;

#define PRESENTATION_VULKAN_FUNCTION_FROM_EXTENSION(name, ext) \
	if (instance->extension_enabled (instance, ext)) { \
		pres->name = (PFN_##name) ctx->vkGetInstanceProcAddr (inst, #name); \
		if (!pres->name) { \
			Sys_Error ("Couldn't find instance-level function %s", #name); \
		} \
	}
#include "QF/Vulkan/funclist.h"

	XVisualInfo template;
	Visual *defaultVisual = XDefaultVisual (x_disp, x_screen);
	template.visualid = XVisualIDFromVisual (defaultVisual);
	int template_mask = VisualIDMask;
	pres->display = x_disp;
	pres->usable_visuals = set_new ();
	pres->visinfo = XGetVisualInfo (x_disp, template_mask, &template,
									&pres->num_visuals);
}

static int
x11_vulkan_get_presentation_support (vulkan_ctx_t *ctx,
									 VkPhysicalDevice physicalDevice,
									 uint32_t queueFamilyIndex)
{
	if (!ctx->presentation) {
		x11_vulkan_init_presentation (ctx);
	}
	vulkan_presentation_t *pres = ctx->presentation;

	set_empty (pres->usable_visuals);
	for (int i = 0; i < pres->num_visuals; i++) {
		VisualID    visID = pres->visinfo[i].visualid;

		if (pres->vkGetPhysicalDeviceXlibPresentationSupportKHR (
				physicalDevice, queueFamilyIndex, pres->display, visID)) {
			set_add (pres->usable_visuals, i);
		}
	}
	return !set_is_empty (pres->usable_visuals);
}

static void
x11_vulkan_choose_visual (vulkan_ctx_t *ctx)
{
	vulkan_presentation_t *pres = ctx->presentation;
	set_iter_t *first = set_first (pres->usable_visuals);
	if (first) {
		x_visinfo = pres->visinfo + first->element;
		x_vis = x_visinfo->visual;
		set_del_iter (first);
	}
}

static void
x11_vulkan_create_window (vulkan_ctx_t *ctx)
{
	vulkan_presentation_t *pres = ctx->presentation;
	pres->window = x_win;
}

static VkSurfaceKHR
x11_vulkan_create_surface (vulkan_ctx_t *ctx)
{
	vulkan_presentation_t *pres = ctx->presentation;
	VkInstance inst = ctx->instance->instance;
	VkSurfaceKHR surface;
	VkXlibSurfaceCreateInfoKHR createInfo = {
		.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR,
		.flags = 0,
		.dpy = pres->display,
		.window = pres->window
	};

	if (pres->vkCreateXlibSurfaceKHR (inst, &createInfo, 0, &surface)
		!= VK_SUCCESS) {
		return 0;
	}
	return surface;
}

vulkan_ctx_t *
X11_Vulkan_Context (void)
{
	vulkan_ctx_t *ctx = calloc (1, sizeof (vulkan_ctx_t));
	ctx->load_vulkan = load_vulkan_library;
	ctx->unload_vulkan = unload_vulkan_library;
	ctx->get_presentation_support = x11_vulkan_get_presentation_support;
	ctx->choose_visual = x11_vulkan_choose_visual;
	ctx->create_window = x11_vulkan_create_window;
	ctx->create_surface = x11_vulkan_create_surface;
	ctx->required_extensions = required_extensions;
	ctx->va_ctx = va_create_context (4);
	return ctx;
}

void
X11_Vulkan_Init_Cvars (void)
{
	vulkan_library_name = Cvar_Get ("vulkan_library", "libvulkan.so.1",
									CVAR_ROM, 0,
									"the name of the vulkan shared library");
}
