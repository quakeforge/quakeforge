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

typedef struct vulkan_ctx_s vulkan_ctx_t;

void Vulkan_Painter_Init (vulkan_ctx_t *ctx);
void Vulkan_Painter_AddLine (vec2f_t p1, vec2f_t p2, float r,
							 const quat_t color, vulkan_ctx_t *ctx);
void Vulkan_Painter_AddCircle (vec2f_t c, float r, const quat_t color,
							   vulkan_ctx_t *ctx);
void Vulkan_Painter_AddBox (vec2f_t c, vec2f_t e, float r, const quat_t color,
							vulkan_ctx_t *ctx);


#endif//__QF_Vulkan_qf_painter_h
