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

#include "QF/hash.h"
#include "QF/quakeio.h"
#include "QF/va.h"

#include "tools/qfcc/include/algebra.h"
#include "tools/qfcc/include/attribute.h"
#include "tools/qfcc/include/def.h"
#include "tools/qfcc/include/defspace.h"
#include "tools/qfcc/include/diagnostic.h"
#include "tools/qfcc/include/dot.h"
#include "tools/qfcc/include/function.h"
#include "tools/qfcc/include/iface_block.h"
#include "tools/qfcc/include/image.h"
#include "tools/qfcc/include/options.h"
#include "tools/qfcc/include/qfcc.h"
#include "tools/qfcc/include/spirv.h"
#include "tools/qfcc/include/spirv_grammar.h"
#include "tools/qfcc/include/statements.h"
#include "tools/qfcc/include/strpool.h"
#include "tools/qfcc/include/struct.h"
#include "tools/qfcc/include/switch.h"
#include "tools/qfcc/include/symtab.h"
#include "tools/qfcc/include/target.h"
#include "tools/qfcc/include/type.h"
#include "tools/qfcc/include/value.h"

#define INSN(i,o) D_var_o(int,(i),o)
#define ADD_DATA(d,s) defspace_add_data((d), (s)->data, (s)->size)

static type_t type_spv_ptrarith = {
	.type = ev_invalid,
	.meta = ty_struct,
};

typedef struct spirvctx_s {
	module_t   *module;

	defspace_t *decl_space;
	defspace_t *code_space;

	const spirv_grammar_t *core;

	struct DARRAY_TYPE (unsigned) type_ids;
	struct DARRAY_TYPE (unsigned) expr_ids;		///< Flushed per function
	struct DARRAY_TYPE (unsigned) label_ids;
	struct DARRAY_TYPE (function_t *) func_queue;
	strpool_t  *strpool;
	unsigned id;
} spirvctx_t;

static unsigned spirv_value (const expr_t *e, spirvctx_t *ctx);

static unsigned
spirv_id (spirvctx_t *ctx)
{
	return ++ctx->id;
}

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

static unsigned
spirv_expr_id (const expr_t *expr, spirvctx_t *ctx)
{
	if (expr->id < ctx->expr_ids.size && ctx->expr_ids.a[expr->id]) {
		return ctx->expr_ids.a[expr->id];
	}
	return 0;
}

static void
spirv_add_expr_id (const expr_t *expr, unsigned id, spirvctx_t *ctx)
{
	if (expr->id >= ctx->expr_ids.size) {
		size_t base = ctx->expr_ids.size;
		DARRAY_RESIZE (&ctx->expr_ids, expr->id + 1);
		while (base < expr->id + 1) {
			ctx->expr_ids.a[base++] = 0;
		}
	}
	ctx->expr_ids.a[expr->id] = id;
}

static unsigned
spirv_label_id (const ex_label_t *label, spirvctx_t *ctx)
{
	if (label->id < ctx->label_ids.size && ctx->label_ids.a[label->id]) {
		return ctx->label_ids.a[label->id];
	}
	unsigned id = spirv_id (ctx);
	if (label->id >= ctx->label_ids.size) {
		size_t base = ctx->label_ids.size;
		DARRAY_RESIZE (&ctx->label_ids, label->id + 1);
		while (base < label->id + 1) {
			ctx->label_ids.a[base++] = 0;
		}
	}
	ctx->label_ids.a[label->id] = id;
	return id;
}

static bool
is_block_terminated (defspace_t *code_space)
{
	def_t *last_insn = nullptr;
	if (code_space->def_tail != &code_space->defs) {
		last_insn = (def_t *) code_space->def_tail;
	}
	if (last_insn
		&& ((INSN(last_insn, 0) & SpvOpCodeMask) == SpvOpReturn
			|| (INSN(last_insn, 0) & SpvOpCodeMask) == SpvOpReturnValue
			|| (INSN(last_insn, 0) & SpvOpCodeMask) == SpvOpUnreachable
			|| (INSN(last_insn, 0) & SpvOpCodeMask) == SpvOpBranchConditional
			|| (INSN(last_insn, 0) & SpvOpCodeMask) == SpvOpBranch
			|| (INSN(last_insn, 0) & SpvOpCodeMask) == SpvOpKill)) {
		return true;
	}
	return false;
}

static def_t *
spirv_new_insn (int op, int size, defspace_t *space, spirvctx_t *ctx)
{
	auto insn = new_def (nullptr, nullptr, space, sc_static);
	insn->offset = defspace_alloc_highwater (space, size);

	INSN (insn, 0) = (op & SpvOpCodeMask) | (size << SpvWordCountShift);
	return insn;
}

static def_t *
spirv_str_insn (int op, int offs, int extra, const char *str,
				defspace_t *space, spirvctx_t *ctx)
{
	int len = strlen (str) + 1;
	int str_size = RUP(len, 4) / 4;
	int size = offs + str_size + extra;
	auto insn = spirv_new_insn (op, size, space, ctx);
	INSN (insn, offs + str_size - 1) = 0;
	memcpy (&INSN (insn, offs), str, len);
	return insn;
}

static void
spirv_Capability (SpvCapability capability, defspace_t *space, spirvctx_t *ctx)
{
	auto insn = spirv_new_insn (SpvOpCapability, 2, space, ctx);
	INSN (insn, 1) = capability;
}

static void
spirv_Extension (const char *ext, defspace_t *space, spirvctx_t *ctx)
{
	spirv_str_insn (SpvOpExtension, 1, 0, ext, space, ctx);
}

static unsigned
spirv_ExtInstImport (const char *imp, spirvctx_t *ctx)
{
	auto space = ctx->module->extinst_imports->space;
	auto insn = spirv_str_insn (SpvOpExtInstImport, 2, 0, imp, space, ctx);
	int id = spirv_id (ctx);
	INSN (insn, 1) = id;
	return id;
}

static unsigned
spirv_extinst_import (module_t *module, const char *import, spirvctx_t *ctx)
{
	if (!module->extinst_imports) {
		module->extinst_imports = new_symtab (nullptr, stab_enum);
		module->extinst_imports->space = defspace_new (ds_backed);
	}
	auto imp = symtab_lookup (module->extinst_imports, import);
	if (!imp) {
		imp = new_symbol (import);
		imp->sy_type = sy_offset;
		symtab_addsymbol (module->extinst_imports, imp);
	}
	if (ctx && !imp->offset) {
		imp->offset = spirv_ExtInstImport (import, ctx);
	}
	return imp->offset;
}

static void
spirv_MemoryModel (SpvAddressingModel addressing, SpvMemoryModel memory,
				   defspace_t *space, spirvctx_t *ctx)
{
	auto insn = spirv_new_insn (SpvOpMemoryModel, 3, space, ctx);
	INSN (insn, 1) = addressing;
	INSN (insn, 2) = memory;
}

static unsigned
spirv_String (const char *name, spirvctx_t *ctx)
{
	auto strid = strpool_addstrid (ctx->strpool, name);
	if (strid->id) {
		return strid->id;
	}
	auto strings = ctx->module->strings;
	auto insn = spirv_str_insn (SpvOpString, 2, 0, name, strings, ctx);
	strid->id = spirv_id (ctx);
	INSN (insn, 1) = strid->id;
	return strid->id;
}

static void
spirv_Source (unsigned lang, unsigned version, unsigned srcid,
			  const char *src_str, spirvctx_t *ctx)
{
	auto strings = ctx->module->strings;
	auto insn = spirv_new_insn (SpvOpSource, 4, strings, ctx);
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
	auto insn = spirv_new_insn (SpvOpName, 2 + RUP(len, 4) / 4, names, ctx);
	INSN (insn, 1) = id;
	memcpy (&INSN (insn, 2), name, len);
}

static void
spirv_Decorate (unsigned id, SpvDecoration decoration, spirvctx_t *ctx)
{
	auto decorations = ctx->module->decorations;
	auto insn = spirv_new_insn (SpvOpDecorate, 3, decorations, ctx);
	INSN (insn, 1) = id;
	INSN (insn, 2) = decoration;
}

static void
spirv_DecorateLiteral (unsigned id, SpvDecoration decoration, void *literal,
					   etype_t type, spirvctx_t *ctx)
{
	if (type != ev_int) {
		internal_error (0, "unexpected type");
	}
	int size = pr_type_size[type];
	auto decorations = ctx->module->decorations;
	auto insn = spirv_new_insn (SpvOpDecorate, 3 + size, decorations, ctx);
	INSN (insn, 1) = id;
	INSN (insn, 2) = decoration;
	if (type == ev_int) {
		INSN (insn, 3) = *(int *)literal;
	}
}

static void
spirv_MemberDecorate (unsigned id, unsigned member,
					  SpvDecoration decoration, spirvctx_t *ctx)
{
	auto decorations = ctx->module->decorations;
	auto insn = spirv_new_insn (SpvOpMemberDecorate, 4, decorations, ctx);
	INSN (insn, 1) = id;
	INSN (insn, 2) = member;
	INSN (insn, 3) = decoration;
}

static void
spirv_MemberDecorateLiteral (unsigned id, unsigned member,
							 SpvDecoration decoration,
							 void *literal, etype_t type, spirvctx_t *ctx)
{
	if (type != ev_int) {
		internal_error (0, "unexpected type");
	}
	int size = pr_type_size[type];
	auto decorations = ctx->module->decorations;
	auto insn = spirv_new_insn (SpvOpMemberDecorate, 4 + size,
								decorations, ctx);
	INSN (insn, 1) = id;
	INSN (insn, 2) = member;
	INSN (insn, 3) = decoration;
	if (type == ev_int) {
		INSN (insn, 4) = *(int *)literal;
	}
}

static void
spirv_decorate_id (unsigned id, attribute_t *attributes, spirvctx_t *ctx)
{
	for (auto attr = attributes; attr; attr = attr->next) {
		unsigned decoration = spirv_enum_val ("Decoration", attr->name);
		if (attr->params) {
			//FIXME some decorations have more than one parameter (rare)
			int val;
			if (is_string_val (attr->params)) {
				//FIXME should get kind from decoration
				const char *name = expr_string (attr->params);
				val = spirv_enum_val (attr->name, name);
			} else {
				val = expr_integral (attr->params);
			}
			spirv_DecorateLiteral (id, decoration, &val, ev_int, ctx);
		} else {
			spirv_Decorate (id, decoration, ctx);
		}
	}
}

static void
spirv_member_decorate_id (unsigned id, int member, attribute_t *attributes,
						  spirvctx_t *ctx)
{
	for (auto attr = attributes; attr; attr = attr->next) {
		unsigned decoration = spirv_enum_val ("Decoration", attr->name);
		if (attr->params) {
			//FIXME some decorations have more than one parameter (rare)
			int val;
			if (is_string_val (attr->params)) {
				//FIXME should get kind from decoration
				const char *name = expr_string (attr->params);
				val = spirv_enum_val (attr->name, name);
			} else {
				val = expr_integral (attr->params);
			}
			spirv_MemberDecorateLiteral (id, member, decoration,
										 &val, ev_int, ctx);
		} else {
			spirv_MemberDecorate (id, member, decoration, ctx);
		}
	}
}

static void
spirv_MemberName (unsigned id, unsigned member, const char *name,
				  spirvctx_t *ctx)
{
	int len = strlen (name) + 1;
	auto names = ctx->module->names;
	auto insn = spirv_new_insn (SpvOpMemberName, 3 + RUP(len, 4) / 4,
								names, ctx);
	INSN (insn, 1) = id;
	INSN (insn, 2) = member;
	memcpy (&INSN (insn, 3), name, len);
}

static void
spirv_DebugLine (const expr_t *e, spirvctx_t *ctx)
{
	unsigned file_id = spirv_String (GETSTR (e->loc.file), ctx);
	auto insn = spirv_new_insn (SpvOpLine, 4, ctx->code_space, ctx);
	INSN (insn, 1) = file_id;
	INSN (insn, 2) = e->loc.line;
	INSN (insn, 3) = e->loc.column;
}

static unsigned spirv_Type (const type_t *type, spirvctx_t *ctx);

static unsigned
spirv_TypeVoid (spirvctx_t *ctx)
{
	auto globals = ctx->module->globals;
	auto insn = spirv_new_insn (SpvOpTypeVoid, 2, globals, ctx);
	INSN (insn, 1) = spirv_id (ctx);
	return INSN (insn, 1);
}

static unsigned
spirv_TypeInt (unsigned bitwidth, bool is_signed, spirvctx_t *ctx)
{
	auto globals = ctx->module->globals;
	auto insn = spirv_new_insn (SpvOpTypeInt, 4, globals, ctx);
	INSN (insn, 1) = spirv_id (ctx);
	INSN (insn, 2) = bitwidth;
	INSN (insn, 3) = is_signed;
	return INSN (insn, 1);
}

static unsigned
spirv_TypeFloat (unsigned bitwidth, spirvctx_t *ctx)
{
	auto globals = ctx->module->globals;
	auto insn = spirv_new_insn (SpvOpTypeFloat, 3, globals, ctx);
	INSN (insn, 1) = spirv_id (ctx);
	INSN (insn, 2) = bitwidth;
	return INSN (insn, 1);
}

static unsigned
spirv_TypeVector (unsigned base, unsigned width, spirvctx_t *ctx)
{
	auto globals = ctx->module->globals;
	auto insn = spirv_new_insn (SpvOpTypeVector, 4, globals, ctx);
	INSN (insn, 1) = spirv_id (ctx);
	INSN (insn, 2) = base;
	INSN (insn, 3) = width;
	return INSN (insn, 1);
}

static unsigned
spirv_TypeMatrix (unsigned col_type, unsigned columns, spirvctx_t *ctx)
{
	auto globals = ctx->module->globals;
	auto insn = spirv_new_insn (SpvOpTypeMatrix, 4, globals, ctx);
	INSN (insn, 1) = spirv_id (ctx);
	INSN (insn, 2) = col_type;
	INSN (insn, 3) = columns;
	return INSN (insn, 1);
}

