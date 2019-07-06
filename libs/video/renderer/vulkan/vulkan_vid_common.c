/*
	vid_common_vulkan.c

	Common Vulkan video driver functions

	Copyright (C) 1996-1997 Id Software, Inc.
	Copyright (C) 2019      Bill Currie <bill@taniwha.org>

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

#ifdef HAVE_MATH_H
# include <math.h>
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include <dlfcn.h>

#include "QF/cvar.h"
#include "QF/dstring.h"
#include "QF/input.h"
#include "QF/qargs.h"
#include "QF/quakefs.h"
#include "QF/sys.h"
#include "QF/va.h"
#include "QF/vid.h"
#include "QF/Vulkan/qf_vid.h"
#include "QF/Vulkan/init.h"

#include "compat.h"
#include "d_iface.h"
#include "r_internal.h"

static void *vulkan_library;

static VulkanInstance_t *vulkan_instance;

static void
load_vulkan_library (void)
{
	vulkan_library = dlopen (vulkan_library_name->string, RTLD_NOW);
	if (!vulkan_library) {
		Sys_Error ("Couldn't load vulkan library %s: %s",
				   vulkan_library_name->name, dlerror ());
	}

	#define EXPORTED_VULKAN_FUNCTION(name) \
	name = (PFN_##name) dlsym (vulkan_library, #name); \
	if (!name) { \
		Sys_Error ("Couldn't find exported vulkan function %s", #name); \
	}

	// Can't use vkGetInstanceProcAddr (0, ...) here because it grabs from
	// the whole exe and thus sees the function pointers instead of the actual
	// functions. Sure, could rename all the function pointers, but while that
	// worked for gl and glsl, it also made it a real pain to work with the
	// spec (or man pages, etc). Of course, namespaces would help, too.
	#define GLOBAL_LEVEL_VULKAN_FUNCTION(name) \
	name = (PFN_##name) dlsym (vulkan_library, #name); \
	if (!name) { \
		Sys_Error ("Couldn't find global-level function %s", #name); \
	}

	#include "QF/Vulkan/funclist.h"
}

void
Vulkan_Init_Cvars ()
{
	vulkan_library_name = Cvar_Get ("vulkan_library", "libvulkan.so.1",
									CVAR_ROM, 0,
									"the name of the vulkan shared library");
	vulkan_use_validation = Cvar_Get ("vulkan_use_validation", "1", CVAR_NONE,
									  0,
									  "enable LunarG Standard Validation "
									  "Layer if available (requires instance "
									  "restart).");
}

const char *extensions[] = {
	"foo",
	0,
};

void
Vulkan_Init_Common (void)
{
	Vulkan_Init_Cvars ();

	load_vulkan_library ();

	vulkan_instance = Vulkan_CreateInstance (PACKAGE_STRING, 0x000702ff, 0, extensions);//FIXME version
	Sys_Printf ("Vulkan_Init_Common\n");
	if (developer->int_val & SYS_VID) {
		Vulkan_DestroyInstance (vulkan_instance);
		Sys_Quit();
	}
}

void
Vulkan_Shutdown_Common (void)
{
	Vulkan_DestroyInstance (vulkan_instance);
}
