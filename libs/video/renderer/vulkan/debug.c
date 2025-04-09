/*
	debug.c

	Vulkan debug support

	Copyright (C) 2022      Bill Currie <bill@taniwha.org>

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

#include "QF/Vulkan/debug.h"
#include "QF/Vulkan/device.h"

void
QFV_CmdBeginLabel (qfv_device_t *device, VkCommandBuffer cmd,
				   const char *name, vec4f_t color)
{
	qfv_devfuncs_t *dfunc = device->funcs;
	if (dfunc->vkCmdBeginDebugUtilsLabelEXT) {
		VkDebugUtilsLabelEXT label = {
			VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT, 0,
			.pLabelName = name,
			.color = { color[0], color[1], color[2], color[3] },
		};
		dfunc->vkCmdBeginDebugUtilsLabelEXT (cmd, &label);
	}
}

void
QFV_CmdEndLabel (qfv_device_t *device, VkCommandBuffer cmd)
{
	qfv_devfuncs_t *dfunc = device->funcs;
	if (dfunc->vkCmdEndDebugUtilsLabelEXT) {
		dfunc->vkCmdEndDebugUtilsLabelEXT (cmd);
	}
}

void
QFV_CmdInsertLabel (qfv_device_t *device, VkCommandBuffer cmd,
					const char *name, vec4f_t color)
{
	qfv_devfuncs_t *dfunc = device->funcs;
	if (dfunc->vkCmdBeginDebugUtilsLabelEXT) {
		VkDebugUtilsLabelEXT label = {
			VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT, 0,
			.pLabelName = name,
			.color = { color[0], color[1], color[2], color[3] },
		};
		dfunc->vkCmdInsertDebugUtilsLabelEXT (cmd, &label);
	}
}
