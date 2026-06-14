/*
	miploop.h

	Looped mipmap rendering

	Copyright (C) 2026 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2026/6/11

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
#ifndef __QF_Vulkan_miploop_h
#define __QF_Vulkan_miploop_h


#include "QF/darray.h"
#include "QF/Vulkan/render.h"
#include "QF/simd/types.h"

typedef struct miplooptex_s {
	const char *var_name;
	const char *set_name;
	uint32_t   *tex_id;
	uint32_t    set_index;
} miplooptex_t;

typedef struct miplooptexset_s
	DARRAY_TYPE (miplooptex_t) miplooptexset_t;

typedef struct miploopctx_s {
	vec3u_t    *miploop_base;
	int32_t    *miploop_layer;
	vec3u_t    *miploop_size;
	uint32_t   *miploop_level;
	vec2u_t    *miploop_range;

	// FIXME per-instance
	qfv_attachmentinfo_t miploop_info;
	qfv_imageinfo_t *image_info;
	const char *img_name;
	uint32_t    layers;
	miplooptexset_t textures;
} miploopctx_t;

typedef struct vulkan_ctx_s vulkan_ctx_t;

void QFV_MipLoop_Init (struct vulkan_ctx_s *ctx);

#endif//__QF_Vulkan_miploop_h
