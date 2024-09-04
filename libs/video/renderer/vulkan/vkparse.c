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

#include <string.h>

#include "QF/cmem.h"
#include "QF/cvar.h"
#include "QF/dstring.h"
#include "QF/hash.h"
#include "QF/mathlib.h"
#include "QF/va.h"

#include "QF/Vulkan/debug.h"
#include "QF/Vulkan/device.h"
#include "QF/Vulkan/instance.h"
#include "QF/Vulkan/image.h"
#include "QF/Vulkan/pipeline.h"
#include "QF/Vulkan/render.h"
#include "QF/Vulkan/shader.h"

#include "vid_vulkan.h"

#include "vkparse.h"
#include "libs/video/renderer/vulkan/vkparse.hinc"

typedef struct parseres_s {
	const char *name;
	plfield_t  *field;
	size_t      offset;
} parseres_t;

typedef struct parseref_s {
	const char *name;
	plparser_t  parse;
	size_t      size;
} parserref_t;

typedef struct handleref_s {
	char       *name;
	uint64_t    handle;
} handleref_t;

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
	void       *data;
	size_t      value_offset;
} parse_single_t;

typedef struct parse_array_s {
	pltype_t    type;
	size_t      stride;
	plparser_t  parser;
	void       *data;
	size_t      value_offset;
	size_t      size_offset;
} parse_array_t;

typedef struct parse_fixed_array_s {
	pltype_t    type;
	size_t      stride;
	plparser_t  parser;
	void       *data;
	size_t      size;
} parse_fixed_array_t;

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

static plfield_t *__attribute__((used))
find_field (plfield_t *fields, const char *field_name,
			const plitem_t *item, plitem_t *messages)
{
	for (plfield_t *f = fields; f->name; f++) {
		if (strcmp (f->name, field_name) == 0) {
			return f;
		}
	}
	PL_Message (messages, item, "error: unknown field '%s'", field_name);
	return 0;
}

static int
parse_basic (const plfield_t *field, const plitem_t *item,
			 void *data, plitem_t *messages, void *context)
{
	int         ret = 1;
	parsectx_t *pctx = context;
	__auto_type etype = (exprtype_t *) field->data;
	exprctx_t   ectx = *pctx->ectx;
	exprval_t   result = { etype, data };
	ectx.result = &result;
	ectx.item = item;
	const char *valstr = PL_String (item);
	//Sys_MaskPrintf (SYS_vulkan_parse, "parse_basic: %s %zd %d %p %p: %s\n",
	//				field->name, field->offset, field->type, field->parser,
	//				field->data, valstr);
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
	//Sys_MaskPrintf (SYS_vulkan_parse, "    %x\n", *(uint32_t *)data);

	return ret;
}

static int
parse_int32_t (const plfield_t *field, const plitem_t *item,
				void *data, plitem_t *messages, void *context)
{
	int         ret = 1;
	int         val = 0;
	parsectx_t *pctx = context;
	exprval_t   result = { &cexpr_int, &val };
	exprctx_t   ectx = *pctx->ectx;
	ectx.result = &result;
	ectx.item = item;
	const char *valstr = PL_String (item);
	//Sys_MaskPrintf (SYS_vulkan_parse,
	//				"parse_int32_t: %s %zd %d %p %p %s\n",
	//				field->name, field->offset, field->type, field->parser,
	//				field->data, valstr);
	ret = !cexpr_eval_string (valstr, &ectx);
	if (!ret) {
		PL_Message (messages, item, "error parsing %s: %s",
					field->name, valstr);
	}
	*(int32_t *) data = val;
	//Sys_MaskPrintf (SYS_vulkan_parse, "    %d\n", *(int32_t *)data);

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
	parsectx_t *pctx = context;
	exprval_t   result = { &cexpr_size_t, &val };
	exprctx_t   ectx = *pctx->ectx;
	ectx.result = &result;
	ectx.item = item;
	const char *valstr = PL_String (item);
	//Sys_MaskPrintf (SYS_vulkan_parse, "parse_uint32_t: %s %zd %d %p %p: %s\n",
	//				field->name, field->offset, field->type, field->parser,
	//				field->data, valstr);
	if (strcmp (valstr, "VK_SUBPASS_EXTERNAL") == 0) {
		//FIXME handle subpass in a separate parser?
		*(uint32_t *) data = VK_SUBPASS_EXTERNAL;
	} else {
		//Sys_MaskPrintf (SYS_vulkan_parse,
		//				"parse_uint32_t: %s %zd %d %p %p %s\n",
		//				field->name, field->offset, field->type, field->parser,
		//				field->data, valstr);
		ret = !cexpr_eval_string (valstr, &ectx);
		if (!ret) {
			PL_Message (messages, item, "error parsing %s: %s",
						field->name, valstr);
		}
		*(uint32_t *) data = val;
		//Sys_MaskPrintf (SYS_vulkan_parse, "    %d\n", *(uint32_t *)data);
	}

	return ret;
}

static int
parse_enum (const plfield_t *field, const plitem_t *item,
			void *data, plitem_t *messages, void *context)
{
	int         ret = 1;
	__auto_type enm = (exprenum_t *) field->data;
	parsectx_t *pctx = context;
	exprctx_t   ectx = *pctx->ectx;
	exprval_t   result = { enm->type, data };
	ectx.parent = pctx->ectx;
	ectx.symtab = enm->symtab;
	ectx.result = &result;
	const char *valstr = PL_String (item);
	//Sys_MaskPrintf (SYS_vulkan_parse, "parse_enum: %s %zd %d %p %p %s\n",
	//				field->name, field->offset, field->type, field->parser,
	//				field->data, valstr);
	ret = !cexpr_parse_enum (enm, valstr, &ectx, data);
	if (!ret) {
		PL_Message (messages, item, "error parsing enum: %s", valstr);
	}
	//Sys_MaskPrintf (SYS_vulkan_parse, "    %d\n", *(int *)data);
	return ret;
}

