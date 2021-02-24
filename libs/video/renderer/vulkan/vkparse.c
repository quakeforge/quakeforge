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

#include "QF/cexpr.h"
#include "QF/cmem.h"
#include "QF/cvar.h"
#include "QF/dstring.h"
#include "QF/hash.h"
#include "QF/input.h"
#include "QF/mathlib.h"
#include "QF/qargs.h"
#include "QF/qfplist.h"
#include "QF/quakefs.h"
#include "QF/sys.h"
#include "QF/va.h"
#include "QF/vid.h"
#include "QF/simd/vec4f.h"
#include "QF/Vulkan/qf_vid.h"
#include "QF/Vulkan/descriptor.h"
#include "QF/Vulkan/debug.h"
#include "QF/Vulkan/device.h"
#include "QF/Vulkan/command.h"
#include "QF/Vulkan/instance.h"
#include "QF/Vulkan/image.h"
#include "QF/Vulkan/pipeline.h"
#include "QF/Vulkan/renderpass.h"
#include "QF/Vulkan/shader.h"
#include "QF/Vulkan/swapchain.h"

#include "compat.h"
#include "d_iface.h"
#include "r_internal.h"
#include "vid_vulkan.h"

#include "util.h"

#define vkparse_internal
#include "vkparse.h"
#undef vkparse_internal

static void flag_or (const exprval_t *val1, const exprval_t *val2,
					 exprval_t *result, exprctx_t *ctx)
{
	*(int *) (result->value) = *(int *) (val1->value) | *(int *) (val2->value);
}

static void flag_and (const exprval_t *val1, const exprval_t *val2,
					 exprval_t *result, exprctx_t *ctx)
{
	*(int *) (result->value) = *(int *) (val1->value) & *(int *) (val2->value);
}

static void flag_cast_int (const exprval_t *val1, const exprval_t *val2,
						   exprval_t *result, exprctx_t *ctx)
{
	// FIXME should check value is valid
	*(int *) (result->value) = *(int *) (val2->value);
}

static void flag_not (const exprval_t *val, exprval_t *result, exprctx_t *ctx)
{
	*(int *) (result->value) = ~(*(int *) (val->value));
}

binop_t flag_binops[] = {
	{ '|', 0, 0, flag_or },
	{ '&', 0, 0, flag_and },
	{ '=', &cexpr_int, 0, flag_cast_int },
	{ '=', &cexpr_plitem, 0, cexpr_cast_plitem },
	{}
};

binop_t enum_binops[] = {
	{ '=', &cexpr_plitem, 0, cexpr_cast_plitem },
	{}
};

unop_t flag_unops[] = {
	{ '~', 0, flag_not },
	{}
};

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

typedef struct parse_data_s {
	size_t      value_offset;
	size_t      size_offset;
} parse_data_t;

typedef struct parse_string_s {
	size_t      value_offset;
} parse_string_t;

typedef struct parse_custom_s {
	int       (*parse) (const plitem_t *item, void **data,
						plitem_t *messages, parsectx_t *context);
	size_t     *offsets;
	size_t      num_offsets;
} parse_custom_t;

static int
parse_basic (const plfield_t *field, const plitem_t *item,
			 void *data, plitem_t *messages, void *context)
{
	int         ret = 1;
	__auto_type etype = (exprtype_t *) field->data;
	exprctx_t   ectx = *((parsectx_t *) context)->ectx;
	exprval_t   result = { etype, data };
	ectx.symtab = 0;
	ectx.result = &result;
	const char *valstr = PL_String (item);
	//Sys_Printf ("parse_basic: %s %zd %d %p %p: %s\n",
	//			field->name, field->offset, field->type, field->parser,
	//			field->data, valstr);
	if (strcmp (valstr, "VK_SUBPASS_EXTERNAL") == 0) {
		//FIXME handle subpass in a separate parser?
		*(uint32_t *) data = VK_SUBPASS_EXTERNAL;
	} else {
		ret = !cexpr_eval_string (valstr, &ectx);
		if (!ret) {
			PL_Message (messages, item, "error parsing %s: %s",
						field->name, valstr);
		}
	}
	//Sys_Printf ("    %x\n", *(uint32_t *)data);

	return ret;
}

static int
parse_uint32_t (const plfield_t *field, const plitem_t *item,
				void *data, plitem_t *messages, void *context)
{
	int         ret = 1;
	// use size_t (and cexpr_size_t) for val so references to array sizes
	// can be used
	size_t      val = 0;
	exprval_t   result = { &cexpr_size_t, &val };
	exprctx_t   ectx = *((parsectx_t *) context)->ectx;
	ectx.symtab = 0;
	ectx.result = &result;
	const char *valstr = PL_String (item);
	//Sys_Printf ("parse_uint32_t: %s %zd %d %p %p: %s\n",
	//			field->name, field->offset, field->type, field->parser,
	//			field->data, valstr);
	if (strcmp (valstr, "VK_SUBPASS_EXTERNAL") == 0) {
		//FIXME handle subpass in a separate parser?
		*(uint32_t *) data = VK_SUBPASS_EXTERNAL;
	} else {
		//Sys_Printf ("parse_uint32_t: %s %zd %d %p %p %s\n",
		//			field->name, field->offset, field->type, field->parser,
		//			field->data, valstr);
		ret = !cexpr_eval_string (valstr, &ectx);
		if (!ret) {
			PL_Message (messages, item, "error parsing %s: %s",
						field->name, valstr);
		}
		*(uint32_t *) data = val;
		//Sys_Printf ("    %d\n", *(uint32_t *)data);
	}

	return ret;
}

