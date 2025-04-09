/*
	vid_win.c

	Win32 vid component

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

*/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "winquake.h"
#define VK_NO_PROTOTYPES
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

#include "QF/cvar.h"
#include "QF/set.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "QF/Vulkan/instance.h"

#include "context_win.h"
#include "vid_internal.h"
#include "vid_vulkan.h"

static char *vulkan_library_name;
static cvar_t vulkan_library_name_cvar = {
	.name = "vulkan_library",
	.description =
		"the name of the vulkan shared library",
	.default_value = "vulkan-1.dll",
	.flags = CVAR_ROM,
	.value = { .type = 0, .value = &vulkan_library_name },
};

typedef struct vulkan_presentation_s {
#define PRESENTATION_VULKAN_FUNCTION_FROM_EXTENSION(name,ext) PFN_##name name;
#include "QF/Vulkan/funclist.h"

	HINSTANCE   instance;
	HWND        window;

} vulkan_presentation_t;

static const char *required_extensions[] = {
	VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
	0
};

static HMODULE vulkan_library;

static void
load_vulkan_library (vulkan_ctx_t *ctx)
{
	vulkan_library = LoadLibrary (vulkan_library_name);
	if (!vulkan_library) {
		DWORD       errcode = GetLastError ();
		Sys_Error ("Couldn't load vulkan library %s: %ld",
				   vulkan_library_name, errcode);
	}

	#define EXPORTED_VULKAN_FUNCTION(name) \
	ctx->name = (PFN_##name) GetProcAddress (vulkan_library, #name); \
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
	FreeLibrary (vulkan_library);
	vulkan_library = 0;
}

static void
win_vulkan_init_presentation (vulkan_ctx_t *ctx)
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

	pres->instance = GetModuleHandle (0);
}

static int
win_vulkan_get_presentation_support (vulkan_ctx_t *ctx,
									 VkPhysicalDevice physicalDevice,
									 uint32_t queueFamilyIndex)
{
	if (!ctx->presentation) {
		win_vulkan_init_presentation (ctx);
	}
	vulkan_presentation_t *pres = ctx->presentation;
	if (pres->vkGetPhysicalDeviceWin32PresentationSupportKHR (
			physicalDevice, queueFamilyIndex)) {
		return 1;
	}
	return 0;
}

static void
win_vulkan_choose_visual (vulkan_ctx_t *ctx)
{
}

static void
win_vulkan_create_window (vulkan_ctx_t *ctx)
{
	vulkan_presentation_t *pres = ctx->presentation;
	pres->window = win_mainwindow;
}

static VkSurfaceKHR
win_vulkan_create_surface (vulkan_ctx_t *ctx)
{
	vulkan_presentation_t *pres = ctx->presentation;
	VkInstance inst = ctx->instance->instance;
	VkSurfaceKHR surface;
	VkWin32SurfaceCreateInfoKHR createInfo = {
		.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
		.flags = 0,
		.hinstance = pres->instance,
		.hwnd = pres->window,
	};

	ctx->window_width = viddef.width;
	ctx->window_height = viddef.height;

	if (pres->vkCreateWin32SurfaceKHR (inst, &createInfo, 0, &surface)
		!= VK_SUCCESS) {
		return 0;
	}
	return surface;
}

static void
delete_vulkan_context (vulkan_ctx_t *ctx)
{
	if (ctx->presentation) {
		free (ctx->presentation);
	}
	va_destroy_context (ctx->va_ctx);
	free (ctx);
}

vulkan_ctx_t *
Win_Vulkan_Context (vid_internal_t *vi)
{
	vulkan_ctx_t *ctx = calloc (1, sizeof (vulkan_ctx_t));
	*ctx = (vulkan_ctx_t) {
		.delete = delete_vulkan_context,
		.load_vulkan = load_vulkan_library,
		.unload_vulkan = unload_vulkan_library,
		.get_presentation_support = win_vulkan_get_presentation_support,
		.choose_visual = win_vulkan_choose_visual,
		.create_window = win_vulkan_create_window,
		.create_surface = win_vulkan_create_surface,
		.required_extensions = required_extensions,
		.va_ctx = va_create_context (VA_CTX_COUNT),
		.twod_scale = 1,
	};

	vi->ctx = ctx;
	return ctx;
}

void
Win_Vulkan_Init_Cvars (void)
{
	Cvar_Register (&vulkan_library_name_cvar, 0, 0);
}
