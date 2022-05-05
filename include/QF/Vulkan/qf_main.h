/*
	qf_main.h

	Vulkan main stuff from the renderer.

	Copyright (C) 2021 Bill Currie <bill@taniwha.org>

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

#ifndef __QF_Vulkan_qf_main_h
#define __QF_Vulkan_qf_main_h

struct vulkan_ctx_s;
struct qfv_renderframe_s;
struct entqueue_s;
struct scene_s;

void Vulkan_NewScene (struct scene_s *scene, struct vulkan_ctx_s *ctx);
void Vulkan_RenderView (struct qfv_renderframe_s *rFrame);
void Vulkan_RenderEntities (struct entqueue_s *queue,
							struct qfv_renderframe_s *rFrame);

#endif//__QF_Vulkan_qf_main_h
