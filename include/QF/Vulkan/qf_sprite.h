/*
	qf_sprite.h

	Vulkan specific sprite model stuff

	Copyright (C) 2012 Bill Currie <bill@taniwha.org>
	Copyright (C) 2021 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2012/1/7
	Date: 2021/1/18

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
#ifndef __QF_Vulkan_qf_sprite_h
#define __QF_Vulkan_qf_sprite_h

#include "QF/darray.h"
#include "QF/model.h"
#include "QF/modelgen.h"
#include "QF/Vulkan/qf_vid.h"
#include "QF/Vulkan/command.h"

typedef struct spritevrt_s {
	float       x, y, u, v;
} spritevrt_t;

typedef struct qfv_sprite_s {
	VkDeviceMemory memory;
	VkBuffer    verts;
	VkImage     image;
	VkImageView view;
	VkDescriptorSet descriptors;
} qfv_sprite_t;

typedef enum {
	QFV_spriteDepth,
	QFV_spriteGBuffer,
	QFV_spriteTranslucent,

	QFV_spriteNumPasses
} QFV_SpriteSubpass;

typedef struct spriteframe_s {
	qfv_cmdbufferset_t cmdSet;
} spriteframe_t;

typedef struct spriteframeset_s
    DARRAY_TYPE (spriteframe_t) spriteframeset_t;

typedef struct spritectx_s {
	spriteframeset_t frames;
	VkPipeline  depth;
	VkPipeline  gbuf;
	VkDescriptorPool pool;
	VkDescriptorSetLayout setLayout;
	VkPipelineLayout layout;
	unsigned    maxImages;
	VkSampler   sampler;
} spritectx_t;

struct vulkan_ctx_s;
struct qfv_renderframe_s;
struct entity_s;
struct mod_sprite_ctx_s;

void Vulkan_Sprint_FreeDescriptors (struct vulkan_ctx_s *ctx,
									qfv_sprite_t *sprite);
void Vulkan_Sprite_DescriptorSet (struct vulkan_ctx_s *ctx,
								  qfv_sprite_t *sprite);
void Vulkan_Mod_SpriteLoadFrames (struct mod_sprite_ctx_s *sprite_ctx,
								  struct vulkan_ctx_s *ctx);

void Vulkan_SpriteBegin (struct qfv_renderframe_s *rFrame);
void Vulkan_DrawSprite (struct entity_s *ent, struct qfv_renderframe_s *rFrame);
void Vulkan_SpriteEnd (struct qfv_renderframe_s *rFrame);

void Vulkan_Sprite_Init (struct vulkan_ctx_s *ctx);
void Vulkan_Sprite_Shutdown (struct vulkan_ctx_s *ctx);

#endif//__QF_Vulkan_qf_sprite_h
