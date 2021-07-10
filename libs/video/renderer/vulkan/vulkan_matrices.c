/*
	vid_common_vulkan.c

	Common Vulkan video driver functions

	Copyright (C) 2021      Bill Currie <bill@taniwha.org>

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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_MATH_H
# include <math.h>
#endif

#include "QF/sys.h"
#include "QF/vid.h"
#include "QF/Vulkan/qf_vid.h"
#include "QF/Vulkan/buffer.h"
#include "QF/Vulkan/device.h"
#include "QF/Vulkan/projection.h"
#include "QF/ui/view.h"

#include "r_internal.h"
#include "vid_vulkan.h"

#include "util.h"

#define MAT_SIZE (16 * sizeof (float))

void
Vulkan_DestroyMatrices (vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;

	__auto_type mat = &ctx->matrices;

	dfunc->vkUnmapMemory (device->dev, mat->memory);
	dfunc->vkFreeMemory (device->dev, mat->memory, 0);
	dfunc->vkDestroyBuffer (device->dev, mat->buffer_2d, 0);
	dfunc->vkDestroyBuffer (device->dev, mat->buffer_3d, 0);
}

void
Vulkan_CreateMatrices (vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;

	__auto_type mat = &ctx->matrices;
	mat->buffer_2d = QFV_CreateBuffer (device, 1 * MAT_SIZE,
									   VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
	mat->buffer_3d = QFV_CreateBuffer (device, 3 * MAT_SIZE,
									   VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

	size_t      size = 0;
	size_t      offset;
	VkMemoryRequirements req;

	dfunc->vkGetBufferMemoryRequirements (device->dev, mat->buffer_2d, &req);
	size += req.size;
	offset = size;

	dfunc->vkGetBufferMemoryRequirements (device->dev, mat->buffer_3d, &req);
	offset = (offset + req.alignment - 1) & ~(req.alignment - 1);
	size += req.size;

	mat->memory = QFV_AllocBufferMemory (device, mat->buffer_2d,
										 VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
										 size, 0);
	QFV_BindBufferMemory (device, mat->buffer_2d, mat->memory, 0);
	QFV_BindBufferMemory (device, mat->buffer_3d, mat->memory, offset);
	void       *data;
	dfunc->vkMapMemory (device->dev, mat->memory, 0, size, 0, &data);
	mat->projection_2d = data;
	mat->projection_3d = (vec4f_t *) ((byte *) mat->projection_2d + offset);
	mat->view_3d = mat->projection_3d + 4;
	mat->sky_3d = mat->view_3d + 4;
}

void
Vulkan_CalcProjectionMatrices (vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;

	__auto_type mat = &ctx->matrices;

	int width = vid.conview->xlen;
	int height = vid.conview->ylen;
	QFV_Orthographic (mat->projection_2d, 0, width, 0, height, -99999, 99999);

	float       aspect = (float) r_refdef.vrect.width / r_refdef.vrect.height;
	QFV_Perspective (mat->projection_3d, r_refdef.fov_y, aspect);
#if 0
	Sys_MaskPrintf (SYS_vulkan, "ortho:\n");
	Sys_MaskPrintf (SYS_vulkan, "   [[%g, %g, %g, %g],\n",
					QuatExpand (mat->projection_2d + 0));
	Sys_MaskPrintf (SYS_vulkan, "    [%g, %g, %g, %g],\n",
					QuatExpand (mat->projection_2d + 4));
	Sys_MaskPrintf (SYS_vulkan, "    [%g, %g, %g, %g],\n",
					QuatExpand (mat->projection_2d + 8));
	Sys_MaskPrintf (SYS_vulkan, "    [%g, %g, %g, %g]]\n",
					QuatExpand (mat->projection_2d + 12));
	Sys_MaskPrintf (SYS_vulkan, "presp:\n");
	Sys_MaskPrintf (SYS_vulkan, "   [[%g, %g, %g, %g],\n",
					QuatExpand (mat->projection_3d + 0));
	Sys_MaskPrintf (SYS_vulkan, "    [%g, %g, %g, %g],\n",
					QuatExpand (mat->projection_3d + 4));
	Sys_MaskPrintf (SYS_vulkan, "    [%g, %g, %g, %g],\n",
					QuatExpand (mat->projection_3d + 8));
	Sys_MaskPrintf (SYS_vulkan, "    [%g, %g, %g, %g]]\n",
					QuatExpand (mat->projection_3d + 12));
#endif
	VkMappedMemoryRange ranges[] = {
		{ VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, 0, mat->memory, 0, MAT_SIZE },
		{ VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, 0, mat->memory,
		  (mat->projection_3d - mat->projection_2d) * sizeof (float),
		  MAT_SIZE},
	};
	dfunc->vkFlushMappedMemoryRanges (device->dev, 2, ranges);
}
