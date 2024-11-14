/*
	target_spirv.c

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

#include <spirv/unified1/GLSL.std.450.h>

#include "QF/quakeio.h"

#include "tools/qfcc/include/attribute.h"
#include "tools/qfcc/include/def.h"
#include "tools/qfcc/include/defspace.h"
#include "tools/qfcc/include/diagnostic.h"
#include "tools/qfcc/include/function.h"
#include "tools/qfcc/include/glsl-lang.h"
#include "tools/qfcc/include/qfcc.h"
#include "tools/qfcc/include/spirv.h"
#include "tools/qfcc/include/statements.h"
#include "tools/qfcc/include/strpool.h"
#include "tools/qfcc/include/symtab.h"
#include "tools/qfcc/include/target.h"
#include "tools/qfcc/include/type.h"

#define INSN(i,o) D_var_o(int,(i),o)
#define ADD_DATA(d,s) defspace_add_data((d), (s)->data, (s)->size)

typedef struct spirvctx_s {
	module_t   *module;

	defspace_t *decl_space;
	defspace_t *code_space;

	struct DARRAY_TYPE (unsigned) type_ids;
	unsigned id;
} spirvctx_t;

static unsigned
spirv_type_id (const type_t *type, spirvctx_t *ctx)
{
	if (type->id < ctx->type_ids.size && ctx->type_ids.a[type->id]) {
		return ctx->type_ids.a[type->id];
	}
	return 0;
}

static void
spirv_add_type_id (const type_t *type, unsigned id, spirvctx_t *ctx)
{
	if (type->id >= ctx->type_ids.size) {
		size_t base = ctx->type_ids.size;
		DARRAY_RESIZE (&ctx->type_ids, type->id + 1);
		while (base < type->id + 1) {
			ctx->type_ids.a[base++] = 0;
		}
	}
	ctx->type_ids.a[type->id] = id;
}

static def_t *
spirv_new_insn (int op, int size, defspace_t *space)
{
	auto insn = new_def (nullptr, nullptr, space, sc_static);
	insn->offset = defspace_alloc_highwater (space, size);

	INSN (insn, 0) = (op & SpvOpCodeMask) | (size << SpvWordCountShift);
	return insn;
}

static def_t *
spirv_str_insn (int op, int offs, int extra, const char *str, defspace_t *space)
{
	int len = strlen (str) + 1;
	int str_size = RUP(len, 4) / 4;
	int size = offs + str_size + extra;
	auto insn = spirv_new_insn (op, size, space);
	INSN (insn, str_size - 1) = 0;
	memcpy (&INSN (insn, offs), str, len);
	return insn;
}

static unsigned
spirv_id (spirvctx_t *ctx)
{
	return ++ctx->id;
}

static void
spirv_Capability (SpvCapability capability, defspace_t *space)
{
	auto insn = spirv_new_insn (SpvOpCapability, 2, space);
	INSN (insn, 1) = capability;
}

static void
spirv_Extension (const char *ext, defspace_t *space)
{
	spirv_str_insn (SpvOpExtension, 1, 0, ext, space);
}

static unsigned
spirv_ExtInstImport (const char *imp, spirvctx_t *ctx)
{
	auto strings = ctx->module->strings;
	auto insn = spirv_str_insn (SpvOpExtInstImport, 2, 0, imp, strings);
	int id = spirv_id (ctx);
	INSN (insn, 1) = id;
	return id;
}

static void
spirv_MemoryModel (SpvAddressingModel addressing, SpvMemoryModel memory,
				   defspace_t *space)
{
	auto insn = spirv_new_insn (SpvOpMemoryModel, 3, space);
	INSN (insn, 1) = addressing;
	INSN (insn, 2) = memory;
}

static unsigned
spirv_String (const char *name, spirvctx_t *ctx)
{
	auto strings = ctx->module->strings;
	auto insn = spirv_str_insn (SpvOpString, 2, 0, name, strings);
	int id = spirv_id (ctx);
	INSN (insn, 1) = id;
	return id;
}

static void
spirv_Source (unsigned lang, unsigned version, unsigned srcid,
			  const char *src_str, spirvctx_t *ctx)
{
	auto strings = ctx->module->strings;
	auto insn = spirv_new_insn (SpvOpSource, 4, strings);
	INSN (insn, 1) = lang;
	INSN (insn, 2) = version;
	INSN (insn, 3) = srcid;
}

static void
spirv_Name (unsigned id, const char *name, spirvctx_t *ctx)
{
	if (!name) {
		name = "";
	}
	int len = strlen (name) + 1;
	auto names = ctx->module->names;
	auto insn = spirv_new_insn (SpvOpName, 2 + RUP(len, 4) / 4, names);
	INSN (insn, 1) = id;
	memcpy (&INSN (insn, 2), name, len);
}

static void
spirv_Decorate (unsigned id, SpvDecoration decoration, void *literal,
				etype_t type, spirvctx_t *ctx)
{
	if (type != ev_int) {
		internal_error (0, "unexpected type");
	}
	int size = pr_type_size[type];
	auto decorations = ctx->module->decorations;
	auto insn = spirv_new_insn (SpvOpDecorate, 3 + size, decorations);
	INSN (insn, 1) = id;
	INSN (insn, 2) = decoration;
	if (type == ev_int) {
		INSN (insn, 3) = *(int *)literal;
	}
}

static void
spirv_MemberName (unsigned id, unsigned member, const char *name,
				  spirvctx_t *ctx)
{
	int len = strlen (name) + 1;
	auto names = ctx->module->names;
	auto insn = spirv_new_insn (SpvOpMemberName, 3 + RUP(len, 4) / 4, names);
	INSN (insn, 1) = id;
	INSN (insn, 2) = member;
	memcpy (&INSN (insn, 3), name, len);
}

static unsigned type_id (const type_t *type, spirvctx_t *ctx);

static unsigned
spirv_TypeVoid (spirvctx_t *ctx)
{
	auto globals = ctx->module->globals;
	auto insn = spirv_new_insn (SpvOpTypeVoid, 2, globals);
	INSN (insn, 1) = spirv_id (ctx);
	return INSN (insn, 1);
}

static unsigned
spirv_TypeInt (unsigned bitwidth, bool is_signed, spirvctx_t *ctx)
{
	auto globals = ctx->module->globals;
	auto insn = spirv_new_insn (SpvOpTypeInt, 4, globals);
	INSN (insn, 1) = spirv_id (ctx);
	INSN (insn, 2) = bitwidth;
	INSN (insn, 3) = is_signed;
	return INSN (insn, 1);
}

static unsigned
spirv_TypeFloat (unsigned bitwidth, spirvctx_t *ctx)
{
	auto globals = ctx->module->globals;
	auto insn = spirv_new_insn (SpvOpTypeFloat, 3, globals);
	INSN (insn, 1) = spirv_id (ctx);
	INSN (insn, 2) = bitwidth;
	return INSN (insn, 1);
}

static unsigned
spirv_TypeVector (unsigned base, unsigned width, spirvctx_t *ctx)
{
	auto globals = ctx->module->globals;
	auto insn = spirv_new_insn (SpvOpTypeVector, 4, globals);
	INSN (insn, 1) = spirv_id (ctx);
	INSN (insn, 2) = base;
	INSN (insn, 3) = width;
	return INSN (insn, 1);
}

static unsigned
spirv_TypeMatrix (unsigned col_type, unsigned columns, spirvctx_t *ctx)
{
	auto globals = ctx->module->globals;
	auto insn = spirv_new_insn (SpvOpTypeMatrix, 4, globals);
	INSN (insn, 1) = spirv_id (ctx);
	INSN (insn, 2) = col_type;
	INSN (insn, 3) = columns;
	return INSN (insn, 1);
}

static unsigned
spirv_TypePointer (const type_t *type, spirvctx_t *ctx)
{
	auto rtype = dereference_type (type);
	unsigned rid = type_id (rtype, ctx);

	auto globals = ctx->module->globals;
	auto insn = spirv_new_insn (SpvOpTypePointer, 4, globals);
	INSN (insn, 1) = spirv_id (ctx);
	INSN (insn, 2) = type->fldptr.tag;
	INSN (insn, 3) = rid;
	return INSN (insn, 1);
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
	auto globals = ctx->module->globals;
	auto insn = spirv_new_insn (SpvOpTypeStruct, 2 + num_members, globals);
	INSN (insn, 1) = id;
	memcpy (&INSN (insn, 2), member_types, sizeof (member_types));

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
	auto type = fsym->type;
	if (spirv_type_id (type, ctx)) {
		return spirv_type_id (type, ctx);
	}
	unsigned ret_type = type_id (fsym->type->func.ret_type, ctx);

	unsigned param_types[num_params];
	num_params = 0;
	for (auto p = fsym->params; p; p = p->next) {
		auto ptype = p->type;
		if (p->qual != pq_const) {
			ptype = tagged_reference_type (SpvStorageClassFunction, ptype);
		}
		param_types[num_params++] = type_id (ptype, ctx);
	}

	unsigned ft_id = spirv_id (ctx);
	auto globals = ctx->module->globals;
	auto insn = spirv_new_insn (SpvOpTypeFunction, 3 + num_params, globals);
	INSN (insn, 1) = ft_id;
	INSN (insn, 2) = ret_type;
	for (int i = 0; i < num_params; i++) {
		INSN (insn, 3 + i) = param_types[i];
	}
	spirv_add_type_id (type, ft_id, ctx);
	return ft_id;
}

static unsigned
type_id (const type_t *type, spirvctx_t *ctx)
{
	type = unalias_type (type);
	if (spirv_type_id (type, ctx)) {
		return spirv_type_id (type, ctx);
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
	spirv_add_type_id (type, id, ctx);
	return id;
}

static unsigned
spirv_DeclLabel (spirvctx_t *ctx)
{
	auto insn = spirv_new_insn (SpvOpLabel, 2, ctx->decl_space);
	INSN (insn, 1) = spirv_id (ctx);
	return INSN (insn, 1);
}

static unsigned
spirv_Label (spirvctx_t *ctx)
{
	auto insn = spirv_new_insn (SpvOpLabel, 2, ctx->code_space);
	INSN (insn, 1) = spirv_id (ctx);
	return INSN (insn, 1);
}

static void
spirv_Unreachable (spirvctx_t *ctx)
{
	spirv_new_insn (SpvOpUnreachable, 1, ctx->code_space);
}

static void
spirv_FunctionEnd (spirvctx_t *ctx)
{
	spirv_new_insn (SpvOpFunctionEnd, 1, ctx->code_space);
}

static unsigned
spirv_FunctionParameter (const char *name, const type_t *type, spirvctx_t *ctx)
{
	unsigned id = spirv_id (ctx);
	auto insn = spirv_new_insn (SpvOpFunctionParameter, 3, ctx->decl_space);
	INSN (insn, 1) = type_id (type, ctx);
	INSN (insn, 2) = id;
	spirv_Name (id, name, ctx);
	return id;
}

static SpvStorageClass
spirv_storage_class (unsigned storage)
{
	auto interface = glsl_iftype_from_sc (storage);
	SpvStorageClass sc = 0;
	if (interface < glsl_num_interfaces) {
		static SpvStorageClass iface_storage[glsl_num_interfaces] = {
			[glsl_in] = SpvStorageClassInput,
			[glsl_out] = SpvStorageClassOutput,
			[glsl_uniform] = SpvStorageClassUniform,
			[glsl_buffer] = SpvStorageClassStorageBuffer,
		};
		sc = iface_storage[interface];
	} else if (storage < sc_count) {
		static SpvStorageClass sc_storage[sc_count] = {
			[sc_global] = SpvStorageClassPrivate,
			[sc_static] = SpvStorageClassPrivate,
			[sc_local] = SpvStorageClassFunction,
			[sc_inout] = SpvStorageClassFunction,
		};
		sc = sc_storage[storage];
	}
	if (!sc) {
		internal_error (0, "invalid storage class: %d", storage);
	}
	return sc;
}

static unsigned
spirv_variable (symbol_t *sym, spirvctx_t *ctx)
{
	if (sym->sy_type != sy_var) {
		internal_error (0, "unexpected variable symbol type");
	}
	auto space = ctx->module->globals;
	auto storage = spirv_storage_class (sym->var.storage);
	if (storage == SpvStorageClassFunction) {
		space = ctx->decl_space;
	} else {
		DARRAY_APPEND (&ctx->module->interface_syms, sym);
	}
	auto type = sym->type;
	unsigned tid = type_id (type, ctx);
	unsigned id = spirv_id (ctx);
	//FIXME init value
	auto insn = spirv_new_insn (SpvOpVariable, 4, space);
	INSN (insn, 1) = tid;
	INSN (insn, 2) = id;
	INSN (insn, 3) = storage;
	spirv_Name (id, sym->name, ctx);
	return id;
}

static unsigned spirv_emit_expr (const expr_t *e, spirvctx_t *ctx);

static unsigned
spirv_undef (const type_t *type, spirvctx_t *ctx)
{
	unsigned type_id = spirv_type_id (type, ctx);
	unsigned id = spirv_id (ctx);
	auto insn = spirv_new_insn (SpvOpUndef, 3, ctx->module->globals);
	INSN (insn, 1) = type_id;
	INSN (insn, 2) = id;
	return id;
}

static unsigned
spirv_function_ref (function_t *func, spirvctx_t *ctx)
{
	unsigned func_id = func->id;
	if (!func_id) {
		func_id = spirv_id (ctx);
		func->id = func_id;
	}
	return func_id;
}

static unsigned
spirv_function (function_t *func, spirvctx_t *ctx)
{
	unsigned ft_id = spirv_TypeFunction (func->sym, ctx);

	defspace_reset (ctx->code_space);
	defspace_reset (ctx->decl_space);

	auto ret_type = func->type->func.ret_type;
	unsigned func_id = spirv_function_ref (func, ctx);

	auto insn = spirv_new_insn (SpvOpFunction, 5, ctx->decl_space);
	INSN (insn, 1) = type_id (ret_type, ctx);
	INSN (insn, 2) = func_id;
	INSN (insn, 3) = 0;
	INSN (insn, 4) = ft_id;
	spirv_Name (func_id, GETSTR (func->s_name), ctx);

	for (auto p = func->sym->params; p; p = p->next) {
		auto ptype = p->type;
		if (p->qual != pq_const) {
			ptype = tagged_reference_type (SpvStorageClassFunction, ptype);
		}
		unsigned pid = spirv_FunctionParameter (p->name, ptype, ctx);
		if (func->parameters) {
			for (auto s = func->parameters->symbols; s; s = s->next) {
				if (p->name && p->name[0] && s->name && s->name[0]
					&& !strcmp (s->name, p->name)) {
					s->id = pid;
					break;
				}
			}
		}
	}

	if (func->locals) {
		spirv_DeclLabel (ctx);
		for (auto sym = func->locals->symbols; sym; sym = sym->next) {
			sym->id = spirv_variable (sym, ctx);
		}
	} else {
		spirv_Label (ctx);
	}
	int start_size = ctx->code_space->size;
	if (func->exprs) {
		spirv_emit_expr (func->exprs, ctx);
	}
	def_t *last_insn = nullptr;
	if (ctx->code_space->def_tail != &ctx->code_space->defs) {
		last_insn = (def_t *) ctx->code_space->def_tail;
	}
	if (!last_insn
		|| ((INSN(last_insn, 0) & SpvOpCodeMask) != SpvOpReturn
			&& (INSN(last_insn, 0) & SpvOpCodeMask) != SpvOpReturnValue
			&& (INSN(last_insn, 0) & SpvOpCodeMask) != SpvOpUnreachable)) {
		if (is_void (ret_type)) {
			spirv_new_insn (SpvOpReturn, 1, ctx->code_space);
		} else {
			unsigned ret_id = spirv_undef (ret_type, ctx);
			auto insn = spirv_new_insn (SpvOpReturnValue, 2, ctx->code_space);
			INSN (insn, 1) = ret_id;
		}
	}
	if (ctx->code_space->size == start_size) {
		spirv_Unreachable (ctx);
	}
	spirv_FunctionEnd (ctx);
	ADD_DATA (ctx->module->func_definitions, ctx->decl_space);
	ADD_DATA (ctx->module->func_definitions, ctx->code_space);
	return func_id;
}

static void
spirv_EntryPoint (unsigned func_id, const char *func_name,
				  SpvExecutionModel model, spirvctx_t *ctx)
{
	int len = strlen (func_name) + 1;
	int iface_start = 3 + RUP(len, 4) / 4;
	auto linkage = ctx->module->entry_points;
	int count = ctx->module->interface_syms.size;
	auto insn = spirv_new_insn (SpvOpEntryPoint, iface_start + count, linkage);
	INSN (insn, 1) = model;
	INSN (insn, 2) = func_id;
	memcpy (&INSN (insn, 3), func_name, len);
	for (int i = 0; i < count; i++) {
		INSN (insn, iface_start + i) = ctx->module->interface_syms.a[i]->id;
	}
}

typedef struct {
	const char *name;
	unsigned    id;
} extinst_t;

typedef unsigned (*spirv_expr_f) (const expr_t *e, spirvctx_t *ctx);

typedef struct {
	const char *op_name;
	SpvOp       op;
	unsigned    types1;
	unsigned    types2;
	spirv_expr_f generate;
	extinst_t  *extinst;
} spvop_t;

static unsigned
spirv_generate_wedge (const expr_t *e, spirvctx_t *ctx)
{
	return 0;
}

static unsigned
spirv_generate_qmul (const expr_t *e, spirvctx_t *ctx)
{
	return 0;
}

static unsigned
spirv_generate_qvmul (const expr_t *e, spirvctx_t *ctx)
{
	return 0;
}

static unsigned
spirv_generate_vqmul (const expr_t *e, spirvctx_t *ctx)
{
	return 0;
}

#define SPV_type(t) (1<<(t))
#define SPV_SINT  (SPV_type(ev_int)|SPV_type(ev_long))
#define SPV_UINT  (SPV_type(ev_uint)|SPV_type(ev_ulong))
#define SPV_FLOAT (SPV_type(ev_float)|SPV_type(ev_double))
#define SPV_INT   (SPV_SINT|SPV_UINT)
#define SPV_PTR   (SPV_type(ev_ptr))

static extinst_t glsl_450 = {
	.name = "GLSL.std.450"
};

static spvop_t spv_ops[] = {
	{"load",   SpvOpLoad,                 SPV_PTR,   0         },

	{"or",     SpvOpLogicalOr,            SPV_INT,   SPV_INT   },
	{"and",    SpvOpLogicalAnd,           SPV_INT,   SPV_INT   },

	{"eq",     SpvOpIEqual,               SPV_INT,   SPV_INT   },
	{"eq",     SpvOpFOrdEqual,            SPV_FLOAT, SPV_FLOAT },
	{"eq",     SpvOpFOrdNotEqual,         SPV_FLOAT, SPV_FLOAT },
	{"ne",     SpvOpINotEqual,            SPV_INT,   SPV_INT   },
	{"le",     SpvOpULessThanEqual,       SPV_UINT,  SPV_UINT  },
	{"le",     SpvOpSLessThanEqual,       SPV_SINT,  SPV_SINT  },
	{"le",     SpvOpFOrdLessThanEqual,    SPV_FLOAT, SPV_FLOAT },
	{"ge",     SpvOpUGreaterThanEqual,    SPV_UINT,  SPV_UINT  },
	{"ge",     SpvOpSGreaterThanEqual,    SPV_SINT,  SPV_SINT  },
	{"ge",     SpvOpFOrdGreaterThanEqual, SPV_FLOAT, SPV_FLOAT },
	{"lt",     SpvOpULessThan,            SPV_UINT,  SPV_UINT  },
	{"lt",     SpvOpSLessThan,            SPV_SINT,  SPV_SINT  },
	{"lt",     SpvOpFOrdLessThan,         SPV_FLOAT, SPV_FLOAT },
	{"gt",     SpvOpUGreaterThan,         SPV_UINT,  SPV_UINT  },
	{"gt",     SpvOpSGreaterThan,         SPV_SINT,  SPV_SINT  },
	{"gt",     SpvOpFOrdGreaterThan,      SPV_FLOAT, SPV_FLOAT },

	{"sub",    SpvOpSNegate,              SPV_INT,   0         },
	{"sub",    SpvOpFNegate,              SPV_FLOAT, 0         },

	{"add",    SpvOpIAdd,                 SPV_INT,   SPV_INT   },
	{"add",    SpvOpFAdd,                 SPV_FLOAT, SPV_FLOAT },
	{"sub",    SpvOpISub,                 SPV_INT,   SPV_INT   },
	{"sub",    SpvOpFSub,                 SPV_FLOAT, SPV_FLOAT },
	{"mul",    SpvOpIMul,                 SPV_INT,   SPV_INT   },
	{"mul",    SpvOpFMul,                 SPV_FLOAT, SPV_FLOAT },
	{"div",    SpvOpUDiv,                 SPV_UINT,  SPV_UINT  },
	{"div",    SpvOpSDiv,                 SPV_SINT,  SPV_SINT  },
	{"div",    SpvOpFDiv,                 SPV_FLOAT, SPV_FLOAT },
	{"rem",    SpvOpSRem,                 SPV_INT,   SPV_INT   },
	{"rem",    SpvOpFRem,                 SPV_FLOAT, SPV_FLOAT },
	{"mod",    SpvOpUMod,                 SPV_UINT,  SPV_UINT  },
	{"mod",    SpvOpSMod,                 SPV_SINT,  SPV_SINT  },
	{"mod",    SpvOpFMod,                 SPV_FLOAT, SPV_FLOAT },

	{"bitor",  SpvOpBitwiseOr,            SPV_INT,   SPV_INT   },
	{"bitxor", SpvOpBitwiseXor,           SPV_INT,   SPV_INT   },
	{"bitand", SpvOpBitwiseAnd,           SPV_INT,   SPV_INT   },

	{"bitnot", SpvOpNot,                  SPV_INT,   0         },
	{"not",    SpvOpLogicalNot,           SPV_INT,   0         },

	{"shl",    SpvOpShiftLeftLogical,     SPV_INT,   SPV_INT   },
	{"shr",    SpvOpShiftRightLogical,    SPV_UINT,  SPV_UINT  },
	{"shr",    SpvOpShiftRightArithmetic, SPV_SINT,  SPV_SINT  },

	{"dot",    SpvOpDot,                  SPV_FLOAT, SPV_FLOAT },
	{"scale",  SpvOpVectorTimesScalar,    SPV_FLOAT, SPV_FLOAT },

	{"cross",  GLSLstd450Cross,           SPV_FLOAT, SPV_FLOAT,
		.extinst = &glsl_450 },
	{"wedge",  .types1 = SPV_FLOAT, .types2 = SPV_FLOAT,
		.generate = spirv_generate_wedge, .extinst = &glsl_450 },
	{"qmul",   .types1 = SPV_FLOAT, .types2 = SPV_FLOAT,
		.generate = spirv_generate_qmul, .extinst = &glsl_450 },
	{"qvmul",  .types1 = SPV_FLOAT, .types2 = SPV_FLOAT,
		.generate = spirv_generate_qvmul, .extinst = &glsl_450 },
	{"vqmul",  .types1 = SPV_FLOAT, .types2 = SPV_FLOAT,
		.generate = spirv_generate_vqmul, .extinst = &glsl_450 },
};

static const spvop_t *
spirv_find_op (const char *op_name, etype_t type1, etype_t type2)
{
	constexpr int num_ops = sizeof (spv_ops) / sizeof (spv_ops[0]);
	for (int i = 0; i < num_ops; i++) {
		if (strcmp (spv_ops[i].op_name, op_name) == 0
			&& spv_ops[i].types1 & SPV_type(type1)
			&& ((!spv_ops[i].types2 && type2 == ev_void)
				|| (spv_ops[i].types2
					&& (spv_ops[i].types2 & SPV_type(type2))))) {
			return &spv_ops[i];
		}
	}
	return nullptr;
}

static unsigned
spirv_uexpr (const expr_t *e, spirvctx_t *ctx)
{
	auto op_name = convert_op (e->expr.op);
	if (!op_name) {
		if (e->expr.op > 32 && e->expr.op < 127) {
			internal_error (e, "unexpected unary op: '%c'\n", e->expr.op);
		} else {
			internal_error (e, "unexpected unary op: %d\n", e->expr.op);
		}
	}
	auto t = get_type (e->expr.e1);
	auto spv_op = spirv_find_op (op_name, t->type, 0);
	if (!spv_op) {
		internal_error (e, "unexpected unary op_name: %s %s\n", op_name,
						pr_type_name[t->type]);
	}
	if (!spv_op->op) {
		internal_error (e, "unimplemented op: %s %s\n", op_name,
						pr_type_name[t->type]);
	}
	unsigned uid = spirv_emit_expr (e->expr.e1, ctx);

	unsigned tid = type_id (get_type (e), ctx);
	unsigned id;
	if (e->expr.constant) {
		auto globals = ctx->module->globals;
		auto insn = spirv_new_insn (SpvOpSpecConstantOp, 5, globals);
		INSN (insn, 1) = tid;
		INSN (insn, 2) = id = spirv_id (ctx);
		INSN (insn, 3) = spv_op->op;
		INSN (insn, 4) = uid;
	} else {
		auto insn = spirv_new_insn (spv_op->op, 4, ctx->code_space);
		INSN (insn, 1) = tid;
		INSN (insn, 2) = id = spirv_id (ctx);
		INSN (insn, 3) = uid;
	}
	return id;
}

static unsigned
spirv_block (const expr_t *e, spirvctx_t *ctx)
{
	unsigned id = 0;
	auto slist = &e->block.list;
	for (auto s = slist->head; s; s = s->next) {
		spirv_emit_expr (s->expr, ctx);
	}
	if (e->block.result) {
		id = spirv_emit_expr (e->block.result, ctx);
	}
	return id;
}

static unsigned
spirv_expr (const expr_t *e, spirvctx_t *ctx)
{
	auto op_name = convert_op (e->expr.op);
	if (!op_name) {
		internal_error (e, "unexpected binary op: %d\n", e->expr.op);
	}
	auto t1 = get_type (e->expr.e1);
	auto t2 = get_type (e->expr.e1);
	auto spv_op = spirv_find_op (op_name, t1->type, t2->type);
	if (!spv_op) {
		internal_error (e, "unexpected binary op_name: %s %s %s\n", op_name,
						pr_type_name[t1->type],
						pr_type_name[t2->type]);
	}
	if (!spv_op->op) {
		internal_error (e, "unimplemented op: %s %s %s\n", op_name,
						pr_type_name[t1->type],
						pr_type_name[t2->type]);
	}
	unsigned bid1 = spirv_emit_expr (e->expr.e1, ctx);
	unsigned bid2 = spirv_emit_expr (e->expr.e2, ctx);

	unsigned tid = type_id (get_type (e), ctx);
	unsigned id;
	if (e->expr.constant) {
		auto globals = ctx->module->globals;
		auto insn = spirv_new_insn (SpvOpSpecConstantOp, 6, globals);
		INSN (insn, 1) = tid;
		INSN (insn, 2) = id = spirv_id (ctx);
		INSN (insn, 3) = spv_op->op;
		INSN (insn, 4) = bid1;
		INSN (insn, 5) = bid2;
	} else {
		auto insn = spirv_new_insn (spv_op->op, 5, ctx->code_space);
		INSN (insn, 1) = tid;
		INSN (insn, 2) = id = spirv_id (ctx);
		INSN (insn, 3) = bid1;
		INSN (insn, 4) = bid2;
	}
	return id;
}

static unsigned
spirv_symbol (const expr_t *e, spirvctx_t *ctx)
{
	auto sym = e->symbol;
	if (sym->id) {
		return sym->id;
	}
	if (sym->sy_type == sy_expr) {
		sym->id = spirv_emit_expr (sym->expr, ctx);
		spirv_Name (sym->id, sym->name, ctx);
		for (auto attr = sym->attributes; attr; attr = attr->next) {
			if (strcmp (attr->name, "constant_id") == 0) {
				int specid = expr_integral (attr->params);
				spirv_Decorate (sym->id, SpvDecorationSpecId,
								&specid, ev_int, ctx);
			}
		}
	} else if (sym->sy_type == sy_func) {
		auto func = sym->metafunc->func;
		sym->id = spirv_function_ref (func, ctx);
	} else if (sym->sy_type == sy_var) {
		sym->id = spirv_variable (sym, ctx);
	} else {
		internal_error (e, "unexpected symbol type: %s for %s",
						symtype_str (sym->sy_type), sym->name);
	}
	return sym->id;
}

static unsigned
spirv_temp (const expr_t *e, spirvctx_t *ctx)
{
	return 0;//FIXME don't want
}

static unsigned
spirv_value (const expr_t *e, spirvctx_t *ctx)
{
	auto value = e->value;
	if (!value->id) {
		unsigned tid = type_id (value->type, ctx);
		unsigned op = SpvOpConstant;
		int val_size = 1;
		pr_ulong_t val = 0;
		if (is_bool (value->type)) {
			op = value->int_val ? SpvOpConstantTrue : SpvOpConstantFalse;
			val_size = 0;
		} else {
			if (type_size (value->type) == 1) {
				val = value->uint_val;
				val_size = 1;
			} else if (type_size (value->type) == 2) {
				val = value->ulong_val;
				val_size = 2;
			} else {
				internal_error (e, "not implemented");
			}
		}
		if (value->is_constexpr) {
			op += SpvOpSpecConstantTrue - SpvOpConstantTrue;
		}
		auto globals = ctx->module->globals;
		if (op == SpvOpConstant && !val) {
			auto insn = spirv_new_insn (SpvOpConstantNull, 3, globals);
			INSN (insn, 1) = tid;
			INSN (insn, 2) = value->id = spirv_id (ctx);
		} else {
			auto insn = spirv_new_insn (op, 3 + val_size, globals);
			INSN (insn, 1) = tid;
			INSN (insn, 2) = value->id = spirv_id (ctx);
			if (val_size > 0) {
				INSN (insn, 3) = val;
			}
			if (val_size > 1) {
				INSN (insn, 4) = val >> 32;
			}
		}
	}
	return value->id;
}

static unsigned
spirv_assign (const expr_t *e, spirvctx_t *ctx)
{
	unsigned src = spirv_emit_expr (e->assign.src, ctx);
	unsigned dst = 0;

	if (is_temp (e->assign.dst)) {
		// spir-v uses SSA, so temps cannot be assigned to directly, so instead
		// use the temp expression as a reference for the result id of the
		// rhs of the assignment.
		//FIXME const cast (store elsewhere)
		((expr_t *) e->assign.dst)->id = src;
		return src;
	}
	if (is_deref (e->assign.dst)) {
		auto ptr = e->assign.dst->expr.e1;
		dst = spirv_emit_expr (ptr, ctx);
	}

	if (!dst) {
		internal_error (e, "invalid assignment?");
	}

	auto insn = spirv_new_insn (SpvOpStore, 3, ctx->code_space);
	INSN (insn, 1) = dst;
	INSN (insn, 2) = src;
	return 0;
}

static unsigned
spirv_call (const expr_t *call, spirvctx_t *ctx)
{
	int num_args = list_count (&call->branch.args->list);
	const expr_t *args[num_args + 1];
	unsigned arg_ids[num_args + 1];
	auto func = call->branch.target;
	auto func_type = get_type (func);
	auto ret_type = func_type->func.ret_type;

	unsigned func_id = spirv_emit_expr (func, ctx);
	unsigned ret_type_id = spirv_type_id (ret_type, ctx);

	list_scatter_rev (&call->branch.args->list, args);
	for (int i = 0; i < num_args; i++) {
		auto a = args[i];
		if (func_type->func.param_quals[i] == pq_const) {
			arg_ids[i] = spirv_emit_expr (a, ctx);
		} else {
			auto psym = new_symbol ("param");
			psym->type = tagged_reference_type (SpvStorageClassFunction, get_type (a));
			psym->sy_type = sy_var;
			psym->var.storage = SpvStorageClassFunction;
			psym->id = spirv_variable (psym, ctx);
			psym->lvalue = true;
			arg_ids[i] = psym->id;
			if (func_type->func.param_quals[i] != pq_out) {
				if (a->type == ex_inout) {
					a = a->inout.in;
				}
				auto pexpr = new_symbol_expr (psym);
				auto assign = assign_expr (pexpr, a);
				spirv_emit_expr (assign, ctx);
			}
		}
	}

	unsigned id = spirv_id (ctx);
	auto insn = spirv_new_insn (SpvOpFunctionCall, 4 + num_args,
								ctx->code_space);
	INSN(insn, 1) = ret_type_id;
	INSN(insn, 2) = id;
	INSN(insn, 3) = func_id;
	for (int i = 0; i < num_args; i++) {
		INSN(insn, 4 + i) = arg_ids[i];
	}
	return id;
}

static unsigned
spirv_branch (const expr_t *e, spirvctx_t *ctx)
{
	if (e->branch.type == pr_branch_call) {
		return spirv_call (e, ctx);
	}
	//FIXME
	return 1;
}

static unsigned
spirv_return (const expr_t *e, spirvctx_t *ctx)
{
	if (e->retrn.ret_val) {
		unsigned ret_id = spirv_emit_expr (e->retrn.ret_val, ctx);
		auto insn = spirv_new_insn (SpvOpReturnValue, 2, ctx->code_space);
		INSN (insn, 1) = ret_id;
	} else {
		spirv_new_insn (SpvOpReturn, 1, ctx->code_space);
	}
	return 0;
}

static unsigned
spirv_extend (const expr_t *e, spirvctx_t *ctx)
{
	auto src_type = get_type (e->extend.src);
	auto res_type = e->extend.type;
	auto src_base = base_type (src_type);
	auto res_base = base_type (res_type);
	int  src_width = type_width (src_type);
	int  res_width = type_width (res_type);

	if (res_base != src_type || res_width <= src_width) {
		internal_error (e, "invalid type combination for extend");
	}
	unsigned sid = spirv_emit_expr (e->extend.src, ctx);
	unsigned eid = sid;
	static int id_counts[4][4] = {
		{-1, 2, 3, 4},
		{-1,-1, 2, 2},
		{-1,-1,-1, 2},
		{-1,-1,-1,-1},
	};
	int nids = id_counts[src_width - 1][res_width - 1];
	if (e->extend.extend != 2
		|| !(src_width == 1 || (src_width == 2 && res_width == 4))) {
		int val = e->extend.extend == 3 ? -1 : e->extend.extend == 1 ? 1 : 0;
		scoped_src_loc (e);
		auto ext = cast_expr (src_base, new_int_expr (val, false));
		eid = spirv_emit_expr (ext, ctx);
		if (src_width == 2 && res_width == 4) {
			nids += 1;
		}
	}
	unsigned cids[4] = {};
	int start = 0;
	if (e->extend.reverse) {
		if (src_width > 1) {
			internal_error (e, "reverse extend for width %d not implemented",
							src_width);
		}
		cids[0] = eid;
		cids[1] = eid;
		cids[2] = eid;
		cids[3] = sid;
		start = 4 - nids;
	} else {
		cids[0] = sid;
		cids[1] = eid;
		cids[2] = eid;
		cids[3] = eid;
	}
	int tid = type_id (res_type, ctx);
	int id = spirv_id (ctx);
	auto insn = spirv_new_insn (SpvOpCompositeConstruct, 3 + nids,
								ctx->code_space);
	INSN (insn, 1) = tid;
	INSN (insn, 2) = id;
	for (int i = 0; i < nids; i++) {
		INSN (insn, 3 + i) = cids[start + i];
	}
	return id;
}

static unsigned
spirv_field (const expr_t *e, spirvctx_t *ctx)
{
	auto res_type = get_type (e);
	ex_list_t list = {};
	// convert the left-branching field expression chain to a list
	for (; e->type == ex_field; e = e->field.object) {
		list_prepend (&list, e);
	}
	int num_fields = list_count (&list);
	// e is now the base object of the field expression chain
	auto base_type = get_type (e);
	unsigned base_id = spirv_emit_expr (e, ctx);
	int op = SpvOpCompositeExtract;

	auto acc_type = res_type;
	bool literal_ind = true;
	if (is_pointer (base_type) || is_reference (base_type)) {
		unsigned storage = base_type->fldptr.tag;
		acc_type = tagged_reference_type (storage, res_type);
		op = SpvOpAccessChain;
		literal_ind = false;
	}

	int acc_type_id = type_id (acc_type, ctx);
	int id = spirv_id (ctx);
	auto insn = spirv_new_insn (op, 4 + num_fields, ctx->code_space);
	INSN (insn, 1) = acc_type_id;
	INSN (insn, 2) = id;
	INSN (insn, 3) = base_id;
	auto field_ind = &INSN (insn, 4);
	for (auto l = list.head; l; l = l->next) {
		if (l->expr->field.member->type != ex_symbol) {
			internal_error (l->expr->field.member, "not a symbol");
		}
		auto sym = l->expr->field.member->symbol;
		if (literal_ind) {
			*field_ind++ = sym->id;
		} else {
			auto ind = new_uint_expr (sym->id);
			*field_ind++ = spirv_emit_expr (ind, ctx);
		}
	}
	if (acc_type != res_type) {
		// base is a pointer or reference so load the value
		unsigned ptr_id = id;
		int res_type_id = type_id (res_type, ctx);

		id = spirv_id (ctx);
		insn = spirv_new_insn (SpvOpLoad, 4, ctx->code_space);
		INSN (insn, 1) = res_type_id;
		INSN (insn, 2) = id;
		INSN (insn, 3) = ptr_id;
	}
	return id;
}

static unsigned
spirv_emit_expr (const expr_t *e, spirvctx_t *ctx)
{
	static spirv_expr_f funcs[ex_count] = {
		[ex_block] = spirv_block,
		[ex_expr] = spirv_expr,
		[ex_uexpr] = spirv_uexpr,
		[ex_symbol] = spirv_symbol,
		[ex_temp] = spirv_temp,//FIXME don't want
		[ex_value] = spirv_value,
		[ex_assign] = spirv_assign,
		[ex_branch] = spirv_branch,
		[ex_return] = spirv_return,
		[ex_extend] = spirv_extend,
		[ex_field] = spirv_field,
	};

	if (e->type >= ex_count) {
		internal_error (e, "bad sub-expression type: %d", e->type);
	}
	if (!funcs[e->type]) {
		internal_error (e, "unexpected sub-expression type: %s",
						expr_names[e->type]);
	}

	if (!e->id) {
		unsigned id = funcs[e->type] (e, ctx);
		//FIXME const cast (store elsewhere)
		((expr_t *) e)->id = id;
	}
	return e->id;
}

bool
spirv_write (struct pr_info_s *pr, const char *filename)
{
	pr->module->entry_points = defspace_new (ds_backed);
	pr->module->exec_modes = defspace_new (ds_backed);
	pr->module->strings = defspace_new (ds_backed);
	pr->module->names = defspace_new (ds_backed);
	pr->module->module_processed = defspace_new (ds_backed);
	pr->module->decorations = defspace_new (ds_backed);
	pr->module->globals = defspace_new (ds_backed);
	pr->module->func_declarations = defspace_new (ds_backed);
	pr->module->func_definitions = defspace_new (ds_backed);
	DARRAY_INIT (&pr->module->interface_syms, 16);

	spirvctx_t ctx = {
		.module = pr->module,
		.code_space = defspace_new (ds_backed),
		.decl_space = defspace_new (ds_backed),
		.type_ids = DARRAY_STATIC_INIT (64),
		.id = 0,
	};
	auto space = defspace_new (ds_backed);
	auto header = spirv_new_insn (0, 5, space);
	INSN (header, 0) = SpvMagicNumber;
	INSN (header, 1) = SpvVersion;
	INSN (header, 2) = 0;	// FIXME get a magic number for QFCC
	INSN (header, 3) = 0;	// Filled in later
	INSN (header, 4) = 0;	// Reserved

	for (auto cap = pr->module->capabilities.head; cap; cap = cap->next) {
		spirv_Capability (expr_uint (cap->expr), space);
	}
	for (auto ext = pr->module->extensions.head; ext; ext = ext->next) {
		spirv_Extension (expr_string (ext->expr), space);
	}
	for (auto imp = pr->module->extinst_imports.head; imp; imp = imp->next) {
		//FIXME need to store id where it can be used for instructions
		spirv_ExtInstImport (expr_string (imp->expr), &ctx);
	}

	spirv_MemoryModel (expr_uint (pr->module->addressing_model),
					   expr_uint (pr->module->memory_model), space);


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

	auto mod = pr->module;
	INSN (header, 3) = ctx.id + 1;
	ADD_DATA (space, mod->entry_points);
	ADD_DATA (space, mod->exec_modes);
	ADD_DATA (space, mod->strings);
	ADD_DATA (space, mod->names);
	ADD_DATA (space, mod->module_processed);
	ADD_DATA (space, mod->decorations);
	ADD_DATA (space, mod->globals);
	ADD_DATA (space, mod->func_declarations);
	ADD_DATA (space, mod->func_definitions);

	QFile *file = Qopen (filename, "wb");
	Qwrite (file, space->data, space->size * sizeof (pr_type_t));
	Qclose (file);
	return false;
}

void
spirv_add_capability (module_t *module, SpvCapability capability)
{
	auto cap = new_uint_expr (capability);
	list_append (&module->capabilities, cap);
}

void
spirv_add_extension (module_t *module, const char *extension)
{
	auto ext = new_string_expr (extension);
	list_append (&module->extensions, ext);
}

void
spirv_add_extinst_import (module_t *module, const char *import)
{
	auto imp = new_string_expr (import);
	list_append (&module->extinst_imports, imp);
}

void
spirv_set_addressing_model (module_t *module, SpvAddressingModel model)
{
	module->addressing_model = new_uint_expr (model);
}

void
spirv_set_memory_model (module_t *module, SpvMemoryModel model)
{
	module->memory_model = new_uint_expr (model);
}

static bool
spirv_value_too_large (const type_t *val_type)
{
	return false;
}

static void
spirv_create_param (symtab_t *parameters, symbol_t *param, param_qual_t qual)
{
	if (!param->name) {
		if (qual == pq_out) {
			warning (0, "unnamed out parameter");
		}
		param->name = save_string ("");

		symtab_appendsymbol (parameters, param);
	} else {
		symtab_addsymbol (parameters, param);
	}
	auto type = param->type;
	if (qual != pq_const) {
		param->lvalue = true;
		type = tagged_reference_type (SpvStorageClassFunction, type);
	}
	param->type = type;
}

static void
spirv_build_scope (symbol_t *fsym)
{
	function_t *func = fsym->metafunc->func;

	for (param_t *p = fsym->params; p; p = p->next) {
		symbol_t   *param;
		if (is_void (p->type)) {
			if (p->name) {
				error (0, "invalid parameter type for %s", p->name);
			} else if (p != fsym->params || p->next) {
				error (0, "void must be the only parameter");
				continue;
			} else {
				continue;
			}
		}
		param = new_symbol_type (p->name, p->type);
		spirv_create_param (func->parameters, param, p->qual);
	}
}

static void
spirv_build_code (function_t *func, const expr_t *statements)
{
	func->exprs = statements;
}

static void
spirv_declare_sym (specifier_t spec, const expr_t *init, symtab_t *symtab,
				   expr_t *block)
{
	symbol_t   *sym = spec.sym;
	symbol_t   *check = symtab_lookup (symtab, sym->name);
	if (check && check->table == symtab) {
		error (0, "%s redefined", sym->name);
	}
	auto storage = spirv_storage_class (spec.storage);
	sym->type = tagged_reference_type (storage, sym->type);
	sym->lvalue = !spec.is_const;
	sym->sy_type = sy_var;
	sym->var.storage = spec.storage;
	symtab_addsymbol (symtab, sym);
	if (symtab->type == stab_local) {
		if (init) {
			if (is_constexpr (init)) {
			} else if (block) {
				auto r = pointer_deref (new_symbol_expr (sym));
				auto e = assign_expr (r, init);
				append_expr (block, e);
			} else {
				error (init, "non-constant initializer");
			}
		}
	}
}

target_t spirv_target = {
	.value_too_large = spirv_value_too_large,
	.build_scope = spirv_build_scope,
	.build_code = spirv_build_code,
	.declare_sym = spirv_declare_sym,
};