static const plitem_t *
parse_reference (const plitem_t *item, const char *type, plitem_t *messages,
				 parsectx_t *pctx)
{
	exprctx_t   ectx = *pctx->ectx;
	plitem_t   *refItem = 0;
	exprval_t   result = { &cexpr_plitem, &refItem };
	ectx.result = &result;
	ectx.item = item;
	const char *name = PL_String (item);
	if (cexpr_eval_string (name, &ectx)) {
		PL_Message (messages, item, "not a %s reference", type);
		return 0;
	}
	return refItem;
}

static void *
vkparse_alloc (void *context, size_t size)
{
	parsectx_t *pctx = context;
	return cmemalloc (pctx->ectx->memsuper, size);
}

static int
parse_ignore (const plfield_t *field, const plitem_t *item,
			  void *data, plitem_t *messages, void *context)
{
	return 1;
}

static int __attribute__((used))
parse_labeledsingle (const plfield_t *field, const plitem_t *item,
					 void *data, plitem_t *messages, void *context)
{
	__auto_type single = (parse_single_t *) field->data;
	void       *flddata = (byte *)data + single->value_offset;

	//Sys_MaskPrintf (SYS_vulkan_parse,"parse_labeledsingle: %s %zd %d %p %p\n",
	//				field->name, field->offset,
	//				field->type, field->parser, field->data);

	const char *key = PL_KeyAtIndex (item, 0);
	if (!key) {
		PL_Message (messages, item, "missing item");
		return 0;
	}
	item = PL_ObjectForKey (item, key);

	if (!PL_CheckType (single->type, PL_Type (item))) {
		PL_TypeMismatch (messages, item, field->name, single->type,
						 PL_Type (item));
		return 0;
	}

	plfield_t   f = { key, 0, single->type, single->parser, 0 };
	void       *value = vkparse_alloc (context, single->stride);
	memset (value, 0, single->stride);
	if (!single->parser (&f, item, value, messages, context)) {
		return 0;
	}

	*(void **) flddata = value;
	return 1;
}

static int
parse_single (const plfield_t *field, const plitem_t *item,
			  void *data, plitem_t *messages, void *context)
{
	__auto_type single = (parse_single_t *) field->data;
	void       *flddata = (byte *)data + single->value_offset;

	//Sys_MaskPrintf (SYS_vulkan_parse, "parse_single: %s %zd %d %p %p\n",
	//				field->name, field->offset,
	//				field->type, field->parser, field->data);

	if (!PL_CheckType (single->type, PL_Type (item))) {
		PL_TypeMismatch (messages, item, field->name, single->type,
						 PL_Type (item));
		return 0;
	}

	plfield_t   f = { field->name, 0, single->type, single->parser, 0 };
	void       *value = vkparse_alloc (context, single->stride);
	memset (value, 0, single->stride);
	if (!single->parser (&f, item, value, messages, context)) {
		return 0;
	}

	*(void **) flddata = value;
	return 1;
}

static int __attribute__((used))
parse_labeledarray (const plfield_t *field, const plitem_t *item,
					void *data, plitem_t *messages, void *context)
{
	__auto_type array = (parse_array_t *) field->data;
	__auto_type value = (void **) ((byte *)data + array->value_offset);
	__auto_type size = (uint32_t *) ((byte *)data + array->size_offset);

	plelement_t element = {
		array->type,
		array->stride,
		vkparse_alloc,
		array->parser,
		array->data,
	};
	plfield_t   f = { 0, 0, 0, 0, &element };

	typedef struct arr_s DARRAY_TYPE(byte) arr_t;
	arr_t      *arr;

	//Sys_MaskPrintf (SYS_vulkan_parse, "parse_array: %s %zd %d %p %p %p\n",
	//				field->name, field->offset, field->type, field->parser,
	//				field->data, data);
	//Sys_MaskPrintf (SYS_vulkan_parse, "    %d %zd %p %zd %zd\n", array->type,
	//				array->stride, array->parser, array->value_offset,
	//				array->size_offset);
	if (!PL_ParseLabeledArray (&f, item, &arr, messages, context)) {
		return 0;
	}
	*value = vkparse_alloc (context, array->stride * arr->size);
	memcpy (*value, arr->a, array->stride * arr->size);
	if ((void *) size >= data) {
		*size = arr->size;
	}
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
		vkparse_alloc,
		array->parser,
		array->data,
	};
	plfield_t   f = { 0, 0, 0, 0, &element };

	typedef struct arr_s DARRAY_TYPE(byte) arr_t;
	arr_t      *arr;

	//Sys_MaskPrintf (SYS_vulkan_parse, "parse_array: %s %zd %d %p %p %p\n",
	//				field->name, field->offset, field->type, field->parser,
	//				field->data, data);
	//Sys_MaskPrintf (SYS_vulkan_parse, "    %d %zd %p %zd %zd\n", array->type,
	//				array->stride, array->parser, array->value_offset,
	//				array->size_offset);
	if (!PL_ParseArray (&f, item, &arr, messages, context)) {
		return 0;
	}
	*value = vkparse_alloc (context, array->stride * arr->size);
	memcpy (*value, arr->a, array->stride * arr->size);
	if ((void *) size >= data) {
		*size = arr->size;
	}
	return 1;
}

