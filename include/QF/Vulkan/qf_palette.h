/*
	qf_palette.h

	Vulkan matrix "pass"

	Copyright (C) 2021 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2021/12/8

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
#ifndef __QF_Vulkan_qf_palette_h
#define __QF_Vulkan_qf_palette_h

#include "QF/darray.h"
#include "QF/Vulkan/qf_vid.h"
#include "QF/Vulkan/command.h"
#include "QF/Vulkan/image.h"

typedef struct palettectx_s {
	struct qfv_tex_s *palette;
	VkSampler    sampler;
	VkDescriptorSet descriptor;
} palettectx_t;

struct vulkan_ctx_s;

void Vulkan_Palette_Update (struct vulkan_ctx_s *ctx, const byte *palette);
void Vulkan_Palette_Init (struct vulkan_ctx_s *ctx, const byte *palette);
void Vulkan_Palette_Shutdown (struct vulkan_ctx_s *ctx);
VkDescriptorSet Vulkan_Palette_Descriptor (struct vulkan_ctx_s *ctx) __attribute__((pure));

#endif//__QF_Vulkan_qf_palette_h
