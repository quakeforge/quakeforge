/*
	qf_iqm.h

	Vulkan specific iqm model stuff

	Copyright (C) 2022 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2022/5/3

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
#ifndef __QF_Vulkan_qf_iqm_h
#define __QF_Vulkan_qf_iqm_h

#include "QF/darray.h"
#include "QF/model.h"
#include "QF/modelgen.h"
#include "QF/Vulkan/qf_vid.h"
#include "QF/Vulkan/command.h"

struct mod_iqm_ctx_s;
void Vulkan_Mod_IQMFinish (struct mod_iqm_ctx_s *iqm_ctx,
						   struct vulkan_ctx_s *ctx);

//void Vulkan_IQMAddBones (struct vulkan_ctx_s *ctx, struct iqm_s *iqm);
//void Vulkan_IQMRemoveBones (struct vulkan_ctx_s *ctx, struct iqm_s *iqm);

//void Vulkan_IQMAddSkin (struct vulkan_ctx_s *ctx, qfv_iqm_skin_t *skin);
//void Vulkan_IQMRemoveSkin (struct vulkan_ctx_s *ctx, qfv_iqm_skin_t *skin);

#endif//__QF_Vulkan_qf_iqm_h