static int __attribute__((used))
parse_fixed_array (const plfield_t *field, const plitem_t *item,
				   void *data, plitem_t *messages, void *context)
{
	__auto_type array = (parse_fixed_array_t *) field->data;

	plelement_t element = {
		array->type,
		array->stride,
		vkparse_alloc,
		array->parser,
		array->data,
	};
	plfield_t   f = { 0, 0, 0, 0, &element };

	typedef struct arr_s DARRAY_TYPE(byte) arr_t;
	arr_t      *arr;

	if (!PL_ParseArray (&f, item, &arr, messages, context)) {
		return 0;
	}
	memset (data, 0, array->stride * array->size);
	size_t      size = min (array->size, arr->size);
	memcpy (data, arr->a, array->stride * size);
	return 1;
}

static char *
vkstrdup (parsectx_t *context, const char *str)
{
	size_t      len = strlen (str) + 1;
	char       *dup = vkparse_alloc (context, len);
	memcpy (dup, str, len);
	return dup;
}

static __attribute__((used)) parse_string_t parse_string_array = { 0 };

static int
parse_string (const plfield_t *field, const plitem_t *item,
			  void *data, plitem_t *messages, void *context)
{
	__auto_type string = (parse_string_t *) field->data;
	__auto_type value = (char **) ((byte *)data + string->value_offset);

	const char *str = PL_String (item);

	//Sys_MaskPrintf (SYS_vulkan_parse, "parse_string: %s %zd %d %p %p %p\n",
	//				field->name, field->offset, field->type, field->parser,
	//				field->data, data);
	//Sys_MaskPrintf (SYS_vulkan_parse, "    %zd\n", string->value_offset);
	//Sys_MaskPrintf (SYS_vulkan_parse, "    %s\n", str);

	*value = vkstrdup (context, str);
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
parse_inherit (const plfield_t *field, const plitem_t *item,
			   void *data, plitem_t *messages, void *context)
{
	parsectx_t *pctx = context;
	exprctx_t   ectx = *pctx->ectx;
	plitem_t   *inheritItem = 0;
	exprval_t   result = { &cexpr_plitem, &inheritItem };
	ectx.result = &result;
	ectx.item = item;
	const char *inheritstr = PL_String (item);
	//Sys_MaskPrintf (SYS_vulkan_parse, "parse_inherit: %s\n", inheritstr);
	int         ret = !cexpr_eval_string (inheritstr, &ectx);
	if (ret) {
		ret = PL_ParseStruct (field->data, inheritItem, data, messages,
							  context);
	}
	return ret;
}

static hashtab_t *parser_table;

static int
parse_next (const plfield_t *field, const plitem_t *item, void *data,
			plitem_t *messages, void *context)
{
	const char *type_name = PL_String (PL_ObjectAtIndex (item, 0));
	plitem_t   *next_def = PL_ObjectAtIndex (item, 1);

	if (!type_name || PL_Type (next_def) != QFDictionary) {
		PL_Message (messages, item, "invalid @next");
		return 0;
	}
	parserref_t *parser = Hash_Find (parser_table, type_name);
	if (!parser) {
		PL_Message (messages, item, "Invalid type for @next: %s", type_name);
		return 0;
	}
	void       *data_ptr = vkparse_alloc (context, parser->size);
	memset (data_ptr, 0, parser->size);
	if (!parser->parse (field, next_def, data_ptr, messages, context)) {
		return 0;
	}
	*(void **) data = data_ptr;
	return 1;
}

static int
parse_RGBA (const plitem_t *item, void **data,
			plitem_t *messages, parsectx_t *context)
{
	int         ret = 1;
	exprctx_t   ectx = *context->ectx;
	exprval_t   result = { &cexpr_vector, data[0] };
	ectx.result = &result;
	ectx.item = item;
	const char *valstr = PL_String (item);
	//Sys_MaskPrintf (SYS_vulkan_parse, "parse_RGBA: %s\n", valstr);
	ret = !cexpr_eval_string (valstr, &ectx);
	//Sys_MaskPrintf (SYS_vulkan_parse, "    "VEC4F_FMT"\n",
	//				VEC4_EXP (*(vec4f_t *)data[0]));
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
					  plitem_t *messages, parsectx_t *pctx)
{
	__auto_type handle = (VkShaderModule *) data[0];
	vulkan_ctx_t *ctx = pctx->vctx;
	qfv_device_t *device = ctx->device;
	scriptctx_t *sctx = ctx->script_context;

	const char *name = PL_String (item);
	*handle = (VkShaderModule) QFV_GetHandle (sctx->shaderModules, name);
	if (*handle) {
		return 1;
	}
	qfvPushDebug (ctx, va (ctx->va_ctx, "parse_VkShaderModule: %d", PL_Line (item)));
	*handle = QFV_CreateShaderModule (device, name);
	qfvPopDebug (ctx);
	if (!*handle) {
		PL_Message (messages, item, "could not find shader %s", name);
		return 0;
	}
	QFV_AddHandle (sctx->shaderModules, name, (uint64_t) *handle);
	return 1;
}

exprtype_t VkImage_type = {
	.name = "VkImage",
	.size = sizeof (VkImage),
	.binops = 0,
	.unops = 0,
	.data = 0
};

exprtype_t VkImageView_type = {
	.name = "VkImageView",
	.size = sizeof (VkImageView),
	.binops = 0,
	.unops = 0,
	.data = 0
};

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
	scriptctx_t *sctx = _ctx;
	__auto_type handleref = (handleref_t *) hr;
	__auto_type layout = (VkDescriptorSetLayout) handleref->handle;
	__auto_type ctx = sctx->vctx;
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
	scriptctx_t *sctx = _ctx;
	__auto_type handleref = (handleref_t *) hr;
	__auto_type module = (VkShaderModule) handleref->handle;
	__auto_type ctx = sctx->vctx;
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
	scriptctx_t *sctx = _ctx;
	__auto_type handleref = (handleref_t *) hr;
	__auto_type layout = (VkPipelineLayout) handleref->handle;
	__auto_type ctx = sctx->vctx;
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
	scriptctx_t *sctx = _ctx;
	__auto_type handleref = (handleref_t *) hr;
	__auto_type pool = (VkDescriptorPool) handleref->handle;
	__auto_type ctx = sctx->vctx;
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
	scriptctx_t *sctx = _ctx;
	__auto_type handleref = (handleref_t *) hr;
	__auto_type sampler = (VkSampler) handleref->handle;
	__auto_type ctx = sctx->vctx;
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
	scriptctx_t *sctx = _ctx;
	__auto_type handleref = (handleref_t *) hr;
	__auto_type image = (VkImage) handleref->handle;
	__auto_type ctx = sctx->vctx;
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
	scriptctx_t *sctx = _ctx;
	__auto_type handleref = (handleref_t *) hr;
	__auto_type imageView = (VkImageView) handleref->handle;
	__auto_type ctx = sctx->vctx;
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
	scriptctx_t *sctx = _ctx;
	__auto_type handleref = (handleref_t *) hr;
	__auto_type renderpass = (VkRenderPass) handleref->handle;
	__auto_type ctx = sctx->vctx;
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;

	if (renderpass) {
		dfunc->vkDestroyRenderPass (device->dev, renderpass, 0);
	};
	handleref_free (handleref, ctx);
}