static int
parse_enum (const plfield_t *field, const plitem_t *item,
			void *data, plitem_t *messages, void *context)
{
	int         ret = 1;
	__auto_type enm = (exprenum_t *) field->data;
	exprctx_t   ectx = *((parsectx_t *)context)->ectx;
	exprval_t   result = { enm->type, data };
	ectx.symtab = enm->symtab;
	ectx.result = &result;
	const char *valstr = PL_String (item);
	//Sys_Printf ("parse_enum: %s %zd %d %p %p %s\n",
	//			field->name, field->offset, field->type, field->parser,
	//			field->data, valstr);
	ret = !cexpr_parse_enum (enm, valstr, &ectx, data);
	if (!ret) {
		PL_Message (messages, item, "error parsing enum: %s", valstr);
	}
	//Sys_Printf ("    %d\n", *(int *)data);
	return ret;
}

static const plitem_t *
parse_reference (const plitem_t *item, const char *type, plitem_t *messages,
				 parsectx_t *pctx)
{
	exprctx_t   ectx = *pctx->ectx;
	vulkan_ctx_t *ctx = pctx->vctx;
	plitem_t   *refItem = 0;
	exprval_t   result = { &cexpr_plitem, &refItem };
	ectx.symtab = 0;
	ectx.result = &result;
	const char *name = PL_String (item);
	if (cexpr_eval_string (name, &ectx)) {
		PL_Message (messages, item, va (ctx->va_ctx, "not a %s reference", type));
		return 0;
	}
	return refItem;
}

static int
parse_single (const plfield_t *field, const plitem_t *item,
			  void *data, plitem_t *messages, void *context)
{
	__auto_type single = (parse_single_t *) field->data;
	void       *flddata = (byte *)data + single->value_offset;

	//Sys_Printf ("parse_single: %s %zd %d %p %p\n", field->name, field->offset,
	//			field->type, field->parser, field->data);

	if (!PL_CheckType (single->type, PL_Type (item))) {
		PL_TypeMismatch (messages, item, field->name, single->type,
						 PL_Type (item));
		return 0;
	}

	plfield_t   f = { 0, 0, single->type, single->parser, 0 };
	void       *value = calloc (1, single->stride);
	if (!single->parser (&f, item, value, messages, context)) {
		free (value);
		return 0;
	}

	*(void **) flddata = value;
	return 1;
}

static void *
array_alloc (void *context, size_t size)
{
	return malloc (size);
}

