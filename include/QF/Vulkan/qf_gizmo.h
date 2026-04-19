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
#include "QF/Vulkan/render.h"

typedef struct vulkan_ctx_s vulkan_ctx_t;
typedef struct gizmo_node_s gizmo_node_t;
typedef struct qfv_dsmanager_s qfv_dsmanager_t;

typedef struct giz_count_s {
	uint32_t    numObjects;
	uint32_t    maxObjects;
} giz_count_t;

typedef struct giz_queue_s {
	uint32_t    obj_id;
	uint32_t    next;
} giz_queue_t;

typedef struct giz_sphere_s {
	uint32_t    cmd;
	float       c[3];
	float       r;
	byte        col[4];
} giz_sphere_t;

typedef struct giz_capsule_s {
	uint32_t    cmd;
	float       p1[3];
	float       p2[3];
	float       r;
	byte        col[4];
} giz_capsule_t;

typedef struct giz_brush_s {
	uint32_t    cmd;
	float       orig[3];
	float       mins[3];
	float       maxs[3];
	byte        col[4];
	// variable length: num_nodes * gizmo_node_t (packed)
} giz_brush_t;

typedef struct giz_plane_s {
	uint32_t    cmd;
	byte        gcol[4];
	byte        scol[4];
	byte        tcol[4];
	float       p[3][4];
} giz_plane_t;

typedef struct gizmoframe_s {
	VkBuffer    counts;
	VkBuffer    queue;
	VkImageView queue_heads_view;
	VkBuffer    objects;
	VkBuffer    obj_ids;
	uint32_t    num_obj_ids;
	uint32_t    num_fs_obj_ids;

	VkImage     queue_heads_image;
	VkDescriptorSet set;
} gizmoframe_t;

typedef struct gizmoframeset_s
	DARRAY_TYPE (gizmoframe_t) gizmoframeset_t;

typedef struct giz_data_s
	DARRAY_TYPE (uint32_t) giz_data_t;

typedef struct gizmoctx_s {
	qfv_dsmanager_t *dsmanager;
	qfv_resource_t *resource;
	gizmoframeset_t frames;
	uint32_t    cmd_width;
	uint32_t    cmd_height;
	giz_data_t  fs_obj_ids;
	giz_data_t  obj_ids;
	giz_data_t  objects;

	vec2f_t    *frag_size;
	uint32_t   *full_screen;
} gizmoctx_t;

void Vulkan_Gizmo_Init (vulkan_ctx_t *ctx);
void Vulkan_Gizmo_AddSphere (vec4f_t c, float r, const quat_t color,
							 vulkan_ctx_t *ctx);
void Vulkan_Gizmo_AddCapsule (vec4f_t p1, vec4f_t p2, float r,
							  const quat_t color, vulkan_ctx_t *ctx);
void Vulkan_Gizmo_AddBrush (vec4f_t orig, const vec4f_t bounds[2],
							int num_nodes, const gizmo_node_t *nodes,
							quat_t color, vulkan_ctx_t *ctx);
void Vulkan_Gizmo_AddPlane (vec4f_t s, vec4f_t t, vec4f_t p,
							quat_t gcol, quat_t scol, quat_t tcol,
							vulkan_ctx_t *ctx);


#endif//__QF_Vulkan_qf_gizmo_h