static hashtab_t *enum_symtab;

typedef struct data_array_s DARRAY_TYPE(byte) data_array_t;
static void
data_array (const exprval_t **params, exprval_t *result, exprctx_t *context)
{
	size_t      offset = 0;
	// params are in reverse order, but this works for calculating the size
	// of the buffer
	for (const exprval_t **param = params; *param; param++) {
		offset += (*param)->type->size;
	}
	__auto_type data = DARRAY_ALLOCFIXED_OBJ (data_array_t, offset,
											  cmemalloc, context->memsuper);
	for (const exprval_t **param = params; *param; param++) {
		size_t      size = (*param)->type->size;
		// pre-decrement offset because params are in reverse order
		offset -= size;
		memcpy (data->a + offset, (*param)->value, size);
	}
	*(data_array_t **) result->value = data;
}

static exprtype_t data_array_type = {
	.name = "array",
	.size = sizeof (data_array_t *),
	.binops = 0,
	.unops = 0,
};
static exprfunc_t data_array_func[] = {
	{ &data_array_type, -1, 0, data_array },
	{}
};
static exprsym_t data_array_symbols[] = {
	{ "array", &cexpr_function, data_array_func },
	{}
};
static exprtab_t data_array_symtab = {
	data_array_symbols,
};

static int
parse_specialization_data (const plitem_t *item, void **data,
						   plitem_t *messages, parsectx_t *pctx)
{
	size_t     *size_ptr = (size_t *) data[0];
	void      **data_ptr = (void **) data[1];

	if (PL_Type (item) == QFBinary) {
		const void *bindata = PL_BinaryData (item);
		size_t      binsize = PL_BinarySize (item);

		*data_ptr = vkparse_alloc (pctx, binsize);
		memcpy (*data_ptr, bindata, binsize);
		*size_ptr = binsize;
		return 1;
	}

	data_array_t *da= 0;
	exprctx_t   ectx = *pctx->ectx;
	exprval_t   result = { &data_array_type, &da };
	ectx.parent = pctx->ectx;
	ectx.symtab = &data_array_symtab;
	ectx.result = &result;
	ectx.item = item;
	const char *valstr = PL_String (item);
	//Sys_MaskPrintf (SYS_vulkan_parse,
	//				"parse_specialization_data: %s %zd %d %p %p %s\n",
	//				field->name, field->offset, field->type, field->parser,
	//				field->data, valstr);
	int         ret = !cexpr_eval_string (valstr, &ectx);
	if (!ret) {
		PL_Message (messages, item, "error parsing specialization data: %s",
					valstr);
	} else {
		*size_ptr = da->size;
		*data_ptr = da->a;
		//for (size_t i = 0; i < da->size; i++) {
		//	Sys_Printf (" %02x", da->a[i]);
		//}
		//Sys_Printf ("\n");
	}
	return ret;
}

static int
parse_task_function (const plitem_t *item, void **data,
					 plitem_t *messages, parsectx_t *pctx)
{
	qfv_renderctx_t *rctx = pctx->data;
	const char *fname = PL_String (item);
	exprsym_t  *fsym = Hash_Find (rctx->task_functions.tab, fname);

	if (!fsym) {
		PL_Message (messages, item, "undefined task function %s", fname);
		return 0;
	}
	if (fsym->type != &cexpr_function) {
		PL_Message (messages, item, "not a function type %s", fname);
		return 0;
	}
	exprfunc_t *func;
	for (func = fsym->value; func->func; func++) {
		if (!func->result) {
			break;
		}
	}
	if (!func->func) {
		PL_Message (messages, item, "%s does not have a void implementation",
					fname);
		return 0;
	}
	size_t      size = func->num_params * sizeof (exprval_t);
	size += func->num_params * sizeof (exprval_t *);
	size_t      base = size;
	for (int i = 0; i < func->num_params; i++) {
		exprtype_t *type = func->param_types[i];
		size = ((size + type->size - 1) & ~(type->size - 1));
		if (i == 0) {
			base = size;
		}
	}
	exprval_t **param_ptrs = vkparse_alloc (pctx, size);
	exprval_t  *params = (exprval_t *) &param_ptrs[func->num_params];
	byte       *param_data = (byte *) param_ptrs + base;
	memset (params, 0, size);
	size_t      offs = 0;
	for (int i = 0; i < func->num_params; i++) {
		exprtype_t *type = func->param_types[i];
		param_ptrs[i] = &params[i];
		params[i] = (exprval_t) {
			.type = type,
			.value = param_data + offs,
		};
		offs = ((offs + type->size - 1) & ~(type->size - 1));
		offs += type->size;
	}
	*(const char **) data[0] = vkstrdup (pctx, fname);
	*(exprfunc_t **) data[1] = func;
	*(exprval_t ***) data[2] = param_ptrs;
	*(void **) data[3] = param_data;
	return 1;
}