static unsigned
spirv_TypePointer (const type_t *type, spirvctx_t *ctx)
{
	auto globals = ctx->module->globals;

	unsigned id = spirv_id (ctx);
	spirv_add_type_id (type, id, ctx);
	auto rtype = dereference_type (type);
	if (type->fldptr.tag == SpvStorageClassPhysicalStorageBuffer
		&& is_struct (rtype)) {
		auto fwd = spirv_new_insn (SpvOpTypeForwardPointer, 3, globals, ctx);
		INSN (fwd, 1) = id;
		INSN (fwd, 2) = type->fldptr.tag;
	}
	unsigned rid = spirv_Type (rtype, ctx);

	auto insn = spirv_new_insn (SpvOpTypePointer, 4, globals, ctx);
	INSN (insn, 1) = id;
	INSN (insn, 2) = type->fldptr.tag;
	INSN (insn, 3) = rid;
	spirv_decorate_id (id, type->attributes, ctx);
	return id;
}

static unsigned
spirv_type_array (unsigned tid, unsigned count, spirvctx_t *ctx)
{
	unsigned id = spirv_id (ctx);
	unsigned count_id = spirv_value (new_int_expr (count, false), ctx);
	auto globals = ctx->module->globals;
	auto insn = spirv_new_insn (SpvOpTypeArray, 4, globals, ctx);
	INSN (insn, 1) = id;
	INSN (insn, 2) = tid;
	INSN (insn, 3) = count_id;
	return id;
}

static unsigned
spirv_type_runtime_array (unsigned tid, spirvctx_t *ctx)
{
	unsigned id = spirv_id (ctx);
	auto globals = ctx->module->globals;
	auto insn = spirv_new_insn (SpvOpTypeRuntimeArray, 3, globals, ctx);
	INSN (insn, 1) = id;
	INSN (insn, 2) = tid;
	return id;
}

static unsigned
spirv_emit_symtab (symtab_t *symtab, spirvctx_t *ctx)
{
	int num_members = 0;
	for (auto s = symtab->symbols; s; s = s->next) {
		if (s->sy_type != sy_offset) {
			continue;
		}
		num_members++;
	}
	if (!num_members) {
		error (0, "0 sized struct");
		return 0;
	}
	unsigned member_types[num_members];
	num_members = 0;
	for (auto s = symtab->symbols; s; s = s->next) {
		if (s->sy_type != sy_offset) {
			continue;
		}
		int m = num_members++;
		member_types[m] = spirv_Type (s->type, ctx);
	}

	unsigned id = spirv_id (ctx);
	auto globals = ctx->module->globals;
	auto insn = spirv_new_insn (SpvOpTypeStruct,
								2 + num_members, globals, ctx);
	INSN (insn, 1) = id;
	memcpy (&INSN (insn, 2), member_types, sizeof (member_types));

	int i = 0;
	num_members = 0;
	for (auto s = symtab->symbols; s; s = s->next, i++) {
		if (s->sy_type != sy_offset) {
			continue;
		}
		int m = num_members++;
		spirv_MemberName (id, m, s->name, ctx);
		spirv_member_decorate_id (id, m, s->attributes, ctx);
	}
	return id;
}

static unsigned
spirv_TypeStruct (const type_t *type, spirvctx_t *ctx)
{
	auto symtab = type_symtab (type);
	unsigned id = spirv_emit_symtab (symtab, ctx);

	spirv_Name (id, unalias_type (type)->name + 4, ctx);
	return id;
}

static unsigned
spirv_TypeArray (const type_t *type, spirvctx_t *ctx)
{
	auto ele_type = dereference_type (type);
	unsigned count = type_count (type);
	unsigned tid = spirv_Type (ele_type, ctx);
	return spirv_type_array (tid, count, ctx);
}

static unsigned
spirv_TypeRuntimeArray (const type_t *type, spirvctx_t *ctx)
{
	auto ele_type = dereference_type (type);
	unsigned tid = spirv_Type (ele_type, ctx);
	return spirv_type_runtime_array (tid, ctx);
}

static void
spirv_mirror_bool (const type_t *type, unsigned id, spirvctx_t *ctx)
{
	// This is rather hacky as spir-v supports only an abstract bool type,
	// thus bool and lbool need to get the same type id
	auto base = base_type (type);
	int width = type_width (type);
	// invert the size
	if (is_bool (base)) {
		base = &type_lbool;
	} else {
		base = &type_bool;
	}
	type = vector_type (base, width);
	spirv_add_type_id (type, id, ctx);
}

static unsigned
spirv_TypeBool (const type_t *type, spirvctx_t *ctx)
{
	auto globals = ctx->module->globals;
	int id = spirv_id (ctx);
	auto insn = spirv_new_insn (SpvOpTypeBool, 2, globals, ctx);
	INSN (insn, 1) = id;
	spirv_mirror_bool (type, id, ctx);
	return id;
}

static unsigned
spirv_TypeImage (image_t *image, spirvctx_t *ctx)
{
	if (image->id) {
		return image->id;
	}
	unsigned tid = spirv_Type (image->sample_type, ctx);
	auto globals = ctx->module->globals;
	image->id = spirv_id (ctx);
	auto insn = spirv_new_insn (SpvOpTypeImage, 9, globals, ctx);
	INSN (insn, 1) = image->id;
	INSN (insn, 2) = tid;
	INSN (insn, 3) = image->dim;
	INSN (insn, 4) = image->depth;
	INSN (insn, 5) = image->arrayed;
	INSN (insn, 6) = image->multisample;
	INSN (insn, 7) = image->sampled;
	INSN (insn, 8) = image->format;
	return image->id;
}

static unsigned
spirv_TypeSampledImage (unsigned image_id, spirvctx_t *ctx)
{
	auto globals = ctx->module->globals;
	unsigned id = spirv_id (ctx);
	auto insn = spirv_new_insn (SpvOpTypeSampledImage, 3, globals, ctx);
	INSN (insn, 1) = id;
	INSN (insn, 2) = image_id;
	return id;
}

static unsigned
spirv_TypeFunction (symbol_t *fsym, spirvctx_t *ctx)
{
	int num_params = 0;
	for (auto p = fsym->params; p; p = p->next) {
		if (is_void (p->type)) {
			if (num_params || p->next) {
				internal_error (0, "void param with other params");
			}
			break;
		}
		num_params++;
	}
	auto type = fsym->type;
	if (spirv_type_id (type, ctx)) {
		return spirv_type_id (type, ctx);
	}
	unsigned ret_type = spirv_Type (fsym->type->func.ret_type, ctx);

	unsigned param_types[num_params + 1];
	num_params = 0;
	for (auto p = fsym->params; p; p = p->next) {
		auto ptype = p->type;
		if (is_void (p->type)) {
			break;
		}
		if (p->qual != pq_const) {
			ptype = tagged_reference_type (SpvStorageClassFunction, ptype);
		}
		param_types[num_params++] = spirv_Type (ptype, ctx);
	}

	unsigned ft_id = spirv_id (ctx);
	auto globals = ctx->module->globals;
	auto insn = spirv_new_insn (SpvOpTypeFunction, 3 + num_params,
								globals, ctx);
	INSN (insn, 1) = ft_id;
	INSN (insn, 2) = ret_type;
	for (int i = 0; i < num_params; i++) {
		INSN (insn, 3 + i) = param_types[i];
	}
	spirv_add_type_id (type, ft_id, ctx);
	return ft_id;
}

static unsigned
spirv_Type (const type_t *type, spirvctx_t *ctx)
{
	type = unalias_type (type);
	if (spirv_type_id (type, ctx)) {
		return spirv_type_id (type, ctx);
	}
	unsigned id = 0;
	if (is_void (type)) {
		id = spirv_TypeVoid (ctx);
	} else if (is_array (type)) {
		//FIXME should size be checked against something for validity?
		if (type_count (type)) {
			id = spirv_TypeArray (type, ctx);
		} else {
			id = spirv_TypeRuntimeArray (type, ctx);
		}
	} else if (is_vector (type)) {
		// spir-v doesn't allow duplicate non-aggregate types, so emit
		// vector as vec3
		auto vtype = vector_type (&type_float, 3);
		id = spirv_Type (vtype, ctx);
	} else if (is_quaternion (type)) {
		// spir-v doesn't allow duplicate non-aggregate types, so emit
		// quaternion as vec4
		auto qtype = vector_type (&type_float, 4);
		id = spirv_Type (qtype, ctx);
	} else if (is_matrix (type)) {
		auto ctype = column_type (type);
		unsigned cid = spirv_Type (ctype, ctx);
		id = spirv_TypeMatrix (cid, type_cols (type), ctx);
	} else if (is_algebra (type)) {
		if (type_count (type) > 1) {
			auto symtab = get_mvec_struct (type);
			id = spirv_emit_symtab (symtab, ctx);
		} else {
			auto atype = vector_type (base_type (type), type_width (type));
			id = spirv_Type (atype, ctx);
		}
	} else if (type_width (type) > 1) {
		auto btype = base_type (type);
		unsigned bid = spirv_Type (btype, ctx);
		id = spirv_TypeVector (bid, type_width (type), ctx);
		if (is_boolean (type)) {
			spirv_mirror_bool (type, id, ctx);
		}
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
		// type id added by spirv_TypePointer
		return spirv_TypePointer (type, ctx);
	} else if (is_struct (type)) {
		id = spirv_TypeStruct (type, ctx);
	} else if (is_boolean (type)) {
		id = spirv_TypeBool (type, ctx);
	} else if (is_image (type) || is_sampled_image (type)) {
		auto image = &imageset.a[type->handle.extra];
		id = spirv_TypeImage (image, ctx);
		if (type->handle.type == &type_sampled_image) {
			id = spirv_TypeSampledImage (id, ctx);
		}
	}
	if (!id) {
		dstring_t  *str = dstring_newstr ();
		print_type_str (str, type);
		internal_error (0, "can't emit type %s", str->str);
	}
	spirv_add_type_id (type, id, ctx);
	spirv_decorate_id (id, type->attributes, ctx);
	return id;
}

// put a label into decl_space. used only when starting a function
static unsigned
spirv_DeclLabel (spirvctx_t *ctx)
{
	auto insn = spirv_new_insn (SpvOpLabel, 2, ctx->decl_space, ctx);
	INSN (insn, 1) = spirv_id (ctx);
	return INSN (insn, 1);
}

static unsigned
spirv_Label (spirvctx_t *ctx)
{
	auto insn = spirv_new_insn (SpvOpLabel, 2, ctx->code_space, ctx);
	INSN (insn, 1) = spirv_id (ctx);
	return INSN (insn, 1);
}

static unsigned
spirv_LabelId (unsigned id, spirvctx_t *ctx)
{
	auto insn = spirv_new_insn (SpvOpLabel, 2, ctx->code_space, ctx);
	INSN (insn, 1) = id;
	return id;
}

static void
spirv_LoopMerge (unsigned merge, unsigned cont, spirvctx_t *ctx)
{
	auto insn = spirv_new_insn (SpvOpLoopMerge, 4, ctx->code_space, ctx);
	INSN (insn, 1) = merge;
	INSN (insn, 2) = cont;
	INSN (insn, 3) = SpvLoopControlMaskNone;
}

static void
spirv_SelectionMerge (unsigned merge, spirvctx_t *ctx)
{
	auto insn = spirv_new_insn (SpvOpSelectionMerge, 3, ctx->code_space, ctx);
	INSN (insn, 1) = merge;
	INSN (insn, 2) = SpvSelectionControlMaskNone;
}

static void
spirv_Branch (unsigned label, spirvctx_t *ctx)
{
	auto insn = spirv_new_insn (SpvOpBranch, 2, ctx->code_space, ctx);
	INSN (insn, 1) = label;
}

static unsigned
spirv_SplitBlockId (unsigned id, spirvctx_t *ctx)
{
	spirv_Branch (id, ctx);
	spirv_LabelId (id, ctx);
	return id;
}

static unsigned
spirv_SplitBlock (spirvctx_t *ctx)
{
	return spirv_SplitBlockId (spirv_id (ctx), ctx);
}

static void
spirv_BranchConditional (bool not, unsigned test, unsigned true_label,
						 unsigned false_label, spirvctx_t *ctx)
{
	auto insn = spirv_new_insn (SpvOpBranchConditional, 4,
								ctx->code_space, ctx);
	INSN (insn, 1) = test;
	INSN (insn, 2) = not ? false_label : true_label;
	INSN (insn, 3) = not ? true_label : false_label;
}

static void
spirv_Unreachable (spirvctx_t *ctx)
{
	spirv_new_insn (SpvOpUnreachable, 1, ctx->code_space, ctx);
}

static void
spirv_FunctionEnd (spirvctx_t *ctx)
{
	spirv_new_insn (SpvOpFunctionEnd, 1, ctx->code_space, ctx);
}

static unsigned
spirv_FunctionParameter (const char *name, const type_t *type, spirvctx_t *ctx)
{
	unsigned id = spirv_id (ctx);
	auto insn = spirv_new_insn (SpvOpFunctionParameter, 3,
								ctx->decl_space, ctx);
	INSN (insn, 1) = spirv_Type (type, ctx);
	INSN (insn, 2) = id;
	spirv_Name (id, name, ctx);
	return id;
}

