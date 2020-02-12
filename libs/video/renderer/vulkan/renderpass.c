/*
	descriptor.c

	Vulkan descriptor functions

	Copyright (C) 1996-1997 Id Software, Inc.
	Copyright (C) 2020      Bill Currie <bill@taniwha.org>

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
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/cvar.h"
#include "QF/dstring.h"
#include "QF/hash.h"
#include "QF/input.h"
#include "QF/mathlib.h"
#include "QF/qargs.h"
#include "QF/quakefs.h"
#include "QF/sys.h"
#include "QF/va.h"
#include "QF/vid.h"
#include "QF/Vulkan/qf_vid.h"
#include "QF/Vulkan/device.h"
#include "QF/Vulkan/image.h"
#include "QF/Vulkan/instance.h"
#include "QF/Vulkan/renderpass.h"

#include "compat.h"
#include "d_iface.h"
#include "r_internal.h"
#include "vid_vulkan.h"

#include "util.h"

qfv_renderpass_t *
QFV_CreateRenderPass (qfv_device_t *device,
					  qfv_attachmentdescription_t *attachments,
					  qfv_subpassparametersset_t *subpassparams,
					  qfv_subpassdependency_t *dependencies)
{
	VkDevice    dev = device->dev;
	qfv_devfuncs_t *dfunc = device->funcs;

	VkSubpassDescription *subpasses = alloca (subpassparams->numSubpasses
											  * sizeof (*subpasses));

	for (uint32_t i =  0; i < subpassparams->numSubpasses; i++) {
		qfv_subpassparameters_t *params = &subpassparams->subpasses[i];
		subpasses[i].flags = 0;
		subpasses[i].pipelineBindPoint = params->pipelineBindPoint;
		subpasses[i].inputAttachmentCount = params->inputAttachments->numReferences;
		subpasses[i].pInputAttachments = params->inputAttachments->references;
		subpasses[i].colorAttachmentCount = params->colorAttachments->numReferences;
		subpasses[i].pColorAttachments = params->colorAttachments->references;
		if (params->resolveAttachments) {
			subpasses[i].pResolveAttachments = params->resolveAttachments->references;
		}
		subpasses[i].pDepthStencilAttachment = params->depthStencilAttachment;
		subpasses[i].preserveAttachmentCount = params->numPreserve;
		subpasses[i].pPreserveAttachments = params->preserveAttachments;
	}

	VkRenderPassCreateInfo createInfo = {
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, 0, 0,
		attachments->numAttachments, attachments->attachments,
		subpassparams->numSubpasses, subpasses,
		dependencies->numDependencies, dependencies->dependencies,
	};

	qfv_renderpass_t *renderpass = malloc (sizeof (*renderpass));
	renderpass->device = device;
	dfunc->vkCreateRenderPass (dev, &createInfo, 0, &renderpass->renderPass);
	return renderpass;
}

qfv_framebuffer_t *
QFV_CreateFramebuffer (qfv_renderpass_t *renderPass,
					   uint32_t numAttachments, qfv_imageview_t **attachments,
					   uint32_t width, uint32_t height, uint32_t layers)
{
	qfv_device_t *device = renderPass->device;
	VkDevice    dev = device->dev;
	qfv_devfuncs_t *dfunc = device->funcs;

	VkImageView *views = alloca (numAttachments * sizeof (*views));
	for (uint32_t i = 0; i < numAttachments; i++) {
		views[i] = attachments[i]->view;
	}

	VkFramebufferCreateInfo createInfo = {
		VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, 0, 0,
		renderPass->renderPass, numAttachments, views, width, height, layers,
	};

	qfv_framebuffer_t *framebuffer = malloc (sizeof (*framebuffer));
	framebuffer->device = device;
	dfunc->vkCreateFramebuffer (dev, &createInfo, 0, &framebuffer->framebuffer);
	return framebuffer;
}

void
QFV_DestroyFramebuffer (qfv_framebuffer_t *framebuffer)
{
	qfv_device_t *device = framebuffer->device;
	VkDevice    dev = device->dev;
	qfv_devfuncs_t *dfunc = device->funcs;

	dfunc->vkDestroyFramebuffer (dev, framebuffer->framebuffer, 0);
	free (framebuffer);
}

void
QFV_DestroyRenderPass (qfv_renderpass_t *renderPass)
{
	qfv_device_t *device = renderPass->device;
	VkDevice    dev = device->dev;
	qfv_devfuncs_t *dfunc = device->funcs;

	dfunc->vkDestroyRenderPass (dev, renderPass->renderPass, 0);
	free (renderPass);
}