static int
parse_task_params (const plitem_t *item, void **data,
				   plitem_t *messages, parsectx_t *pctx)
{
	exprfunc_t *func = *(exprfunc_t **) data[0];
	exprval_t **params = *(exprval_t ***) data[1];
	if (!func) {
		PL_Message (messages, item, "task function not set");
		return 0;
	}
	if (PL_A_NumObjects (item) != func->num_params) {
		PL_Message (messages, item, "incorrect number of parameters");
		return 0;
	}
	for (int i = 0; i < func->num_params; i++) {
		const char *paramstr = PL_String (PL_ObjectAtIndex (item, i));
		exprval_t  *param = params[func->num_params - i - 1];
		exprctx_t   ectx = *pctx->ectx;
		if (param->type->data) {
			ectx.parent = pctx->ectx;
			ectx.symtab = ((exprenum_t *) param->type->data)->symtab;
		}
		// cexpr params are in reverse order
		ectx.result = param;

		if (cexpr_eval_string (paramstr, &ectx)) {
			PL_Message (messages, item, "error parsing param %d", i);
			return 0;
		}
	}
	return 1;
}

#include "libs/video/renderer/vulkan/vkparse.cinc"

static exprsym_t qfv_renderframeset_t_symbols[] = {
	{"size", &cexpr_size_t, (void *)field_offset (qfv_renderframeset_t, size)},
	{ }
};
static exprtab_t qfv_renderframeset_t_symtab = {
	qfv_renderframeset_t_symbols,
};
exprtype_t qfv_renderframeset_t_type = {
	.name = "frameset",
	.size = sizeof (qfv_renderframeset_t *),
	.binops = cexpr_struct_binops,
	.unops = 0,
	.data = &qfv_renderframeset_t_symtab,
};

static hashtab_t *
handlref_symtab (void (*free_func)(void*,void*), scriptctx_t *sctx)
{
	return Hash_NewTable (23, handleref_getkey, free_func, sctx,
						  &sctx->hashctx);
}

static const char *
enum_symtab_getkey (const void *e, void *unused)
{
	__auto_type enm = (const exprenum_t *) e;
	return enm->type->name;
}

static const char *
parser_getkey (const void *e, void *unused)
{
	__auto_type parser = (const parserref_t *) e;
	return parser->name;
}

static exprtab_t root_symtab = {
	.symbols = cexpr_lib_symbols,
};

static void
root_symtab_shutdown (void *data)
{
	Hash_DelTable (root_symtab.tab);
}

static void __attribute__((constructor))
root_symtab_init (void)
{
	// using a null hashctx here is safe because this function is run before
	// main and thus before any possibility of threading.
	exprctx_t root_context = { .symtab = &root_symtab };
	cexpr_init_symtab (&root_symtab, &root_context);
	Sys_RegisterShutdown (root_symtab_shutdown, 0);
}

exprenum_t *
QFV_GetEnum (const char *name)
{
	return Hash_Find (enum_symtab, name);
}

static int
parse_object (vulkan_ctx_t *ctx, memsuper_t *memsuper, plitem_t *plist,
			  plparser_t parser, void *object, plitem_t *properties)
{
	scriptctx_t *sctx = ctx->script_context;
	plitem_t   *messages = PL_NewArray ();
	exprctx_t   exprctx = { .symtab = &root_symtab };
	parsectx_t  parsectx = { &exprctx, ctx, properties };
	auto rctx = ctx->render_context;
	exprsym_t   var_syms[] = {
		{"output", &qfv_output_t_type, &sctx->output},
		{"frames", &qfv_renderframeset_t_type, &rctx->frames},
		{"msaaSamples", &VkSampleCountFlagBits_type, &ctx->msaaSamples},
		{"physDevLimits", &VkPhysicalDeviceLimits_type,
			&ctx->device->physDev->p.properties.limits },
		{QFV_PROPERTIES, &cexpr_plitem, &parsectx.properties},
		{}
	};
	exprtab_t   vars_tab = { var_syms, 0 };

	exprctx.external_variables = &vars_tab;
	exprctx.messages = messages;
	exprctx.hashctx = &sctx->hashctx;
	exprctx.memsuper = memsuper;

	cexpr_init_symtab (&vars_tab, &exprctx);


	if (!parser (0, plist, object, messages, &parsectx)) {
		for (int i = 0; i < PL_A_NumObjects (messages); i++) {
			Sys_Printf ("%s\n", PL_String (PL_ObjectAtIndex (messages, i)));
		}
		return 0;
	}
	Hash_DelTable (vars_tab.tab);
	PL_Release (messages);

	return 1;
}

typedef struct {
	uint32_t    count;
	VkImageCreateInfo *info;
} imagecreate_t;

typedef struct {
	uint32_t    count;
	VkImageViewCreateInfo *info;
} imageviewcreate_t;

int
QFV_ParseOutput (vulkan_ctx_t *ctx, qfv_output_t *output, plitem_t *plist,
				 plitem_t *properties)
{
	memsuper_t *memsuper = new_memsuper ();
	int         ret = 0;
	qfv_output_t op = {};

	if (parse_object (ctx, memsuper, plist, parse_qfv_output_t, &op,
					  properties)) {
		memcpy (output, &op, sizeof (*output));
		ret = 1;
	}
	delete_memsuper (memsuper);
	return ret;
}