static int
parse_array (const plfield_t *field, const plitem_t *item,
			 void *data, plitem_t *messages, void *context)
{
	__auto_type array = (parse_array_t *) field->data;
	__auto_type value = (void **) ((byte *)data + array->value_offset);
	__auto_type size = (uint32_t *) ((byte *)data + array->size_offset);

	plelement_t element = {
		array->type,
		array->stride,
		array_alloc,
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
	if (!PL_ParseArray (&f, item, &arr, messages, context)) {
		return 0;
	}
	*value = malloc (array->stride * arr->size);
	memcpy (*value, arr->a, array->stride * arr->size);
	if ((void *) size >= data) {
		*size = arr->size;
	}
	free (arr);
	return 1;
}

static int
parse_data (const plfield_t *field, const plitem_t *item,
			void *data, plitem_t *messages, void *context)
{
	__auto_type datad = (parse_data_t *) field->data;
	__auto_type value = (void **) ((byte *)data + datad->value_offset);
	__auto_type size = (size_t *) ((byte *)data + datad->size_offset);

	const void *bindata = PL_BinaryData (item);
	size_t      binsize = PL_BinarySize (item);

	Sys_Printf ("parse_data: %s %zd %d %p %p %p\n",
				field->name, field->offset, field->type, field->parser,
				field->data, data);
	Sys_Printf ("    %zd %zd\n", datad->value_offset, datad->size_offset);
	Sys_Printf ("    %zd %p\n", binsize, bindata);

	*value = malloc (binsize);
	memcpy (*value, bindata, binsize);
	if ((void *) size > data) {
		*size = binsize;
	}
	return 1;
}

static int
parse_string (const plfield_t *field, const plitem_t *item,
			  void *data, plitem_t *messages, void *context)
{
	__auto_type string = (parse_string_t *) field->data;
	__auto_type value = (char **) ((byte *)data + string->value_offset);

	const char *str = PL_String (item);

	//Sys_Printf ("parse_string: %s %zd %d %p %p %p\n",
	//			field->name, field->offset, field->type, field->parser,
	//			field->data, data);
	//Sys_Printf ("    %zd\n", string->value_offset);
	//Sys_Printf ("    %s\n", str);

	*value = strdup (str);
	return 1;
}

static int
parse_custom (const plfield_t *field, const plitem_t *item,
			  void *data, plitem_t *messages, void *context)
{
	__auto_type custom = (parse_custom_t *) field->data;
	void      **offsets = alloca (custom->num_offsets * sizeof (void *));
	for (size_t i = 0; i < custom->num_offsets; i++) {
		offsets[i] = data + custom->offsets[i];
	}
	return custom->parse (item, offsets, messages, context);
}

static int
parse_RGBA (const plitem_t *item, void **data,
			plitem_t *messages, parsectx_t *context)
{
	int         ret = 1;
	exprctx_t   ectx = *context->ectx;
	exprval_t   result = { &cexpr_vector, data[0] };
	ectx.symtab = 0;
	ectx.result = &result;
	const char *valstr = PL_String (item);
	Sys_Printf ("parse_RGBA: %s\n", valstr);
	ret = !cexpr_eval_string (valstr, &ectx);
	Sys_Printf ("    "VEC4F_FMT"\n", VEC4_EXP (*(vec4f_t *)data[0]));
	return ret;
}

uint64_t
QFV_GetHandle (hashtab_t *tab, const char *name)
{
	handleref_t *hr = Hash_Find (tab, name);
	if (hr) {
		return hr->handle;
	}
	return 0;
}

void
QFV_AddHandle (hashtab_t *tab, const char *name, uint64_t handle)
{
	handleref_t *hr = malloc (sizeof (handleref_t));
	hr->name = strdup (name);
	hr->handle = handle;
	Hash_Add (tab, hr);
}

static int
parse_VkRenderPass (const plitem_t *item, void **data,
					plitem_t *messages, parsectx_t *context)
{
	__auto_type handle = (VkRenderPass *) data[0];
	int         ret = 1;
	parsectx_t *pctx = context;
	exprctx_t   ectx = *pctx->ectx;
	vulkan_ctx_t *ctx = pctx->vctx;

	const char *name = PL_String (item);
	Sys_Printf ("parse_VkRenderPass: %s\n", name);
	if (name[0] != '$') {
		name = va (ctx->va_ctx, "$"QFV_PROPERTIES".%s", name);
	}

	*handle = (VkRenderPass) QFV_GetHandle (ctx->renderpasses, name);
	if (*handle) {
		return 1;
	}

	plitem_t   *setItem = 0;
	exprval_t   result = { &cexpr_plitem, &setItem };
	ectx.symtab = 0;
	ectx.result = &result;
	ret = !cexpr_eval_string (name, &ectx);
	if (ret) {
		VkRenderPass setLayout;
		setLayout = QFV_ParseRenderPass (ctx, setItem, pctx->properties);
		*handle = (VkRenderPass) setLayout;

		QFV_AddHandle (ctx->setLayouts, name, (uint64_t) setLayout);
	}
	return ret;
}

static int
parse_VkShaderModule (const plitem_t *item, void **data,
					  plitem_t *messages, parsectx_t *context)
{
	__auto_type handle = (VkShaderModule *) data[0];
	vulkan_ctx_t *ctx = context->vctx;
	qfv_device_t *device = ctx->device;

	const char *name = PL_String (item);
	*handle = (VkShaderModule) QFV_GetHandle (ctx->shaderModules, name);
	if (*handle) {
		return 1;
	}
	if (!(*handle = QFV_CreateShaderModule (device, name))) {
		PL_Message (messages, item, "could not find shader %s", name);
		return 0;
	}
	QFV_AddHandle (ctx->shaderModules, name, (uint64_t) *handle);
	return 1;
}

static int
parse_VkDescriptorSetLayout (const plfield_t *field, const plitem_t *item,
							 void *data, plitem_t *messages, void *context)
{
	__auto_type handle = (VkDescriptorSetLayout *) data;
	int         ret = 1;
	parsectx_t *pctx = context;
	exprctx_t   ectx = *pctx->ectx;
	vulkan_ctx_t *ctx = pctx->vctx;

	const char *name = PL_String (item);
	Sys_Printf ("parse_VkDescriptorSetLayout: %s\n", name);
	if (name[0] != '$') {
		name = va (ctx->va_ctx, "$"QFV_PROPERTIES".setLayouts.%s", name);
	}

	*handle = (VkDescriptorSetLayout) QFV_GetHandle (ctx->setLayouts, name);
	if (*handle) {
		return 1;
	}

	plitem_t   *setItem = 0;
	exprval_t   result = { &cexpr_plitem, &setItem };
	ectx.symtab = 0;
	ectx.result = &result;
	ret = !cexpr_eval_string (name, &ectx);
	if (ret) {
		VkDescriptorSetLayout setLayout;
		setLayout = QFV_ParseDescriptorSetLayout (ctx, setItem,
												  pctx->properties);
		*handle = (VkDescriptorSetLayout) setLayout;

		QFV_AddHandle (ctx->setLayouts, name, (uint64_t) setLayout);
	}
	return ret;
}

static int
parse_VkPipelineLayout (const plitem_t *item, void **data,
						plitem_t *messages, parsectx_t *context)
{
	__auto_type handle = (VkPipelineLayout *) data[0];
	int         ret = 1;
	exprctx_t   ectx = *context->ectx;
	vulkan_ctx_t *ctx = context->vctx;

	const char *name = PL_String (item);
	Sys_Printf ("parse_VkPipelineLayout: %s\n", name);
	if (name[0] != '$') {
		name = va (ctx->va_ctx, "$"QFV_PROPERTIES".pipelineLayouts.%s", name);
	}

	*handle = (VkPipelineLayout) QFV_GetHandle (ctx->pipelineLayouts, name);
	if (*handle) {
		return 1;
	}

	plitem_t   *setItem = 0;
	exprval_t   result = { &cexpr_plitem, &setItem };
	ectx.symtab = 0;
	ectx.result = &result;
	ret = !cexpr_eval_string (name, &ectx);
	if (ret) {
		VkPipelineLayout layout;
		layout = QFV_ParsePipelineLayout (ctx, setItem, context->properties);
		*handle = (VkPipelineLayout) layout;

		QFV_AddHandle (ctx->pipelineLayouts, name, (uint64_t) layout);
	}
	return ret;
}

static int
parse_VkImage (const plitem_t *item, void **data, plitem_t *messages,
			   parsectx_t *context)
{
	__auto_type handle = (VkImage *) data[0];
	int         ret = 1;
	exprctx_t   ectx = *context->ectx;
	vulkan_ctx_t *ctx = context->vctx;

	const char *name = PL_String (item);
	Sys_Printf ("parse_VkImage: %s\n", name);
	if (name[0] != '$') {
		name = va (ctx->va_ctx, "$"QFV_PROPERTIES".images.%s", name);
	}

	*handle = (VkImage) QFV_GetHandle (ctx->images, name);
	if (*handle) {
		return 1;
	}

	plitem_t   *imageItem = 0;
	exprval_t   result = { &cexpr_plitem, &imageItem };
	ectx.symtab = 0;
	ectx.result = &result;
	ret = !cexpr_eval_string (name, &ectx);
	if (ret) {
		VkImage image;
		image = QFV_ParseImage (ctx, imageItem, context->properties);
		*handle = (VkImage) image;

		QFV_AddHandle (ctx->images, name, (uint64_t) image);
	}
	return ret;
}

static exprtype_t imageview_type = {
	"VkImageView",
	sizeof (VkImageView),
	0, 0, 0
};

static int
parse_VkImageView (const plfield_t *field, const plitem_t *item, void *data,
				   plitem_t *messages, void *_context)
{
	parsectx_t *context = _context;
	__auto_type handle = (VkImageView *) data;
	int         ret = 1;
	exprctx_t   ectx = *context->ectx;
	vulkan_ctx_t *ctx = context->vctx;

	const char *name = PL_String (item);
	Sys_Printf ("parse_VkImageView: %s\n", name);
	if (name[0] != '$') {
		name = va (ctx->va_ctx, "$"QFV_PROPERTIES".imageViews.%s", name);
	}

	*handle = (VkImageView) QFV_GetHandle (ctx->imageViews, name);
	if (*handle) {
		return 1;
	}

	exprval_t  *value = 0;
	exprval_t   result = { &cexpr_exprval, &value };
	ectx.symtab = 0;
	ectx.result = &result;
	ret = !cexpr_eval_string (name, &ectx);

	plitem_t   *imageViewItem = 0;
	if (ret) {
		VkImageView imageView;
		if (value->type == &imageview_type) {
			imageView = *(VkImageView *) value->value;
		} else if (value->type == &cexpr_plitem) {
			imageView = QFV_ParseImageView (ctx, imageViewItem,
											context->properties);
			QFV_AddHandle (ctx->imageViews, name, (uint64_t) imageView);
		} else {
			PL_Message (messages, item, "not a VkImageView");
			return 0;
		}
		*handle = (VkImageView) imageView;
	}
	return ret;
}

static const char *
handleref_getkey (const void *hr, void *unused)
{
	return ((handleref_t *)hr)->name;
}

static void
handleref_free (void *hr, void *_ctx)
{
	__auto_type handleref = (handleref_t *) hr;
	free (handleref->name);
	free (handleref);
}

static void
setLayout_free (void *hr, void *_ctx)
{
	__auto_type handleref = (handleref_t *) hr;
	__auto_type layout = (VkDescriptorSetLayout) handleref->handle;
	__auto_type ctx = (vulkan_ctx_t *) _ctx;
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;

	if (layout) {
		dfunc->vkDestroyDescriptorSetLayout (device->dev, layout, 0);
	}
	handleref_free (handleref, ctx);
}

static void
shaderModule_free (void *hr, void *_ctx)
{
	__auto_type handleref = (handleref_t *) hr;
	__auto_type module = (VkShaderModule) handleref->handle;
	__auto_type ctx = (vulkan_ctx_t *) _ctx;
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;

	if (module) {
		dfunc->vkDestroyShaderModule (device->dev, module, 0);
	}
	handleref_free (handleref, ctx);
}

static void
pipelineLayout_free (void *hr, void *_ctx)
{
	__auto_type handleref = (handleref_t *) hr;
	__auto_type layout = (VkPipelineLayout) handleref->handle;
	__auto_type ctx = (vulkan_ctx_t *) _ctx;
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;

	if (layout) {
		dfunc->vkDestroyPipelineLayout (device->dev, layout, 0);
	};
	handleref_free (handleref, ctx);
}

static void
descriptorPool_free (void *hr, void *_ctx)
{
	__auto_type handleref = (handleref_t *) hr;
	__auto_type pool = (VkDescriptorPool) handleref->handle;
	__auto_type ctx = (vulkan_ctx_t *) _ctx;
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;

	if (pool) {
		dfunc->vkDestroyDescriptorPool (device->dev, pool, 0);
	};
	handleref_free (handleref, ctx);
}

static void
sampler_free (void *hr, void *_ctx)
{
	__auto_type handleref = (handleref_t *) hr;
	__auto_type sampler = (VkSampler) handleref->handle;
	__auto_type ctx = (vulkan_ctx_t *) _ctx;
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;

	if (sampler) {
		dfunc->vkDestroySampler (device->dev, sampler, 0);
	};
	handleref_free (handleref, ctx);
}

static void
image_free (void *hr, void *_ctx)
{
	__auto_type handleref = (handleref_t *) hr;
	__auto_type image = (VkImage) handleref->handle;
	__auto_type ctx = (vulkan_ctx_t *) _ctx;
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;

	if (image) {
		dfunc->vkDestroyImage (device->dev, image, 0);
	};
	handleref_free (handleref, ctx);
}

static void
imageView_free (void *hr, void *_ctx)
{
	__auto_type handleref = (handleref_t *) hr;
	__auto_type imageView = (VkImageView) handleref->handle;
	__auto_type ctx = (vulkan_ctx_t *) _ctx;
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;

	if (imageView) {
		dfunc->vkDestroyImageView (device->dev, imageView, 0);
	};
	handleref_free (handleref, ctx);
}

static void
renderpass_free (void *hr, void *_ctx)
{
	__auto_type handleref = (handleref_t *) hr;
	__auto_type renderpass = (VkRenderPass) handleref->handle;
	__auto_type ctx = (vulkan_ctx_t *) _ctx;
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;

	if (renderpass) {
		dfunc->vkDestroyRenderPass (device->dev, renderpass, 0);
	};
	handleref_free (handleref, ctx);
}

static hashtab_t *enum_symtab;

static int
parse_BasePipeline (const plitem_t *item, void **data,
					plitem_t *messages, parsectx_t *context)
{
	*(VkPipeline *) data = 0;
	PL_Message (messages, item, "not implemented");
	return 0;
}

#include "libs/video/renderer/vulkan/vkparse.cinc"

static void
imageviewset_index (const exprval_t *a, size_t index, exprval_t *c,
					exprctx_t *ctx)
{
	__auto_type set = *(qfv_imageviewset_t **) a->value;
	exprval_t  *val = 0;
	if (index >= set->size) {
		cexpr_error (ctx, "invalid index: %zd", index);
	} else {
		val = cexpr_value (&imageview_type, ctx);
		*(VkImageView *) val->value = set->a[index];
	}
	*(exprval_t **) c->value = val;
}

static void
imageviewset_int (const exprval_t *a, const exprval_t *b, exprval_t *c,
				  exprctx_t *ctx)
{
	size_t      index = *(int *) b->value;
	imageviewset_index (a, index, c, ctx);
}

static void
imageviewset_uint (const exprval_t *a, const exprval_t *b, exprval_t *c,
				   exprctx_t *ctx)
{
	size_t      index = *(unsigned *) b->value;
	imageviewset_index (a, index, c, ctx);
}

static void
imageviewset_size_t (const exprval_t *a, const exprval_t *b, exprval_t *c,
					 exprctx_t *ctx)
{
	size_t      index = *(size_t *) b->value;
	imageviewset_index (a, index, c, ctx);
}

binop_t imageviewset_binops[] = {
	{ '.', &cexpr_field, &cexpr_exprval, cexpr_struct_pointer_getfield },
	{ '[', &cexpr_int, &imageview_type, imageviewset_int },
	{ '[', &cexpr_uint, &imageview_type, imageviewset_uint },
	{ '[', &cexpr_size_t, &imageview_type, imageviewset_size_t },
	{}
};

static exprsym_t imageviewset_symbols[] = {
	{"size", &cexpr_size_t, (void *)field_offset (qfv_imageviewset_t, size)},
	{ }
};
static exprtab_t imageviewset_symtab = {
	imageviewset_symbols,
};
exprtype_t imageviewset_type = {
	"imageviewset",
	sizeof (qfv_imageviewset_t *),
	imageviewset_binops,
	0,
	&imageviewset_symtab,
};
static exprsym_t qfv_swapchain_t_symbols[] = {
	{"format", &VkFormat_type, (void *)field_offset (qfv_swapchain_t, format)},
	{"extent", &VkExtent2D_type, (void *)field_offset (qfv_swapchain_t, extent)},
	{"views", &imageviewset_type, (void *)field_offset (qfv_swapchain_t, imageViews)},
	{ }
};
static exprtab_t qfv_swapchain_t_symtab = {
	qfv_swapchain_t_symbols,
};
exprtype_t qfv_swapchain_t_type = {
	"qfv_swapchain_t",
	sizeof (qfv_swapchain_t),
	cexpr_struct_binops,
	0,
	&qfv_swapchain_t_symtab,
};

static exprsym_t vulkan_frameset_t_symbols[] = {
	{"size", &cexpr_size_t, (void *)field_offset (vulkan_frameset_t, size)},
	{ }
};
static exprtab_t vulkan_frameset_t_symtab = {
	vulkan_frameset_t_symbols,
};
exprtype_t vulkan_frameset_t_type = {
	"frameset",
	sizeof (vulkan_frameset_t *),
	cexpr_struct_binops,
	0,
	&vulkan_frameset_t_symtab,
};

typedef struct qfv_renderpass_s {
	qfv_attachmentdescription_t *attachments;
	qfv_subpassparametersset_t *subpasses;
	qfv_subpassdependency_t *dependencies;
} qfv_renderpass_t;

static plelement_t parse_qfv_renderpass_attachments_data = {
	QFDictionary,
	sizeof (VkAttachmentDescription),
	array_alloc,
	parse_VkAttachmentDescription,
	0,
};

static plelement_t parse_qfv_renderpass_subpasses_data = {
	QFDictionary,
	sizeof (VkSubpassDescription),
	array_alloc,
	parse_VkSubpassDescription,
	0,
};

static plelement_t parse_qfv_renderpass_dependencies_data = {
	QFDictionary,
	sizeof (VkSubpassDependency),
	array_alloc,
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

static hashtab_t *
handlref_symtab (void (*free_func)(void*,void*), vulkan_ctx_t *ctx)
{
	return Hash_NewTable (23, handleref_getkey, free_func,
						  ctx, &ctx->hashlinks);
}

static const char *
enum_symtab_getkey (const void *e, void *unused)
{
	__auto_type enm = (const exprenum_t *) e;
	return enm->type->name;
}

void
QFV_InitParse (vulkan_ctx_t *ctx)
{
	exprctx_t   context = {};
	enum_symtab = Hash_NewTable (61, enum_symtab_getkey, 0, 0,
								 &ctx->hashlinks);
	context.hashlinks = &ctx->hashlinks;
	vkgen_init_symtabs (&context);
	cexpr_init_symtab (&qfv_swapchain_t_symtab, &context);
	cexpr_init_symtab (&vulkan_frameset_t_symtab, &context);
	cexpr_init_symtab (&imageviewset_symtab, &context);

	if (!ctx->setLayouts) {
		ctx->shaderModules = handlref_symtab (shaderModule_free, ctx);
		ctx->setLayouts = handlref_symtab (setLayout_free, ctx);
		ctx->pipelineLayouts = handlref_symtab (pipelineLayout_free, ctx);
		ctx->descriptorPools = handlref_symtab (descriptorPool_free, ctx);
		ctx->samplers = handlref_symtab (sampler_free, ctx);
		ctx->images = handlref_symtab (image_free, ctx);
		ctx->imageViews = handlref_symtab (imageView_free, ctx);
		ctx->renderpasses = handlref_symtab (renderpass_free, ctx);
	}
}

exprenum_t *
QFV_GetEnum (const char *name)
{
	return Hash_Find (enum_symtab, name);
}

static int
parse_object (vulkan_ctx_t *ctx, plitem_t *plist,
			  plparser_t parser, void *object, plitem_t *properties)
{
	plitem_t   *messages = PL_NewArray ();
	exprctx_t   exprctx = {};
	parsectx_t  parsectx = { &exprctx, ctx, properties };
	exprsym_t   var_syms[] = {
		{"swapchain", &qfv_swapchain_t_type, ctx->swapchain},
		{"frames", &vulkan_frameset_t_type, &ctx->frames},
		{"msaaSamples", &VkSampleCountFlagBits_type, &ctx->msaaSamples},
		{"swapImageIndex", &cexpr_uint, &ctx->swapImageIndex},
		{QFV_PROPERTIES, &cexpr_plitem, &parsectx.properties},
		{}
	};
	exprtab_t   vars_tab = { var_syms, 0 };

	exprctx.external_variables = &vars_tab;
	exprctx.memsuper = new_memsuper ();
	exprctx.messages = messages;
	exprctx.hashlinks = &ctx->hashlinks;

	cexpr_init_symtab (&vars_tab, &exprctx);


	if (!parser (0, plist, object, messages, &parsectx)) {
		for (int i = 0; i < PL_A_NumObjects (messages); i++) {
			Sys_Printf ("%s\n", PL_String (PL_ObjectAtIndex (messages, i)));
		}
		return 0;
	}
	PL_Free (messages);
	delete_memsuper (exprctx.memsuper);

	return 1;
}

static int
parse_qfv_renderpass (const plfield_t *field, const plitem_t *item, void *data,
					  plitem_t *messages, void *context)
{
	return PL_ParseStruct (renderpass_fields, item, data, messages, context);
}

VkRenderPass
QFV_ParseRenderPass (vulkan_ctx_t *ctx, plitem_t *plist, plitem_t *properties)
{
	qfv_device_t *device = ctx->device;

	qfv_renderpass_t renderpass_data = {};

	if (!parse_object (ctx, plist, parse_qfv_renderpass, &renderpass_data,
					   properties)) {
		return 0;
	}

	VkRenderPass renderpass;
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

VkPipeline
QFV_ParsePipeline (vulkan_ctx_t *ctx, plitem_t *plist, plitem_t *properties)
{
	qfv_device_t *device = ctx->device;

	__auto_type cInfo = QFV_AllocGraphicsPipelineCreateInfoSet (1, alloca);
	memset (&cInfo->a[0], 0, sizeof (cInfo->a[0]));

	if (!parse_object (ctx, plist, parse_VkGraphicsPipelineCreateInfo,
					   &cInfo->a[0], properties)) {
		return 0;
	}

	cInfo->a[0].renderPass = ctx->renderpass;
	__auto_type plSet = QFV_CreateGraphicsPipelines (device, 0, cInfo);
	VkPipeline pipeline = plSet->a[0];
	free (plSet);
	return pipeline;
}

VkDescriptorPool
QFV_ParseDescriptorPool (vulkan_ctx_t *ctx, plitem_t *plist,
						 plitem_t *properties)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;

	VkDescriptorPoolCreateInfo cInfo = {};

	if (!parse_object (ctx, plist, parse_VkDescriptorPoolCreateInfo, &cInfo,
					   properties)) {
		return 0;
	}

	VkDescriptorPool pool;
	dfunc->vkCreateDescriptorPool (device->dev, &cInfo, 0, &pool);

	return pool;
}

VkDescriptorSetLayout
QFV_ParseDescriptorSetLayout (vulkan_ctx_t *ctx, plitem_t *plist,
							  plitem_t *properties)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;

	VkDescriptorSetLayoutCreateInfo cInfo = {};

	if (!parse_object (ctx, plist, parse_VkDescriptorSetLayoutCreateInfo,
					   &cInfo, properties)) {
		return 0;
	}

	VkDescriptorSetLayout setLayout;
	dfunc->vkCreateDescriptorSetLayout (device->dev, &cInfo, 0, &setLayout);

	return setLayout;
}

VkPipelineLayout
QFV_ParsePipelineLayout (vulkan_ctx_t *ctx, plitem_t *plist,
						 plitem_t *properties)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;

	VkPipelineLayoutCreateInfo cInfo = {};

	if (!parse_object (ctx, plist, parse_VkPipelineLayoutCreateInfo,
					   &cInfo, properties)) {
		return 0;
	}

	VkPipelineLayout layout;
	dfunc->vkCreatePipelineLayout (device->dev, &cInfo, 0, &layout);

	return layout;
}

