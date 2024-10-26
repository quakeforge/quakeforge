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

#include <spirv/unified1/GLSL.std.450.h>

#include "QF/quakeio.h"

#include "tools/qfcc/include/attribute.h"
#include "tools/qfcc/include/def.h"
#include "tools/qfcc/include/defspace.h"
#include "tools/qfcc/include/diagnostic.h"
#include "tools/qfcc/include/function.h"
#include "tools/qfcc/include/qfcc.h"
#include "tools/qfcc/include/spirv.h"
#include "tools/qfcc/include/statements.h"
#include "tools/qfcc/include/strpool.h"
#include "tools/qfcc/include/symtab.h"
#include "tools/qfcc/include/target.h"
#include "tools/qfcc/include/type.h"

typedef struct spirvctx_s {
	defspace_t *space;
	defspace_t *linkage;
	defspace_t *strings;
	defspace_t *names;
	defspace_t *annotations;
	defspace_t *types;
	defspace_t *code;
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
	auto def = new_def (nullptr, nullptr, space, sc_static);
	def->offset = defspace_alloc_loc (space, size);

	D_INT (def) = (op & SpvOpCodeMask) | (size << SpvWordCountShift);
	return def;
}

static def_t *
spirv_str_insn (int op, int offs, int extra, const char *str, defspace_t *space)
{
	int len = strlen (str) + 1;
	int str_size = RUP(len, 4) / 4;
	int size = offs + str_size + extra;
	auto def = spirv_new_insn (op, size, space);
	D_var_o(int, def, str_size - 1) = 0;
	memcpy (&D_var_o(int, def, offs), str, len);
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
spirv_Extension (const char *ext, defspace_t *space)
{
	spirv_str_insn (SpvOpExtension, 1, 0, ext, space);
}

static unsigned
spirv_ExtInstImport (const char *imp, spirvctx_t *ctx)
{
	auto def = spirv_str_insn (SpvOpExtInstImport, 2, 0, imp, ctx->space);
	int id = spirv_id (ctx);
	D_var_o(int, def, 1) = id;
	return id;
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
	auto def = spirv_str_insn (SpvOpString, 2, 0, name, ctx->strings);
	int id = spirv_id (ctx);
	D_var_o(int, def, 1) = id;
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
spirv_Decorate (unsigned id, SpvDecoration decoration, void *literal,
				etype_t type, spirvctx_t *ctx)
{
	if (type != ev_int) {
		internal_error (0, "unexpected type");
	}
	int size = pr_type_size[type];
	auto def = spirv_new_insn (SpvOpDecorate, 3 + size, ctx->annotations);
	D_var_o(int, def, 1) = id;
	D_var_o(int, def, 2) = decoration;
	if (type == ev_int) {
		D_var_o(int, def, 3) = *(int *)literal;
	}
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
	spirv_add_type_id (type, ft_id, ctx);
	return ft_id;
}

static unsigned
type_id (const type_t *type, spirvctx_t *ctx)
{
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

static extinst_t glsl_450 = {
	.name = "GLSL.std.450"
};

static spvop_t spv_ops[] = {
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

static unsigned spirv_emit_expr (const expr_t *e, spirvctx_t *ctx);

static unsigned
spirv_uexpr (const expr_t *e, spirvctx_t *ctx)
{
	if (e->expr.op == '+') {
		return spirv_emit_expr (e->expr.e1, ctx);
	}

	auto op_name = convert_op (e->expr.op);
	if (!op_name) {
		internal_error (e, "unexpected unary op: %d\n", e->expr.op);
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
		auto def = spirv_new_insn (SpvOpSpecConstantOp, 5, ctx->types);
		D_var_o(int, def, 1) = tid;
		D_var_o(int, def, 2) = id = spirv_id (ctx);
		D_var_o(int, def, 3) = spv_op->op;
		D_var_o(int, def, 4) = uid;
	} else {
		auto def = spirv_new_insn (spv_op->op, 4, ctx->types);
		D_var_o(int, def, 1) = tid;
		D_var_o(int, def, 2) = id = spirv_id (ctx);
		D_var_o(int, def, 3) = uid;
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
		auto def = spirv_new_insn (SpvOpSpecConstantOp, 6, ctx->types);
		D_var_o(int, def, 1) = tid;
		D_var_o(int, def, 2) = id = spirv_id (ctx);
		D_var_o(int, def, 3) = spv_op->op;
		D_var_o(int, def, 4) = bid1;
		D_var_o(int, def, 5) = bid2;
	} else {
		auto def = spirv_new_insn (spv_op->op, 5, ctx->types);
		D_var_o(int, def, 1) = tid;
		D_var_o(int, def, 2) = id = spirv_id (ctx);
		D_var_o(int, def, 3) = bid1;
		D_var_o(int, def, 4) = bid2;
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
	} else {
		internal_error (e, "unexpected symbol type: %s for %s",
						symtype_str (sym->sy_type), sym->name);
	}
	return sym->id;
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
		if (op == SpvOpConstant && !val) {
			auto def = spirv_new_insn (SpvOpConstantNull, 3, ctx->types);
			D_var_o(int, def, 1) = tid;
			D_var_o(int, def, 2) = value->id = spirv_id (ctx);
		} else {
			auto def = spirv_new_insn (op, 3 + val_size, ctx->types);
			D_var_o(int, def, 1) = tid;
			D_var_o(int, def, 2) = value->id = spirv_id (ctx);
			if (val_size > 0) {
				D_var_o(int, def, 3) = val;
			}
			if (val_size > 1) {
				D_var_o(int, def, 4) = val >> 32;
			}
		}
	}
	return value->id;
}

static unsigned
spirv_emit_expr (const expr_t *e, spirvctx_t *ctx)
{
	static spirv_expr_f funcs[ex_count] = {
		[ex_expr] = spirv_expr,
		[ex_uexpr] = spirv_uexpr,
		[ex_symbol] = spirv_symbol,
		[ex_value] = spirv_value,
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
	spirvctx_t ctx = {
		.space = defspace_new (ds_backed),
		.linkage = defspace_new (ds_backed),
		.strings = defspace_new (ds_backed),
		.names = defspace_new (ds_backed),
		.annotations = defspace_new (ds_backed),
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

	for (auto cap = pr->module->capabilities.head; cap; cap = cap->next) {
		spirv_Capability (expr_uint (cap->expr), ctx.space);
	}
	for (auto ext = pr->module->extensions.head; ext; ext = ext->next) {
		spirv_Extension (expr_string (ext->expr), ctx.space);
	}
	for (auto imp = pr->module->extinst_imports.head; imp; imp = imp->next) {
		//FIXME need to store id where it can be used for instructions
		spirv_ExtInstImport (expr_string (imp->expr), &ctx);
	}

	spirv_MemoryModel (expr_uint (pr->module->addressing_model),
					   expr_uint (pr->module->memory_model), ctx.space);


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
	defspace_add_data (ctx.space, ctx.annotations->data, ctx.annotations->size);
	defspace_add_data (ctx.space, ctx.types->data, ctx.types->size);
	defspace_add_data (ctx.space, ctx.code->data, ctx.code->size);

	QFile *file = Qopen (filename, "wb");
	Qwrite (file, ctx.space->data, ctx.space->size * sizeof (pr_type_t));
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
		type = reference_type (type);
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

target_t spirv_target = {
	.value_too_large = spirv_value_too_large,
	.build_scope = spirv_build_scope,
};