int vulkan_frame_count;
static cvar_t vulkan_frame_count_cvar = {
	.name = "vulkan_frame_count",
	.description =
		"Number of frames to render in the background. More frames can "
		"increase performance, but at the cost of latency. The default of 2 is"
		" recommended.",
	.default_value = "2",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &vulkan_frame_count },
};
int vulkan_presentation_mode;
static cvar_t vulkan_presentation_mode_cvar = {
	.name = "vulkan_presentation_mode",
	.description =
		"desired presentation mode (may fall back to fifo).",
	.default_value = "mailbox",
	.flags = CVAR_NONE,
	.value = {
		.type = &VkPresentModeKHR_type,
		.value = &vulkan_presentation_mode,
	},
};
int msaaSamples;
static cvar_t msaaSamples_cvar = {
	.name = "msaaSamples",
	.description =
		"desired MSAA sample size.",
	.default_value = "VK_SAMPLE_COUNT_1_BIT",
	.flags = CVAR_NONE,
	.value = { .type = &VkSampleCountFlagBits_type, .value = &msaaSamples },
};
static exprenum_t validation_enum;
static exprtype_t validation_type = {
	.name = "vulkan_use_validation",
	.size = sizeof (int),
	.binops = cexpr_flag_binops,
	.unops = cexpr_flag_unops,
	.data = &validation_enum,
	.get_string = cexpr_flags_get_string,
};

static int validation_values[] = {
	0,
	VK_DEBUG_UTILS_MESSAGE_SEVERITY_FLAG_BITS_MAX_ENUM_EXT,
};
static exprsym_t validation_symbols[] = {
	{"none", &validation_type, validation_values + 0},
	{"all", &validation_type, validation_values + 1},
	{}
};
static exprtab_t validation_symtab = {
	validation_symbols,
};
static exprenum_t validation_enum = {
	&validation_type,
	&validation_symtab,
};
static cvar_t vulkan_use_validation_cvar = {
	.name = "vulkan_use_validation",
	.description =
		"enable KRONOS Validation Layer if available (requires instance "
		"restart).",
	.default_value = "error|warning",
	.flags = CVAR_NONE,
	.value = { .type = &validation_type, .value = &vulkan_use_validation },
};

static void
vulkan_frame_count_f (void *data, const cvar_t *cvar)
{
	if (vulkan_frame_count < 1) {
		Sys_Printf ("Invalid frame count: %d. Setting to 1\n",
					vulkan_frame_count);
		vulkan_frame_count = 1;
	}
}

void
Vulkan_Init_Cvars (void)
{
	qfZoneScoped (true);
	int         num_syms = 0;
	for (exprsym_t *sym = VkDebugUtilsMessageSeverityFlagBitsEXT_symbols;
		 sym->name; sym++, num_syms++) {
	}
	for (exprsym_t *sym = validation_symbols; sym->name; sym++, num_syms++) {
	}
	validation_symtab.symbols = calloc (num_syms + 1, sizeof (exprsym_t));
	num_syms = 0;
	for (exprsym_t *sym = VkDebugUtilsMessageSeverityFlagBitsEXT_symbols;
		 sym->name; sym++, num_syms++) {
		validation_symtab.symbols[num_syms] = *sym;
		validation_symtab.symbols[num_syms].type = &validation_type;
	}
	for (exprsym_t *sym = validation_symbols; sym->name; sym++, num_syms++) {
		validation_symtab.symbols[num_syms] = *sym;
	}
	Cvar_Register (&vulkan_use_validation_cvar, 0, 0);
	// FIXME implement fallback choices (instead of just fifo)
	Cvar_Register (&vulkan_presentation_mode_cvar, 0, 0);
	Cvar_Register (&vulkan_frame_count_cvar, vulkan_frame_count_f, 0);
	Cvar_Register (&msaaSamples_cvar, 0, 0);
}

static exprsym_t builtin_plist_syms[] = {
	{ .name = "main_def",
	  .value = (void *)
#include "libs/video/renderer/vulkan/rp_main_def.plc"
		},
	{ .name = "main_fwd",
	  .value = (void *)
#include "libs/video/renderer/vulkan/rp_main_fwd.plc"
		},
	{ .name = "smp_quake",
	  .value = (void *)
#include "libs/video/renderer/vulkan/smp_quake.plc"
		},
	{}
};
static plitem_t **builtin_plists;
static exprtab_t builtin_configs = { .symbols = builtin_plist_syms };

static void
build_configs (scriptctx_t *sctx)
{
	qfZoneScoped (true);
	int         num_plists = 0;
	for (exprsym_t *sym = builtin_plist_syms; sym->name; sym++) {
		num_plists++;
	}
	builtin_plists = malloc (num_plists * sizeof (plitem_t *));
	num_plists = 0;
	for (exprsym_t *sym = builtin_plist_syms; sym->name; sym++) {
		plitem_t   *item = PL_GetDictionary (sym->value, &sctx->hashctx);
		if (!item) {
			// Syntax errors in the compiled-in plists are unrecoverable
			Sys_Error ("Error parsing plist for %s", sym->name);
		}
		builtin_plists[num_plists] = item;
		sym->value = &builtin_plists[num_plists];
		sym->type = &cexpr_plitem;
		num_plists++;
	}
	exprctx_t   ectx = { .hashctx = &sctx->hashctx };
	cexpr_init_symtab (&builtin_configs, &ectx);
}

