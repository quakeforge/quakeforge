/*
	renderpass.c

	Vulkan render pass and frame buffer functions

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

VkRenderPass
QFV_CreateRenderPass (qfv_device_t *device,
					  qfv_attachmentdescription_t *attachments,
					  qfv_subpassparametersset_t *subpassparams,
					  qfv_subpassdependency_t *dependencies)
{
	VkDevice    dev = device->dev;
	qfv_devfuncs_t *dfunc = device->funcs;

	if (developer->int_val & SYS_vulkan) {
		Sys_Printf ("attachments: %zd\n", attachments->size);
		for (size_t i = 0; i < attachments->size; i++) {
			Sys_Printf ("  attachment: %zd\n", i);
			Sys_Printf ("    flags: %x\n", attachments->a[i].flags);
			Sys_Printf ("    format: %d\n", attachments->a[i].format);
			Sys_Printf ("    samples: %x\n", attachments->a[i].samples);
			Sys_Printf ("    loadOp: %d\n", attachments->a[i].loadOp);
			Sys_Printf ("    storeOp: %d\n", attachments->a[i].storeOp);
			Sys_Printf ("    stencilLoadOp: %d\n",
						attachments->a[i].stencilLoadOp);
			Sys_Printf ("    stencilStoreOp: %d\n",
						attachments->a[i].stencilStoreOp);
			Sys_Printf ("    initialLayout: %d\n",
						attachments->a[i].initialLayout);
			Sys_Printf ("    finalLayout: %d\n",
						attachments->a[i].finalLayout);
		}
		Sys_Printf ("subpassparams: %zd\n", subpassparams->size);
		for (size_t i = 0; i < subpassparams->size; i++) {
			VkSubpassDescription *sp = &subpassparams->a[i];
			Sys_Printf ("    flags: %x\n", sp->flags);
			Sys_Printf ("    piplineBindPoint: %d\n", sp->pipelineBindPoint);
			Sys_Printf ("    inputAttachmentCount: %d\n",
						sp->inputAttachmentCount);
			for (size_t j = 0; j < sp->inputAttachmentCount; j++) {
				const VkAttachmentReference *ref = &sp->pInputAttachments[j];
				Sys_Printf ("      c %d %d\n", ref->attachment, ref->layout);
			}
			Sys_Printf ("    colorAttachmentCount: %d\n",
						sp->colorAttachmentCount);
			for (size_t j = 0; j < sp->colorAttachmentCount; j++) {
				const VkAttachmentReference *ref = &sp->pColorAttachments[j];
				Sys_Printf ("      c %d %d\n", ref->attachment, ref->layout);
			}
			if (sp->pResolveAttachments) {
				for (size_t j = 0; j < sp->colorAttachmentCount; j++) {
					const VkAttachmentReference *ref
						= &sp->pResolveAttachments[j];
					Sys_Printf ("      r %d %d\n", ref->attachment,
								ref->layout);
				}
			}
			Sys_Printf ("    pDepthStencilAttachment: %p\n",
						sp->pDepthStencilAttachment);
			if (sp->pDepthStencilAttachment) {
				const VkAttachmentReference *ref = sp->pDepthStencilAttachment;
				Sys_Printf ("        %d %d\n", ref->attachment, ref->layout);
			}
			Sys_Printf ("    preserveAttachmentCount: %d\n",
						sp->preserveAttachmentCount);
			for (size_t j = 0; j < sp->preserveAttachmentCount; j++) {
				Sys_Printf ("        %d\n", sp->pPreserveAttachments[j]);
			}
		}
		Sys_Printf ("dependencies: %zd\n", dependencies->size);
		for (size_t i = 0; i < dependencies->size; i++) {
			Sys_Printf ("    srcSubpass: %d\n", dependencies->a[i].srcSubpass);
			Sys_Printf ("    dstSubpass: %d\n", dependencies->a[i].dstSubpass);
			Sys_Printf ("    srcStageMask: %x\n", dependencies->a[i].srcStageMask);
			Sys_Printf ("    dstStageMask: %x\n", dependencies->a[i].dstStageMask);
			Sys_Printf ("    srcAccessMask: %x\n", dependencies->a[i].srcAccessMask);
			Sys_Printf ("    dstAccessMask: %x\n", dependencies->a[i].dstAccessMask);
			Sys_Printf ("    dependencyFlags: %x\n", dependencies->a[i].dependencyFlags);
		}
	}
	VkRenderPassCreateInfo createInfo = {
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, 0, 0,
		attachments->size, attachments->a,
		subpassparams->size, subpassparams->a,
		dependencies->size, dependencies->a,
	};

	VkRenderPass renderpass;
	dfunc->vkCreateRenderPass (dev, &createInfo, 0, &renderpass);
	return renderpass;
}

VkFramebuffer
QFV_CreateFramebuffer (qfv_device_t *device, VkRenderPass renderPass,
					   qfv_imageviewset_t *attachments,
					   VkExtent2D extent, uint32_t layers)
{
	VkDevice    dev = device->dev;
	qfv_devfuncs_t *dfunc = device->funcs;

	VkFramebufferCreateInfo createInfo = {
		VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, 0, 0,
		renderPass, attachments->size, attachments->a,
		extent.width, extent.height, layers,
	};

	VkFramebuffer framebuffer;
	dfunc->vkCreateFramebuffer (dev, &createInfo, 0, &framebuffer);
	return framebuffer;
}
