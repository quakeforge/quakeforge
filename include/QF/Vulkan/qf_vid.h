/*
	Vulkan/qf_vid.h

	vulkan vid stuff from the renderer.

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

#ifndef __QF_Vulkan_vid_h
#define __QF_Vulkan_vid_h

#include "QF/Vulkan/cvars.h"

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include <vulkan/vulkan.h>

//FIXME location
typedef enum {
	QFV_passDepth,			// geometry
	QFV_passTranslucent,	// geometry
	QFV_passGBuffer,		// geometry
	QFV_passLighting,		// single quad
	QFV_passCompose,		// single quad

	QFV_NumPasses
} QFV_Subpass;

enum {
	QFV_attachDepth,
	QFV_attachColor,
	QFV_attachEmission,
	QFV_attachNormal,
	QFV_attachPosition,
	QFV_attachOpaque,
	QFV_attachTranslucent,
	QFV_attachSwapchain,
};

struct vulkan_ctx_s;
void Vulkan_DestroyFrames (struct vulkan_ctx_s *ctx);
void Vulkan_CreateFrames (struct vulkan_ctx_s *ctx);
void Vulkan_CreateCapture (struct vulkan_ctx_s *ctx);
void Vulkan_CreateRenderPass (struct vulkan_ctx_s *ctx);
void Vulkan_DestroyRenderPasses (struct vulkan_ctx_s *ctx);
void Vulkan_CreateMatrices (struct vulkan_ctx_s *ctx);
void Vulkan_DestroyMatrices (struct vulkan_ctx_s *ctx);
void Vulkan_CalcProjectionMatrices (struct vulkan_ctx_s *ctx);
void Vulkan_CalcViewMatrix (struct vulkan_ctx_s *ctx);
void Vulkan_CreateSwapchain (struct vulkan_ctx_s *ctx);
void Vulkan_CreateDevice (struct vulkan_ctx_s *ctx);
void Vulkan_Init_Common (struct vulkan_ctx_s *ctx);
void Vulkan_Shutdown_Common (struct vulkan_ctx_s *ctx);
void Vulkan_CreateStagingBuffers (struct vulkan_ctx_s *ctx);

VkPipeline Vulkan_CreatePipeline (struct vulkan_ctx_s *ctx, const char *name);
VkDescriptorPool Vulkan_CreateDescriptorPool (struct vulkan_ctx_s *ctx,
											  const char *name);
VkPipelineLayout Vulkan_CreatePipelineLayout (struct vulkan_ctx_s *ctx,
											  const char *name);
VkSampler Vulkan_CreateSampler (struct vulkan_ctx_s *ctx, const char *name);
VkDescriptorSetLayout Vulkan_CreateDescriptorSetLayout(struct vulkan_ctx_s*ctx,
													   const char *name);

#endif // __QF_Vulkan_vid_h