static void
delete_configs (void)
{
	int         num_plists = 0;
	if (builtin_plists) {
		for (exprsym_t *sym = builtin_plist_syms; sym->name; sym++) {
			PL_Release (builtin_plists[num_plists]);
			num_plists++;
		}
		free (builtin_plists);
	}
	Hash_DelTable (builtin_configs.tab);
}

plitem_t *
Vulkan_GetConfig (vulkan_ctx_t *ctx, const char *name)
{
	qfZoneScoped (true);
	scriptctx_t *sctx = ctx->script_context;
	if (!builtin_configs.tab) {
		build_configs (sctx);
	}

	plitem_t   *config = 0;
	exprval_t   result = { .type = &cexpr_plitem, .value = &config };
	exprctx_t   ectx = {
		.result = &result,
		.symtab = &builtin_configs,
		.memsuper = new_memsuper (),
		.hashctx = &sctx->hashctx,
		.messages = PL_NewArray (),
	};
	if (cexpr_eval_string (name, &ectx)) {
		dstring_t  *msg = dstring_newstr ();

		for (int i = 0; i < PL_A_NumObjects (ectx.messages); i++) {
			dasprintf (msg, "%s\n",
					   PL_String (PL_ObjectAtIndex (ectx.messages, i)));
		}
		Sys_Printf ("%s", msg->str);
		dstring_delete (msg);
		config = 0;
	}
	PL_Release (ectx.messages);
	delete_memsuper (ectx.memsuper);
	return config;
}

void Vulkan_Script_Init (vulkan_ctx_t *ctx)
{
	qfZoneScoped (true);
	scriptctx_t *sctx = calloc (1, sizeof (scriptctx_t));
	sctx->vctx = ctx;
	ctx->script_context = sctx;

	exprctx_t   ectx = {};
	enum_symtab = Hash_NewTable (61, enum_symtab_getkey, 0, 0, &sctx->hashctx);
	parser_table = Hash_NewTable (61, parser_getkey, 0, 0, &sctx->hashctx);
	//FIXME using the script context's hashctx causes the cvar system to access
	//freed hash links due to shutdown order issues. The proper fix would be to
	//create a symtabs shutdown (for thread safety), but this works for now.
	ectx.hashctx = 0;//&sctx->hashctx;
	vkgen_init_symtabs (&ectx);
	cexpr_init_symtab (&qfv_renderframeset_t_symtab, &ectx);
	cexpr_init_symtab (&data_array_symtab, &ectx);

	sctx->shaderModules = handlref_symtab (shaderModule_free, sctx);
	sctx->setLayouts = handlref_symtab (setLayout_free, sctx);
	sctx->pipelineLayouts = handlref_symtab (pipelineLayout_free, sctx);
	sctx->descriptorPools = handlref_symtab (descriptorPool_free, sctx);
	sctx->samplers = handlref_symtab (sampler_free, sctx);
	sctx->images = handlref_symtab (image_free, sctx);
	sctx->imageViews = handlref_symtab (imageView_free, sctx);
	sctx->renderpasses = handlref_symtab (renderpass_free, sctx);
}

static void
clear_table (hashtab_t **table)
{
	if (*table) {
		hashtab_t  *tab = *table;
		*table = 0;
		Hash_DelTable (tab);
	}
}

void Vulkan_Script_Shutdown (vulkan_ctx_t *ctx)
{
	scriptctx_t *sctx = ctx->script_context;

	clear_table (&sctx->pipelineLayouts);
	clear_table (&sctx->setLayouts);
	clear_table (&sctx->shaderModules);
	clear_table (&sctx->descriptorPools);
	clear_table (&sctx->samplers);
	clear_table (&sctx->images);
	clear_table (&sctx->imageViews);
	clear_table (&sctx->renderpasses);

	delete_configs ();

	exprctx_t   ectx = {};
	vkgen_shutdown_symtabs (&ectx);

	Hash_DelTable (qfv_renderframeset_t_symtab.tab);
	Hash_DelTable (data_array_symtab.tab);

	free (validation_symtab.symbols);

	Hash_DelTable (enum_symtab);
	Hash_DelTable (parser_table);

	Hash_DelContext (sctx->hashctx);

	free (sctx);
}

void Vulkan_Script_SetOutput (vulkan_ctx_t *ctx, qfv_output_t *output)
{
	scriptctx_t *sctx = ctx->script_context;
	sctx->output = *output;
}

exprtab_t *
QFV_CreateSymtab (plitem_t *dict, const char *properties,
				  const char **extra_items, exprsym_t *extra_syms,
				  exprctx_t *ectx)
{
	qfZoneScoped (true);
	plitem_t   *props = PL_ObjectForKey (dict, properties);
	int         num_keys = PL_D_NumKeys (props);
	int         num_extra = 0;
	int         num_syms = 0;

	if (extra_items) {
		for (const char **e = extra_items; *e; e++) {
			if (PL_ObjectForKey (dict, *e)) {
				num_extra++;
			}
		}
	}
	if (extra_syms) {
		for (exprsym_t *sym = extra_syms; sym->name; sym++, num_syms++) { }
	}

	int         total_items = num_keys + num_extra;
	int         total_syms = total_items + num_syms;
	size_t      size = (sizeof (exprtab_t)
						+ (total_syms + 1) * sizeof (exprsym_t)
						+ total_items * sizeof (plitem_t *));
	exprtab_t  *symtab = malloc (size);
	*symtab = (exprtab_t) {
		.symbols = (exprsym_t *) &symtab[1],
	};
	plitem_t  **items = (plitem_t **) &symtab->symbols[total_syms + 1];
	for (int i = 0; i < num_keys; i++) {
		symtab->symbols[i] = (exprsym_t) {
			.name = PL_KeyAtIndex (props, i),
			.type = &cexpr_plitem,
			.value = items + i,
		};
		items[i] = PL_ObjectForKey (props, symtab->symbols[i].name);
	}
	for (int i = 0, j = 0; num_extra && extra_items[i]; i++) {
		plitem_t   *val = PL_ObjectForKey (dict, extra_items[i]);
		if (val) {
			symtab->symbols[num_keys + j] = (exprsym_t) {
				.name = extra_items[i],
				.type = &cexpr_plitem,
				.value = items + num_keys + j,
			};
			items[num_keys + j] = val;
			j++;
		}
	}
	for (int i = 0; i < num_syms; i++) {
		symtab->symbols[total_items + i] = extra_syms[i];
	}
	symtab->symbols[total_syms] = (exprsym_t) { };

	cexpr_init_symtab (symtab, ectx);
	return symtab;
}

