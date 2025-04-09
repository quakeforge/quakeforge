/*
	Vulkan/qf_vid.h

	vulkan vid stuff from the renderer.

	Copyright (C) 2019 Bill Currie <bill@taniwha.org>

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

#ifndef __QF_Vulkan_vid_h
#define __QF_Vulkan_vid_h

#include "QF/Vulkan/cvars.h"

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include <vulkan/vulkan.h>

/** \defgroup vulkan Vulkan Renderer
*/

enum {
	QFV_rp_particles,
	QFV_rp_shadowmap,
	QFV_rp_preoutput,
	QFV_rp_translucent,
	QFV_rp_main,
	QFV_rp_output,
};

//FIXME location
typedef enum {
	QFV_passDepth,				// geometry
	QFV_passTranslucentFrag,	// geometry
	QFV_passGBuffer,			// geometry
	QFV_passLighting,			// single triangle
	QFV_passCompose,			// single triangle

	QFV_NumPasses
} QFV_Subpass;

enum {
	QFV_attachDepth,
	QFV_attachColor,
	QFV_attachEmission,
	QFV_attachNormal,
	QFV_attachPosition,
	QFV_attachLight,
	QFV_attachSwapchain,
};

struct vulkan_ctx_s;
void Vulkan_CreateSwapchain (struct vulkan_ctx_s *ctx);
void Vulkan_CreateDevice (struct vulkan_ctx_s *ctx);
void Vulkan_Init_Common (struct vulkan_ctx_s *ctx);
void Vulkan_Shutdown_Common (struct vulkan_ctx_s *ctx);
void Vulkan_CreateStagingBuffers (struct vulkan_ctx_s *ctx);

struct qfv_output_s;
void Vulkan_ConfigOutput (struct vulkan_ctx_s *ctx,
						  struct qfv_output_s *output);

VkSampler Vulkan_CreateSampler (struct vulkan_ctx_s *ctx, const char *name);

struct entity_s;
void Vulkan_BeginEntityLabel (struct vulkan_ctx_s *ctx, VkCommandBuffer cmd,
							  struct entity_s ent);

struct plitem_s *Vulkan_GetConfig (struct vulkan_ctx_s *ctx, const char *name);

extern int vulkan_frame_width;
extern int vulkan_frame_height;
extern int vulkan_oit_fragments;

#endif // __QF_Vulkan_vid_h