VkSampler
QFV_ParseSampler (vulkan_ctx_t *ctx, plitem_t *plist, plitem_t *properties)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;

	VkSamplerCreateInfo cInfo = {};

	if (!parse_object (ctx, plist, parse_VkSamplerCreateInfo, &cInfo,
					   properties)) {
		return 0;
	}

	VkSampler sampler;
	dfunc->vkCreateSampler (device->dev, &cInfo, 0, &sampler);

	return sampler;
}

VkImage
QFV_ParseImage (vulkan_ctx_t *ctx, plitem_t *plist, plitem_t *properties)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;

	VkImageCreateInfo cInfo = {};

	if (!parse_object (ctx, plist, parse_VkImageCreateInfo, &cInfo,
					   properties)) {
		return 0;
	}

	VkImage image;
	dfunc->vkCreateImage (device->dev, &cInfo, 0, &image);

	return image;
}

VkImageView
QFV_ParseImageView (vulkan_ctx_t *ctx, plitem_t *plist, plitem_t *properties)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;

	VkImageViewCreateInfo cInfo = {};

	if (!parse_object (ctx, plist, parse_VkImageViewCreateInfo, &cInfo,
					   properties)) {
		return 0;
	}

	VkImageView imageView;
	dfunc->vkCreateImageView (device->dev, &cInfo, 0, &imageView);

	return imageView;
}

