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
#include "QF/Vulkan/shader.h"

#include "QF/Vulkan/qf_renderpass.h"

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
	size_t      value_offset;
} parse_single_t;

typedef struct parse_array_s {
	pltype_t    type;
	size_t      stride;
	plparser_t  parser;
	size_t      value_offset;
	size_t      size_offset;
} parse_array_t;

typedef struct parse_fixed_array_s {
	pltype_t    type;
	size_t      stride;
	plparser_t  parser;
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
	// use size_t (and cexpr_size_t) for val so references to array sizes
	// can be used
	size_t      val = 0;
	parsectx_t *pctx = context;
	exprval_t   result = { &cexpr_size_t, &val };
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

	plfield_t   f = { 0, 0, single->type, single->parser, 0 };
	void       *value = vkparse_alloc (context, single->stride);
	memset (value, 0, single->stride);
	if (!single->parser (&f, item, value, messages, context)) {
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
		vkparse_alloc,
		array->parser,
		0,
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

static int
parse_fixed_array (const plfield_t *field, const plitem_t *item,
				   void *data, plitem_t *messages, void *context)
{
	__auto_type array = (parse_fixed_array_t *) field->data;

	plelement_t element = {
		array->type,
		array->stride,
		vkparse_alloc,
		array->parser,
		0,
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

	size_t      len = strlen (str) + 1;
	*value = vkparse_alloc (context, len);
	memcpy (*value, str, len);
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
	Sys_MaskPrintf (SYS_vulkan_parse, "parse_inherit: %s\n", inheritstr);
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
	Sys_MaskPrintf (SYS_vulkan_parse, "parse_RGBA: %s\n", valstr);
	ret = !cexpr_eval_string (valstr, &ectx);
	Sys_MaskPrintf (SYS_vulkan_parse, "    "VEC4F_FMT"\n",
					VEC4_EXP (*(vec4f_t *)data[0]));
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

static const char *
resource_path (vulkan_ctx_t *ctx, const char *prefix, const char *name)
{
	if (name[0] != '$') {
		if (prefix) {
			name = va (ctx->va_ctx, "$"QFV_PROPERTIES".%s.%s", prefix, name);
		} else {
			name = va (ctx->va_ctx, "$"QFV_PROPERTIES".%s", name);
		}
	}
	return name;
}

static int
parse_VkRenderPass (const plitem_t *item, void **data,
					plitem_t *messages, parsectx_t *pctx)
{
	__auto_type handle = (VkRenderPass *) data[0];
	int         ret = 1;
	exprctx_t   ectx = *pctx->ectx;
	vulkan_ctx_t *ctx = pctx->vctx;
	scriptctx_t *sctx = ctx->script_context;

	const char *name = PL_String (item);
	const char *path = resource_path (ctx, 0, name);
	Sys_MaskPrintf (SYS_vulkan_parse, "parse_VkRenderPass: %s\n", path);

	*handle = (VkRenderPass) QFV_GetHandle (sctx->renderpasses, path);
	if (*handle) {
		return 1;
	}

	plitem_t   *setItem = 0;
	exprval_t   result = { &cexpr_plitem, &setItem };
	ectx.result = &result;
	ectx.item = item;
	ret = !cexpr_eval_string (path, &ectx);
	if (ret) {
		VkRenderPass setLayout;
		setLayout = QFV_ParseRenderPass (ctx, setItem, pctx->properties);
		*handle = (VkRenderPass) setLayout;

		// path not guaranteed to survive cexpr_eval_string due to va
		path = resource_path (ctx, 0, name);
		QFV_AddHandle (sctx->renderpasses, path, (uint64_t) setLayout);
	}
	return ret;
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

static int
parse_VkDescriptorSetLayout (const plfield_t *field, const plitem_t *item,
							 void *data, plitem_t *messages, void *context)
{
	__auto_type handle = (VkDescriptorSetLayout *) data;
	int         ret = 1;
	parsectx_t *pctx = context;
	exprctx_t   ectx = *pctx->ectx;
	vulkan_ctx_t *ctx = pctx->vctx;
	scriptctx_t *sctx = ctx->script_context;

	const char *name = PL_String (item);
	const char *path = resource_path (ctx, "setLayouts", name);
	Sys_MaskPrintf (SYS_vulkan_parse, "parse_VkDescriptorSetLayout: %s\n",
					path);

	*handle = (VkDescriptorSetLayout) QFV_GetHandle (sctx->setLayouts, path);
	if (*handle) {
		return 1;
	}

	plitem_t   *setItem = 0;
	exprval_t   result = { &cexpr_plitem, &setItem };
	ectx.result = &result;
	ectx.item = item;
	ret = !cexpr_eval_string (path, &ectx);
	if (ret) {
		VkDescriptorSetLayout setLayout;
		setLayout = QFV_ParseDescriptorSetLayout (ctx, setItem,
												  pctx->properties);
		*handle = (VkDescriptorSetLayout) setLayout;

		// path not guaranteed to survive cexpr_eval_string due to va
		path = resource_path (ctx, "setLayouts", name);
		QFV_AddHandle (sctx->setLayouts, path, (uint64_t) setLayout);
	}
	return ret;
}

static int
parse_VkPipelineLayout (const plitem_t *item, void **data,
						plitem_t *messages, parsectx_t *pctx)
{
	__auto_type handle = (VkPipelineLayout *) data[0];
	int         ret = 1;
	exprctx_t   ectx = *pctx->ectx;
	vulkan_ctx_t *ctx = pctx->vctx;
	scriptctx_t *sctx = ctx->script_context;

	const char *name = PL_String (item);
	const char *path = resource_path (ctx, "pipelineLayouts", name);
	Sys_MaskPrintf (SYS_vulkan_parse, "parse_VkPipelineLayout: %s\n", path);

	*handle = (VkPipelineLayout) QFV_GetHandle (sctx->pipelineLayouts, path);
	if (*handle) {
		return 1;
	}

	plitem_t   *setItem = 0;
	exprval_t   result = { &cexpr_plitem, &setItem };
	ectx.result = &result;
	ectx.item = item;
	ret = !cexpr_eval_string (path, &ectx);
	if (ret) {
		VkPipelineLayout layout;
		layout = QFV_ParsePipelineLayout (ctx, setItem, pctx->properties);
		*handle = (VkPipelineLayout) layout;

		// path not guaranteed to survive cexpr_eval_string due to va
		path = resource_path (ctx, "pipelineLayouts", name);
		QFV_AddHandle (sctx->pipelineLayouts, path, (uint64_t) layout);
	}
	return ret;
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

static int
parse_VkImage (const plitem_t *item, void **data, plitem_t *messages,
			   parsectx_t *pctx)
{
	__auto_type handle = (VkImage *) data[0];
	int         ret = 1;
	exprctx_t   ectx = *pctx->ectx;
	vulkan_ctx_t *ctx = pctx->vctx;
	scriptctx_t *sctx = ctx->script_context;

	const char *name = PL_String (item);
	const char *path = resource_path (ctx, "images", name);
	Sys_MaskPrintf (SYS_vulkan_parse, "parse_VkImage: %s\n", path);

	*handle = (VkImage) QFV_GetHandle (sctx->images, path);
	if (*handle) {
		return 1;
	}

	exprval_t   result = { };
	ectx.result = &result;
	ectx.item = item;
	ret = !cexpr_eval_string (path, &ectx);
	if (ret) {
		if (result.type == &cexpr_plitem) {
			plitem_t   *imageItem = *(plitem_t **) result.value;
			VkImage image;
			image = QFV_ParseImage (ctx, imageItem, pctx->properties);
			*handle = (VkImage) image;

			// path not guaranteed to survive cexpr_eval_string due to va
			path = resource_path (ctx, "images", name);
			QFV_AddHandle (sctx->images, path, (uint64_t) image);
		} else if (result.type == &VkImage_type) {
			*handle = *(VkImage *) result.value;
		} else {
			ret = 0;
		}
	}
	return ret;
}

static int
parse_VkImageView (const plfield_t *field, const plitem_t *item, void *data,
				   plitem_t *messages, void *context)
{
	parsectx_t *pctx = context;
	__auto_type handle = (VkImageView *) data;
	int         ret = 1;
	exprctx_t   ectx = *pctx->ectx;
	vulkan_ctx_t *ctx = pctx->vctx;
	scriptctx_t *sctx = ctx->script_context;

	const char *name = PL_String (item);
	const char *path = resource_path (ctx, "imageViews", name);
	Sys_MaskPrintf (SYS_vulkan_parse, "parse_VkImageView: %s\n", path);

	*handle = (VkImageView) QFV_GetHandle (sctx->imageViews, path);
	if (*handle) {
		return 1;
	}

	exprval_t  *value = 0;
	exprval_t   result = { &cexpr_exprval, &value };
	ectx.result = &result;
	ectx.item = item;
	ret = !cexpr_eval_string (path, &ectx);

	plitem_t   *imageViewItem = 0;
	if (ret) {
		VkImageView imageView;
		if (value->type == &VkImageView_type) {
			imageView = *(VkImageView *) value->value;
		} else if (value->type == &cexpr_plitem) {
			imageView = QFV_ParseImageView (ctx, imageViewItem,
											pctx->properties);
			// path not guaranteed to survive cexpr_eval_string due to va
			path = resource_path (ctx, "imageViews", name);
			QFV_AddHandle (sctx->imageViews, path, (uint64_t) imageView);
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

static int
parse_BasePipeline (const plitem_t *item, void **data,
					plitem_t *messages, parsectx_t *pctx)
{
	*(VkPipeline *) data = 0;
	PL_Message (messages, item, "not implemented");
	return 0;
}

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

#include "libs/video/renderer/vulkan/vkparse.cinc"

static exprsym_t vulkan_frameset_t_symbols[] = {
	{"size", &cexpr_size_t, (void *)field_offset (vulkan_frameset_t, size)},
	{ }
};
static exprtab_t vulkan_frameset_t_symtab = {
	vulkan_frameset_t_symbols,
};
exprtype_t vulkan_frameset_t_type = {
	.name = "frameset",
	.size = sizeof (vulkan_frameset_t *),
	.binops = cexpr_struct_binops,
	.unops = 0,
	.data = &vulkan_frameset_t_symtab,
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
static void __attribute__((constructor))
root_symtab_init (void)
{
	// using a null hashctx here is safe because this function is run before
	// main and thus before any possibility of threading.
	exprctx_t root_context = { .symtab = &root_symtab };
	cexpr_init_symtab (&root_symtab, &root_context);
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
	exprsym_t   var_syms[] = {
		{"output", &qfv_output_t_type, &sctx->output},
		{"frames", &vulkan_frameset_t_type, &ctx->frames},
		{"msaaSamples", &VkSampleCountFlagBits_type, &ctx->msaaSamples},
		{"physDevLimits", &VkPhysicalDeviceLimits_type,
			&ctx->device->physDev->properties->limits },
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
	PL_Free (messages);

	return 1;
}

VkRenderPass
QFV_ParseRenderPass (vulkan_ctx_t *ctx, plitem_t *plist, plitem_t *properties)
{
	memsuper_t *memsuper = new_memsuper ();
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;

	VkRenderPassCreateInfo cInfo = {};

	if (!parse_object (ctx, memsuper, plist, parse_VkRenderPassCreateInfo,
					   &cInfo, properties)) {
		delete_memsuper (memsuper);
		return 0;
	}

	VkRenderPass renderpass;
	qfvPushDebug (ctx, va (ctx->va_ctx, "QFV_ParseRenderPass: %d",
				  PL_Line (plist)));
	dfunc->vkCreateRenderPass (device->dev, &cInfo, 0, &renderpass);
	qfvPopDebug (ctx);

	delete_memsuper (memsuper);
	return renderpass;
}

VkPipeline
QFV_ParseComputePipeline (vulkan_ctx_t *ctx, plitem_t *plist,
						  plitem_t *properties)
{
	memsuper_t *memsuper = new_memsuper ();
	qfv_device_t *device = ctx->device;

	__auto_type cInfo = QFV_AllocComputePipelineCreateInfoSet (1, alloca);
	memset (&cInfo->a[0], 0, sizeof (cInfo->a[0]));

	if (!parse_object (ctx, memsuper, plist, parse_VkComputePipelineCreateInfo,
					   &cInfo->a[0], properties)) {
		delete_memsuper (memsuper);
		return 0;
	}

	qfvPushDebug (ctx, va (ctx->va_ctx,
						   "QFV_ParseComputePipeline: %d", PL_Line (plist)));

	__auto_type plSet = QFV_CreateComputePipelines (device, 0, cInfo);
	qfvPopDebug (ctx);
	VkPipeline pipeline = plSet->a[0];
	free (plSet);
	delete_memsuper (memsuper);
	return pipeline;
}

VkPipeline
QFV_ParseGraphicsPipeline (vulkan_ctx_t *ctx, plitem_t *plist,
						   plitem_t *properties)
{
	memsuper_t *memsuper = new_memsuper ();
	qfv_device_t *device = ctx->device;

	__auto_type cInfo = QFV_AllocGraphicsPipelineCreateInfoSet (1, alloca);
	memset (&cInfo->a[0], 0, sizeof (cInfo->a[0]));

	if (!parse_object (ctx, memsuper, plist, parse_VkGraphicsPipelineCreateInfo,
					   &cInfo->a[0], properties)) {
		delete_memsuper (memsuper);
		return 0;
	}

	qfvPushDebug (ctx, va (ctx->va_ctx,
						   "QFV_ParsePipeline: %d", PL_Line (plist)));

	__auto_type plSet = QFV_CreateGraphicsPipelines (device, 0, cInfo);
	qfvPopDebug (ctx);
	VkPipeline pipeline = plSet->a[0];
	free (plSet);
	delete_memsuper (memsuper);
	return pipeline;
}

VkDescriptorPool
QFV_ParseDescriptorPool (vulkan_ctx_t *ctx, plitem_t *plist,
						 plitem_t *properties)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	memsuper_t *memsuper = new_memsuper ();

	VkDescriptorPoolCreateInfo cInfo = {};

	if (!parse_object (ctx, memsuper, plist, parse_VkDescriptorPoolCreateInfo,
					   &cInfo, properties)) {
		delete_memsuper (memsuper);
		return 0;
	}

	VkDescriptorPool pool;
	qfvPushDebug (ctx, va (ctx->va_ctx, "QFV_ParseDescriptorPool: %d", PL_Line (plist)));
	dfunc->vkCreateDescriptorPool (device->dev, &cInfo, 0, &pool);
	qfvPopDebug (ctx);

	delete_memsuper (memsuper);
	return pool;
}

VkDescriptorSetLayout
QFV_ParseDescriptorSetLayout (vulkan_ctx_t *ctx, plitem_t *plist,
							  plitem_t *properties)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	memsuper_t *memsuper = new_memsuper ();

	VkDescriptorSetLayoutCreateInfo cInfo = {};

	if (!parse_object (ctx, memsuper, plist,
					   parse_VkDescriptorSetLayoutCreateInfo,
					   &cInfo, properties)) {
		delete_memsuper (memsuper);
		return 0;
	}

	VkDescriptorSetLayout setLayout;
	qfvPushDebug (ctx, va (ctx->va_ctx, "QFV_ParseDescriptorSetLayout: %d", PL_Line (plist)));
	dfunc->vkCreateDescriptorSetLayout (device->dev, &cInfo, 0, &setLayout);
	QFV_duSetObjectName (device, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,
						 setLayout, va (ctx->va_ctx, "descriptorSetLayout:%d",
										PL_Line (plist)));
	qfvPopDebug (ctx);

	delete_memsuper (memsuper);
	return setLayout;
}

VkPipelineLayout
QFV_ParsePipelineLayout (vulkan_ctx_t *ctx, plitem_t *plist,
						 plitem_t *properties)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	memsuper_t *memsuper = new_memsuper ();

	VkPipelineLayoutCreateInfo cInfo = {};

	if (!parse_object (ctx, memsuper, plist, parse_VkPipelineLayoutCreateInfo,
					   &cInfo, properties)) {
		delete_memsuper (memsuper);
		return 0;
	}

	VkPipelineLayout layout;
	qfvPushDebug (ctx, va (ctx->va_ctx, "QFV_ParsePipelineLayout: %d", PL_Line (plist)));
	dfunc->vkCreatePipelineLayout (device->dev, &cInfo, 0, &layout);
	QFV_duSetObjectName (device, VK_OBJECT_TYPE_PIPELINE_LAYOUT,
						 layout, va (ctx->va_ctx, "pipelineLayout:%d",
										PL_Line (plist)));
	qfvPopDebug (ctx);

	delete_memsuper (memsuper);
	return layout;
}

VkSampler
QFV_ParseSampler (vulkan_ctx_t *ctx, plitem_t *plist, plitem_t *properties)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	memsuper_t *memsuper = new_memsuper ();

	VkSamplerCreateInfo cInfo = {};

	if (!parse_object (ctx, memsuper, plist, parse_VkSamplerCreateInfo, &cInfo,
					   properties)) {
		delete_memsuper (memsuper);
		return 0;
	}

	VkSampler sampler;
	qfvPushDebug (ctx, va (ctx->va_ctx, "QFV_ParseSampler: %d", PL_Line (plist)));
	dfunc->vkCreateSampler (device->dev, &cInfo, 0, &sampler);
	qfvPopDebug (ctx);

	delete_memsuper (memsuper);
	return sampler;
}

VkImage
QFV_ParseImage (vulkan_ctx_t *ctx, plitem_t *plist, plitem_t *properties)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	memsuper_t *memsuper = new_memsuper ();

	VkImageCreateInfo cInfo = {};

	if (!parse_object (ctx, memsuper, plist, parse_VkImageCreateInfo, &cInfo,
					   properties)) {
		delete_memsuper (memsuper);
		return 0;
	}

	VkImage image;
	qfvPushDebug (ctx, va (ctx->va_ctx, "QFV_ParseImage: %d", PL_Line (plist)));
	dfunc->vkCreateImage (device->dev, &cInfo, 0, &image);
	qfvPopDebug (ctx);

	delete_memsuper (memsuper);
	return image;
}

VkImageView
QFV_ParseImageView (vulkan_ctx_t *ctx, plitem_t *plist, plitem_t *properties)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	memsuper_t *memsuper = new_memsuper ();

	VkImageViewCreateInfo cInfo = {};

	if (!parse_object (ctx, memsuper, plist, parse_VkImageViewCreateInfo,
					   &cInfo, properties)) {
		delete_memsuper (memsuper);
		return 0;
	}

	VkImageView imageView;
	qfvPushDebug (ctx, va (ctx->va_ctx, "QFV_ParseImageView: %d", PL_Line (plist)));
	dfunc->vkCreateImageView (device->dev, &cInfo, 0, &imageView);
	qfvPopDebug (ctx);

	delete_memsuper (memsuper);
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
	vkparse_alloc,
	parse_VkImageCreateInfo,
};

static plelement_t qfv_imageviewcreate_dict = {
	QFDictionary,
	sizeof (VkImageViewCreateInfo),
	vkparse_alloc,
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
	scriptctx_t *sctx = ctx->script_context;
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	memsuper_t *memsuper = new_memsuper ();

	imagecreate_t create = {};

	pltype_t    type = PL_Type (item);

	if (type == QFDictionary) {
		if (!parse_object (ctx, memsuper, item, parse_imagecreate_dict,
						   &create, properties)) {
			delete_memsuper (memsuper);
			return 0;
		}
	} else {
		Sys_MaskPrintf (SYS_vulkan_parse, "Neither array nor dictionary: %d\n",
						PL_Line (item));
		delete_memsuper (memsuper);
		return 0;
	}

	__auto_type set = QFV_AllocImages (create.count, malloc);
	for (uint32_t i = 0; i < create.count; i++) {
		qfvPushDebug (ctx, va (ctx->va_ctx, "QFV_ParseImageSet: %d", PL_Line (item)));
		dfunc->vkCreateImage (device->dev, &create.info[i], 0, &set->a[i]);

		const char *name = PL_KeyAtIndex (item, i);
		QFV_duSetObjectName (device, VK_OBJECT_TYPE_IMAGE, set->a[i],
							 va (ctx->va_ctx, "image:%s", name));
		qfvPopDebug (ctx);
		name = resource_path (ctx, "images", name);
		QFV_AddHandle (sctx->images, name, (uint64_t) set->a[i]);
	}

	delete_memsuper (memsuper);
	return set;
}

qfv_imageviewset_t *
QFV_ParseImageViewSet (vulkan_ctx_t *ctx, plitem_t *item,
					   plitem_t *properties)
{
	scriptctx_t *sctx = ctx->script_context;
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	memsuper_t *memsuper = new_memsuper ();

	imageviewcreate_t create = {};

	pltype_t    type = PL_Type (item);

	if (type == QFDictionary) {
		if (!parse_object (ctx, memsuper, item, parse_imageviewcreate_dict,
						   &create, properties)) {
			delete_memsuper (memsuper);
			return 0;
		}
	} else {
		Sys_Printf ("Neither array nor dictionary: %d\n", PL_Line (item));
		delete_memsuper (memsuper);
		return 0;
	}

	__auto_type set = QFV_AllocImageViews (create.count, malloc);
	for (uint32_t i = 0; i < create.count; i++) {
		qfvPushDebug (ctx, va (ctx->va_ctx, "QFV_ParseImageViewSet: %d", PL_Line (item)));
		dfunc->vkCreateImageView (device->dev, &create.info[i], 0, &set->a[i]);
		qfvPopDebug (ctx);

		const char *name = PL_KeyAtIndex (item, i);
		name = resource_path (ctx, "imageViews", name);
		QFV_AddHandle (sctx->imageViews, name, (uint64_t) set->a[i]);
	}

	delete_memsuper (memsuper);
	return set;
}

VkFramebuffer
QFV_ParseFramebuffer (vulkan_ctx_t *ctx, plitem_t *plist, plitem_t *properties)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	memsuper_t *memsuper = new_memsuper ();

	VkFramebufferCreateInfo cInfo = {};

	if (!parse_object (ctx, memsuper, plist, parse_VkFramebufferCreateInfo,
					   &cInfo, properties)) {
		delete_memsuper (memsuper);
		return 0;
	}

	VkFramebuffer framebuffer;
	qfvPushDebug (ctx, va (ctx->va_ctx, "QFV_ParseFramebuffer: %d", PL_Line (plist)));
	dfunc->vkCreateFramebuffer (device->dev, &cInfo, 0, &framebuffer);
	qfvPopDebug (ctx);
	Sys_MaskPrintf (SYS_vulkan_parse, "framebuffer, renderPass: %#zx, %#zx\n",
					(size_t) framebuffer, (size_t) cInfo.renderPass);

	delete_memsuper (memsuper);
	return framebuffer;
}

static int
parse_clearvalueset (const plfield_t *field, const plitem_t *item, void *data,
					 plitem_t *messages, void *context)
{
	plelement_t element = {
		QFDictionary,
		sizeof (VkClearValue),
		vkparse_alloc,
		parse_VkClearValue,
		0,
	};
	plfield_t   f = { 0, 0, 0, 0, &element };

	if (!PL_ParseArray (&f, item, data, messages, context)) {
		return 0;
	}
	return 1;
}

clearvalueset_t *
QFV_ParseClearValues (vulkan_ctx_t *ctx, plitem_t *plist, plitem_t *properties)
{
	clearvalueset_t *cv = 0;
	memsuper_t *memsuper = new_memsuper ();
	clearvalueset_t *clearValues = 0;

	if (parse_object (ctx, memsuper, plist, parse_clearvalueset, &clearValues,
					  properties)) {
		cv = DARRAY_ALLOCFIXED (clearvalueset_t, clearValues->size, malloc);
		memcpy (cv->a, clearValues->a, cv->size * sizeof (cv->a[0]));
	}
	delete_memsuper (memsuper);
	return cv;
}

static int
parse_subpassset (const plfield_t *field, const plitem_t *item, void *data,
					 plitem_t *messages, void *context)
{
	plelement_t element = {
		QFDictionary,
		sizeof (qfv_subpass_t),
		vkparse_alloc,
		parse_qfv_subpass_t,
		0,
	};
	plfield_t   f = { 0, 0, 0, 0, &element };

	if (!PL_ParseArray (&f, item, data, messages, context)) {
		return 0;
	}
	return 1;
}

qfv_subpassset_t *
QFV_ParseSubpasses (vulkan_ctx_t *ctx, plitem_t *plist, plitem_t *properties)
{
	qfv_subpassset_t *sp = 0;
	memsuper_t *memsuper = new_memsuper ();
	qfv_subpassset_t *subpasses = 0;

	if (parse_object (ctx, memsuper, plist, parse_subpassset, &subpasses,
					  properties)) {
		sp = DARRAY_ALLOCFIXED (qfv_subpassset_t, subpasses->size, malloc);
		memcpy (sp->a, subpasses->a, sp->size * sizeof (sp->a[0]));
		// the name is in memsuper which is about to be freed
		for (size_t i = 0; i < sp->size; i++) {
			sp->a[i].name = strdup (sp->a[i].name);
		}
	}
	delete_memsuper (memsuper);
	return sp;
}

static int
parse_rgba (const plfield_t *field, const plitem_t *item, void *data,
			plitem_t *messages, void *context)
{
	return parse_RGBA (item, &data, messages, context);
}

int
QFV_ParseRGBA (vulkan_ctx_t *ctx, float *rgba, plitem_t *plist,
			   plitem_t *properties)
{
	memsuper_t *memsuper = new_memsuper ();
	int         ret = 0;
	vec4f_t     color;

	if (parse_object (ctx, memsuper, plist, parse_rgba, &color, properties)) {
		memcpy (rgba, &color, sizeof (color));
		ret = 1;
	}
	delete_memsuper (memsuper);
	return ret;
}

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
		"increase performance, but at the cost of latency. The default of 3 is"
		" recommended.",
	.default_value = "3",
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
	{ .name = "quake_deferred",
	  .value = (void *)
#include "libs/video/renderer/vulkan/pl_quake_def.plc"
		},
	{ .name = "qf_output",
	  .value = (void *)
#include "libs/video/renderer/vulkan/pl_output.plc"
		},
	{ .name = "deferred",
	  .value = (void *)
#include "libs/video/renderer/vulkan/rp_deferred.plc"
		},
	{ .name = "shadow",
	  .value = (void *)
#include "libs/video/renderer/vulkan/rp_shadow.plc"
		},
	{ .name = "forward",
	  .value = (void *)
#include "libs/video/renderer/vulkan/rp_forward.plc"
		},
	{ .name = "output",
	  .value = (void *)
#include "libs/video/renderer/vulkan/rp_output.plc"
		},
	{}
};
static plitem_t **builtin_plists;
static exprtab_t builtin_configs = { .symbols = builtin_plist_syms };

static void
build_configs (scriptctx_t *sctx)
{
	int         num_plists = 0;
	for (exprsym_t *sym = builtin_plist_syms; sym->name; sym++) {
		num_plists++;
	}
	builtin_plists = malloc (num_plists * sizeof (plitem_t *));
	num_plists = 0;
	for (exprsym_t *sym = builtin_plist_syms; sym->name; sym++) {
		plitem_t   *item = PL_GetPropertyList (sym->value, &sctx->hashctx);
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

static plitem_t *
qfv_load_pipeline (vulkan_ctx_t *ctx, const char *name)
{
	scriptctx_t *sctx = ctx->script_context;
	if (!sctx->pipelineDef) {
		sctx->pipelineDef = Vulkan_GetConfig (ctx, "quake_deferred");
	}

	plitem_t   *item = sctx->pipelineDef;
	if (!item || !(item = PL_ObjectForKey (item, name))) {
		Sys_Printf ("error loading %s\n", name);
	} else {
		Sys_MaskPrintf (SYS_vulkan_parse, "Found %s def\n", name);
	}
	return item;
}

plitem_t *
Vulkan_GetConfig (vulkan_ctx_t *ctx, const char *name)
{
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
	PL_Free (ectx.messages);
	delete_memsuper (ectx.memsuper);
	return config;
}

void Vulkan_Script_Init (vulkan_ctx_t *ctx)
{
	scriptctx_t *sctx = calloc (1, sizeof (scriptctx_t));
	sctx->vctx = ctx;
	ctx->script_context = sctx;

	exprctx_t   ectx = {};
	enum_symtab = Hash_NewTable (61, enum_symtab_getkey, 0, 0, &sctx->hashctx);
	parser_table = Hash_NewTable (61, parser_getkey, 0, 0, &sctx->hashctx);
	ectx.hashctx = &sctx->hashctx;
	vkgen_init_symtabs (&ectx);
	cexpr_init_symtab (&qfv_output_t_symtab, &ectx);
	cexpr_init_symtab (&vulkan_frameset_t_symtab, &ectx);
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

	PL_Free (sctx->pipelineDef);
	clear_table (&sctx->pipelineLayouts);
	clear_table (&sctx->setLayouts);
	clear_table (&sctx->shaderModules);
	clear_table (&sctx->descriptorPools);
	clear_table (&sctx->samplers);

	free (sctx);
}

void Vulkan_Script_SetOutput (vulkan_ctx_t *ctx, qfv_output_t *output)
{
	scriptctx_t *sctx = ctx->script_context;
	sctx->output = *output;
}

VkPipeline
Vulkan_CreateComputePipeline (vulkan_ctx_t *ctx, const char *name)
{
	scriptctx_t *sctx = ctx->script_context;
	plitem_t   *item = qfv_load_pipeline (ctx, "pipelines");
	if (!(item = PL_ObjectForKey (item, name))) {
		Sys_Printf ("error loading pipeline %s\n", name);
		return 0;
	} else {
		Sys_MaskPrintf (SYS_vulkan_parse, "Found pipeline def %s\n", name);
	}
	VkPipeline pipeline = QFV_ParseComputePipeline (ctx, item,
													sctx->pipelineDef);
	QFV_duSetObjectName (ctx->device, VK_OBJECT_TYPE_PIPELINE, pipeline,
						 va (ctx->va_ctx, "pipeline:%s", name));
	return pipeline;
}

VkPipeline
Vulkan_CreateGraphicsPipeline (vulkan_ctx_t *ctx, const char *name)
{
	scriptctx_t *sctx = ctx->script_context;
	plitem_t   *item = qfv_load_pipeline (ctx, "pipelines");
	if (!(item = PL_ObjectForKey (item, name))) {
		Sys_Printf ("error loading pipeline %s\n", name);
		return 0;
	} else {
		Sys_MaskPrintf (SYS_vulkan_parse, "Found pipeline def %s\n", name);
	}
	VkPipeline pipeline = QFV_ParseGraphicsPipeline (ctx, item,
													 sctx->pipelineDef);
	QFV_duSetObjectName (ctx->device, VK_OBJECT_TYPE_PIPELINE, pipeline,
						 va (ctx->va_ctx, "pipeline:%s", name));
	return pipeline;
}

VkDescriptorPool
Vulkan_CreateDescriptorPool (vulkan_ctx_t *ctx, const char *name)
{
	scriptctx_t *sctx = ctx->script_context;
	hashtab_t  *tab = sctx->descriptorPools;
	const char *path;
	path = va (ctx->va_ctx, "$"QFV_PROPERTIES".descriptorPools.%s", name);
	__auto_type pool = (VkDescriptorPool) QFV_GetHandle (tab, path);
	if (pool) {
		return pool;
	}

	plitem_t   *item = qfv_load_pipeline (ctx, "descriptorPools");
	if (!(item = PL_ObjectForKey (item, name))) {
		Sys_Printf ("error loading descriptor pool %s\n", name);
		return 0;
	} else {
		Sys_MaskPrintf (SYS_vulkan_parse, "Found descriptor pool def %s\n",
						name);
	}
	pool = QFV_ParseDescriptorPool (ctx, item, sctx->pipelineDef);
	QFV_AddHandle (tab, path, (uint64_t) pool);
	QFV_duSetObjectName (ctx->device, VK_OBJECT_TYPE_DESCRIPTOR_POOL, pool,
						 va (ctx->va_ctx, "descriptor_pool:%s", name));
	return pool;
}

VkPipelineLayout
Vulkan_CreatePipelineLayout (vulkan_ctx_t *ctx, const char *name)
{
	scriptctx_t *sctx = ctx->script_context;
	hashtab_t  *tab = sctx->pipelineLayouts;
	const char *path;
	path = va (ctx->va_ctx, "$"QFV_PROPERTIES".pipelineLayouts.%s", name);
	__auto_type layout = (VkPipelineLayout) QFV_GetHandle (tab, path);
	if (layout) {
		return layout;
	}

	plitem_t   *item = qfv_load_pipeline (ctx, "pipelineLayouts");
	if (!(item = PL_ObjectForKey (item, name))) {
		Sys_Printf ("error loading pipeline layout %s\n", name);
		return 0;
	} else {
		Sys_MaskPrintf (SYS_vulkan_parse, "Found pipeline layout def %s\n",
						name);
	}
	layout = QFV_ParsePipelineLayout (ctx, item, sctx->pipelineDef);
	QFV_AddHandle (tab, path, (uint64_t) layout);
	QFV_duSetObjectName (ctx->device, VK_OBJECT_TYPE_PIPELINE_LAYOUT, layout,
						 va (ctx->va_ctx, "pipeline_layout:%s", name));
	return layout;
}

VkSampler
Vulkan_CreateSampler (vulkan_ctx_t *ctx, const char *name)
{
	scriptctx_t *sctx = ctx->script_context;
	hashtab_t  *tab = sctx->samplers;
	const char *path;
	path = va (ctx->va_ctx, "$"QFV_PROPERTIES".samplers.%s", name);
	__auto_type sampler = (VkSampler) QFV_GetHandle (tab, path);
	if (sampler) {
		return sampler;
	}

	plitem_t   *item = qfv_load_pipeline (ctx, "samplers");
	if (!(item = PL_ObjectForKey (item, name))) {
		Sys_Printf ("error loading sampler %s\n", name);
		return 0;
	} else {
		Sys_MaskPrintf (SYS_vulkan_parse, "Found sampler def %s\n", name);
	}
	sampler = QFV_ParseSampler (ctx, item, sctx->pipelineDef);
	QFV_AddHandle (tab, path, (uint64_t) sampler);
	QFV_duSetObjectName (ctx->device, VK_OBJECT_TYPE_SAMPLER, sampler,
						 va (ctx->va_ctx, "sampler:%s", name));
	return sampler;
}

VkDescriptorSetLayout
Vulkan_CreateDescriptorSetLayout(vulkan_ctx_t *ctx, const char *name)
{
	scriptctx_t *sctx = ctx->script_context;
	hashtab_t  *tab = sctx->setLayouts;
	const char *path;
	path = va (ctx->va_ctx, "$"QFV_PROPERTIES".setLayouts.%s", name);
	__auto_type set = (VkDescriptorSetLayout) QFV_GetHandle (tab, path);
	if (set) {
		return set;
	}

	plitem_t   *item = qfv_load_pipeline (ctx, "setLayouts");
	if (!(item = PL_ObjectForKey (item, name))) {
		Sys_Printf ("error loading descriptor set %s\n", name);
		return 0;
	} else {
		Sys_MaskPrintf (SYS_vulkan_parse, "Found descriptor set def %s\n",
						name);
	}
	set = QFV_ParseDescriptorSetLayout (ctx, item, sctx->pipelineDef);
	QFV_AddHandle (tab, path, (uint64_t) set);
	QFV_duSetObjectName (ctx->device, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,
						 set, va (ctx->va_ctx, "descriptor_set:%s", name));
	return set;
}
