/*
	qf_output.h

	Vulkan output pass

	Copyright (C) 2022 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2022/11/21

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
#ifndef __QF_Vulkan_qf_output_h
#define __QF_Vulkan_qf_output_h

#include "QF/darray.h"
#include "QF/Vulkan/qf_vid.h"
#include "QF/Vulkan/command.h"

typedef struct outputframe_s {
	VkCommandBuffer cmd;
	VkImageView input;
	VkDescriptorSet set;
} outputframe_t;

typedef struct outputframeset_s
    DARRAY_TYPE (outputframe_t) outputframeset_t;

typedef struct outputctx_s {
	outputframeset_t frames;
	VkPipeline   output;
	VkPipeline   waterwarp;
	VkPipeline   fisheye;
	VkPipelineLayout output_layout;
	VkPipelineLayout warp_layout;
	VkPipelineLayout fish_layout;
	VkSampler    sampler;
	VkImageView  input;
	VkFramebuffer *framebuffers;	// one per swapchain image
} outputctx_t;

struct vulkan_ctx_s;

void Vulkan_Output_Init (struct vulkan_ctx_s *ctx);
void Vulkan_Output_Shutdown (struct vulkan_ctx_s *ctx);
void Vulkan_Output_CreateRenderPasses (struct vulkan_ctx_s *ctx);
void Vulkan_Output_SetInput (struct vulkan_ctx_s *ctx, VkImageView input);

#endif//__QF_Vulkan_qf_output_h
