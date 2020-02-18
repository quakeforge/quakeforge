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

struct vulkan_ctx_s;
void Vulkan_DestroyFramebuffers (struct vulkan_ctx_s *ctx);
void Vulkan_CreateFramebuffers (struct vulkan_ctx_s *ctx);
void Vulkan_DestroyRenderPass (struct vulkan_ctx_s *ctx);
void Vulkan_CreateRenderPass (struct vulkan_ctx_s *ctx);
void Vulkan_CreateSwapchain (struct vulkan_ctx_s *ctx);
void Vulkan_CreateDevice (struct vulkan_ctx_s *ctx);
void Vulkan_Init_Common (struct vulkan_ctx_s *ctx);
void Vulkan_Shutdown_Common (struct vulkan_ctx_s *ctx);

#endif // __QF_Vulkan_vid_h
