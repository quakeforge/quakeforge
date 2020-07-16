/*
	vkparse.c

	Parser for scripted vulkan structs

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
#include "QF/input.h"
#include "QF/mathlib.h"
#include "QF/qargs.h"
#include "QF/qfplist.h"
#include "QF/quakefs.h"
#include "QF/sys.h"
#include "QF/va.h"
#include "QF/vid.h"
#include "QF/Vulkan/qf_vid.h"
#include "QF/Vulkan/device.h"
#include "QF/Vulkan/command.h"
#include "QF/Vulkan/instance.h"
#include "QF/Vulkan/image.h"
#include "QF/Vulkan/renderpass.h"
#include "QF/Vulkan/swapchain.h"

#include "compat.h"
#include "d_iface.h"
#include "r_internal.h"
#include "vid_vulkan.h"

#include "util.h"
#include "vkparse.h"

typedef struct enumval_s {
	const char *name;
	int         value;
} enumval_t;

typedef struct parse_single_s {
	plparser_t  parser;
	size_t      value_offset;
} parse_single_t;

typedef struct parse_array_s {
	plparser_t  parser;
	size_t      size_offset;
	size_t      value_offset;
} parse_array_t;

static int parse_uint32_t (const plfield_t *field, const plitem_t *item,
						   void *data, plitem_t *messages)
{
	return 0;
}

static int parse_enum (const plfield_t *field, const plitem_t *item,
					   void *data, plitem_t *messages)
{
	return 0;
}

static int parse_flags (const plfield_t *field, const plitem_t *item,
						void *data, plitem_t *messages)
{
	return 0;
}

static int parse_single (const plfield_t *field, const plitem_t *item,
						 void *data, plitem_t *messages)
{
	return 0;
}

static int parse_array (const plfield_t *field, const plitem_t *item,
						void *data, plitem_t *messages)
{
	return 0;
}

#include "libs/video/renderer/vulkan/vkparse.inc"

typedef struct qfv_renderpass_s {
	qfv_attachmentdescription_t *attachments;
	qfv_subpassparametersset_t *subpasses;
	qfv_subpassdependency_t *dependencies;
} qfv_renderpass_t;

static plelement_t parse_qfv_renderpass_attachments_data = {
	QFDictionary,
	sizeof (VkAttachmentDescription),
	malloc,
	parse_VkAttachmentDescription,
	0,
};

static plelement_t parse_qfv_renderpass_subpasses_data = {
	QFDictionary,
	sizeof (VkSubpassDescription),
	malloc,
	parse_VkSubpassDescription,
	0,
};

static plelement_t parse_qfv_renderpass_dependencies_data = {
	QFDictionary,
	sizeof (VkSubpassDependency),
	malloc,
	parse_VkSubpassDependency,
	0,
};

static plfield_t renderpass_fields[] = {
	{ "attachments", field_offset (qfv_renderpass_t, attachments), QFArray,
		parse_array, &parse_qfv_renderpass_attachments_data },
	{ "subpasses", field_offset (qfv_renderpass_t, subpasses), QFArray,
		parse_array, &parse_qfv_renderpass_subpasses_data },
	{ "dependencies", field_offset (qfv_renderpass_t, dependencies), QFArray,
		parse_array, &parse_qfv_renderpass_dependencies_data },
	{}
};

VkRenderPass
QFV_ParseRenderPass (qfv_device_t *device, plitem_t *plist)
{
	qfv_renderpass_t renderpass_data = {};
	plitem_t   *messages = PL_NewArray ();
	VkRenderPass renderpass;

	if (!PL_ParseDictionary (renderpass_fields, plist,
							 &renderpass_data, messages)) {
		for (int i = 0; i < PL_A_NumObjects (messages); i++) {
			Sys_Printf ("%s\n", PL_String (PL_ObjectAtIndex (messages, i)));
		}
		return 0;
	}
	PL_Free (messages);
	renderpass = QFV_CreateRenderPass (device,
									   renderpass_data.attachments,
									   renderpass_data.subpasses,
									   renderpass_data.dependencies);

	free (renderpass_data.attachments);
	for (size_t i = 0; i < renderpass_data.subpasses->size; i++) {
		free ((void *) renderpass_data.subpasses->a[i].pInputAttachments);
		free ((void *) renderpass_data.subpasses->a[i].pColorAttachments);
		free ((void *) renderpass_data.subpasses->a[i].pResolveAttachments);
		free ((void *) renderpass_data.subpasses->a[i].pDepthStencilAttachment);
		free ((void *) renderpass_data.subpasses->a[i].pPreserveAttachments);
	}
	free (renderpass_data.subpasses);
	free (renderpass_data.dependencies);
	return renderpass;
}
