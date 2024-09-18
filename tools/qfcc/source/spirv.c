/*
	spirv.c

	qfcc spir-v file support

	Copyright (C) 2024 Bill Currie <bill@taniwha.org>

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

#include <spirv/unified1/spirv.h>

#include "QF/quakeio.h"

#include "tools/qfcc/include/def.h"
#include "tools/qfcc/include/defspace.h"
#include "tools/qfcc/include/diagnostic.h"
#include "tools/qfcc/include/function.h"
#include "tools/qfcc/include/qfcc.h"
#include "tools/qfcc/include/spirv.h"
#include "tools/qfcc/include/statements.h"
#include "tools/qfcc/include/strpool.h"
#include "tools/qfcc/include/symtab.h"
#include "tools/qfcc/include/type.h"

typedef struct spirvctx_s {
	defspace_t *space;
	defspace_t *linkage;
	defspace_t *strings;
	defspace_t *names;
	defspace_t *types;
	defspace_t *code;
	struct DARRAY_TYPE (unsigned) type_ids;
	unsigned id;
} spirvctx_t;

static def_t *
spirv_new_insn (int op, int size, defspace_t *space)
{
	auto def = new_def (nullptr, nullptr, space, sc_static);
	def->offset = defspace_alloc_loc (space, size);

	D_INT (def) = (op & SpvOpCodeMask) | (size << SpvWordCountShift);
	return def;
}

static unsigned
spirv_id (spirvctx_t *ctx)
{
	return ++ctx->id;
}

static void
spirv_Capability (SpvCapability capability, defspace_t *space)
{
	auto def = spirv_new_insn (SpvOpCapability, 2, space);
	D_var_o(int, def, 1) = capability;
}

static void
spirv_MemoryModel (SpvAddressingModel addressing, SpvMemoryModel memory,
				   defspace_t *space)
{
	auto def = spirv_new_insn (SpvOpMemoryModel, 3, space);
	D_var_o(int, def, 1) = addressing;
	D_var_o(int, def, 2) = memory;
}

static unsigned
spirv_String (const char *name, spirvctx_t *ctx)
{
	int id = spirv_id (ctx);
	int len = strlen (name) + 1;
	auto def = spirv_new_insn (SpvOpString, 2 + RUP(len, 4) / 4, ctx->strings);
	D_var_o(int, def, 1) = id;
	memcpy (&D_var_o(int, def, 2), name, len);
	return id;
}

static void
spirv_Source (unsigned lang, unsigned version, unsigned srcid,
			  const char *src_str, spirvctx_t *ctx)
{
	auto def = spirv_new_insn (SpvOpSource, 4, ctx->strings);
	D_var_o(int, def, 1) = lang;
	D_var_o(int, def, 2) = version;
	D_var_o(int, def, 3) = srcid;
}

static void
spirv_Name (unsigned id, const char *name, spirvctx_t *ctx)
{
	int len = strlen (name) + 1;
	auto def = spirv_new_insn (SpvOpName, 2 + RUP(len, 4) / 4, ctx->names);
	D_var_o(int, def, 1) = id;
	memcpy (&D_var_o(int, def, 2), name, len);
}

static void
spirv_MemberName (unsigned id, unsigned member, const char *name,
				  spirvctx_t *ctx)
{
	int len = strlen (name) + 1;
	auto def = spirv_new_insn (SpvOpMemberName, 3 + RUP(len, 4) / 4,
							   ctx->names);
	D_var_o(int, def, 1) = id;
	D_var_o(int, def, 2) = member;
	memcpy (&D_var_o(int, def, 3), name, len);
}

static unsigned type_id (const type_t *type, spirvctx_t *ctx);

static unsigned
spirv_TypeVoid (spirvctx_t *ctx)
{
	auto def = spirv_new_insn (SpvOpTypeVoid, 2, ctx->types);
	D_var_o(int, def, 1) = spirv_id (ctx);
	return D_var_o(int, def, 1);
}

static unsigned
spirv_TypeInt (unsigned bitwidth, bool is_signed, spirvctx_t *ctx)
{
	auto def = spirv_new_insn (SpvOpTypeInt, 4, ctx->types);
	D_var_o(int, def, 1) = spirv_id (ctx);
	D_var_o(int, def, 2) = bitwidth;
	D_var_o(int, def, 3) = is_signed;
	return D_var_o(int, def, 1);
}

static unsigned
spirv_TypeFloat (unsigned bitwidth, spirvctx_t *ctx)
{
	auto def = spirv_new_insn (SpvOpTypeFloat, 3, ctx->types);
	D_var_o(int, def, 1) = spirv_id (ctx);
	D_var_o(int, def, 2) = bitwidth;
	return D_var_o(int, def, 1);
}

static unsigned
spirv_TypeVector (unsigned base, unsigned width, spirvctx_t *ctx)
{
	auto def = spirv_new_insn (SpvOpTypeVector, 4, ctx->types);
	D_var_o(int, def, 1) = spirv_id (ctx);
	D_var_o(int, def, 2) = base;
	D_var_o(int, def, 3) = width;
	return D_var_o(int, def, 1);
}

static unsigned
spirv_TypeMatrix (unsigned col_type, unsigned columns, spirvctx_t *ctx)
{
	auto def = spirv_new_insn (SpvOpTypeMatrix, 4, ctx->types);
	D_var_o(int, def, 1) = spirv_id (ctx);
	D_var_o(int, def, 2) = col_type;
	D_var_o(int, def, 3) = columns;
	return D_var_o(int, def, 1);
}

static unsigned
spirv_TypePointer (const type_t *type, spirvctx_t *ctx)
{
	auto rtype = dereference_type (type);
	unsigned rid = type_id (rtype, ctx);

	auto def = spirv_new_insn (SpvOpTypePointer, 4, ctx->types);
	D_var_o(int, def, 1) = spirv_id (ctx);
	D_var_o(int, def, 2) = SpvStorageClassFunction;//FIXME
	D_var_o(int, def, 3) = rid;
	return D_var_o(int, def, 1);
}

static unsigned
spirv_TypeStruct (const type_t *type, spirvctx_t *ctx)
{
	auto symtab = type->symtab;
	int num_members = 0;
	for (auto s = symtab->symbols; s; s = s->next) {
		num_members++;
	}
	unsigned member_types[num_members];
	num_members = 0;
	for (auto s = symtab->symbols; s; s = s->next) {
		member_types[num_members++] = type_id (s->type, ctx);
	}

	unsigned id = spirv_id (ctx);
	auto def = spirv_new_insn (SpvOpTypeStruct, 2 + num_members, ctx->types);
	D_var_o(int, def, 1) = id;
	memcpy (&D_var_o(int, def, 2), member_types, sizeof (member_types));

	spirv_Name (id, type->name + 4, ctx);

	num_members = 0;
	for (auto s = symtab->symbols; s; s = s->next) {
		int m = num_members++;
		spirv_MemberName (id, m, s->name, ctx);
	}
	return id;
}

static unsigned
spirv_TypeFunction (symbol_t *fsym, spirvctx_t *ctx)
{
	int num_params = 0;
	for (auto p = fsym->params; p; p = p->next) {
		num_params++;
	}
	unsigned ret_type = type_id (fsym->type->func.ret_type, ctx);

	unsigned param_types[num_params];
	num_params = 0;
	for (auto p = fsym->params; p; p = p->next) {
		auto ptype = p->type;
		if (p->qual != pq_const) {
			ptype = pointer_type (ptype);
		}
		param_types[num_params++] = type_id (ptype, ctx);
	}

	unsigned ft_id = spirv_id (ctx);
	auto def = spirv_new_insn (SpvOpTypeFunction, 3 + num_params, ctx->types);
	D_var_o(int, def, 1) = ft_id;
	D_var_o(int, def, 2) = ret_type;
	for (int i = 0; i < num_params; i++) {
		D_var_o(int, def, 3 + i) = param_types[i];
	}
	return ft_id;
}

static unsigned
type_id (const type_t *type, spirvctx_t *ctx)
{
	if (type->id < ctx->type_ids.size && ctx->type_ids.a[type->id]) {
		return ctx->type_ids.a[type->id];
	}
	if (type->id >= ctx->type_ids.size) {
		size_t base = ctx->type_ids.size;
		DARRAY_RESIZE (&ctx->type_ids, type->id + 1);
		while (base < type->id + 1) {
			ctx->type_ids.a[base++] = 0;
		}
	}
	unsigned id = 0;
	if (is_void (type)) {
		id = spirv_TypeVoid (ctx);
	} else if (is_vector (type)) {
		// spir-v doesn't allow duplicate non-aggregate types, so emit
		// vector as vec3
		auto vtype = vector_type (&type_float, 3);
		id = type_id (vtype, ctx);
	} else if (is_quaternion (type)) {
		// spir-v doesn't allow duplicate non-aggregate types, so emit
		// quaternion as vec4
		auto qtype = vector_type (&type_float, 4);
		id = type_id (qtype, ctx);
	} else if (type_cols (type) > 1) {
		auto ctype = vector_type (base_type (type), type_rows (type));
		unsigned cid = type_id (ctype, ctx);
		id = spirv_TypeMatrix (cid, type_cols (type), ctx);
	} else if (type_width (type) > 1) {
		auto btype = base_type (type);
		unsigned bid = type_id (btype, ctx);
		id = spirv_TypeVector (bid, type_width (type), ctx);
	} else if (is_int (type)) {
		id = spirv_TypeInt (32, true, ctx);
	} else if (is_uint (type)) {
		id = spirv_TypeInt (32, false, ctx);
	} else if (is_long (type)) {
		id = spirv_TypeInt (64, true, ctx);
	} else if (is_ulong (type)) {
		id = spirv_TypeInt (64, false, ctx);
	} else if (is_float (type)) {
		id = spirv_TypeFloat (32, ctx);
	} else if (is_double (type)) {
		id = spirv_TypeFloat (64, ctx);
	} else if (is_ptr (type)) {
		id = spirv_TypePointer (type, ctx);
	} else if (is_struct (type)) {
		id = spirv_TypeStruct (type, ctx);
	}
	if (!id) {
		dstring_t  *str = dstring_newstr ();
		print_type_str (str, type);
		internal_error (0, "can't emit type %s", str->str);
	}

	ctx->type_ids.a[type->id] = id;
	return id;
}

static unsigned
spirv_Label (spirvctx_t *ctx)
{
	auto def = spirv_new_insn (SpvOpLabel, 2, ctx->code);
	D_var_o(int, def, 1) = spirv_id (ctx);
	return D_var_o(int, def, 1);
}

static void
spirv_Unreachable (spirvctx_t *ctx)
{
	spirv_new_insn (SpvOpUnreachable, 1, ctx->code);
}

static void
spirv_FunctionEnd (spirvctx_t *ctx)
{
	spirv_new_insn (SpvOpFunctionEnd, 1, ctx->code);
}

static unsigned
spirv_FunctionParameter (const char *name, const type_t *type, spirvctx_t *ctx)
{
	unsigned id = spirv_id (ctx);
	auto def = spirv_new_insn (SpvOpFunctionParameter, 3, ctx->code);
	D_var_o(int, def, 1) = type_id (type, ctx);
	D_var_o(int, def, 2) = id;
	spirv_Name (id, name, ctx);
	return id;
}

static unsigned
spirv_function (function_t *func, spirvctx_t *ctx)
{
	unsigned ft_id = spirv_TypeFunction (func->sym, ctx);

	unsigned func_id = spirv_id (ctx);
	auto def = spirv_new_insn (SpvOpFunction, 5, ctx->code);
	D_var_o(int, def, 1) = type_id (func->type->func.ret_type, ctx);
	D_var_o(int, def, 2) = func_id;
	D_var_o(int, def, 3) = 0;
	D_var_o(int, def, 4) = ft_id;
	spirv_Name (func_id, GETSTR (func->s_name), ctx);

	for (auto p = func->sym->params; p; p = p->next) {
		auto ptype = p->type;
		if (p->qual != pq_const) {
			ptype = pointer_type (ptype);
		}
		spirv_FunctionParameter (p->name, ptype, ctx);
	}

	if (!func->sblock) {
		spirv_Label (ctx);
		spirv_Unreachable (ctx);
	}
	for (auto sblock = func->sblock; sblock; sblock = sblock->next) {
		if (!sblock->id) {
			sblock->id = spirv_Label (ctx);
		}
		spirv_Unreachable (ctx);
	}
	spirv_FunctionEnd (ctx);
	return func_id;
}

static void
spirv_EntryPoint (unsigned func_id, const char *func_name,
				  SpvExecutionModel model, spirvctx_t *ctx)
{
	int len = strlen (func_name) + 1;
	auto def = spirv_new_insn (SpvOpEntryPoint, 3 + RUP(len, 4) / 4,
							   ctx->linkage);
	D_var_o(int, def, 1) = model;
	D_var_o(int, def, 2) = func_id;
	memcpy (&D_var_o(int, def, 3), func_name, len);
}

bool
spirv_write (struct pr_info_s *pr, const char *filename)
{
	spirvctx_t ctx = {
		.space = defspace_new (ds_backed),
		.linkage = defspace_new (ds_backed),
		.strings = defspace_new (ds_backed),
		.names = defspace_new (ds_backed),
		.types = defspace_new (ds_backed),
		.code = defspace_new (ds_backed),
		.type_ids = DARRAY_STATIC_INIT (64),
		.id = 0,
	};
	auto header = spirv_new_insn (0, 5, ctx.space);
	D_var_o(int, header, 0) = SpvMagicNumber;
	D_var_o(int, header, 1) = SpvVersion;
	D_var_o(int, header, 2) = 0;	// FIXME get a magic number for QFCC
	D_var_o(int, header, 3) = 0;	// Filled in later
	D_var_o(int, header, 4) = 0;	// Reserved

	//FIXME none of these should be hard-coded
	spirv_Capability (SpvCapabilityShader, ctx.space);
	//spirv_Capability (SpvCapabilityLinkage, ctx.space);
	spirv_MemoryModel (SpvAddressingModelLogical, SpvMemoryModelGLSL450,
					   ctx.space);

	auto srcid = spirv_String (pr->src_name, &ctx);
	spirv_Source (0, 1, srcid, nullptr, &ctx);
	for (auto func = pr->func_head; func; func = func->next)
	{
		auto func_id = spirv_function (func, &ctx);
		if (strcmp ("main", func->o_name) == 0) {
			auto model = SpvExecutionModelVertex;//FIXME
			spirv_EntryPoint (func_id, func->o_name, model, &ctx);
		}
	}

	auto main_sym = symtab_lookup (pr->symtab, "main");
	if (main_sym && main_sym->sy_type == sy_func) {
	}

	D_var_o(int, header, 3) = ctx.id + 1;
	defspace_add_data (ctx.space, ctx.linkage->data, ctx.linkage->size);
	defspace_add_data (ctx.space, ctx.strings->data, ctx.strings->size);
	defspace_add_data (ctx.space, ctx.names->data, ctx.names->size);
	defspace_add_data (ctx.space, ctx.types->data, ctx.types->size);
	defspace_add_data (ctx.space, ctx.code->data, ctx.code->size);

	QFile *file = Qopen (filename, "wb");
	Qwrite (file, ctx.space->data, ctx.space->size * sizeof (pr_type_t));
	Qclose (file);
	return false;
}
