/*
	qf_gizmo.h

	3D drawing loosely based on Guerilla's UIPainter

	Copyright (C) 2025 Bill Currie <bill@taniwha.org>

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

#ifndef __QF_Vulkan_qf_gizmo_h
#define __QF_Vulkan_qf_gizmo_h

#include "QF/simd/types.h"

typedef struct vulkan_ctx_s vulkan_ctx_t;
typedef struct gizmo_node_s gizmo_node_t;

void Vulkan_Gizmo_Init (vulkan_ctx_t *ctx);
void Vulkan_Gizmo_AddSphere (vec4f_t c, float r, const quat_t color,
							 vulkan_ctx_t *ctx);
void Vulkan_Gizmo_AddCapsule (vec4f_t p1, vec4f_t p2, float r,
							  const quat_t color, vulkan_ctx_t *ctx);
void Vulkan_Gizmo_AddBrush (vec4f_t orig, const vec4f_t bounds[2],
							int num_nodes, const gizmo_node_t *nodes,
							quat_t color, vulkan_ctx_t *ctx);


#endif//__QF_Vulkan_qf_gizmo_h
