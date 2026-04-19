/*
	qf_painter.h

	2D drawing based on Guerilla's UIPainter

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

#ifndef __QF_Vulkan_qf_painter_h
#define __QF_Vulkan_qf_painter_h

#include "QF/simd/types.h"
#include "QF/Vulkan/staging.h"

typedef struct vulkan_ctx_s vulkan_ctx_t;

typedef struct uip_cmd_s {
	uint32_t    cmd;
	uint32_t    next;
} uip_cmd_t;

typedef struct uip_line_s {
	uip_cmd_t   cmd;
	float       a[2];
	float       b[2];
	float       r;
	byte        col[4];
} uip_line_t;

typedef struct uip_circle_s {
	uip_cmd_t   cmd;
	float       c[2];
	float       r;
	byte        col[4];
} uip_circle_t;

typedef struct uip_box_s {
	uip_cmd_t   cmd;
	float       c[2];
	float       e[2];
	float       r;
	byte        col[4];
} uip_box_t;

typedef struct painterframe_s {
	VkImage     cmd_heads_image;
	VkImageView cmd_heads_view;
	VkBuffer    cmd_queue;
	VkDescriptorSet set;
} painterframe_t;

typedef struct painterframeset_s
	DARRAY_TYPE (painterframe_t) painterframeset_t;

typedef struct uip_queue_s
	DARRAY_TYPE (uint32_t) uip_queue_t;

typedef struct painterctx_s {
	qfv_dsmanager_t *dsmanager;
	qfv_resource_t *resource;
	painterframeset_t frames;
	qfv_extent_t cmd_extent;
	uint32_t    max_queues;
	uint32_t   *cmd_heads;
	uint32_t   *cmd_tails;
	uip_queue_t cmd_queue;
} painterctx_t;

void Vulkan_Painter_Init (vulkan_ctx_t *ctx);
void Vulkan_Painter_AddLine (vec2f_t p1, vec2f_t p2, float r,
							 const quat_t color, vulkan_ctx_t *ctx);
void Vulkan_Painter_AddCircle (vec2f_t c, float r, const quat_t color,
							   vulkan_ctx_t *ctx);
void Vulkan_Painter_AddBox (vec2f_t c, vec2f_t e, float r, const quat_t color,
							vulkan_ctx_t *ctx);


#endif//__QF_Vulkan_qf_painter_h