static SpvStorageClass
spirv_storage_class (unsigned storage, const type_t *type)
{
	auto interface = iftype_from_sc (storage);
	SpvStorageClass sc = 0;
	if (interface < iface_num_interfaces) {
		static SpvStorageClass iface_storage[iface_num_interfaces] = {
			[iface_in] = SpvStorageClassInput,
			[iface_out] = SpvStorageClassOutput,
			[iface_uniform] = SpvStorageClassUniform,
			[iface_buffer] = SpvStorageClassStorageBuffer,
			[iface_push_constant] = SpvStorageClassPushConstant,
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
	if (sc == SpvStorageClassUniform) {
		// tested here because SpvStorageUniformConstant is 0
		if (is_reference (type)) {
			type = dereference_type (type);
		}
		if (is_array (type)) {
			type = dereference_type (type);
		}
		if (is_handle (type)) {
			sc = SpvStorageClassUniformConstant;
		}
	}
	return sc;
}

static unsigned spirv_emit_expr (const expr_t *e, spirvctx_t *ctx);

static unsigned
spirv_variable (symbol_t *sym, spirvctx_t *ctx)
{
	if (sym->sy_type == sy_expr) {
		// this is probably a const (eg, const float pi = 3.14)
		return 0;
	}
	if (sym->sy_type != sy_var) {
		internal_error (0, "unexpected non-variable symbol type");
	}
	auto space = ctx->module->globals;
	auto storage = spirv_storage_class (sym->var.storage, sym->type);
	if (storage == SpvStorageClassFunction) {
		space = ctx->decl_space;
	} else {
		DARRAY_APPEND (&ctx->module->entry_points->interface_syms, sym);
	}
	int count = 4;
	uint32_t init = 0;
	if (sym->var.init) {
		count = 5;
		init = spirv_emit_expr (sym->var.init, ctx);
	}
	auto type = sym->type;
	unsigned tid = spirv_Type (type, ctx);
	unsigned id = spirv_id (ctx);
	auto insn = spirv_new_insn (SpvOpVariable, count, space, ctx);
	INSN (insn, 1) = tid;
	INSN (insn, 2) = id;
	INSN (insn, 3) = storage;
	if (init) {
		INSN (insn, 4) = init;
	}
	spirv_Name (id, sym->name, ctx);
	spirv_decorate_id (id, sym->attributes, ctx);
	return id;
}

static unsigned
spirv_undef (const type_t *type, spirvctx_t *ctx)
{
	unsigned tid = spirv_Type (type, ctx);
	unsigned id = spirv_id (ctx);
	auto insn = spirv_new_insn (SpvOpUndef, 3, ctx->module->globals, ctx);
	INSN (insn, 1) = tid;
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
	if (options.block_dot.expr && func->exprs) {
		auto cf = current_func;
		current_func = func;
		dump_dot ("expr", func->exprs, dump_dot_expr);
		current_func = cf;
	}
	unsigned ft_id = spirv_TypeFunction (func->sym, ctx);

	defspace_reset (ctx->code_space);
	defspace_reset (ctx->decl_space);

	auto ret_type = func->type->func.ret_type;
	unsigned func_id = spirv_function_ref (func, ctx);

	auto insn = spirv_new_insn (SpvOpFunction, 5, ctx->decl_space, ctx);
	INSN (insn, 1) = spirv_Type (ret_type, ctx);
	INSN (insn, 2) = func_id;
	INSN (insn, 3) = 0;
	INSN (insn, 4) = ft_id;
	spirv_Name (func_id, GETSTR (func->s_name), ctx);

	for (auto p = func->sym->params; p; p = p->next) {
		auto ptype = p->type;
		if (is_void (ptype)) {
			break;
		}
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
	if (!is_block_terminated (ctx->code_space)) {
		if (is_void (ret_type)) {
			spirv_new_insn (SpvOpReturn, 1, ctx->code_space, ctx);
		} else {
			unsigned ret_id = spirv_undef (ret_type, ctx);
			auto insn = spirv_new_insn (SpvOpReturnValue, 2,
										ctx->code_space, ctx);
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
spirv_EntryPoint (entrypoint_t *entrypoint, spirvctx_t *ctx)
{
	if (!entrypoint->func) {
		error (0, "entry point %s never defined", entrypoint->name);
		return;
	}

	unsigned func_id = spirv_function (entrypoint->func, ctx);
	while (ctx->func_queue.size) {
		auto func = DARRAY_REMOVE (&ctx->func_queue);
		spirv_function (func, ctx);
	}

	int len = strlen (entrypoint->name) + 1;
	int iface_start = 3 + RUP(len, 4) / 4;
	auto linkage = ctx->module->entry_point_space;
	auto interface_syms = &entrypoint->interface_syms;
	int count = interface_syms->size;
	auto insn = spirv_new_insn (SpvOpEntryPoint, iface_start + count,
								linkage, ctx);
	INSN (insn, 1) = entrypoint->model;
	INSN (insn, 2) = func_id;
	memcpy (&INSN (insn, 3), entrypoint->name, len);
	for (int i = 0; i < count; i++) {
		INSN (insn, iface_start + i) = interface_syms->a[i]->id;
	}

	auto exec_modes = ctx->module->exec_modes;
	if (entrypoint->invocations) {
		insn = spirv_new_insn (SpvOpExecutionMode, 4, exec_modes, ctx);
		INSN (insn, 1) = func_id;
		INSN (insn, 2) = SpvExecutionModeInvocations;
		INSN (insn, 3) = expr_integral (entrypoint->invocations);
	}
	// assume that if 1 is set, all are set
	if (entrypoint->local_size[0]) {
		insn = spirv_new_insn (SpvOpExecutionMode, 6, exec_modes, ctx);
		INSN (insn, 1) = func_id;
		//FIXME LocalSizeId
		INSN (insn, 2) = SpvExecutionModeLocalSize;
		INSN (insn, 3) = expr_integral (entrypoint->local_size[0]);
		INSN (insn, 4) = expr_integral (entrypoint->local_size[1]);
		INSN (insn, 5) = expr_integral (entrypoint->local_size[2]);
	}
	if (entrypoint->primitive_in) {
		insn = spirv_new_insn (SpvOpExecutionMode, 3, exec_modes, ctx);
		INSN (insn, 1) = func_id;
		INSN (insn, 2) = expr_integral (entrypoint->primitive_in);
	}
	if (entrypoint->primitive_out) {
		insn = spirv_new_insn (SpvOpExecutionMode, 3, exec_modes, ctx);
		INSN (insn, 1) = func_id;
		INSN (insn, 2) = expr_integral (entrypoint->primitive_out);
	}
	if (entrypoint->max_vertices) {
		insn = spirv_new_insn (SpvOpExecutionMode, 4, exec_modes, ctx);
		INSN (insn, 1) = func_id;
		INSN (insn, 2) = SpvExecutionModeOutputVertices;
		INSN (insn, 3) = expr_integral (entrypoint->max_vertices);
	}
	if (entrypoint->spacing) {
		insn = spirv_new_insn (SpvOpExecutionMode, 3, exec_modes, ctx);
		INSN (insn, 1) = func_id;
		INSN (insn, 2) = expr_integral (entrypoint->spacing);
	}
	if (entrypoint->order) {
		insn = spirv_new_insn (SpvOpExecutionMode, 3, exec_modes, ctx);
		INSN (insn, 1) = func_id;
		INSN (insn, 2) = expr_integral (entrypoint->order);
	}
	if (entrypoint->frag_depth) {
		insn = spirv_new_insn (SpvOpExecutionMode, 3, exec_modes, ctx);
		INSN (insn, 1) = func_id;
		INSN (insn, 2) = expr_integral (entrypoint->frag_depth);
	}
	if (entrypoint->point_mode) {
		insn = spirv_new_insn (SpvOpExecutionMode, 3, exec_modes, ctx);
		INSN (insn, 1) = func_id;
		INSN (insn, 2) = SpvExecutionModePointMode;
	}
	if (entrypoint->early_fragment_tests) {
		insn = spirv_new_insn (SpvOpExecutionMode, 3, exec_modes, ctx);
		INSN (insn, 1) = func_id;
		INSN (insn, 2) = SpvExecutionModeEarlyFragmentTests;
	}
	for (auto m = entrypoint->modes; m; m = m->next) {
		insn = spirv_new_insn (SpvOpExecutionMode, 3, exec_modes, ctx);
		INSN (insn, 1) = func_id;
		INSN (insn, 2) = expr_integral (m->params);
	}
}

static unsigned
spirv_ptr_load (const type_t *res_type, unsigned ptr_id, unsigned align,
				spirvctx_t *ctx)
{
	int res_type_id = spirv_Type (res_type, ctx);

	unsigned id = spirv_id (ctx);
	int count = align ? 6 : 4;
	auto insn = spirv_new_insn (SpvOpLoad, count, ctx->code_space, ctx);
	INSN (insn, 1) = res_type_id;
	INSN (insn, 2) = id;
	INSN (insn, 3) = ptr_id;
	if (align) {
		INSN (insn, 4) = SpvMemoryAccessAlignedMask;
		INSN (insn, 5) = align;
	}
	return id;
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
	bool        mat1;
	bool        mat2;
	spirv_expr_f generate;
	extinst_t  *extinst;
} spvop_t;

static unsigned
spirv_generate_wedge (const expr_t *e, spirvctx_t *ctx)
{
	scoped_src_loc (e);
	auto v1 = e->expr.e1;
	auto v2 = e->expr.e2;
	auto v1x = struct_field_expr (v1, "x");
	auto v1y = struct_field_expr (v1, "y");
	auto v2x = struct_field_expr (v2, "x");
	auto v2y = struct_field_expr (v2, "y");
	auto west = binary_expr ('-', binary_expr ('*', v1x, v2y),
								  binary_expr ('*', v2x, v1y));
	return spirv_emit_expr (west, ctx);
}

static unsigned
spirv_generate_qmul (const expr_t *e, spirvctx_t *ctx)
{
	scoped_src_loc (e);
	auto q1 = e->expr.e1;
	auto q2 = e->expr.e2;
	auto q1s = struct_field_expr (q1, "w");
	auto q1v = cast_expr (&type_vector, new_swizzle_expr (q1, "xyz"));
	auto q2s = struct_field_expr (q2, "w");
	auto q2v = cast_expr (&type_vector, new_swizzle_expr (q2, "xyz"));
	auto q1sq2v = binary_expr ('*', q1s, q2v);
	auto q2sq1v = binary_expr ('*', q2s, q1v);
	auto qs = binary_expr ('-', binary_expr ('*', q1s, q2s),
								binary_expr (QC_DOT, q1v, q2v));
	auto qv = binary_expr ('+', binary_expr ('+', q1sq2v, q2sq1v),
								binary_expr (QC_CROSS, q1v, q2v));
	const expr_t *vec[] = {qv, qs};
	auto q = new_vector_list_gather (&type_quaternion, vec, 2);
	return spirv_emit_expr (edag_add_expr (q), ctx);
}

static unsigned
spirv_generate_qvmul (const expr_t *e, spirvctx_t *ctx)
{
	//NOTE this assumes the quaternion is normalized a quaternion that
	//isn't normalized will not result in correct scaling
	//  vec3 uv = cross (q.xyz, v);
	//  vec3 uuv = cross (q.xyz, uv);
	//  return v + ((uv * q.w) + uuv) * 2;
	// not assuming normalized would require scaling v (in the return) by
	// (q.w * q.w + dot (u, u)), which adds 4 instructions
	scoped_src_loc (e);
	auto q = e->expr.e1;
	auto v = e->expr.e2;
	auto q_w = struct_field_expr (q, "w");
	auto q_xyz = cast_expr (&type_vector, new_swizzle_expr (q, "xyz"));
	auto two = new_int_expr (2, true);
	auto uv = binary_expr (QC_CROSS, q_xyz, v);
	auto uuv = binary_expr (QC_CROSS, q_xyz, uv);
	auto side = binary_expr ('+', uuv, binary_expr ('*', uv, q_w));
	side = binary_expr ('*', two, side);
	return spirv_emit_expr (binary_expr ('+', v, side), ctx);
}

static unsigned
spirv_generate_vqmul (const expr_t *e, spirvctx_t *ctx)
{
	//NOTE this assumes the quaternion is normalized
	//see spirv_generate_qvmul
	scoped_src_loc (e);
	auto v = e->expr.e1;
	auto q = e->expr.e2;
	auto q_w = struct_field_expr (q, "w");
	auto q_xyz = cast_expr (&type_vector, new_swizzle_expr (q, "xyz"));
	auto two = new_int_expr (2, true);
	auto uv = binary_expr (QC_CROSS, q_xyz, v);
	auto uuv = binary_expr (QC_CROSS, q_xyz, uv);
	auto side = binary_expr ('-', uuv, binary_expr ('*', uv, q_w));
	side = binary_expr ('*', two, side);
	return spirv_emit_expr (binary_expr ('+', v, side), ctx);
}

static unsigned
spirv_generate_matrix (const expr_t *e, spirvctx_t *ctx)
{
	auto mat_type = get_type (e);
	int count = type_cols (mat_type);
	scoped_src_loc (e);
	unsigned columns[count];
	auto col_type = column_type (mat_type);
	auto e1 = e->expr.e1;
	auto e2 = e->expr.e2;
	for (int i = 0; i < count; i++) {
		auto ind = new_int_expr (i, false);
		auto a = new_array_expr (e1, ind);
		auto b = new_array_expr (e2, ind);
		a->array.type = col_type;
		b->array.type = col_type;
		auto c = typed_binary_expr (col_type, e->expr.op, a, b);

		columns[i] = spirv_emit_expr (c, ctx);
	}

	int tid = spirv_Type (mat_type, ctx);
	int id = spirv_id (ctx);
	auto insn = spirv_new_insn (SpvOpCompositeConstruct, 3 + count,
								ctx->code_space, ctx);
	INSN (insn, 1) = tid;
	INSN (insn, 2) = id;
	for (int i = 0; i < count; i++) {
		INSN (insn, 3 + i) = columns[i];
	}
	return id;
}

static unsigned
spirv_generate_not (const expr_t *e, spirvctx_t *ctx)
{
	scoped_src_loc (e);
	auto expr = e->expr.e1;
	auto zero = new_int_expr (0, true);
	auto not = binary_expr (QC_EQ, expr, zero);
	return spirv_emit_expr (not, ctx);
}

static unsigned
spirv_generate_load (const expr_t *e, spirvctx_t *ctx)
{
	auto res_type = get_type (e);
	auto ptr = e->expr.e1;
	auto ptr_type = get_type (ptr);
	unsigned align = 0;
	if (ptr_type->fldptr.tag == SpvStorageClassPhysicalStorageBuffer) {
		align = type_align (res_type) * sizeof (pr_type_t);
	}

	unsigned ptr_id = spirv_emit_expr (ptr, ctx);
	return spirv_ptr_load (res_type, ptr_id, align, ctx);
}

#define SPV_meta(m,t)
#define SPV_type(m,t) ((unsigned)((1<<((m)+16))|(1<<(t))))
#define SPV_type_cmp(a,b) (((a) & (b)) == (b))
#define SPV_BOOL  (SPV_type(ty_bool, ev_int)   |SPV_type(ty_bool, ev_long))
#define SPV_SINT  (SPV_type(ty_basic, ev_int)  |SPV_type(ty_basic, ev_long))
#define SPV_UINT  (SPV_type(ty_basic, ev_uint) |SPV_type(ty_basic, ev_ulong))
#define SPV_FLOAT (SPV_type(ty_basic, ev_float)|SPV_type(ty_basic, ev_double))
#define SPV_INT   (SPV_SINT|SPV_UINT)
#define SPV_PTR   (SPV_type(ty_basic, ev_ptr))
#define SPV_QUAT  (SPV_type(ty_basic, ev_quaternion))
#define SPV_VEC   (SPV_type(ty_basic, ev_vector))

static extinst_t glsl_450 = {
	.name = "GLSL.std.450"
};

static spvop_t spv_ops[] = {
	{"load",   .types1 = SPV_PTR, .generate = spirv_generate_load },

	{"or",     SpvOpLogicalOr,            SPV_BOOL,  SPV_BOOL  },
	{"and",    SpvOpLogicalAnd,           SPV_BOOL,  SPV_BOOL  },

	{"eq",     SpvOpLogicalEqual,         SPV_BOOL,  SPV_BOOL  },
	{"eq",     SpvOpIEqual,               SPV_INT,   SPV_INT   },
	{"eq",     SpvOpFOrdEqual,            SPV_FLOAT, SPV_FLOAT },
	{"ne",     SpvOpLogicalNotEqual,      SPV_BOOL,  SPV_BOOL  },
	{"ne",     SpvOpFOrdNotEqual,         SPV_FLOAT, SPV_FLOAT },
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
	{"add",    SpvOpFAdd,                 SPV_VEC,   SPV_VEC   },
	{"add",    SpvOpFAdd,                 SPV_QUAT,  SPV_QUAT },
	{"add",  .types1 = SPV_FLOAT, .types2 = SPV_FLOAT,
			 .mat1 = true, .mat2 = true,
			 .generate = spirv_generate_matrix },
	{"sub",    SpvOpISub,                 SPV_INT,   SPV_INT   },
	{"sub",    SpvOpFSub,                 SPV_FLOAT, SPV_FLOAT },
	{"sub",    SpvOpFSub,                 SPV_VEC,   SPV_VEC },
	{"sub",    SpvOpFSub,                 SPV_QUAT,  SPV_QUAT },
	{"add",  .types1 = SPV_FLOAT, .types2 = SPV_FLOAT,
			 .mat1 = true, .mat2 = true,
			 .generate = spirv_generate_matrix },
	{"mul",    SpvOpFMul,                 SPV_VEC,   SPV_VEC },
	{"mul",    SpvOpIMul,                 SPV_INT,   SPV_INT   },
	{"mul",    SpvOpFMul,                 SPV_FLOAT, SPV_FLOAT },
	{"mul",    SpvOpMatrixTimesVector,    SPV_FLOAT, SPV_FLOAT,
			 .mat1 = true, .mat2 = false },
	{"mul",    SpvOpVectorTimesMatrix,    SPV_FLOAT, SPV_FLOAT,
			 .mat1 = false, .mat2 = true },
	{"mul",    SpvOpMatrixTimesMatrix,    SPV_FLOAT, SPV_FLOAT,
			 .mat1 = true, .mat2 = true },
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
	{"not",    SpvOpLogicalNot,           SPV_BOOL,  0         },
	{"not",    .types1 = SPV_FLOAT|SPV_INT,
		.generate = spirv_generate_not },

	{"shl",    SpvOpShiftLeftLogical,     SPV_INT,   SPV_INT   },
	{"shr",    SpvOpShiftRightLogical,    SPV_UINT,  SPV_INT   },
	{"shr",    SpvOpShiftRightArithmetic, SPV_SINT,  SPV_INT   },

	{"dot",    SpvOpDot,                  SPV_FLOAT, SPV_FLOAT },
	{"dot",    SpvOpDot,                  SPV_VEC,   SPV_VEC },
	{"scale",  SpvOpVectorTimesScalar,    SPV_FLOAT, SPV_FLOAT },
	{"scale",  SpvOpVectorTimesScalar,    SPV_VEC,   SPV_FLOAT },
	{"scale",  SpvOpVectorTimesScalar,    SPV_QUAT,  SPV_FLOAT },
	{"scale",  SpvOpMatrixTimesScalar,    SPV_FLOAT, SPV_FLOAT,
		.mat1 = true, .mat2 = false },

	{"cross",  (SpvOp)GLSLstd450Cross,    SPV_FLOAT, SPV_FLOAT,
		.extinst = &glsl_450 },
	{"cross",  (SpvOp)GLSLstd450Cross,    SPV_VEC,   SPV_VEC,
		.extinst = &glsl_450 },
	{"wedge",  .types1 = SPV_FLOAT, .types2 = SPV_FLOAT,
		.generate = spirv_generate_wedge, .extinst = &glsl_450 },
	{"qmul",   .types1 = SPV_QUAT, .types2 = SPV_QUAT,
		.generate = spirv_generate_qmul, .extinst = &glsl_450 },
	{"qvmul",  .types1 = SPV_QUAT, .types2 = SPV_VEC,
		.generate = spirv_generate_qvmul, .extinst = &glsl_450 },
	{"vqmul",  .types1 = SPV_VEC, .types2 = SPV_QUAT,
		.generate = spirv_generate_vqmul, .extinst = &glsl_450 },
};

static const spvop_t *
spirv_find_op (const char *op_name, const type_t *type1, const type_t *type2)
{
	constexpr int num_ops = sizeof (spv_ops) / sizeof (spv_ops[0]);
	etype_t t1 = type1->type;
	etype_t t2 = type2 ? type2->type : ev_void;
	ty_meta_e m1 = type1->meta;
	ty_meta_e m2 = type2 ? type2->meta : ty_basic;
	bool mat1 = is_matrix (type1);
	bool mat2 = is_matrix (type2);

	for (int i = 0; i < num_ops; i++) {
		if (strcmp (spv_ops[i].op_name, op_name) == 0
			&& SPV_type_cmp (spv_ops[i].types1, SPV_type(m1, t1))
			&& ((!spv_ops[i].types2 && t2 == ev_void)
				|| (spv_ops[i].types2
					&& SPV_type_cmp (spv_ops[i].types2, SPV_type(m2, t2))))
			&& spv_ops[i].mat1 == mat1 && spv_ops[i].mat2 == mat2) {
			return &spv_ops[i];
		}
	}
	return nullptr;
}

static unsigned
spirv_cast (const expr_t *e, spirvctx_t *ctx)
{
	auto src_type = get_type (e->expr.e1);
	auto dst_type = e->expr.type;
	SpvOp op = 0;
	if (is_real (src_type)) {
		if (is_unsigned (dst_type)) {
			op = SpvOpConvertFToU;
		} else if (is_signed (dst_type)) {
			op = SpvOpConvertFToS;
		} else if (is_real (dst_type)) {
			op = SpvOpFConvert;
		}
	} else if (is_real (dst_type)) {
		if (is_unsigned (src_type)) {
			op = SpvOpConvertUToF;
		} else if (is_signed (src_type)) {
			op = SpvOpConvertSToF;
		}
	} else if (is_unsigned (dst_type)) {
		if (type_size (base_type (dst_type))
			!= type_size (base_type (src_type))) {
			op = SpvOpUConvert;
		}
	} else if (is_signed (dst_type)) {
		if (type_size (base_type (dst_type))
			!= type_size (base_type (src_type))) {
			op = SpvOpSConvert;
		}
	} else if (is_structural (dst_type)) {
		// assume set up by spirv_check_types_compatible
		op = SpvOpCopyLogical;
	}
	if (!op) {
		internal_error (e, "unexpected type combination");
	}

	unsigned cid = spirv_emit_expr (e->expr.e1, ctx);
	unsigned tid = spirv_Type (dst_type, ctx);
	unsigned id = spirv_id (ctx);
	auto insn = spirv_new_insn (op, 4, ctx->code_space, ctx);
	INSN (insn, 1) = tid;
	INSN (insn, 2) = id;
	INSN (insn, 3) = cid;
	return id;
}

static unsigned
spirv_uexpr (const expr_t *e, spirvctx_t *ctx)
{
	if (e->expr.op == 'C') {
		return spirv_cast (e, ctx);
	}
	if (e->expr.op == '=') {
		auto val = e->expr.e1;
		if (!is_integral_val (val)) {
			error (val, "not a constant integer");
			return 0;
		}
		return expr_integral (val);
	}
	auto op_name = convert_op (e->expr.op);
	if (!op_name) {
		if (e->expr.op > 32 && e->expr.op < 127) {
			internal_error (e, "unexpected unary op: '%c'\n", e->expr.op);
		} else {
			internal_error (e, "unexpected unary op: %d\n", e->expr.op);
		}
	}
	auto t = get_type (e->expr.e1);
	auto spv_op = spirv_find_op (op_name, t, nullptr);
	if (!spv_op) {
		internal_error (e, "unexpected unary op_name: %s %s\n", op_name,
						get_type_string(t));
	}
	if (!spv_op->op && !spv_op->generate) {
		internal_error (e, "unimplemented op: %s %s\n", op_name,
						get_type_string(t));
	}
	if (spv_op->generate) {
		return spv_op->generate (e, ctx);
	}
	unsigned uid = spirv_emit_expr (e->expr.e1, ctx);

	unsigned tid = spirv_Type (get_type (e), ctx);
	unsigned id;
	if (e->expr.constant) {
		auto globals = ctx->module->globals;
		auto insn = spirv_new_insn (SpvOpSpecConstantOp, 5, globals, ctx);
		INSN (insn, 1) = tid;
		INSN (insn, 2) = id = spirv_id (ctx);
		INSN (insn, 3) = spv_op->op;
		INSN (insn, 4) = uid;
	} else {
		auto insn = spirv_new_insn (spv_op->op, 4, ctx->code_space, ctx);
		INSN (insn, 1) = tid;
		INSN (insn, 2) = id = spirv_id (ctx);
		INSN (insn, 3) = uid;
	}
	return id;
}

static unsigned
spirv_bool (const expr_t *e, spirvctx_t *ctx)
{
	internal_error (e, "oops");
}

static unsigned
spirv_label (const expr_t *e, spirvctx_t *ctx)
{
	auto label = &e->label;
	unsigned id = spirv_label_id (label, ctx);
	return spirv_LabelId (id, ctx);
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
	auto t2 = get_type (e->expr.e2);
	auto spv_op = spirv_find_op (op_name, t1, t2);
	if (!spv_op) {
		internal_error (e, "unexpected binary op_name: %s %s %s\n", op_name,
						get_type_string(t1),
						get_type_string(t2));
	}
	if (!spv_op->op && !spv_op->generate) {
		internal_error (e, "unimplemented op: %s %s %s\n", op_name,
						get_type_string(t1),
						get_type_string(t2));
	}
	if (spv_op->generate) {
		return spv_op->generate (e, ctx);
	}
	unsigned bid1 = spirv_emit_expr (e->expr.e1, ctx);
	unsigned bid2 = spirv_emit_expr (e->expr.e2, ctx);

	unsigned tid = spirv_Type (get_type (e), ctx);
	unsigned id;
	if (e->expr.constant) {
		auto globals = ctx->module->globals;
		auto insn = spirv_new_insn (SpvOpSpecConstantOp, 6, globals, ctx);
		INSN (insn, 1) = tid;
		INSN (insn, 2) = id = spirv_id (ctx);
		INSN (insn, 3) = spv_op->op;
		INSN (insn, 4) = bid1;
		INSN (insn, 5) = bid2;
	} else {
		if (spv_op->extinst) {
			auto insn = spirv_new_insn (SpvOpExtInst, 7, ctx->code_space, ctx);
			const char *set = spv_op->extinst->name;
			INSN (insn, 1) = tid;
			INSN (insn, 2) = id = spirv_id (ctx);
			INSN (insn, 3) = spirv_extinst_import (ctx->module, set, ctx);
			INSN (insn, 4) = spv_op->op;
			INSN (insn, 5) = bid1;
			INSN (insn, 6) = bid2;
		} else {
			auto insn = spirv_new_insn (spv_op->op, 5, ctx->code_space, ctx);
			INSN (insn, 1) = tid;
			INSN (insn, 2) = id = spirv_id (ctx);
			INSN (insn, 3) = bid1;
			INSN (insn, 4) = bid2;
		}
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
		spirv_decorate_id (sym->id, sym->attributes, ctx);
	} else if (sym->sy_type == sy_func) {
		auto func = sym->metafunc->func;
		if (!func) {
			error (e, "%s called but not defined", sym->name);
			return 0;
		}
		if (!func->id) {
			DARRAY_APPEND (&ctx->func_queue, func);
		}
		sym->id = spirv_function_ref (func, ctx);
	} else if (sym->sy_type == sy_var) {
		sym->id = spirv_variable (sym, ctx);
	} else if (sym->sy_type == sy_const) {
		auto e = new_value_expr (sym->value, false);
		sym->id = spirv_value (e, ctx);
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
spirv_string_value (const ex_value_t *value, spirvctx_t *ctx)
{
	return spirv_String (value->string_val, ctx);
}

static unsigned
spirv_vector_value (const ex_value_t *value, spirvctx_t *ctx)
{
	auto base = base_type (value->type);
	int width = type_width (value->type);
	ex_value_t *comp_vals[width];
	auto val = &value->raw_value;
	if (type_size (base) < 1 || type_size (base) > 2) {
		internal_error (nullptr, "invalid vector component size");
	}
	for (int i = 0; i < width; i++) {
		comp_vals[i] = new_type_value (base, val);
		val += type_size (base);
	}
	unsigned comp_ids[width];
	for (int i = 0; i < width; i++) {
		auto e = new_value_expr (comp_vals[i], false);
		comp_ids[i] = spirv_value (e, ctx);
	}

	auto space = ctx->module->globals;
	int tid = spirv_Type (value->type, ctx);
	int id = spirv_id (ctx);
	auto insn = spirv_new_insn (SpvOpConstantComposite, 3 + width, space, ctx);
	INSN (insn, 1) = tid;
	INSN (insn, 2) = id;
	for (int i = 0; i < width; i++) {
		INSN (insn, 3 + i) = comp_ids[i];
	}
	return id;
}

static unsigned
spirv_matrix_value (const ex_value_t *value, spirvctx_t *ctx)
{
	auto ctype = column_type (value->type);
	int count = type_cols (value->type);
	ex_value_t *columns[count];
	auto val = &value->raw_value;
	for (int i = 0; i < count; i++) {
		columns[i] = new_type_value (ctype, val);
		val += type_size (ctype);
		columns[i]->id = spirv_vector_value (columns[i], ctx);
	}

	auto space = ctx->module->globals;
	int tid = spirv_Type (value->type, ctx);
	int id = spirv_id (ctx);
	auto insn = spirv_new_insn (SpvOpConstantComposite, 3 + count, space, ctx);
	INSN (insn, 1) = tid;
	INSN (insn, 2) = id;
	for (int i = 0; i < count; i++) {
		INSN (insn, 3 + i) = columns[i]->id;
	}
	return id;
}

static unsigned
spirv_nil (const expr_t *e, spirvctx_t *ctx)
{
	unsigned tid = spirv_Type (e->nil, ctx);
	unsigned id = spirv_id (ctx);
	auto globals = ctx->module->globals;
	auto insn = spirv_new_insn (SpvOpConstantNull, 3, globals, ctx);
	INSN (insn, 1) = tid;
	INSN (insn, 2) = id;
	return id;
}

static unsigned
spirv_value (const expr_t *e, spirvctx_t *ctx)
{
	auto value = e->value;
	if (!value->id) {
		if (is_string (value->type)) {
			return spirv_string_value (value, ctx);
		}
		if (is_matrix (value->type)) {
			return spirv_matrix_value (value, ctx);
		}
		if (is_nonscalar (value->type)) {
			return spirv_vector_value (value, ctx);
		}
		unsigned tid = spirv_Type (value->type, ctx);
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
		if (op == SpvOpConstant && !val && !is_math (value->type)) {
			auto insn = spirv_new_insn (SpvOpConstantNull, 3, globals, ctx);
			INSN (insn, 1) = tid;
			INSN (insn, 2) = value->id = spirv_id (ctx);
		} else {
			auto insn = spirv_new_insn (op, 3 + val_size, globals, ctx);
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
spirv_compound (const expr_t *e, spirvctx_t *ctx)
{
	int num_ele = 0;
	for (auto ele = e->compound.head; ele; ele = ele->next) {
		num_ele++;
	}
	unsigned ele_ids[num_ele];
	int ind = 0;
	for (auto ele = e->compound.head; ele; ele = ele->next) {
		ele_ids[ind++] = spirv_emit_expr (ele->expr, ctx);
	}

	auto space = ctx->code_space;
	auto op = SpvOpCompositeConstruct;
	if (is_constant (e)) {
		space = ctx->module->globals;
		op = SpvOpConstantComposite;
	}
	auto type = e->compound.type;
	int tid = spirv_Type (type, ctx);
	int id = spirv_id (ctx);
	auto insn = spirv_new_insn (op, 3 + num_ele, space, ctx);
	INSN (insn, 1) = tid;
	INSN (insn, 2) = id;
	for (int i = 0; i < num_ele; i++) {
		INSN (insn, 3 + i) = ele_ids[i];
	}
	return id;
}

static unsigned
spirv_alias (const expr_t *e, spirvctx_t *ctx)
{
	if (e->alias.offset) {
		internal_error (e, "offset alias in spir-v");
	}
	auto type = e->alias.type;
	auto expr = e->alias.expr;
	unsigned eid = spirv_emit_expr (expr, ctx);
	unsigned tid = spirv_Type (type, ctx);
	if (tid == spirv_Type (get_type (expr), ctx)) {
		// the types differe at the high level, but are the same at the
		// low level (eg, vector vs vec3 or quaternion vs vec4)
		return eid;
	}
	unsigned id = spirv_id (ctx);
	auto insn = spirv_new_insn (SpvOpBitcast, 4, ctx->code_space, ctx);
	INSN (insn, 1) = tid;
	INSN (insn, 2) = id;
	INSN (insn, 3) = eid;
	return id;
}

static unsigned
spirv_vector (const expr_t *e, spirvctx_t *ctx)
{
	auto compound = vector_to_compound (e);
	return spirv_compound (compound, ctx);
}

static unsigned
spirv_access_chain (const expr_t *e, spirvctx_t *ctx,
					const type_t **res_type, const type_t **acc_type)
{
	*res_type = get_type (e);
	ex_list_t list = {};
	// convert the left-branching field/array expression chain to a list
	while (e->type == ex_field || e->type == ex_array) {
		const type_t *mtype;//for comparison of membe and object types
		if (e->type == ex_field
			&& is_algebra (mtype = get_type (e->field.member))
			&& mtype == get_type (e->field.object)) {
			// This is a GA group access for getting the underlying vector
			// type from the host group, so just bypass the access.
			e = e->field.object;
		}
		for (; e->type == ex_field; e = e->field.object) {
			list_prepend (&list, e);
		}
		for (; e->type == ex_array; e = e->array.base) {
			list_prepend (&list, e);
		}
	}
	// e is now the base object of the field/array expression chain
	auto base_type = get_type (e);
	unsigned base_id = spirv_emit_expr (e, ctx);
	if (!list.head) {
		*acc_type = *res_type;
		return base_id;
	}
	int op = SpvOpCompositeExtract;

	*acc_type = *res_type;
	bool literal_ind = true;
	if (is_pointer (base_type) || is_reference (base_type)) {
		unsigned storage = base_type->fldptr.tag;
		*acc_type = tagged_reference_type (storage, *res_type);
		op = SpvOpAccessChain;
		literal_ind = false;
	}

	int num_obj = list_count (&list);
	const expr_t *ind_expr[num_obj];
	unsigned ind_id[num_obj];
	int num_ind = 0;
	list_scatter (&list, ind_expr);
	for (int i = 0; i < num_obj; i++) {
		auto obj = ind_expr[i];
		bool direct_ind = false;
		unsigned index;
		if (obj->type == ex_field) {
			if (is_pointer (get_type (obj->field.object))) {
				unsigned storage = base_type->fldptr.tag;
				base_type = get_type (obj->field.object);
				*acc_type = tagged_pointer_type (storage, base_type);
				break;
			}
			if (obj->field.member->type != ex_symbol) {
				internal_error (obj->field.member, "not a symbol");
			}
			auto sym = obj->field.member->symbol;
			index = sym->id;
		} else if (obj->type == ex_array) {
			if (is_pointer (get_type (obj->array.base))) {
				unsigned storage = base_type->fldptr.tag;
				base_type = get_type (obj->array.base);
				*acc_type = tagged_pointer_type (storage, base_type);
				break;
			}
			auto ind = obj->array.index;
			if (is_integral_val (ind)) {
				index = expr_integral (ind);
			} else {
				if (is_reference (get_type (ind))) {
					ind = pointer_deref (ind);
				}
				index = spirv_emit_expr (ind, ctx);
				direct_ind = true;
			}
		} else {
			internal_error (obj, "what the what?!?");
		}
		if (literal_ind || direct_ind) {
			ind_id[num_ind++] = index;
		} else {
			scoped_src_loc (obj);
			auto ind = new_uint_expr (index);
			ind_id[num_ind++] = spirv_emit_expr (ind, ctx);
		}
		e = obj;
	}
	// e is now the base object of the pointer chain

	int acc_type_id = spirv_Type (*acc_type, ctx);
	int id = spirv_id (ctx);
	auto insn = spirv_new_insn (op, 4 + num_ind, ctx->code_space, ctx);
	INSN (insn, 1) = acc_type_id;
	INSN (insn, 2) = id;
	INSN (insn, 3) = base_id;
	auto field_ind = &INSN (insn, 4);
	for (int i = 0; i < num_ind; i++) {
		field_ind[i] = ind_id[i];
	}
	if (num_ind == num_obj) {
		return id;
	}
	unsigned ptr_id = spirv_ptr_load (base_type, id, 0, ctx);
	const expr_t *ptr = nullptr;
	for (int i = num_ind; i < num_obj; i++) {
		auto obj = ind_expr[i];
		base_type = get_type (e);
		if (obj->type == ex_field) {
			internal_error (obj, "not yet");
		} else if (obj->type == ex_array) {
			scoped_src_loc (obj->array.index);
			int base_ind = 0;
			if (is_array (base_type)) {
				base_ind = base_type->array.base;
			}
			auto ele_type = obj->array.type;
			auto base = new_int_expr (base_ind, false);
			int size = type_aligned_size (ele_type) * sizeof (pr_type_t);
			auto scale = new_int_expr (size, false);
			auto offset = binary_expr ('*', base, scale);
			auto index = binary_expr ('*', obj->array.index, scale);
			offset = binary_expr ('-', index, offset);
			if (is_array (base_type)
				|| is_nonscalar (base_type) || is_matrix (base_type)) {
			} else {
				// "wedge" the spirv-id for the pointer into the expression
				// being generated. spirv_emit_expr will use the id instead
				// of evaluating the expression.
				ptr = new_expr_copy (e);
				spirv_add_expr_id (ptr, ptr_id, ctx);
			}
			unsigned storage = base_type->fldptr.tag;
			*acc_type = tagged_pointer_type (storage, ele_type);
			ptr = offset_pointer_expr (ptr, offset);
			ptr = cast_expr (*acc_type, ptr);
			ptr_id = spirv_emit_expr (ptr, ctx);
			e = obj;
		} else {
			internal_error (obj, "what the what?!?");
		}
	}
	return ptr_id;
}

static unsigned
spirv_address (const expr_t *e, spirvctx_t *ctx)
{
	auto lvalue = e->address.lvalue;
	if (lvalue->type != ex_field && lvalue->type != ex_array) {
		internal_error (e, "not field or array");
	}
	if (e->address.offset) {
		internal_error (e, "offset on address");
	}
	const type_t *res_type;
	const type_t *acc_type;
	unsigned id = spirv_access_chain (lvalue, ctx, &res_type, &acc_type);
	return id;
}

static unsigned
spirv_offset (const expr_t *e, spirvctx_t *ctx)
{
	auto member = e->offset.member;
	if (member->type != ex_symbol) {
		internal_error (member, "not a symbol");
	}
	return member->symbol->id;
}

static unsigned
spirv_assign (const expr_t *e, spirvctx_t *ctx)
{
	unsigned src = spirv_emit_expr (e->assign.src, ctx);
	unsigned dst = 0;
	unsigned align = 0;	// default to not emitting Aligned

	if (is_temp (e->assign.dst)) {
		// spir-v uses SSA, so temps cannot be assigned to directly, so instead
		// use the temp expression as a reference for the result id of the
		// rhs of the assignment.
		spirv_add_expr_id (e, src, ctx);
		return src;
	}
	if (e->assign.dst->type == ex_field || e->assign.dst->type == ex_array) {
		const type_t *res_type;
		const type_t *acc_type;
		dst = spirv_access_chain (e->assign.dst, ctx, &res_type, &acc_type);
		if (res_type == acc_type) {
			internal_error (e, "assignment to temp?");
		}
	} else if (is_deref (e->assign.dst)) {
		auto ptr = e->assign.dst->expr.e1;
		auto ptr_type = get_type (ptr);
		if (is_pointer (ptr_type)) {
			align = type_align (ptr_type) * sizeof (pr_type_t);
		}
		dst = spirv_emit_expr (ptr, ctx);
	}

	if (!dst) {
		internal_error (e, "invalid assignment?");
	}

	int count = align ? 5 : 3;
	auto insn = spirv_new_insn (SpvOpStore, count, ctx->code_space, ctx);
	INSN (insn, 1) = dst;
	INSN (insn, 2) = src;
	if (align) {
		INSN (insn, 3) = SpvMemoryAccessAlignedMask;
		INSN (insn, 4) = align;
	}
	return 0;
}

static unsigned
spirv_call (const expr_t *call, spirvctx_t *ctx)
{
	int num_args = list_count (&call->branch.args->list);
	const expr_t *args[num_args + 1];
	const expr_t *params[num_args + 1];
	unsigned arg_ids[num_args + 1];
	auto func = call->branch.target;
	auto func_type = get_type (func);
	auto ret_type = func_type->func.ret_type;

	unsigned func_id = spirv_emit_expr (func, ctx);
	unsigned ret_type_id = spirv_Type (ret_type, ctx);

	list_scatter_rev (&call->branch.args->list, args);
	for (int i = 0; i < num_args; i++) {
		auto a = args[i];
		if (func_type->func.param_quals[i] == pq_const) {
			arg_ids[i] = spirv_emit_expr (a, ctx);
		} else {
			auto psym = new_symbol ("param");
			psym->type = tagged_reference_type (SpvStorageClassFunction,
												get_type (a));
			psym->sy_type = sy_var;
			psym->var.storage = (storage_class_t) SpvStorageClassFunction;
			psym->id = spirv_variable (psym, ctx);
			psym->lvalue = true;
			arg_ids[i] = psym->id;
			params[i] = new_symbol_expr (psym);
			if (func_type->func.param_quals[i] != pq_out) {
				if (a->type == ex_inout) {
					a = a->inout.in;
				}
				auto assign = assign_expr (params[i], a);
				if (!is_error (assign)) {
					spirv_emit_expr (assign, ctx);
				}
			}
		}
	}

	unsigned id = spirv_id (ctx);
	auto insn = spirv_new_insn (SpvOpFunctionCall, 4 + num_args,
								ctx->code_space, ctx);
	INSN(insn, 1) = ret_type_id;
	INSN(insn, 2) = id;
	INSN(insn, 3) = func_id;
	for (int i = 0; i < num_args; i++) {
		INSN(insn, 4 + i) = arg_ids[i];
	}
	for (int i = 0; i < num_args; i++) {
		auto a = args[i];
		auto qual = func_type->func.param_quals[i];
		if (qual == pq_out || qual == pq_inout) {
			if (a->type != ex_inout) {
				internal_error (a, "non-inout expr for inout or out param");
			}
			a = a->inout.out;
			auto assign = assign_expr (a, params[i]);
			spirv_emit_expr (assign, ctx);
		}
	}
	return id;
}

static unsigned
spirv_jump (const expr_t *e, spirvctx_t *ctx)
{
	auto target_label = &e->branch.target->label;
	unsigned target = spirv_label_id (target_label, ctx);
	spirv_Branch (target, ctx);
	return 0;
}

static unsigned
spirv_branch (const expr_t *e, spirvctx_t *ctx)
{
	if (e->branch.type == pr_branch_call) {
		return spirv_call (e, ctx);
	}
	if (e->branch.type == pr_branch_jump) {
		return spirv_jump (e, ctx);
	}
	internal_error (e, "unexpected branch");
}

static unsigned
spirv_return (const expr_t *e, spirvctx_t *ctx)
{
	if (e->retrn.ret_val) {
		unsigned ret_id = spirv_emit_expr (e->retrn.ret_val, ctx);
		auto insn = spirv_new_insn (SpvOpReturnValue, 2, ctx->code_space, ctx);
		INSN (insn, 1) = ret_id;
	} else {
		spirv_new_insn (SpvOpReturn, 1, ctx->code_space, ctx);
	}
	return 0;
}

static unsigned
spirv_swizzle (const expr_t *e, spirvctx_t *ctx)
{
	int count = type_width (e->swizzle.type);
	if (count == 1) {
		// spir-v doesn't accept 1-component swizzles, so convert to indexed
		// access
		scoped_src_loc (e);
		auto a = new_array_expr (e->swizzle.src,
								 new_int_expr (e->swizzle.source[0], false));
		a->array.type = e->swizzle.type;
		return spirv_emit_expr (a, ctx);
	}
	unsigned src_id = spirv_emit_expr (e->swizzle.src, ctx);
	unsigned tid = spirv_Type (e->swizzle.type, ctx);
	unsigned id = spirv_id (ctx);
	auto insn = spirv_new_insn (SpvOpVectorShuffle, 5 + count,
								ctx->code_space, ctx);
	INSN (insn, 1) = tid;
	INSN (insn, 2) = id;
	INSN (insn, 3) = src_id;
	INSN (insn, 4) = src_id;
	for (int i = 0; i < count; i++) {
		INSN (insn, 5 + i) = e->swizzle.source[i];
	}
	return id;
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

	if (res_base != src_base || res_width <= src_width) {
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
	int tid = spirv_Type (res_type, ctx);
	int id = spirv_id (ctx);
	auto insn = spirv_new_insn (SpvOpCompositeConstruct, 3 + nids,
								ctx->code_space, ctx);
	INSN (insn, 1) = tid;
	INSN (insn, 2) = id;
	for (int i = 0; i < nids; i++) {
		INSN (insn, 3 + i) = cids[start + i];
	}
	return id;
}

static unsigned
spirv_list (const expr_t *e, spirvctx_t *ctx)
{
	for (auto le = e->list.head; le; le = le->next) {
		spirv_emit_expr (le->expr, ctx);
	}
	return 0;
}

static unsigned
spirv_incop (const expr_t *e, spirvctx_t *ctx)
{
	scoped_src_loc (e);

	// binary_expr will take care of pointers (FIXME is this correct for
	// spir-v?)
	auto one = new_int_expr (1, false);
	auto type = get_type (e->incop.expr);
	if (is_scalar (type)) {
		one = cast_expr (type, one);
	}
	auto dst = new_expr_copy (e->incop.expr);
	auto incop = binary_expr (e->incop.op, e->incop.expr, one);
	unsigned inc_id = spirv_emit_expr (incop, ctx);
	unsigned src_id = spirv_expr_id (incop->expr.e1, ctx);
	auto assign = new_assign_expr (dst, incop);
	spirv_emit_expr (assign, ctx);
	return e->incop.postop ? src_id : inc_id;
}

static unsigned
spirv_cond (const expr_t *e, spirvctx_t *ctx)
{
	unsigned test_id = spirv_emit_expr (e->cond.test, ctx);
	unsigned true_id = spirv_emit_expr (e->cond.true_expr, ctx);
	unsigned false_id = spirv_emit_expr (e->cond.false_expr, ctx);

	auto type = get_type (e);
	unsigned tid = spirv_Type (type, ctx);
	unsigned id = spirv_id (ctx);
	auto insn = spirv_new_insn (SpvOpSelect, 6, ctx->code_space, ctx);
	INSN (insn, 1) = tid;
	INSN (insn, 2) = id;
	INSN (insn, 3) = test_id;
	INSN (insn, 4) = true_id;
	INSN (insn, 5) = false_id;
	return id;
}

static unsigned
spirv_field_array (const expr_t *e, spirvctx_t *ctx)
{
	const type_t *res_type;
	const type_t *acc_type;

	unsigned id = spirv_access_chain (e, ctx, &res_type, &acc_type);

	if (acc_type != res_type) {
		// base is a pointer or reference so load the value
		unsigned align = 0;
		if (is_pointer (acc_type)) {
			align = type_align (res_type) * sizeof (pr_type_t);
		}
		id = spirv_ptr_load (res_type, id, align, ctx);
	}
	return id;
}

static unsigned
spirv_loop (const expr_t *e, spirvctx_t *ctx)
{
	auto break_label = &e->loop.break_label->label;
	auto continue_label = &e->loop.continue_label->label;
	unsigned merge = spirv_label_id (break_label, ctx);
	unsigned cont = spirv_label_id (continue_label, ctx);
	if (e->loop.do_while) {
		unsigned loop = spirv_SplitBlock (ctx);
		spirv_LoopMerge (merge, cont, ctx);
		spirv_SplitBlock (ctx);
		spirv_emit_expr (e->loop.body, ctx);
		spirv_SplitBlockId (cont, ctx);
		if (e->loop.continue_body) {
			spirv_emit_expr (e->loop.continue_body, ctx);
		}
		unsigned test = spirv_emit_expr (e->loop.test, ctx);
		spirv_BranchConditional (e->loop.not, test, merge, loop, ctx);
		spirv_LabelId (merge, ctx);
	} else {
		unsigned loop = spirv_SplitBlock (ctx);
		spirv_LoopMerge (merge, cont, ctx);
		spirv_SplitBlock (ctx);
		unsigned body = spirv_id (ctx);
		unsigned test = spirv_emit_expr (e->loop.test, ctx);
		spirv_BranchConditional (e->loop.not, test, body, merge, ctx);
		spirv_LabelId (body, ctx);
		spirv_emit_expr (e->loop.body, ctx);
		spirv_SplitBlockId (cont, ctx);
		if (e->loop.continue_body) {
			spirv_emit_expr (e->loop.continue_body, ctx);
		}
		spirv_Branch (loop, ctx);
		spirv_LabelId (merge, ctx);
	}
	return 0;
}

static unsigned
spirv_select (const expr_t *e, spirvctx_t *ctx)
{
	unsigned merge = spirv_id (ctx);
	unsigned true_label = spirv_id (ctx);
	unsigned false_label = merge;
	if (e->select.false_body) {
		false_label = spirv_id (ctx);
	}
	unsigned test = spirv_emit_expr (e->select.test, ctx);
	spirv_SelectionMerge (merge, ctx);
	spirv_BranchConditional (e->select.not, test, true_label, false_label, ctx);
	spirv_LabelId (true_label, ctx);
	if (e->select.true_body) {
		spirv_emit_expr (e->select.true_body, ctx);
	}
	if (!is_block_terminated (ctx->code_space)) {
		spirv_Branch (merge, ctx);
	}
	if (e->select.false_body) {
		spirv_LabelId (false_label, ctx);
		spirv_emit_expr (e->select.false_body, ctx);
		if (!is_block_terminated (ctx->code_space)) {
			spirv_Branch (merge, ctx);
		}
	}
	spirv_LabelId (merge, ctx);
	return 0;
}

static unsigned
spirv_intrinsic (const expr_t *e, spirvctx_t *ctx)
{
	auto intr = e->intrinsic;
	auto instruction = spirv_instruction (ctx->core, intr.opcode);
	if (!instruction) {
		return 0;
	}
	unsigned op = instruction->opcode;

	int count = list_count (&intr.operands);
	int start = 0;
	const expr_t *operands[count + 1] = {};
	unsigned op_ids[count + 1] = {};
	list_scatter (&intr.operands, operands);
	if (op == SpvOpExtInst) {
		auto set = expr_string (operands[0]);
		auto extset = spirv_grammar (set);
		if (!extset) {
			error (operands[0], "unrecognized extension set %s", set);
			return 0;
		}
		op_ids[0] = spirv_extinst_import (ctx->module, set, ctx);
		auto extinst = spirv_instruction (extset, operands[1]);
		if (!extinst) {
			return 0;
		}
		op_ids[1] = extinst->opcode;
		start = 2;
	}
	for (int i = start; i < count; i++) {
		op_ids[i] = spirv_emit_expr (operands[i], ctx);
		if (intr.res_type && !is_void (intr.res_type)) {
			start = 2;
		}
	}
	unsigned tid = spirv_Type (intr.res_type, ctx);
	unsigned id = spirv_id (ctx);

	auto insn = spirv_new_insn (op, 1 + start + count, ctx->code_space, ctx);
	INSN (insn, 1) = tid;
	INSN (insn, 2) = id;
	memcpy (&INSN (insn, 3), op_ids, count * sizeof (op_ids[0]));
	return id;
}

static unsigned
spirv_switch (const expr_t *e, spirvctx_t *ctx)
{
	int num_labels = 0;
	auto test = e->switchblock.test;
	auto body = e->switchblock.body;
	auto break_label = e->switchblock.break_label;
	case_label_t _default_label = { .label = break_label };
	auto default_label = &_default_label;
	if (e->switchblock.default_label) {
		default_label = e->switchblock.default_label;
	}
	auto labels = e->switchblock.labels;
	for (auto l = labels; *l; l++) {
		num_labels++;
	}
	unsigned merge = spirv_label_id (&break_label->label, ctx);
	unsigned sel = spirv_emit_expr (test, ctx);
	unsigned dfl = spirv_label_id (&default_label->label->label, ctx);
	spirv_SelectionMerge (merge, ctx);
	auto insn = spirv_new_insn (SpvOpSwitch, 3 + 2 * num_labels,
								ctx->code_space, ctx);
	INSN (insn, 1) = sel;
	INSN (insn, 2) = dfl;
	for (int i = 0; i < num_labels; i++) {
		INSN (insn, 3 + i * 2) = expr_integral (labels[i]->value);
		auto label = &labels[i]->label->label;
		INSN (insn, 4 + i * 2) = spirv_label_id (label, ctx);
	}
	spirv_emit_expr (body, ctx);
	spirv_label (break_label, ctx);
	return 0;
}

static unsigned
spirv_xvalue (const expr_t *e, spirvctx_t *ctx)
{
	return spirv_emit_expr (e->xvalue.expr, ctx);
}

static unsigned
spirv_bitfield (const expr_t *e, spirvctx_t *ctx)
{
	auto bitfield = e->bitfield;
	auto backing_type = get_type (bitfield.src);
	unsigned base = spirv_emit_expr (bitfield.src, ctx);
	unsigned offset = spirv_emit_expr (bitfield.start, ctx);
	unsigned count = spirv_emit_expr (bitfield.length, ctx);
	unsigned tid = spirv_Type (backing_type, ctx);
	unsigned id = spirv_id (ctx);
	if (bitfield.insert) {
		unsigned insert = spirv_emit_expr (bitfield.insert, ctx);
		auto insn = spirv_new_insn (SpvOpBitFieldInsert, 7,
									ctx->code_space, ctx);
		INSN (insn, 1) = tid;
		INSN (insn, 2) = id;
		INSN (insn, 3) = base;
		INSN (insn, 4) = insert;
		INSN (insn, 5) = offset;
		INSN (insn, 6) = count;
	} else {
		SpvOp op = is_unsigned (bitfield.type) ? SpvOpBitFieldUExtract
											   : SpvOpBitFieldSExtract;
		auto insn = spirv_new_insn (op, 6, ctx->code_space, ctx);
		INSN (insn, 1) = tid;
		INSN (insn, 2) = id;
		INSN (insn, 3) = base;
		INSN (insn, 4) = offset;
		INSN (insn, 5) = count;
	}
	return id;
}

static unsigned
spirv_ptroffset (const expr_t *e, spirvctx_t *ctx)
{
	//FIXME move
	if (!type_spv_ptrarith.symtab) {
		static struct_def_t spv_ptrarith_struct[] = {
			{"result", &type_uint},
			{"carry",  &type_uint},
			{}
		};
		make_structure (".spv.ptrarith", 's', spv_ptrarith_struct,
						&type_spv_ptrarith);
		chain_type (&type_spv_ptrarith);
	}

	scoped_src_loc (e);
	auto ptr_type = get_type (e);
	auto ptr = e->ptroffset.ptr;
	auto offs = e->ptroffset.offset;
	ptr = cast_expr (&type_uvec2, ptr);
	if (type_size (get_type (offs)) != 1) {
		error (offs, "64-bit offset not supported (yet)");
		return 0;
	}
	offs = cast_expr (&type_uint, offs);
	const expr_t *add_operands[] = {
		edag_add_expr (new_swizzle_expr (ptr, "x")),
		offs,
	};
	auto add = new_intrinsic_expr (nullptr);
	add->intrinsic.opcode = new_int_expr (SpvOpIAddCarry, false);
	add->intrinsic.res_type = &type_spv_ptrarith;
	add->intrinsic.is_pure = true;
	list_gather (&add->intrinsic.operands, add_operands,
				 countof (add_operands));
	rua_ctx_t rctx = {};
	auto lo = expr_process (new_field_expr (add, new_name_expr ("result")), &rctx);
	auto carry = expr_process (new_field_expr (add, new_name_expr ("carry")), &rctx);
	auto hi = binary_expr ('+', carry, new_swizzle_expr (ptr, "y"));
	auto ptroffset = constructor_expr (new_type_expr (&type_uvec2),
									   expr_append_expr (new_list_expr (hi),
														 lo));
	ptroffset = cast_expr (ptr_type, ptroffset);
	return spirv_emit_expr (ptroffset, ctx);
}

static unsigned
spirv_emit_expr (const expr_t *e, spirvctx_t *ctx)
{
	if (spirv_expr_id (e, ctx)) {
		return spirv_expr_id (e, ctx);
	}
	static spirv_expr_f funcs[ex_count] = {
		[ex_bool] = spirv_bool,
		[ex_label] = spirv_label,
		[ex_block] = spirv_block,
		[ex_expr] = spirv_expr,
		[ex_uexpr] = spirv_uexpr,
		[ex_symbol] = spirv_symbol,
		[ex_temp] = spirv_temp,//FIXME don't want
		[ex_nil] = spirv_nil,
		[ex_value] = spirv_value,
		[ex_vector] = spirv_vector,
		[ex_compound] = spirv_compound,
		[ex_alias] = spirv_alias,
		[ex_address] = spirv_address,
		[ex_offset] = spirv_offset,
		[ex_assign] = spirv_assign,
		[ex_branch] = spirv_branch,
		[ex_return] = spirv_return,
		[ex_swizzle] = spirv_swizzle,
		[ex_extend] = spirv_extend,
		[ex_list] = spirv_list,
		[ex_incop] = spirv_incop,
		[ex_cond] = spirv_cond,
		[ex_field] = spirv_field_array,
		[ex_array] = spirv_field_array,
		[ex_loop] = spirv_loop,
		[ex_select] = spirv_select,
		[ex_intrinsic] = spirv_intrinsic,
		[ex_switch] = spirv_switch,
		[ex_xvalue] = spirv_xvalue,
		[ex_bitfield] = spirv_bitfield,
		[ex_ptroffset] = spirv_ptroffset,
	};

	if (e->type >= ex_count) {
		internal_error (e, "bad sub-expression type: %d", e->type);
	}
	if (!funcs[e->type]) {
		internal_error (e, "unexpected sub-expression type: %s",
						expr_names[e->type]);
	}

	if (options.code.debug) {
		auto cs = ctx->code_space;
		if (cs->size >= 4 && cs->data[cs->size - 4].value == 0x40008) {
			//back up and overwrite the previous debug line instruction
			cs->size -= 4;
		}
		spirv_DebugLine (e, ctx);
	}
	unsigned id = funcs[e->type] (e, ctx);
	spirv_add_expr_id (e, id, ctx);
	edag_flush ();
	return spirv_expr_id (e, ctx);
}

bool
spirv_write (struct pr_info_s *pr, const char *filename)
{
	pr->module->entry_point_space = defspace_new (ds_backed);
	pr->module->exec_modes = defspace_new (ds_backed);
	pr->module->strings = defspace_new (ds_backed);
	pr->module->names = defspace_new (ds_backed);
	pr->module->module_processed = defspace_new (ds_backed);
	pr->module->decorations = defspace_new (ds_backed);
	pr->module->globals = defspace_new (ds_backed);
	pr->module->func_declarations = defspace_new (ds_backed);
	pr->module->func_definitions = defspace_new (ds_backed);

	spirvctx_t ctx = {
		.module = pr->module,
		.code_space = defspace_new (ds_backed),
		.decl_space = defspace_new (ds_backed),
		.core = spirv_grammar ("core"),
		.type_ids = DARRAY_STATIC_INIT (64),
		.expr_ids = DARRAY_STATIC_INIT (64),
		.label_ids = DARRAY_STATIC_INIT (64),
		.func_queue = DARRAY_STATIC_INIT (16),
		.strpool = strpool_new (),
		.id = 0,
	};
	if (!ctx.core) {
		internal_error (0, "could not find core grammar");
	}
	auto space = defspace_new (ds_backed);
	auto header = spirv_new_insn (0, 5, space, &ctx);
	INSN (header, 0) = SpvMagicNumber;
	INSN (header, 1) = SpvVersion;
	INSN (header, 2) = 0;	// FIXME get a magic number for QFCC
	INSN (header, 3) = 0;	// Filled in later
	INSN (header, 4) = 0;	// Reserved

	if (pr->module->entry_points && !pr->module->entry_points->next) {
		// only one entry point
		// FIXME this should be optional
		for (size_t i = 0; i < pr->module->global_syms.size; i++) {
			auto sym = pr->module->global_syms.a[i];
			bool no_auto = false;
			//FIXME this is a bit of a hack for glsl symbol declaration
			//and attribute handling order
			for (auto attr = sym->attributes; attr; attr = attr->next) {
				if (strcmp (attr->name, "BuiltIn") == 0) {
					no_auto = true;
					break;
				}
			}
			if (!no_auto) {
				sym->id = spirv_variable (sym, &ctx);
			}
		}
	}

	for (auto ep = pr->module->entry_points; ep; ep = ep->next) {
		spirv_EntryPoint (ep, &ctx);
	}

	auto mod = pr->module;
	for (auto cap = pr->module->capabilities.head; cap; cap = cap->next) {
		spirv_Capability (expr_uint (cap->expr), space, &ctx);
	}
	for (auto ext = pr->module->extensions.head; ext; ext = ext->next) {
		spirv_Extension (expr_string (ext->expr), space, &ctx);
	}
	if (mod->extinst_imports) {
		ADD_DATA (space, mod->extinst_imports->space);
	}

	spirv_MemoryModel (expr_uint (pr->module->addressing_model),
					   expr_uint (pr->module->memory_model), space, &ctx);


	auto srcid = spirv_String (pr->src_name, &ctx);
	spirv_Source (0, 1, srcid, nullptr, &ctx);

	auto main_sym = symtab_lookup (pr->symtab, "main");
	if (main_sym && main_sym->sy_type == sy_func) {
	}

	INSN (header, 3) = ctx.id + 1;
	ADD_DATA (space, mod->entry_point_space);
	ADD_DATA (space, mod->exec_modes);
	ADD_DATA (space, mod->strings);
	ADD_DATA (space, mod->names);
	ADD_DATA (space, mod->module_processed);
	ADD_DATA (space, mod->decorations);
	ADD_DATA (space, mod->globals);
	ADD_DATA (space, mod->func_declarations);
	ADD_DATA (space, mod->func_definitions);

	return write_output (filename, space->data,
						 space->size * sizeof (pr_type_t));
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
	spirv_extinst_import (module, import, nullptr);
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
	for (auto ep = pr.module->entry_points; ep; ep = ep->next) {
		if (strcmp (ep->name, func->o_name) == 0) {
			if (ep->func && ep->func != func) {
				error (statements, "entry point %s redefined", ep->name);
			}
			ep->func = func;
		}
	}
}

static void
spirv_add_attr (attribute_t **attributes, const char *name, const expr_t *val)
{
	for (auto a = *attributes; a; a = a->next) {
		if (strcmp (a->name, name) == 0) {
			a->params = val;
			return;
		}
	}

	auto attr = new_attrfunc (name, val);
	attr->next = *attributes;
	*attributes = attr;
}

static void
spirv_add_str_attr (attribute_t **attrs, const char *name, const expr_t *val)
{
	if (!is_string_val (val)) {
		error (val, "not a constant integer");
		return;
	}
	spirv_add_attr (attrs, name, val);
}

static void
spirv_add_int_attr (attribute_t **attrs, const char *name, const expr_t *val)
{
	if (!is_integral_val (val)) {
		error (val, "not a constant integer");
		return;
	}
	spirv_add_attr (attrs, name, val);
}

static void
spirv_field_attributes (attribute_t **attributes, symbol_t *sym)
{
	for (auto a = attributes; *a; ) {
		auto attr = *a;

		int count = 0;
		if (attr->params) {
			count = list_count (&attr->params->list);
		}
		const expr_t *params[count + 1];
		params[count] = nullptr;
		if (attr->params) {
			list_scatter (&attr->params->list, params);
		}

		if (strcmp (attr->name, "offset") == 0) {
			auto val = params[0];
			if (!is_integral_val (val)) {
				error (val, "not a constant integer");
				return;
			}
			sym->offset = expr_integral (val);
		} else if (strcmp (attr->name, "builtin") == 0) {
			spirv_add_str_attr (&sym->attributes, "BuiltIn", params[0]);
		} else {
			a = &attr->next;
			continue;
		}
		*a = (*a)->next;
	}
}

typedef struct {
	const char *name;
	const char *decoration;
} spirv_qual_t;

static spirv_qual_t spirv_qualifier_map[] = {
	{ "centroid",      "Centroid" },
	{ "patch",         "Patch" },
	{ "sample",        "Sample" },
	{ "flat",          "Flat" },
	{ "noperspective", "NoPerspective" },
	{ "smooth",        nullptr },	// default interpolation
};

static bool
spirv_qualifier (const char *name, const char **qual)
{
	for (size_t i = 0; i < countof (spirv_qualifier_map); i++) {
		if (strcmp (name, spirv_qualifier_map[i].name) == 0) {
			*qual = spirv_qualifier_map[i].decoration;
			return true;
		}
	}
	return false;
}

static void
spirv_var_attributes (specifier_t *spec, attribute_t **attributes)
{
	symbol_t   *sym = spec->sym;
	for (auto a = attributes; *a; ) {
		auto attr = *a;
		const char *qual = nullptr;

		int num_params = 0;
		if (attr->params) {
			num_params = list_count (&attr->params->list);
		}
		const expr_t *params[num_params + 1];
		params[num_params] = nullptr;
		if (attr->params) {
			list_scatter (&attr->params->list, params);
		}

		if (strcmp (attr->name, "in") == 0) {
			spec->storage = sc_from_iftype (iface_in);
			if (num_params) {
				if (is_string_val (params[0])) {
					spirv_add_str_attr (&sym->attributes, "BuiltIn",
										params[0]);
				} else {
					spirv_add_int_attr (&sym->attributes, "Location",
										params[0]);
				}
			}
		} else if (strcmp (attr->name, "out") == 0) {
			spec->storage = sc_from_iftype (iface_out);
			if (num_params) {
				if (is_string_val (params[0])) {
					spirv_add_str_attr (&sym->attributes, "BuiltIn",
										params[0]);
				} else {
					spirv_add_int_attr (&sym->attributes, "Location",
										params[0]);
				}
			}
		} else if (strcmp (attr->name, "builtin") == 0) {
			spirv_add_str_attr (&sym->attributes, "BuiltIn", params[0]);
		} else if (strcmp (attr->name, "uniform") == 0) {
			spec->storage = sc_from_iftype (iface_uniform);
		} else if (strcmp (attr->name, "buffer") == 0) {
			spec->storage = sc_from_iftype (iface_buffer);
		} else if (strcmp (attr->name, "shared") == 0) {
			spec->storage = sc_from_iftype (iface_shared);
		} else if (strcmp (attr->name, "push_constant") == 0) {
			spec->storage = sc_from_iftype (iface_push_constant);
		} else if (strcmp (attr->name, "set") == 0) {
			spirv_add_int_attr (&sym->attributes, "DescriptorSet", params[0]);
		} else if (strcmp (attr->name, "binding") == 0) {
			spirv_add_int_attr (&sym->attributes, "Binding", params[0]);
		} else if (spirv_qualifier (attr->name, &qual)) {
			if (qual) {
				spirv_add_attr (&sym->attributes, qual, nullptr);
			}
		} else if (strcmp (attr->name, "input_attachment_index") == 0) {
			spirv_add_int_attr (&sym->attributes, "InputAttachmentIndex",
								params[0]);
		} else {
			a = &attr->next;
			continue;
		}
		*a = (*a)->next;
	}
}

static const expr_t *
spirv_convert_const (symbol_t *sym, void *data)
{
	const expr_t *init = data;
	auto type = dereference_type (sym->type);
	type = tagged_reference_type (SpvStorageClassFunction, type);
	auto tab = current_func->locals;
	auto s = new_symbol (sym->name);
	s->type = type;
	s->sy_type = sy_var;
	s->var.storage = sc_local;
	s->var.init = init;
	s->lvalue = sym->lvalue;
	spirv_add_attr (&s->attributes, "NonWritable", nullptr);
	symtab_addsymbol (tab, s);

	auto r = new_symbol_expr (s);
	return r;
}

static void
spirv_declare_sym (specifier_t spec, const expr_t *init, symtab_t *symtab,
				   expr_t *block)
{
	symbol_t   *sym = spec.sym;
	if (sym->name[0]) {
		symbol_t   *check = symtab_lookup (symtab, sym->name);
		if (check && check->table == symtab) {
			error (0, "%s redefined", sym->name);
		}
	}
	spirv_var_attributes (&spec, &spec.attributes);
	auto storage = spirv_storage_class (spec.storage, sym->type);
	auto type = auto_type (sym->type, init);
	if (is_void (type)) {
		error (0, "variable '%s' declared void", sym->name);
		type = type_default;
	}
	if (is_reference (type)) {
		type = dereference_type (type);
	}
	auto entry_point = pr.module->entry_points;
	if (is_array (type) && !type->array.count) {
		if (storage == SpvStorageClassInput) {
			if (entry_point->gl_in_length) {
				int count = expr_integral (entry_point->gl_in_length);
				type = array_type (type->array.type, count);
			}
		} else {
			if (init && init->type == ex_compound) {
				auto ele_type = dereference_type (type);
				type = array_type (ele_type, num_elements (init));
			}
		}
	}
	sym->type = tagged_reference_type (storage, type);
	sym->lvalue = !spec.is_const;
	sym->sy_type = sy_var;
	sym->var.storage = spec.storage;
	if (sym->name[0]) {
		symtab_addsymbol (symtab, sym);
	}
	if (symtab->type == stab_param || symtab->type == stab_local) {
		if (init) {
			if (!block && is_constexpr (init)) {
				notice (init, "!block %s\n", sym->name);
			} else if (block) {
				auto r = new_symbol_expr (sym);
				auto e = assign_expr (r, init);
				append_expr (block, e);
			} else {
				error (init, "non-constant initializer");
			}
		}
	} else {
		if (init) {
			if (is_constant (init)) {
				if (init->type == ex_compound && !init->compound.type) {
					auto i = new_compound_init ();
					i->compound = init->compound;
					i->compound.type = type;
					init = i;
				}
				sym->sy_type = sy_convert;
				sym->convert = (symconv_t) {
					.conv = spirv_convert_const,
					.data = (void *) init,
				};
			} else {
				error (init, "non-constant initializer");
			}
		}
	}
	if (storage == SpvStorageClassInput
		|| storage == SpvStorageClassOutput) {
		DARRAY_APPEND (&pr.module->global_syms, sym);
	}
}

static bool
spirv_create_entry_point (const char *name, const char *model_name,
						  attribute_t *mode)
{
	for (auto ep = pr.module->entry_points; ep; ep = ep->next) {
		if (strcmp (ep->name, name) == 0) {
			error (0, "entry point %s already exists", name);
			return false;
		}
	}
	unsigned model = spirv_enum_val ("ExecutionModel", model_name);
	if (model == SpvExecutionModelFragment) {
		auto m = new_attrfunc ("mode",
					new_int_expr (SpvExecutionModeOriginUpperLeft, false));
		m->next = mode;
		mode = m;
	}
	entrypoint_t *ep = malloc (sizeof (entrypoint_t));
	*(ep) = (entrypoint_t) {
		.next = pr.module->entry_points,
		.model = model,
		.name = save_string (name),
		.modes = mode,
		.interface_syms = DARRAY_STATIC_INIT (16),
	};
	pr.module->entry_points = ep;
	return true;
}

static symtab_t *
get_object_symtab (const type_t *type)
{
	if (is_algebra (type)) {
		return get_mvec_struct (type);
	}
	if (is_struct (type) || (is_nonscalar (type) && type->symtab)) {
		return type->symtab;
	}
	return nullptr;
}

static void
spirv_convert_element_chain (element_chain_t *element_chain)
{
	auto type = element_chain->type;

	int num_elements = type_count (type);
	element_t *elements[num_elements + 1] = {};
	for (auto ele = element_chain->head; ele; ele = ele->next) {
		if (ele->id < 0 || ele->id > num_elements) {
			internal_error (0, "invalid element chain offset");
		}
		elements[ele->id] = ele;
	}

	const type_t *ele_type = nullptr;
	auto symtab = get_object_symtab (type);
	auto sym = symtab ? symtab->symbols : nullptr;

	if (is_array (type)) {
		ele_type = dereference_type (type);
	}
	for (int i = 0; i < num_elements; i++) {
		if (sym) {
			ele_type = sym->type;
			sym = sym->next;
		}
		if (!elements[i]) {
			elements[i] = new_element (new_zero_expr (ele_type), nullptr);
		}
	}
	element_chain->head = nullptr;
	element_chain->tail = &element_chain->head;
	for (int i = 0; i < num_elements; i++) {
		auto ele = elements[i];
		ele->next = nullptr;
		append_init_element (element_chain, ele);
	}
}

static const expr_t *
spirv_initialized_temp (const type_t *type, const expr_t *src)
{
	if (src->type == ex_compound && src->compound.initialized_temp) {
		return src;
	}
	if (src->type != ex_compound && is_algebra (type)) {
		if (is_reference (get_type (src))) {
			src = pointer_deref (src);
		}
		src = expr_optimize (src);
		src = algebra_compound_expr (type, src);
		if (src->type != ex_compound) {
			return src;
		}
	}

	if (src->compound.type) {
		type = src->compound.type;
	}

	scoped_src_loc (src);
	auto new = new_compound_init ();
	if (!build_element_chain (&new->compound, type, src, 0)) {
		return new_error_expr ();
	}
	new->compound.type = type;
	new->compound.initialized_temp = true;
	spirv_convert_element_chain (&new->compound);
	return new;
}

static const expr_t *
spirv_assign_vector (const expr_t *dst, const expr_t *src)
{
	auto new = vector_to_compound (src);
	return new_assign_expr (dst, new);;
}

static const expr_t *
spirv_proc_switch (const expr_t *expr, rua_ctx_t *ctx)
{
	scoped_src_loc (expr);
	auto test = expr_process (expr->switchblock.test, ctx);
	if (is_error (test)) {
		return test;
	}
	auto type = get_type (test);
	if (is_reference (type)) {
		type = dereference_type (type);
		test = pointer_deref (test);
	}
	if (!is_integral (type)) {
		//FIXME ? it would be nice to support switch on float ranges
		return error (test, "switch expression must have integral type");
	}
	auto sb = ctx->switch_block;
	ctx->switch_block = new_switch_block ();
	ctx->switch_block->test = test;
	auto body = expr_process (expr->switchblock.body, ctx);
	auto break_label = expr->switchblock.break_label;

	case_label_t _default_label = {};
	auto default_label = &_default_label;
	// fetch and remove the default case if it exists
	default_label = Hash_DelElement (ctx->switch_block->labels, default_label);
	auto labels = (case_label_t **) Hash_GetList (ctx->switch_block->labels);

	if (!default_label) {
		if (options.warnings.enum_switch && is_enum (type)) {
			check_enum_switch (ctx->switch_block);
		}
	}
	ctx->switch_block = sb;

	auto swtch = new_switch_expr (test, body, break_label);
	swtch->switchblock.default_label = default_label;
	swtch->switchblock.labels = labels;

	return swtch;
}

static const expr_t *
spirv_vector_compare (int op, const expr_t *e1, const expr_t *e2)
{
	// both e1 and e2 should have the same types here
	auto type = get_type (e1);
	if (op != QC_EQ && op != QC_NE) {
		return error (e2, "invalid comparison for %s", type->name);
	}
	int hop = op == QC_EQ ? '&' : '|';
	auto e = new_binary_expr (op, e1, e2);
	e->expr.type = bool_type (type);
	auto h = new_horizontal_expr (hop, e, &type_int);
	return h;
}

static const expr_t *
spirv_shift_op (int op, const expr_t *e1, const expr_t *e2)
{
	auto t1 = get_type (e1);
	auto t2 = get_type (e2);

	if (is_matrix (t1) || is_matrix (t2)) {
		return error (e1, "invalid operands for %s", get_op_string (op));
	}
	if (!is_integral (t1) || !is_integral (t2)) {
		return error (e1, "invalid operands for %s", get_op_string (op));
	}
	if (is_uint (t1)) {
		t2 = vector_type (&type_int, type_width (t1));
	}
	if (is_ulong (t1)) {
		t2 = vector_type (&type_long, type_width (t1));
	}
	e1 = cast_expr (t1, e1);
	e2 = cast_expr (t2, e2);
	auto e = new_binary_expr (op, e1, e2);
	e->expr.type = t1;
	return fold_constants (e);
}

static bool __attribute__((pure))
spirv_types_logically_match (const type_t *dst, const type_t *src)
{
	if (type_same (dst, src)) {
		return true;
	}
	if (is_array (dst) && is_array (src)) {
		if (type_count (dst) != type_count (src)) {
			return false;
		}
		dst = dereference_type (dst);
		src = dereference_type (src);
		return spirv_types_logically_match (dst, src);
	}
	if (is_struct (dst) && is_struct (src)) {
		if (type_count (dst) != type_count (src)) {
			return false;
		}
		auto dsym = dst->symtab->symbols;
		auto ssym = src->symtab->symbols;
		for (; dsym && ssym; dsym = dsym->next, ssym = ssym->next) {
			if (!spirv_types_logically_match (dsym->type, ssym->type)) {
				return false;
			}
		}
		return true;
	}
	return false;
}

static const expr_t *
spirv_test_expr (const expr_t *expr)
{
	// ruamoko and spirv are mostly compatible for bools other than lbool
	// but that's handled by spirv_mirror_bool, and can't cast to bool
	auto test = ruamoko_test_expr (expr);
	if (test->type == ex_alias && is_bool (test->alias.type)) {
		scoped_src_loc (expr);
		if (test->alias.offset) {
			internal_error (test, "unexpected offset alias expression");
		}
		expr = test->alias.expr;
		test = typed_binary_expr (&type_bool, QC_NE, expr,
								  new_zero_expr (get_type (expr)));
	}
	return test;
}

static const expr_t *
spirv_cast_expr (const type_t *dstType, const expr_t *expr)
{
	if (is_boolean (dstType)) {
		return test_expr (expr);
	}

	auto src_type = get_type (expr);
	if (!spirv_types_logically_match (dstType, src_type)) {
		return nullptr;
	}
	auto cast = new_unary_expr ('C', expr);
	cast->expr.type = dstType;
	return cast;
}

static const expr_t *
spirv_check_types_compatible (const expr_t **dst, const expr_t **src)
{
	auto dst_type = get_type (*dst);
	auto src_type = get_type (*src);

	if (spirv_types_logically_match (dst_type, src_type)) {
		*src = spirv_cast_expr (dst_type, *src);
	}
	return nullptr;
}

static SpvCapability spirv_base_capabilities[] = {
	SpvCapabilityMatrix,
	SpvCapabilityShader,
	SpvCapabilityInputAttachment,
	SpvCapabilitySampled1D,
	SpvCapabilityImage1D,
	SpvCapabilitySampledBuffer,
	SpvCapabilityImageBuffer,
	SpvCapabilityImageQuery,
	SpvCapabilityDerivativeControl,
	SpvCapabilityStorageImageExtendedFormats,
	SpvCapabilityDeviceGroup,
	SpvCapabilityShaderNonUniform,
	SpvCapabilityPhysicalStorageBufferAddresses,
};

static const char *
spirv_type_getkey (const void *t, void *unused)
{
	return ((const type_t *) t)->name;
}

static void
spirv_init (void)
{
	static module_t module = {		//FIXME probably not what I want
		.global_syms = DARRAY_STATIC_INIT (16),
	};
	module.forward_structs = Hash_NewTable (61, spirv_type_getkey, 0, 0, 0);
	pr.module = &module;

	//FIXME unhardcode
	spirv_set_addressing_model (pr.module,
								SpvAddressingModelPhysicalStorageBuffer64);
	//FIXME look into Vulkan, or even configurable
	spirv_set_memory_model (pr.module, SpvMemoryModelGLSL450);

	for (size_t i = 0; i < countof (spirv_base_capabilities); i++) {
		auto cap = spirv_base_capabilities[i];
		spirv_add_capability (pr.module, cap);
	}
}

static bool
spirv_function_attr (const attribute_t *attr, metafunc_t *func)
{
	int count = 0;
	if (attr->params && attr->params->type == ex_list) {
		count = list_count (&attr->params->list);
	}
	const expr_t *params[count + 1] = {};
	if (attr->params) {
		// attribute params are reversed
		list_scatter_rev (&attr->params->list, params);
	}
	if (strcmp (attr->name, "shader") == 0) {
		if (func) {
			auto type = func->type;
			if (!is_func (type)) {
				internal_error (0, "non-function type");
			}
			if (!is_void (type->func.ret_type)) {
				error (0, "shader entry point must return void");
			}
			if (type->func.num_params) {
				error (0, "shader entry point must not take any parameters");
			}
			const char *model_name = "Vertex";
			if (count >= 1 && is_string_val (params[0])) {
				model_name = expr_string (params[0]);
			} else {
				error (0, "shader attribute requires a string");
			}
			attribute_t *mode = nullptr;
			for (int i = 1; i < count; i++) {
				const char *mode_name = nullptr;
				if (count >= 1 && is_string_val (params[i])) {
					mode_name = expr_string (params[i]);
					unsigned mid = spirv_enum_val ("ExecutionMode", mode_name);
					auto m = new_attrfunc ("mode", new_int_expr (mid, false));
					m->next = mode;
					mode = m;
				} else {
					error (0, "capability attribute requires a string");
				}
			}
			spirv_create_entry_point (func->name, model_name, mode);
		}
		return true;
	} else if (strcmp (attr->name, "capability") == 0) {
		//FIXME this should apply only to entry points
		const char *capability_name = nullptr;
		if (count >= 1 && is_string_val (params[0])) {
			capability_name = expr_string (params[0]);
		} else {
			error (0, "capability attribute requires a string");
		}
		uint32_t capability = spirv_enum_val ("Capability", capability_name);
		spirv_add_capability (pr.module, capability);
		return true;
	} else if (strcmp (attr->name, "extension") == 0) {
		const char *extension_name = nullptr;
		if (count >= 1 && is_string_val (params[0])) {
			extension_name = expr_string (params[0]);
		} else {
			error (0, "capability attribute requires a string");
		}
		spirv_add_extension (pr.module, extension_name);
		return true;
	}
	return false;
}

static const type_t *
spirv_pointer_type (const type_t *aux)
{
	auto type = aux;
	if (is_struct (type) && !type->symtab) {
		auto tag = va ("@bda.%s", type->name + 4);
		type_t new = {
			.type = ev_invalid,
			.name = save_string (va ("tag %s", tag)),
			.meta = ty_struct,
			.attributes = type->attributes,
		};
		type = find_type (&new);
		if (!type->symtab) {
			Hash_Add (pr.module->forward_structs, (void *) type);
		} else {
			internal_error (0, "tag %s already exists", tag);
		}
	} else {
		type = iface_block_type (type, "@bda");
	}
	return tagged_pointer_type (SpvStorageClassPhysicalStorageBuffer, type);
}

static void
spirv_finish_struct (const type_t *type)
{
	auto tag = va ("tag @bda.%s", type->name + 4);
	if (Hash_Del (pr.module->forward_structs, tag)) {
		iface_block_type (type, "@bda");
	}
}

static int
spirv_ptr_type_size (const type_t *type)
{
	return type_aligned_size (type) * sizeof (pr_type_t);
}

static const expr_t *
spirv_pointer_diff (const expr_t *ptra, const expr_t *ptrb)
{
	return error (ptra, "pointer difference not implemented without Int64");
	auto e1 = cast_expr (&type_long, ptra);
	auto e2 = cast_expr (&type_long, ptrb);
	auto type = get_type (ptra)->fldptr.type;
	auto psize = new_long_expr (spirv_ptr_type_size (type), true);
	return binary_expr ('/', binary_expr ('-', e1, e2), psize);
}

target_t spirv_target = {
	.init = spirv_init,
	.value_too_large = spirv_value_too_large,
	.build_scope = spirv_build_scope,
	.build_code = spirv_build_code,
	.field_attributes = spirv_field_attributes,
	.var_attributes = spirv_var_attributes,
	.declare_sym = spirv_declare_sym,
	.create_entry_point = spirv_create_entry_point,
	.finish_struct = spirv_finish_struct,
	.initialized_temp = spirv_initialized_temp,
	.assign_vector = spirv_assign_vector,
	.proc_switch = spirv_proc_switch,
	.proc_caselabel = ruamoko_proc_caselabel,
	.vector_compare = spirv_vector_compare,
	.shift_op = spirv_shift_op,
	.test_expr = spirv_test_expr,
	.cast_expr = spirv_cast_expr,
	.check_types_compatible = spirv_check_types_compatible,
	.pointer_diff = spirv_pointer_diff,
	.ptr_type_size = spirv_ptr_type_size,
	.type_assignable = spirv_types_logically_match,
	.init_type_ok = spirv_types_logically_match,
	.setup_intrinsic_symtab = spirv_setup_intrinsic_symtab,
	.function_attr = spirv_function_attr,

	.short_circuit = false,
	.pointer_type = spirv_pointer_type,
	.pointer_size = 2,	// internal sizes are in ints rather than bytes
};
