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
	pltype_t    type;
	size_t      stride;
	plparser_t  parser;
	size_t      value_offset;
} parse_single_t;

typedef struct parse_array_s {
	pltype_t    type;
	size_t      stride;
	plparser_t  parser;
	size_t      value_offset;
	size_t      size_offset;
} parse_array_t;

static int find_enum (const char *valstr, enumval_t *enumval, int *val)
{
	while (enumval->name && strcmp (enumval->name, valstr) != 0) {
		enumval++;
	}
	if (enumval->name) {
		*val = enumval->value;
		return 1;
	}
	return 0;
}

static int parse_uint32_t (const plfield_t *field, const plitem_t *item,
						   void *data, plitem_t *messages)
{
	int         ret = 1;
	const char *valstr = PL_String (item);
	//Sys_Printf ("parse_uint32_t: %s %zd %d %p %p: %s\n",
	//			field->name, field->offset, field->type, field->parser,
	//			field->data, valstr);
	if (strcmp (valstr, "VK_SUBPASS_EXTERNAL") == 0) {
		*(uint32_t *) data = VK_SUBPASS_EXTERNAL;
	} else {
		char       *end;
		unsigned long val = strtoul (valstr, &end, 0);
		if (*valstr && !*end && val <= 0xffffffff) {
			*(uint32_t *) data = val;
		} else if (val > 0xffffffff) {
			PL_Message (messages, item, "%lu bigger than 32 bits", val);
			ret = 0;
		} else {
			PL_Message (messages, item, "invalid char at %d in '%s'\n",
						(int) (end - valstr), valstr);
			ret = 0;
		}
	}

	return ret;
}

static int parse_enum (const plfield_t *field, const plitem_t *item,
					   void *data, plitem_t *messages)
{
	int         ret = 1;
	int         val;
	const char *valstr = PL_String (item);
	__auto_type enumval = (enumval_t *) field->data;
	//Sys_Printf ("parse_enum: %s %zd %d %p %p %s\n",
	//			field->name, field->offset, field->type, field->parser,
	//			field->data, valstr);
	if (find_enum (valstr, enumval, &val)) {
		*(int *) data = val;
	} else {
		PL_Message (messages, item, "invalid enum: %s", valstr);
		ret = 0;
	}
	return ret;
}

static int parse_flags (const plfield_t *field, const plitem_t *item,
						void *data, plitem_t *messages)
{
	int         ret = 1;
	int         val;
	const char *valstr = PL_String (item);
	__auto_type enumval = (enumval_t *) field->data;
	//Sys_Printf ("parse_flags: %s %zd %d %p %p %s\n",
	//			field->name, field->offset, field->type, field->parser,
	//			field->data, valstr);
	if (find_enum (valstr, enumval, &val)) {
		*(int *) data = val;
	} else if (strcmp (valstr, "0") == 0) {
		*(int *) data = 0;
	} else {
		PL_Message (messages, item, "invalid enum: %s", valstr);
		ret = 0;
	}
	return ret;
}

static int parse_single (const plfield_t *field, const plitem_t *item,
						 void *data, plitem_t *messages)
{
	__auto_type single = (parse_single_t *) field->data;
	void       *flddata = (byte *)data + single->value_offset;

	//Sys_Printf ("parse_single: %s %zd %d %p %p\n", field->name, field->offset,
	//			field->type, field->parser, field->data);

	if (PL_Type (item) != single->type) {
		PL_Message (messages, item, "error: wrong type");
		return 0;
	}

	plfield_t   f = { 0, 0, single->type, single->parser, 0 };
	void       *value = calloc (1, single->stride);
	if (!single->parser (&f, item, value, messages)) {
		free (value);
		return 0;
	}

	*(void **) flddata = value;
	return 1;
}

static int parse_array (const plfield_t *field, const plitem_t *item,
						void *data, plitem_t *messages)
{
	__auto_type array = (parse_array_t *) field->data;
	__auto_type value = (void **) ((byte *)data + array->value_offset);
	__auto_type size = (uint32_t *) ((byte *)data + array->size_offset);

	plelement_t element = {
		array->type,
		array->stride,
		malloc,
		array->parser,
		0,
	};
	plfield_t   f = { 0, 0, 0, 0, &element };

	typedef struct arr_s DARRAY_TYPE(byte) arr_t;
	arr_t      *arr;

	//Sys_Printf ("parse_array: %s %zd %d %p %p %p\n",
	//			field->name, field->offset, field->type, field->parser,
	//			field->data, data);
	//Sys_Printf ("    %d %zd %p %zd %zd\n", array->type, array->stride,
	//			array->parser, array->value_offset, array->size_offset);
	if (!PL_ParseArray (&f, item, &arr, messages)) {
		return 0;
	}
	*value = malloc (array->stride * arr->size);
	memcpy (*value, arr->a, array->stride * arr->size);
	if ((void *) size > data) {
		*size = arr->size;
	}
	free (arr);
	return 1;
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
		PL_ParseArray, &parse_qfv_renderpass_attachments_data },
	{ "subpasses", field_offset (qfv_renderpass_t, subpasses), QFArray,
		PL_ParseArray, &parse_qfv_renderpass_subpasses_data },
	{ "dependencies", field_offset (qfv_renderpass_t, dependencies), QFArray,
		PL_ParseArray, &parse_qfv_renderpass_dependencies_data },
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