typedef struct {
	uint32_t    count;
	VkImageCreateInfo *info;
} imagecreate_t;

typedef struct {
	uint32_t    count;
	VkImageViewCreateInfo *info;
} imageviewcreate_t;

static plelement_t qfv_imagecreate_dict = {
	QFDictionary,
	sizeof (VkImageCreateInfo),
	array_alloc,
	parse_VkImageCreateInfo,
};

static plelement_t qfv_imageviewcreate_dict = {
	QFDictionary,
	sizeof (VkImageViewCreateInfo),
	array_alloc,
	parse_VkImageViewCreateInfo,
};

static int
parse_imagecreate_dict (const plfield_t *field, const plitem_t *item,
						void *data, plitem_t *messages, void *context)
{
	plfield_t   f = { "images", 0, QFArray, parse_array,
					  &qfv_imagecreate_dict };
	typedef struct arr_s DARRAY_TYPE(byte) arr_t;
	arr_t      *arr = 0;
	int         ret;

	if ((ret = PL_ParseLabeledArray (&f, item, &arr, messages, context))) {
		imagecreate_t  *imagecreate = data;
		imagecreate->count = arr->size;
		imagecreate->info = (VkImageCreateInfo *) arr->a;
	} else {
		//FIXME leaky boat when succeeds
		if (arr) {
			free (arr);
		}
	}
	return ret;
}

