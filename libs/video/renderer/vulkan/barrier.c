/*
	barrier.c

	Memory barrier helpers

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

#include "QF/Vulkan/qf_vid.h"
#include "QF/Vulkan/barrier.h"

const VkImageMemoryBarrier imageLayoutTransitionBarriers[] = {
	// undefined -> transfer dst optimal
	{	VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, 0,
		0,
		VK_ACCESS_TRANSFER_WRITE_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, 0,
		{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
	},
	// transfer dst optimal -> shader read only optimal
	{	VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, 0,
		VK_ACCESS_TRANSFER_WRITE_BIT,
		VK_ACCESS_SHADER_READ_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, 0,
		{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
	},
	// shader read only optimal -> transfer dst optimal
	{	VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, 0,
		VK_ACCESS_SHADER_READ_BIT,
		VK_ACCESS_TRANSFER_WRITE_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, 0,
		{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
	},
	// undefined -> depth stencil attachment optimal
	{	VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, 0,
		0,
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT
			| VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, 0,
		{ VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 }
	},
	// undefined -> color attachment optimal
	{	VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, 0,
		0,
		VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
			| VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, 0,
		{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
	},
	{ /* end of transition barriers */ }
};

const qfv_pipelinestagepair_t imageLayoutTransitionStages[] = {
	// undefined -> transfer dst optimal
	{	VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT },
	// transfer dst optimal -> shader read only optimal
	{	VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT },
	// shader read only optimal -> transfer dst optimal
	{	VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT },
	// undefined -> depth stencil attachment optimal
	{	VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT },
	// undefined -> color attachment optimal
	{	VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT },
};