void
QFV_DestroySymtab (exprtab_t *tab)
{
	Hash_DelTable (tab->tab);
	free (tab);
}

struct qfv_jobinfo_s *
QFV_ParseJobInfo (vulkan_ctx_t *ctx, plitem_t *item, qfv_renderctx_t *rctx)
{
	qfZoneScoped (true);
	memsuper_t *memsuper = new_memsuper ();
	qfv_jobinfo_t *ji = cmemalloc (memsuper, sizeof (qfv_jobinfo_t));
	*ji = (qfv_jobinfo_t) { .memsuper = memsuper };

	scriptctx_t *sctx = ctx->script_context;
	plitem_t   *messages = PL_NewArray ();

	exprctx_t   exprctx = {
		.symtab = &root_symtab,
		.messages = messages,
		.hashctx = &sctx->hashctx,
		.memsuper = memsuper,
	};
	parsectx_t  parsectx = {
		.ectx = &exprctx,
		.vctx = ctx,
		.data = rctx,
	};

	static const char *extra_items[] = {
		"images",
		"imageviews",
		"renderpasses",
		0
	};
	exprsym_t   var_syms[] = {
		{"render_output", &qfv_output_t_type, &sctx->output},
		{"frames", &qfv_renderframeset_t_type, &rctx->frames},
		{"msaaSamples", &VkSampleCountFlagBits_type, &ctx->msaaSamples},
		{"physDevLimits", &VkPhysicalDeviceLimits_type,
			&ctx->device->physDev->p.properties.limits },
		{}
	};
	exprctx.external_variables = QFV_CreateSymtab (item, "properties",
												   extra_items, var_syms,
												   &exprctx);

	int         ret;
	if (!(ret = parse_qfv_jobinfo_t (0, item, ji, messages, &parsectx))) {
		for (int i = 0; i < PL_A_NumObjects (messages); i++) {
			Sys_Printf ("%s\n", PL_String (PL_ObjectAtIndex (messages, i)));
		}
	}
	QFV_DestroySymtab (exprctx.external_variables);
	PL_Release (messages);
	if (!ret) {
		delete_memsuper (memsuper);
		ji = 0;
	}

	return ji;
}

struct qfv_samplerinfo_s *
QFV_ParseSamplerInfo (vulkan_ctx_t *ctx, plitem_t *item, qfv_renderctx_t *rctx)
{
	memsuper_t *memsuper = new_memsuper ();
	qfv_samplerinfo_t *si = cmemalloc (memsuper, sizeof (qfv_samplerinfo_t));
	*si = (qfv_samplerinfo_t) { .memsuper = memsuper };

	scriptctx_t *sctx = ctx->script_context;
	plitem_t   *messages = PL_NewArray ();

	exprctx_t   exprctx = {
		.symtab = &root_symtab,
		.messages = messages,
		.hashctx = &sctx->hashctx,
		.memsuper = memsuper,
	};
	parsectx_t  parsectx = {
		.ectx = &exprctx,
		.vctx = ctx,
		.data = rctx,
	};

	exprsym_t   var_syms[] = {
		{"physDevLimits", &VkPhysicalDeviceLimits_type,
			&ctx->device->physDev->p.properties.limits },
		{}
	};
	exprctx.external_variables = QFV_CreateSymtab (item, "properties",
												   0, var_syms, &exprctx);

	int         ret;
	if (!(ret = parse_qfv_samplerinfo_t (0, item, si, messages, &parsectx))) {
		for (int i = 0; i < PL_A_NumObjects (messages); i++) {
			Sys_Printf ("%s\n", PL_String (PL_ObjectAtIndex (messages, i)));
		}
	}
	QFV_DestroySymtab (exprctx.external_variables);
	PL_Release (messages);
	if (!ret) {
		delete_memsuper (memsuper);
		si = 0;
	}

	return si;
}

int
QFV_ParseLayoutInfo (vulkan_ctx_t *ctx, memsuper_t *memsuper,
					 exprtab_t *symtab, const char *ref,
					 qfv_layoutinfo_t *layout)
{
	*layout = (qfv_layoutinfo_t) {};

	scriptctx_t *sctx = ctx->script_context;
	plitem_t   *messages = PL_NewArray ();

	exprctx_t   exprctx = {
		.symtab = &root_symtab,
		.messages = messages,
		.hashctx = &sctx->hashctx,
		.memsuper = memsuper,
		.external_variables = symtab,
	};
	parsectx_t  parsectx = {
		.ectx = &exprctx,
		.vctx = ctx,
	};

	plitem_t   *item = PL_NewString (ref);
	int         ret;
	if (!(ret = parse_qfv_layoutinfo_t (0, item, layout, messages,
										&parsectx))) {
		for (int i = 0; i < PL_A_NumObjects (messages); i++) {
			Sys_Printf ("%s\n", PL_String (PL_ObjectAtIndex (messages, i)));
		}
	}
	PL_Release (messages);
	PL_Release (item);

	return ret;
}
