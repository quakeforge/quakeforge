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

#include "QF/va.h"

#include "QF/Vulkan/buffer.h"
#include "QF/Vulkan/debug.h"
#include "QF/Vulkan/device.h"
#include "QF/Vulkan/image.h"
#include "QF/Vulkan/instance.h"
#include "QF/Vulkan/resource.h"

int
QFV_CreateResource (qfv_device_t *device, qfv_resource_t *resource)
{
	qfv_devfuncs_t *dfunc = device->funcs;
	qfv_physdev_t *physdev = device->physDev;
	VkPhysicalDeviceMemoryProperties *memprops = &physdev->memory_properties;
	VkMemoryRequirements req;
	VkDeviceSize size = 0;

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
					__auto_type image = &obj->image;
					image->image = QFV_CreateImage (device,
													image->cubemap,
													image->type,
													image->format,
													image->extent,
													image->num_mipmaps,
													image->num_layers,
													image->samples,
													image->usage);
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
					if (imgview->image >= resource->num_objects
						|| imgobj->type != qfv_res_image) {
						Sys_Error ("%s:%s invalid image for view",
								   resource->name, obj->name);
					}
				}
				break;
			default:
				Sys_Error ("%s:%s invalid resource type %d",
						   resource->name, obj->name, obj->type);
		}
		size = QFV_NextOffset (size, &req);
		size += req.size;
	}
	VkMemoryPropertyFlags properties = resource->memory_properties;
	for (uint32_t type = 0; type < memprops->memoryTypeCount; type++) {
		if ((req.memoryTypeBits & (1 << type))
			&& ((memprops->memoryTypes[type].propertyFlags & properties)
				                == properties)) {
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

		offset = QFV_NextOffset (offset, &req);
		switch (obj->type) {
			case qfv_res_buffer:
				{
					__auto_type buffer = &obj->buffer;
					QFV_BindBufferMemory (device, buffer->buffer,
										  resource->memory, offset);
				}
				break;
			case qfv_res_image:
				{
					__auto_type image = &obj->image;
					QFV_BindImageMemory (device, image->image,
										 resource->memory, offset);
				}
				break;
			case qfv_res_buffer_view:
			case qfv_res_image_view:
				break;
		}
		offset += req.size;
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
					__auto_type image = &imgobj->image;
					imgview->view = QFV_CreateImageView (device,
														 image->image,
														 imgview->type,
														 imgview->format,
														 imgview->aspect);
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
				}
				break;
			case qfv_res_image_view:
				{
					__auto_type imgview = &obj->image_view;
					dfunc->vkDestroyImageView (device->dev, imgview->view, 0);
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
				}
				break;
			case qfv_res_image:
				{
					__auto_type image = &obj->image;
					dfunc->vkDestroyImage (device->dev, image->image, 0);
				}
				break;
			case qfv_res_buffer_view:
			case qfv_res_image_view:
				break;
		}
	}
	dfunc->vkFreeMemory (device->dev, resource->memory, 0);
}
