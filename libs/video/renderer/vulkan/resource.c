/*
	resource.c

	Vulkan resource functions

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

#include "QF/mathlib.h"
#include "QF/va.h"

#include "QF/Vulkan/buffer.h"
#include "QF/Vulkan/debug.h"
#include "QF/Vulkan/device.h"
#include "QF/Vulkan/image.h"
#include "QF/Vulkan/instance.h"
#include "QF/Vulkan/resource.h"

static void
create_image (qfv_device_t *device, qfv_resobj_t *image_obj)
{
	qfv_devfuncs_t *dfunc = device->funcs;
	__auto_type image = &image_obj->image;
	if (image->image) {
		// the image was created externally and is being
		return;
	}
	VkImageCreateInfo createInfo = {
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, 0,
		.flags = image->flags,
		.imageType = image->type,
		.format = image->format,
		.extent = image->extent,
		.mipLevels = image->num_mipmaps,
		.arrayLayers = image->num_layers,
		.samples = image->samples,
		.tiling = image->tiling,
		.usage = image->usage,
		.sharingMode = image->sharing,
		.queueFamilyIndexCount = image->num_queue_inds,
		.pQueueFamilyIndices = image->queue_inds,
		.initialLayout = image->initialLayout,
	};
	dfunc->vkCreateImage (device->dev, &createInfo, 0, &image->image);
}

static void
create_image_view (qfv_device_t *device, qfv_resobj_t *imgview_obj,
				   qfv_resobj_t *imgobj)
{
	qfv_devfuncs_t *dfunc = device->funcs;
	__auto_type view = &imgview_obj->image_view;
	VkImage     image = view->external_image ? view->external_image
											 : imgobj->image.image;

	VkImageViewCreateInfo createInfo = {
		VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, 0,
		.flags = view->flags,
		.image = image,
		.viewType = view->type,
		.format = view->format,
		.components = view->components,
		.subresourceRange = view->subresourceRange,
	};
	dfunc->vkCreateImageView (device->dev, &createInfo, 0, &view->view);
}

int
QFV_CreateResource (qfv_device_t *device, qfv_resource_t *resource)
{
	qfv_devfuncs_t *dfunc = device->funcs;
	qfv_physdev_t *physdev = device->physDev;
	size_t      atom = physdev->properties->limits.nonCoherentAtomSize;
	VkPhysicalDeviceMemoryProperties *memprops = &physdev->memory_properties;
	VkMemoryRequirements req;
	VkDeviceSize size = 0;

	if (!(resource->memory_properties
		  & (VK_MEMORY_PROPERTY_HOST_CACHED_BIT
			 | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
			 | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT))) {
		// if the memory isn't host visible then there's no need to worry about
		// alignment with nonCoherentAtomSize
		atom = 0;
	}
	for (unsigned i = 0; i < resource->num_objects; i++) {
		__auto_type obj = &resource->objects[i];
		switch (obj->type) {
			case qfv_res_buffer:
				{
					__auto_type buffer = &obj->buffer;
					buffer->buffer = QFV_CreateBuffer (device,
													   buffer->size,
													   buffer->usage);
					QFV_duSetObjectName (device, VK_OBJECT_TYPE_BUFFER,
										 buffer->buffer,
										 va (resource->va_ctx, "buffer:%s:%s",
											 resource->name, obj->name));
					dfunc->vkGetBufferMemoryRequirements (device->dev,
														  buffer->buffer, &req);
				}
				break;
			case qfv_res_buffer_view:
				{
					__auto_type buffview = &obj->buffer_view;
					__auto_type buffobj = &resource->objects[buffview->buffer];
					if (buffview->buffer >= resource->num_objects
						|| buffobj->type != qfv_res_buffer) {
						Sys_Error ("%s:%s invalid buffer for view",
								   resource->name, obj->name);
					}
				}
				break;
			case qfv_res_image:
				{
					create_image (device, obj);
					__auto_type image = &obj->image;
					QFV_duSetObjectName (device, VK_OBJECT_TYPE_IMAGE,
										 image->image,
										 va (resource->va_ctx, "image:%s:%s",
											 resource->name, obj->name));
					dfunc->vkGetImageMemoryRequirements (device->dev,
														 image->image, &req);
				}
				break;
			case qfv_res_image_view:
				{
					__auto_type imgview = &obj->image_view;
					__auto_type imgobj = &resource->objects[imgview->image];
					if (!imgview->external_image
						&& (imgview->image >= resource->num_objects
							|| imgobj->type != qfv_res_image)) {
						Sys_Error ("%s:%s invalid image for view",
								   resource->name, obj->name);
					}
				}
				break;
			default:
				Sys_Error ("%s:%s invalid resource type %d",
						   resource->name, obj->name, obj->type);
		}
		req.alignment = max (req.alignment, atom);
		size = QFV_NextOffset (size, &req);
		size += QFV_NextOffset (req.size, &req);
	}
	VkMemoryPropertyFlags properties = resource->memory_properties;
	for (uint32_t type = 0; type < memprops->memoryTypeCount; type++) {
		if ((req.memoryTypeBits & (1 << type))
			&& ((memprops->memoryTypes[type].propertyFlags & properties)
				                == properties)) {
			Sys_MaskPrintf (SYS_vulkan, "QFV_CreateResource: %s:mti: %d\n",
							resource->name, type);
			VkMemoryAllocateInfo allocate_info = {
				.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
				.allocationSize = size,
				.memoryTypeIndex = type,
			};
			VkResult res = dfunc->vkAllocateMemory (device->dev, &allocate_info,
													0, &resource->memory);
			if (res == VK_SUCCESS) {
				break;
			}
		}
	}
	resource->size = size;
	QFV_duSetObjectName (device, VK_OBJECT_TYPE_DEVICE_MEMORY,
						 resource->memory, va (resource->va_ctx, "memory:%s",
											   resource->name));

	VkDeviceSize offset = 0;
	for (unsigned i = 0; i < resource->num_objects; i++) {
		__auto_type obj = &resource->objects[i];
		switch (obj->type) {
			case qfv_res_buffer:
				{
					__auto_type buffer = &obj->buffer;
					dfunc->vkGetBufferMemoryRequirements (device->dev,
														  buffer->buffer, &req);
				}
				break;
			case qfv_res_image:
				{
					__auto_type image = &obj->image;
					dfunc->vkGetImageMemoryRequirements (device->dev,
														 image->image, &req);
				}
				break;
			case qfv_res_buffer_view:
			case qfv_res_image_view:
				break;
		}

		req.alignment = max (req.alignment, atom);
		offset = QFV_NextOffset (offset, &req);
		switch (obj->type) {
			case qfv_res_buffer:
				{
					__auto_type buffer = &obj->buffer;
					QFV_BindBufferMemory (device, buffer->buffer,
										  resource->memory, offset);
					buffer->offset = offset;
				}
				break;
			case qfv_res_image:
				{
					__auto_type image = &obj->image;
					QFV_BindImageMemory (device, image->image,
										 resource->memory, offset);
					image->offset = offset;
				}
				break;
			case qfv_res_buffer_view:
			case qfv_res_image_view:
				break;
		}
		req.alignment = max (req.alignment, atom);
		offset = QFV_NextOffset (offset, &req);
		offset += QFV_NextOffset (req.size, &req);
	}

	for (unsigned i = 0; i < resource->num_objects; i++) {
		__auto_type obj = &resource->objects[i];
		switch (obj->type) {
			case qfv_res_buffer:
			case qfv_res_image:
				break;
			case qfv_res_buffer_view:
				{
					__auto_type buffview = &obj->buffer_view;
					__auto_type buffobj = &resource->objects[buffview->buffer];
					__auto_type buffer = &buffobj->buffer;
					buffview->view = QFV_CreateBufferView (device,
														   buffer->buffer,
														   buffview->format,
														   buffview->offset,
														   buffview->size);
					QFV_duSetObjectName (device, VK_OBJECT_TYPE_BUFFER_VIEW,
										 buffview->view,
										 va (resource->va_ctx, "bview:%s:%s",
											 resource->name, obj->name));
				}
				break;
			case qfv_res_image_view:
				{
					__auto_type imgview = &obj->image_view;
					__auto_type imgobj = &resource->objects[imgview->image];
					create_image_view (device, obj, imgobj);
					QFV_duSetObjectName (device, VK_OBJECT_TYPE_IMAGE_VIEW,
										 imgview->view,
										 va (resource->va_ctx, "iview:%s:%s",
											 resource->name, obj->name));
				}
				break;
		}
	}
	return 0;
}

void
QFV_DestroyResource (qfv_device_t *device, qfv_resource_t *resource)
{
	qfv_devfuncs_t *dfunc = device->funcs;

	for (unsigned i = 0; i < resource->num_objects; i++) {
		__auto_type obj = &resource->objects[i];
		switch (obj->type) {
			case qfv_res_buffer:
			case qfv_res_image:
				break;
			case qfv_res_buffer_view:
				{
					__auto_type buffview = &obj->buffer_view;
					dfunc->vkDestroyBufferView (device->dev, buffview->view, 0);
					buffview->view = 0;
				}
				break;
			case qfv_res_image_view:
				{
					__auto_type imgview = &obj->image_view;
					dfunc->vkDestroyImageView (device->dev, imgview->view, 0);
					imgview->view = 0;
				}
				break;
		}
	}
	for (unsigned i = 0; i < resource->num_objects; i++) {
		__auto_type obj = &resource->objects[i];
		switch (obj->type) {
			case qfv_res_buffer:
				{
					__auto_type buffer = &obj->buffer;
					dfunc->vkDestroyBuffer (device->dev, buffer->buffer, 0);
					buffer->buffer = 0;
				}
				break;
			case qfv_res_image:
				{
					__auto_type image = &obj->image;
					dfunc->vkDestroyImage (device->dev, image->image, 0);
					image->image = 0;
				}
				break;
			case qfv_res_buffer_view:
			case qfv_res_image_view:
				break;
		}
	}
	dfunc->vkFreeMemory (device->dev, resource->memory, 0);
	resource->memory = 0;
}

void
QFV_ResourceInitTexImage (qfv_resobj_t *image, const char *name,
						  int mips, const tex_t *tex)
{
	*image = (qfv_resobj_t) {
		.name = name,
		.type = qfv_res_image,
		.image = {
			.type = VK_IMAGE_TYPE_2D,
			.format = QFV_ImageFormat (tex->format, 0),
			.extent = {
				.width = tex->width,
				.height = tex->height,
				.depth = 1,
			},
			.num_mipmaps = mips ? QFV_MipLevels (tex->width, tex->height) : 1,
			.num_layers = 1,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT
					| VK_IMAGE_USAGE_TRANSFER_SRC_BIT
					| VK_IMAGE_USAGE_SAMPLED_BIT,
		},
	};
}
