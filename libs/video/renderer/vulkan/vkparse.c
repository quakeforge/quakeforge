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
	size_t      val = 0;
	exprctx_t   ectx = *((parsectx_t *) context)->ectx;
	exprval_t   result = { &cexpr_size_t, &val };
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
	exprctx_t   ectx = *((parsectx_t *) context)->ectx;
	vulkan_ctx_t *ctx = ((parsectx_t *) context)->vctx;

	const char *name = PL_String (item);
	Sys_Printf ("parse_VkDescriptorSetLayout: %s\n", name);
	name = va (ctx->va_ctx, "$"QFV_PROPERTIES".setLayouts.%s", name);

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
		setLayout = QFV_ParseDescriptorSetLayout (ctx, setItem);
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
	exprctx_t   ectx = *((parsectx_t *) context)->ectx;
	vulkan_ctx_t *ctx = ((parsectx_t *) context)->vctx;

	const char *name = PL_String (item);
	Sys_Printf ("parse_VkPipelineLayout: %s\n", name);
	name = va (ctx->va_ctx, "$"QFV_PROPERTIES".pipelineLayouts.%s", name);

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
		layout = QFV_ParsePipelineLayout (ctx, setItem);
		*handle = (VkPipelineLayout) layout;

		QFV_AddHandle (ctx->pipelineLayouts, name, (uint64_t) layout);
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

static exprsym_t imageset_symbols[] = {
	{"size", &cexpr_size_t, (void *)field_offset (qfv_imageset_t, size)},
	{ }
};
static exprtab_t imageset_symtab = {
	imageset_symbols,
};
exprtype_t imageset_type = {
	"imageset",
	sizeof (qfv_imageset_t *),
	cexpr_struct_pointer_binops,
	0,
	&imageset_symtab,
};
static exprsym_t qfv_swapchain_t_symbols[] = {
	{"format", &VkFormat_type, (void *)field_offset (qfv_swapchain_t, format)},
	{"images", &imageset_type, (void *)field_offset (qfv_swapchain_t, images)},
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
	cexpr_init_symtab (&imageset_symtab, &context);

	if (!ctx->setLayouts) {
		ctx->shaderModules = handlref_symtab (shaderModule_free, ctx);
		ctx->setLayouts = handlref_symtab (setLayout_free, ctx);
		ctx->pipelineLayouts = handlref_symtab (pipelineLayout_free, ctx);
		ctx->descriptorPools = handlref_symtab (descriptorPool_free, ctx);
		ctx->samplers = handlref_symtab (sampler_free, ctx);
	}
}

exprenum_t *
QFV_GetEnum (const char *name)
{
	return Hash_Find (enum_symtab, name);
}

static int
parse_object (vulkan_ctx_t *ctx, plitem_t *plist,
			  plparser_t parser, void *object)
{
	plitem_t   *messages = PL_NewArray ();
	exprctx_t   exprctx = {};
	parsectx_t  parsectx = { &exprctx, ctx };
	exprsym_t   var_syms[] = {
		{"swapchain", &qfv_swapchain_t_type, ctx->swapchain},
		{"frames", &vulkan_frameset_t_type, &ctx->frames},
		{"msaaSamples", &VkSampleCountFlagBits_type, &ctx->msaaSamples},
		{QFV_PROPERTIES, &cexpr_plitem, &ctx->pipelineDef},
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
QFV_ParseRenderPass (vulkan_ctx_t *ctx, plitem_t *plist)
{
	qfv_device_t *device = ctx->device;

	qfv_renderpass_t renderpass_data = {};

	if (!parse_object (ctx, plist, parse_qfv_renderpass, &renderpass_data)) {
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
QFV_ParsePipeline (vulkan_ctx_t *ctx, plitem_t *plist)
{
	qfv_device_t *device = ctx->device;

	__auto_type cInfo = QFV_AllocGraphicsPipelineCreateInfoSet (1, alloca);
	memset (&cInfo->a[0], 0, sizeof (cInfo->a[0]));

	if (!parse_object (ctx, plist, parse_VkGraphicsPipelineCreateInfo,
					   &cInfo->a[0])) {
		return 0;
	}

	cInfo->a[0].renderPass = ctx->renderpass.renderpass;
	__auto_type plSet = QFV_CreateGraphicsPipelines (device, 0, cInfo);
	VkPipeline pipeline = plSet->a[0];
	free (plSet);
	return pipeline;
}

VkDescriptorPool
QFV_ParseDescriptorPool (vulkan_ctx_t *ctx, plitem_t *plist)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;

	VkDescriptorPoolCreateInfo cInfo = {};

	if (!parse_object (ctx, plist, parse_VkDescriptorPoolCreateInfo, &cInfo)) {
		return 0;
	}

	VkDescriptorPool pool;
	dfunc->vkCreateDescriptorPool (device->dev, &cInfo, 0, &pool);

	return pool;
}

VkDescriptorSetLayout
QFV_ParseDescriptorSetLayout (vulkan_ctx_t *ctx, plitem_t *plist)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;

	VkDescriptorSetLayoutCreateInfo cInfo = {};

	if (!parse_object (ctx, plist, parse_VkDescriptorSetLayoutCreateInfo,
					   &cInfo)) {
		return 0;
	}

	VkDescriptorSetLayout setLayout;
	dfunc->vkCreateDescriptorSetLayout (device->dev, &cInfo, 0, &setLayout);

	return setLayout;
}

VkPipelineLayout
QFV_ParsePipelineLayout (vulkan_ctx_t *ctx, plitem_t *plist)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;

	VkPipelineLayoutCreateInfo cInfo = {};

	if (!parse_object (ctx, plist, parse_VkPipelineLayoutCreateInfo,
					   &cInfo)) {
		return 0;
	}

	VkPipelineLayout layout;
	dfunc->vkCreatePipelineLayout (device->dev, &cInfo, 0, &layout);

	return layout;
}

VkSampler
QFV_ParseSampler (vulkan_ctx_t *ctx, plitem_t *plist)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;

	VkSamplerCreateInfo cInfo = {};

	if (!parse_object (ctx, plist, parse_VkSamplerCreateInfo, &cInfo)) {
		return 0;
	}

	VkSampler sampler;
	dfunc->vkCreateSampler (device->dev, &cInfo, 0, &sampler);

	return sampler;
}
