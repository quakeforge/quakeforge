/*
	vulkan_main.c

	Vulkan rendering

	Copyright (C) 2021 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2021/1/19

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
# include "string.h"
#endif
#ifdef HAVE_STRINGS_H
# include "strings.h"
#endif

#include "QF/cmd.h"
#include "QF/cvar.h"
#include "QF/image.h"
#include "QF/render.h"
#include "QF/screen.h"
#include "QF/sys.h"

#include "QF/scene/entity.h"

#include "QF/Vulkan/qf_alias.h"
#include "QF/Vulkan/qf_bsp.h"
#include "QF/Vulkan/qf_compose.h"
#include "QF/Vulkan/qf_iqm.h"
#include "QF/Vulkan/qf_lighting.h"
#include "QF/Vulkan/qf_lightmap.h"
#include "QF/Vulkan/qf_main.h"
#include "QF/Vulkan/qf_matrices.h"
#include "QF/Vulkan/qf_output.h"
#include "QF/Vulkan/qf_particles.h"
#include "QF/Vulkan/qf_scene.h"
#include "QF/Vulkan/qf_sprite.h"
#include "QF/Vulkan/qf_translucent.h"
#include "QF/Vulkan/qf_vid.h"
#include "QF/Vulkan/swapchain.h"

#include "mod_internal.h"
#include "r_internal.h"
#include "vid_vulkan.h"

void
Vulkan_NewScene (scene_t *scene, vulkan_ctx_t *ctx)
{
	int         i;

	for (i = 0; i < 256; i++) {
		d_lightstylevalue[i] = 264;		// normal light value
	}

	r_refdef.worldmodel = scene->worldmodel;
	EntQueue_Clear (r_ent_queue);

	// Force a vis update
	R_MarkLeaves (0, 0, 0, 0);

	R_ClearParticles ();
	Vulkan_RegisterTextures (scene->models, scene->num_models, ctx);
	//Vulkan_BuildLightmaps (scene->models, scene->num_models, ctx);
	Vulkan_BuildDisplayLists (scene->models, scene->num_models, ctx);
	Vulkan_LoadLights (scene, ctx);
}
