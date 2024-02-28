/*
	qf_model.h

	Vulkan specific model stuff

	Copyright (C) 2012 Bill Currie <bill@taniwha.org>
	Copyright (C) 2021 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2012/1/7
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
#ifndef __QF_Vulkan_qf_model_h
#define __QF_Vulkan_qf_model_h

#include "QF/darray.h"
#include "QF/model.h"
#include "QF/Vulkan/qf_vid.h"

typedef struct vulkan_ctx_s vulkan_ctx_t;

typedef struct modelctx_s {
	vulkan_ctx_t *ctx;
	VkDeviceMemory texture_memory;
} modelctx_t;

typedef struct skin_s skin_t;
typedef struct mod_alias_ctx_s mod_alias_ctx_t;
typedef struct mod_alias_skin_s mod_alias_skin_t;
typedef struct qfv_alias_skin_s qfv_alias_skin_t;
typedef struct mframe_s mframe_t;
qfv_alias_skin_t *
Vulkan_Mod_LoadSkin (mod_alias_ctx_t *alias_ctx, mod_alias_skin_t *askin,
					 int skinsize, qfv_alias_skin_t *vskin, vulkan_ctx_t *ctx);
void Vulkan_Skin_Clear (qfv_alias_skin_t *skin, vulkan_ctx_t *ctx);

void Vulkan_Skin_SetupSkin (skin_t *skin, struct vulkan_ctx_s *ctx);
void Vulkan_Skin_Destroy (skin_t *skin, struct vulkan_ctx_s *ctx);

#endif//__QF_Vulkan_qf_model_h