static int
parse_imageviewcreate_dict (const plfield_t *field, const plitem_t *item,
							void *data, plitem_t *messages, void *context)
{
	plfield_t   f = { "images", 0, QFArray, parse_array,
					  &qfv_imageviewcreate_dict };
	typedef struct arr_s DARRAY_TYPE(byte) arr_t;
	arr_t      *arr = 0;
	int         ret;

	if ((ret = PL_ParseLabeledArray (&f, item, &arr, messages, context))) {
		imageviewcreate_t  *imageviewcreate = data;
		imageviewcreate->count = arr->size;
		imageviewcreate->info = (VkImageViewCreateInfo *) arr->a;
	} else {
		//FIXME leaky boat when succeeds
		if (arr) {
			free (arr);
		}
	}
	return ret;
}

qfv_imageset_t *
QFV_ParseImageSet (vulkan_ctx_t *ctx, plitem_t *item, plitem_t *properties)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;

	imagecreate_t create = {};

	pltype_t    type = PL_Type (item);

	if (type == QFDictionary) {
		if (!parse_object (ctx, item, parse_imagecreate_dict, &create,
						   properties)) {
			return 0;
		}
	} else {
		Sys_Printf ("Neither array nor dictionary: %d\n", PL_Line (item));
		return 0;
	}

	__auto_type set = QFV_AllocImages (create.count, malloc);
	for (uint32_t i = 0; i < create.count; i++) {
		dfunc->vkCreateImage (device->dev, &create.info[i], 0, &set->a[i]);

		const char *name = PL_KeyAtIndex (item, i);
		QFV_duSetObjectName (device, VK_OBJECT_TYPE_IMAGE, set->a[i],
							 va (ctx->va_ctx, "image:%s", name));
		name = va (ctx->va_ctx, "$"QFV_PROPERTIES".images.%s", name);
		QFV_AddHandle (ctx->images, name, (uint64_t) set->a[i]);
	}

	return set;
}

qfv_imageviewset_t *
QFV_ParseImageViewSet (vulkan_ctx_t *ctx, plitem_t *item,
					   plitem_t *properties)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;

	imageviewcreate_t create = {};

	pltype_t    type = PL_Type (item);

	if (type == QFDictionary) {
		if (!parse_object (ctx, item, parse_imageviewcreate_dict, &create,
						   properties)) {
			return 0;
		}
	} else {
		Sys_Printf ("Neither array nor dictionary: %d\n", PL_Line (item));
		return 0;
	}

	__auto_type set = QFV_AllocImageViews (create.count, malloc);
	for (uint32_t i = 0; i < create.count; i++) {
		dfunc->vkCreateImageView (device->dev, &create.info[i], 0, &set->a[i]);

		const char *name = PL_KeyAtIndex (item, i);
		name = va (ctx->va_ctx, "$"QFV_PROPERTIES".imageViews.%s", name);
		QFV_AddHandle (ctx->imageViews, name, (uint64_t) set->a[i]);
	}

	return set;
}

VkFramebuffer
QFV_ParseFramebuffer (vulkan_ctx_t *ctx, plitem_t *plist, plitem_t *properties)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;

	VkFramebufferCreateInfo cInfo = {};

	if (!parse_object (ctx, plist, parse_VkFramebufferCreateInfo, &cInfo,
					   properties)) {
		return 0;
	}

	VkFramebuffer framebuffer;
	dfunc->vkCreateFramebuffer (device->dev, &cInfo, 0, &framebuffer);
	printf ("framebuffer, renderPass: %p, %p\n", framebuffer, cInfo.renderPass);

	return framebuffer;
}

static int
parse_clearvalueset (const plfield_t *field, const plitem_t *item, void *data,
					 plitem_t *messages, void *context)
{
	parsectx_t *parsectx = context;
	vulkan_ctx_t *ctx = parsectx->vctx;

	plelement_t element = {
		QFDictionary,
		sizeof (VkClearValue),
		array_alloc,
		parse_VkClearValue,
		0,
	};
	plfield_t   f = { 0, 0, 0, 0, &element };

	if (!PL_ParseArray (&f, item, &ctx->clearValues, messages, context)) {
		return 0;
	}
	return 1;
}

int
QFV_ParseClearValues (vulkan_ctx_t *ctx, plitem_t *plist, plitem_t *properties)
{
	if (!parse_object (ctx, plist, parse_clearvalueset, &ctx->clearValues,
					   properties)) {
		return 0;
	}
	return 1;
}
