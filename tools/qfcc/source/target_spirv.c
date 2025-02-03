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
#include "tools/qfcc/include/options.h"
#include "tools/qfcc/include/qfcc.h"
#include "tools/qfcc/include/spirv.h"
#include "tools/qfcc/include/spirv_grammar.h"
#include "tools/qfcc/include/statements.h"
#include "tools/qfcc/include/strpool.h"
#include "tools/qfcc/include/symtab.h"
#include "tools/qfcc/include/target.h"
#include "tools/qfcc/include/type.h"
#include "tools/qfcc/include/value.h"

#define INSN(i,o) D_var_o(int,(i),o)
#define ADD_DATA(d,s) defspace_add_data((d), (s)->data, (s)->size)

typedef struct spirvctx_s {
	module_t   *module;

	defspace_t *decl_space;
	defspace_t *code_space;

	struct DARRAY_TYPE (unsigned) type_ids;
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
			|| (INSN(last_insn, 0) & SpvOpCodeMask) == SpvOpBranch)) {
		return true;
	}
	return false;
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
	INSN (insn, offs + str_size - 1) = 0;
	memcpy (&INSN (insn, offs), str, len);
	return insn;
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
	auto space = ctx->module->extinst_imports->space;
	auto insn = spirv_str_insn (SpvOpExtInstImport, 2, 0, imp, space);
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
				   defspace_t *space)
{
	auto insn = spirv_new_insn (SpvOpMemoryModel, 3, space);
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
	auto insn = spirv_str_insn (SpvOpString, 2, 0, name, strings);
	strid->id = spirv_id (ctx);
	INSN (insn, 1) = strid->id;
	return strid->id;
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
spirv_Decorate (unsigned id, SpvDecoration decoration, spirvctx_t *ctx)
{
	auto decorations = ctx->module->decorations;
	auto insn = spirv_new_insn (SpvOpDecorate, 3, decorations);
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
	auto insn = spirv_new_insn (SpvOpDecorate, 3 + size, decorations);
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
	auto insn = spirv_new_insn (SpvOpMemberDecorate, 4, decorations);
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
	auto insn = spirv_new_insn (SpvOpMemberDecorate, 4 + size, decorations);
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
	auto insn = spirv_new_insn (SpvOpMemberName, 3 + RUP(len, 4) / 4, names);
	INSN (insn, 1) = id;
	INSN (insn, 2) = member;
	memcpy (&INSN (insn, 3), name, len);
}

static void
spirv_DebugLine (const expr_t *e, spirvctx_t *ctx)
{
	unsigned file_id = spirv_String (GETSTR (e->loc.file), ctx);
	auto insn = spirv_new_insn (SpvOpLine, 4, ctx->code_space);
	INSN (insn, 1) = file_id;
	INSN (insn, 2) = e->loc.line;
	INSN (insn, 3) = e->loc.column;
}

static unsigned spirv_Type (const type_t *type, spirvctx_t *ctx);

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
	unsigned rid = spirv_Type (rtype, ctx);

	auto globals = ctx->module->globals;
	auto insn = spirv_new_insn (SpvOpTypePointer, 4, globals);
	INSN (insn, 1) = spirv_id (ctx);
	INSN (insn, 2) = type->fldptr.tag;
	INSN (insn, 3) = rid;
	return INSN (insn, 1);
}

static unsigned
spirv_type_array (unsigned tid, unsigned count, spirvctx_t *ctx)
{
	unsigned id = spirv_id (ctx);
	unsigned count_id = spirv_value (new_int_expr (count, false), ctx);
	auto globals = ctx->module->globals;
	auto insn = spirv_new_insn (SpvOpTypeArray, 4, globals);
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
	auto insn = spirv_new_insn (SpvOpTypeRuntimeArray, 3, globals);
	INSN (insn, 1) = id;
	INSN (insn, 2) = tid;
	return id;
}

static unsigned spirv_BlockType (const type_t *type, int *size, int *align,
								 spirvctx_t *ctx);

static unsigned
spirv_array (const type_t *type, int *size, int *align, spirvctx_t *ctx)
{
	auto etype = dereference_type (type);
	int e_size;
	int e_align;
	unsigned eid = spirv_BlockType (etype, &e_size, &e_align, ctx);
	unsigned count = type_count (type);

	unsigned id;
	if (count) {
		id = spirv_type_array (eid, count, ctx);
	} else {
		id = spirv_type_runtime_array (eid, ctx);
		count = 1;
	}
	e_size = RUP (e_size, e_align);
	*size = e_size * count;
	*align = e_align;
	e_size *= sizeof (uint32_t);
	spirv_DecorateLiteral (id, SpvDecorationArrayStride, &e_size, ev_int, ctx);
	return id;
}

static void
spirv_matrix_deco (const type_t *type, int id, int member, spirvctx_t *ctx)
{
	while (is_array (type)) {
		type = dereference_type (type);
	}
	if (!is_matrix (type)) {
		return;
	}
	int stride = type_size (column_type (type));
	stride *= sizeof (uint32_t);
	spirv_MemberDecorateLiteral (id, member, SpvDecorationMatrixStride,
								 &stride, ev_int, ctx);
	//FIXME
	spirv_MemberDecorate (id, member, SpvDecorationColMajor, ctx);

}

static unsigned
spirv_struct (const type_t *type, int *size, int *align, spirvctx_t *ctx)
{
	bool block = size && align;
	auto symtab = type_symtab (type);
	int num_members = 0;
	for (auto s = symtab->symbols; s; s = s->next) {
		num_members++;
	}
	if (!num_members) {
		*size = 0;
		*align = 1;
		error (0, "0 sized struct");
		return 0;
	}
	unsigned member_types[num_members];
	int member_sizes[num_members] = {};
	int member_align[num_members] = {};
	num_members = 0;
	for (auto s = symtab->symbols; s; s = s->next) {
		int m = num_members++;
		if (block) {
			member_types[m] = spirv_BlockType (s->type, &member_sizes[m],
											   &member_align[m], ctx);
		} else {
			member_sizes[m] = type_size (s->type);
			member_align[m] = type_align (s->type);
			member_types[m] = spirv_Type (s->type, ctx);
		}
	}

	unsigned id = spirv_id (ctx);
	auto globals = ctx->module->globals;
	auto insn = spirv_new_insn (SpvOpTypeStruct,
								2 + num_members, globals);
	INSN (insn, 1) = id;
	memcpy (&INSN (insn, 2), member_types, sizeof (member_types));

	spirv_Name (id, unalias_type (type)->name + 4, ctx);

	unsigned offset = 0;
	unsigned total_size = 0;
	int alignment = 1;
	int i = 0;
	num_members = 0;
	for (auto s = symtab->symbols; s; s = s->next, i++) {
		int m = num_members++;
		spirv_MemberName (id, m, s->name, ctx);
		spirv_member_decorate_id (id, m, s->attributes, ctx);
		if (block) {
			if (s->offset >= 0 && symtab->type == stab_block) {
				offset = s->offset;
			}
			if (member_align[i] > alignment) {
				alignment = member_align[i];
			}
			offset = RUP (offset, member_align[i] * sizeof (uint32_t));
			spirv_MemberDecorateLiteral (id, m, SpvDecorationOffset,
										 &offset, ev_int, ctx);
			offset += member_sizes[i] * sizeof (uint32_t);
			if (offset > total_size) {
				total_size = offset;
			}

			spirv_matrix_deco (s->type, id, m, ctx);
		}
	}
	if (block) {
		total_size = RUP (total_size, alignment * sizeof (uint32_t));
		if (symtab->type != stab_block) {
			total_size /= sizeof (uint32_t);
		}
		*size = total_size;
		*align = alignment;
	}
	return id;
}

static unsigned
spirv_BlockType (const type_t *type, int *size, int *align, spirvctx_t *ctx)
{
	if (is_structural (type)) {
		unsigned id;
		if (is_array (type)) {
			id = spirv_array (type, size, align, ctx);
		} else {
			// struct or union, but union not allowed
			id = spirv_struct (type, size, align, ctx);
		}
		return id;
	} else {
		// non-aggregate, non-matrix type, so no need for special handling
		*size = type_size (type);
		*align = type_align (type);
		return spirv_Type (type, ctx);
	}
}

static unsigned
spirv_TypeStruct (const type_t *type, spirvctx_t *ctx)
{
	auto symtab = type_symtab (type);
	bool block = symtab->type == stab_block;

	unsigned id;
	if (block) {
		int size, align;
		id = spirv_struct (type, &size, &align, ctx);
	} else {
		id = spirv_struct (type, nullptr, nullptr, ctx);
	}
	return id;
}

static unsigned
spirv_TypeArray (const type_t *type, spirvctx_t *ctx)
{
	auto ele_type = dereference_type (type);
	auto count_expr = new_int_expr (type_count (type), false);
	unsigned count = spirv_value (count_expr, ctx);
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
	auto insn = spirv_new_insn (SpvOpTypeBool, 2, globals);
	INSN (insn, 1) = id;
	spirv_mirror_bool (type, id, ctx);
	return id;
}

static unsigned
spirv_TypeImage (glsl_image_t *image, spirvctx_t *ctx)
{
	if (image->id) {
		return image->id;
	}
	unsigned tid = spirv_Type (image->sample_type, ctx);
	auto globals = ctx->module->globals;
	image->id = spirv_id (ctx);
	auto insn = spirv_new_insn (SpvOpTypeImage, 9, globals);
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
	auto insn = spirv_new_insn (SpvOpTypeSampledImage, 3, globals);
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
		id = spirv_TypePointer (type, ctx);
	} else if (is_struct (type)) {
		id = spirv_TypeStruct (type, ctx);
	} else if (is_boolean (type)) {
		id = spirv_TypeBool (type, ctx);
	} else if (is_handle (type)
			   && (type->handle.type == &type_glsl_image
				   || type->handle.type == &type_glsl_sampled_image)) {
		auto image = &glsl_imageset.a[type->handle.extra];
		id = spirv_TypeImage (image, ctx);
		if (type->handle.type == &type_glsl_sampled_image) {
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

static unsigned
spirv_LabelId (unsigned id, spirvctx_t *ctx)
{
	auto insn = spirv_new_insn (SpvOpLabel, 2, ctx->code_space);
	INSN (insn, 1) = id;
	return id;
}

static void
spirv_LoopMerge (unsigned merge, unsigned cont, spirvctx_t *ctx)
{
	auto insn = spirv_new_insn (SpvOpLoopMerge, 4, ctx->code_space);
	INSN (insn, 1) = merge;
	INSN (insn, 2) = cont;
	INSN (insn, 3) = SpvLoopControlMaskNone;
}

static void
spirv_SelectionMerge (unsigned merge, spirvctx_t *ctx)
{
	auto insn = spirv_new_insn (SpvOpSelectionMerge, 3, ctx->code_space);
	INSN (insn, 1) = merge;
	INSN (insn, 2) = SpvSelectionControlMaskNone;
}

static void
spirv_Branch (unsigned label, spirvctx_t *ctx)
{
	auto insn = spirv_new_insn (SpvOpBranch, 2, ctx->code_space);
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
	auto insn = spirv_new_insn (SpvOpBranchConditional, 4, ctx->code_space);
	INSN (insn, 1) = test;
	INSN (insn, 2) = not ? false_label : true_label;
	INSN (insn, 3) = not ? true_label : false_label;
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
	INSN (insn, 1) = spirv_Type (type, ctx);
	INSN (insn, 2) = id;
	spirv_Name (id, name, ctx);
	return id;
}

static SpvStorageClass
spirv_storage_class (unsigned storage, const type_t *type)
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
	if (sc == SpvStorageClassUniform) {
		// tested here because SpvStorageUniformConstant is 0
		if (is_reference (type)) {
			type = dereference_type (type);
		}
		if (is_handle (type)) {
			sc = SpvStorageClassUniformConstant;
		}
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
	auto storage = spirv_storage_class (sym->var.storage, sym->type);
	if (storage == SpvStorageClassFunction) {
		space = ctx->decl_space;
	} else {
		DARRAY_APPEND (&ctx->module->entry_points->interface_syms, sym);
	}
	auto type = sym->type;
	unsigned tid = spirv_Type (type, ctx);
	unsigned id = spirv_id (ctx);
	//FIXME init value
	auto insn = spirv_new_insn (SpvOpVariable, 4, space);
	INSN (insn, 1) = tid;
	INSN (insn, 2) = id;
	INSN (insn, 3) = storage;
	spirv_Name (id, sym->name, ctx);
	spirv_decorate_id (id, sym->attributes, ctx);
	return id;
}

static unsigned spirv_emit_expr (const expr_t *e, spirvctx_t *ctx);

static unsigned
spirv_undef (const type_t *type, spirvctx_t *ctx)
{
	unsigned spirv_Type = spirv_type_id (type, ctx);
	unsigned id = spirv_id (ctx);
	auto insn = spirv_new_insn (SpvOpUndef, 3, ctx->module->globals);
	INSN (insn, 1) = spirv_Type;
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
	auto insn = spirv_new_insn (SpvOpEntryPoint, iface_start + count, linkage);
	INSN (insn, 1) = entrypoint->model;
	INSN (insn, 2) = func_id;
	memcpy (&INSN (insn, 3), entrypoint->name, len);
	for (int i = 0; i < count; i++) {
		INSN (insn, iface_start + i) = interface_syms->a[i]->id;
	}

	auto exec_modes = ctx->module->exec_modes;
	if (entrypoint->invocations) {
		insn = spirv_new_insn (SpvOpExecutionMode, 4, exec_modes);
		INSN (insn, 1) = func_id;
		INSN (insn, 2) = SpvExecutionModeInvocations;
		INSN (insn, 3) = expr_integral (entrypoint->invocations);
	}
	// assume that if 1 is set, all are set
	if (entrypoint->local_size[0]) {
		insn = spirv_new_insn (SpvOpExecutionMode, 6, exec_modes);
		INSN (insn, 1) = func_id;
		//FIXME LocalSizeId
		INSN (insn, 2) = SpvExecutionModeLocalSize;
		INSN (insn, 3) = expr_integral (entrypoint->local_size[0]);
		INSN (insn, 4) = expr_integral (entrypoint->local_size[1]);
		INSN (insn, 5) = expr_integral (entrypoint->local_size[2]);
	}
	if (entrypoint->primitive_in) {
		insn = spirv_new_insn (SpvOpExecutionMode, 3, exec_modes);
		INSN (insn, 1) = func_id;
		INSN (insn, 2) = expr_integral (entrypoint->primitive_in);
	}
	if (entrypoint->primitive_out) {
		insn = spirv_new_insn (SpvOpExecutionMode, 3, exec_modes);
		INSN (insn, 1) = func_id;
		INSN (insn, 2) = expr_integral (entrypoint->primitive_out);
	}
	if (entrypoint->max_vertices) {
		insn = spirv_new_insn (SpvOpExecutionMode, 4, exec_modes);
		INSN (insn, 1) = func_id;
		INSN (insn, 2) = SpvExecutionModeOutputVertices;
		INSN (insn, 3) = expr_integral (entrypoint->max_vertices);
	}
	if (entrypoint->spacing) {
		insn = spirv_new_insn (SpvOpExecutionMode, 3, exec_modes);
		INSN (insn, 1) = func_id;
		INSN (insn, 2) = expr_integral (entrypoint->spacing);
	}
	if (entrypoint->order) {
		insn = spirv_new_insn (SpvOpExecutionMode, 3, exec_modes);
		INSN (insn, 1) = func_id;
		INSN (insn, 2) = expr_integral (entrypoint->order);
	}
	if (entrypoint->frag_depth) {
		insn = spirv_new_insn (SpvOpExecutionMode, 3, exec_modes);
		INSN (insn, 1) = func_id;
		INSN (insn, 2) = expr_integral (entrypoint->frag_depth);
	}
	if (entrypoint->point_mode) {
		insn = spirv_new_insn (SpvOpExecutionMode, 3, exec_modes);
		INSN (insn, 1) = func_id;
		INSN (insn, 2) = SpvExecutionModePointMode;
	}
	if (entrypoint->early_fragment_tests) {
		insn = spirv_new_insn (SpvOpExecutionMode, 3, exec_modes);
		INSN (insn, 1) = func_id;
		INSN (insn, 2) = SpvExecutionModeEarlyFragmentTests;
	}
	for (auto m = entrypoint->modes; m; m = m->next) {
		insn = spirv_new_insn (SpvOpExecutionMode, 3, exec_modes);
		INSN (insn, 1) = func_id;
		INSN (insn, 2) = expr_integral (m->params);
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
	bool        mat1;
	bool        mat2;
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
								ctx->code_space);
	INSN (insn, 1) = tid;
	INSN (insn, 2) = id;
	for (int i = 0; i < count; i++) {
		INSN (insn, 3 + i) = columns[i];
	}
	return id;
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
	{"add",  .types1 = SPV_FLOAT, .types2 = SPV_FLOAT,
			 .mat1 = true, .mat2 = true,
			 .generate = spirv_generate_matrix },
	{"sub",    SpvOpISub,                 SPV_INT,   SPV_INT   },
	{"sub",    SpvOpFSub,                 SPV_FLOAT, SPV_FLOAT },
	{"add",  .types1 = SPV_FLOAT, .types2 = SPV_FLOAT,
			 .mat1 = true, .mat2 = true,
			 .generate = spirv_generate_matrix },
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
	{"not",    SpvOpLogicalNot,           SPV_INT,   0         },

	{"shl",    SpvOpShiftLeftLogical,     SPV_INT,   SPV_INT   },
	{"shr",    SpvOpShiftRightLogical,    SPV_UINT,  SPV_INT   },
	{"shr",    SpvOpShiftRightArithmetic, SPV_SINT,  SPV_INT   },

	{"dot",    SpvOpDot,                  SPV_FLOAT, SPV_FLOAT },
	{"scale",  SpvOpVectorTimesScalar,    SPV_FLOAT, SPV_FLOAT },
	{"scale",  SpvOpMatrixTimesScalar,    SPV_FLOAT, SPV_FLOAT,
		.mat1 = true, .mat2 = false },

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
spirv_find_op (const char *op_name, const type_t *type1, const type_t *type2)
{
	constexpr int num_ops = sizeof (spv_ops) / sizeof (spv_ops[0]);
	etype_t t1 = type1->type;
	etype_t t2 = type2 ? type2->type : ev_void;
	bool mat1 = is_matrix (type1);
	bool mat2 = is_matrix (type2);

	for (int i = 0; i < num_ops; i++) {
		if (strcmp (spv_ops[i].op_name, op_name) == 0
			&& spv_ops[i].types1 & SPV_type(t1)
			&& ((!spv_ops[i].types2 && t2 == ev_void)
				|| (spv_ops[i].types2
					&& (spv_ops[i].types2 & SPV_type(t2))))
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
	}
	if (!op) {
		internal_error (e, "unexpected type combination");
	}

	unsigned cid = spirv_emit_expr (e->expr.e1, ctx);
	unsigned tid = spirv_Type (dst_type, ctx);
	unsigned id = spirv_id (ctx);
	auto insn = spirv_new_insn (op, 4, ctx->code_space);
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
	if (!spv_op->op) {
		internal_error (e, "unimplemented op: %s %s\n", op_name,
						get_type_string(t));
	}
	unsigned uid = spirv_emit_expr (e->expr.e1, ctx);

	unsigned tid = spirv_Type (get_type (e), ctx);
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
	auto insn = spirv_new_insn (SpvOpConstantComposite, 3 + width, space);
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
	auto insn = spirv_new_insn (SpvOpConstantComposite, 3 + count, space);
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
	auto insn = spirv_new_insn (SpvOpConstantNull, 3, globals);
	INSN (insn, 1) = tid;
	INSN (insn, 2) = id;
	return id;
}

static unsigned
spirv_value (const expr_t *e, spirvctx_t *ctx)
{
	auto value = e->value;
	if (!value->id) {
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

	auto type = e->compound.type;
	int tid = spirv_Type (type, ctx);
	int id = spirv_id (ctx);
	auto insn = spirv_new_insn (SpvOpCompositeConstruct, 3 + num_ele,
								ctx->code_space);
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
	int tid = spirv_Type (type, ctx);
	int id = spirv_id (ctx);
	auto insn = spirv_new_insn (SpvOpBitcast, 4, ctx->code_space);
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
	list_scatter (&list, ind_expr);
	for (int i = 0; i < num_obj; i++) {
		auto obj = ind_expr[i];
		bool direct_ind = false;
		unsigned index;
		if (obj->type == ex_field) {
			if (obj->field.member->type != ex_symbol) {
				internal_error (obj->field.member, "not a symbol");
			}
			auto sym = obj->field.member->symbol;
			index = sym->id;
		} else if (obj->type == ex_array) {
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
			ind_id[i] = index;
		} else {
			auto ind = new_uint_expr (index);
			ind_id[i] = spirv_emit_expr (ind, ctx);
		}
	}

	int acc_type_id = spirv_Type (*acc_type, ctx);
	int id = spirv_id (ctx);
	auto insn = spirv_new_insn (op, 4 + num_obj, ctx->code_space);
	INSN (insn, 1) = acc_type_id;
	INSN (insn, 2) = id;
	INSN (insn, 3) = base_id;
	auto field_ind = &INSN (insn, 4);
	for (int i = 0; i < num_obj; i++) {
		field_ind[i] = ind_id[i];
	}
	return id;
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
	if (e->assign.dst->type == ex_field || e->assign.dst->type == ex_array) {
		const type_t *res_type;
		const type_t *acc_type;
		dst = spirv_access_chain (e->assign.dst, ctx, &res_type, &acc_type);
		if (res_type == acc_type) {
			internal_error (e, "assignment to temp?");
		}
	} else if (is_deref (e->assign.dst)) {
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
		auto insn = spirv_new_insn (SpvOpReturnValue, 2, ctx->code_space);
		INSN (insn, 1) = ret_id;
	} else {
		spirv_new_insn (SpvOpReturn, 1, ctx->code_space);
	}
	return 0;
}

static unsigned
spirv_swizzle (const expr_t *e, spirvctx_t *ctx)
{
	int count = type_width (e->swizzle.type);
	unsigned src_id = spirv_emit_expr (e->swizzle.src, ctx);
	unsigned tid = spirv_Type (e->swizzle.type, ctx);
	unsigned id = spirv_id (ctx);
	auto insn = spirv_new_insn (SpvOpVectorShuffle, 5 + count, ctx->code_space);
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
	int tid = spirv_Type (res_type, ctx);
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
	auto dst = new_expr ();
	*dst = *e->incop.expr;
	dst->id = 0;
	auto incop = binary_expr (e->incop.op, e->incop.expr, one);
	unsigned inc_id = spirv_emit_expr (incop, ctx);
	unsigned src_id = incop->expr.e1->id;
	auto assign = assign_expr (dst, incop);
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
	auto insn = spirv_new_insn (SpvOpSelect, 6, ctx->code_space);
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
		unsigned ptr_id = id;
		int res_type_id = spirv_Type (res_type, ctx);

		id = spirv_id (ctx);
		auto insn = spirv_new_insn (SpvOpLoad, 4, ctx->code_space);
		INSN (insn, 1) = res_type_id;
		INSN (insn, 2) = id;
		INSN (insn, 3) = ptr_id;
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
	spirv_emit_expr (e->select.true_body, ctx);
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
	unsigned op = spirv_instruction_opcode ("core", intr.opcode);

	int count = list_count (&intr.operands);
	int start = 0;
	const expr_t *operands[count + 1] = {};
	unsigned op_ids[count + 1] = {};
	list_scatter (&intr.operands, operands);
	if (op == SpvOpExtInst) {
		auto set = expr_string (operands[0]);
		op_ids[0] = spirv_extinst_import (ctx->module, set, ctx);
		op_ids[1] = spirv_instruction_opcode (set, operands[1]);
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

	auto insn = spirv_new_insn (op, 1 + start + count, ctx->code_space);
	INSN (insn, 1) = tid;
	INSN (insn, 2) = id;
	memcpy (&INSN (insn, 3), op_ids, count * sizeof (op_ids[0]));
	return id;
}

static unsigned
spirv_emit_expr (const expr_t *e, spirvctx_t *ctx)
{
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
	};

	if (e->type >= ex_count) {
		internal_error (e, "bad sub-expression type: %d", e->type);
	}
	if (!funcs[e->type]) {
		internal_error (e, "unexpected sub-expression type: %s",
						expr_names[e->type]);
	}

	if (!e->id) {
		if (options.code.debug) {
			auto cs = ctx->code_space;
			if (cs->size >= 4 && cs->data[cs->size - 4].value == 0x40008) {
				//back up and overwrite the previous debug line instruction
				cs->size -= 4;
			}
			spirv_DebugLine (e, ctx);
		}
		unsigned id = funcs[e->type] (e, ctx);
		//FIXME const cast (store elsewhere)
		((expr_t *) e)->id = id;
	}
	return e->id;
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
		.type_ids = DARRAY_STATIC_INIT (64),
		.label_ids = DARRAY_STATIC_INIT (64),
		.func_queue = DARRAY_STATIC_INIT (16),
		.strpool = strpool_new (),
		.id = 0,
	};
	auto space = defspace_new (ds_backed);
	auto header = spirv_new_insn (0, 5, space);
	INSN (header, 0) = SpvMagicNumber;
	INSN (header, 1) = SpvVersion;
	INSN (header, 2) = 0;	// FIXME get a magic number for QFCC
	INSN (header, 3) = 0;	// Filled in later
	INSN (header, 4) = 0;	// Reserved

	for (auto ep = pr->module->entry_points; ep; ep = ep->next) {
		spirv_EntryPoint (ep, &ctx);
	}

	auto mod = pr->module;
	for (auto cap = pr->module->capabilities.head; cap; cap = cap->next) {
		spirv_Capability (expr_uint (cap->expr), space);
	}
	for (auto ext = pr->module->extensions.head; ext; ext = ext->next) {
		spirv_Extension (expr_string (ext->expr), space);
	}
	if (mod->extinst_imports) {
		ADD_DATA (space, mod->extinst_imports->space);
	}

	spirv_MemoryModel (expr_uint (pr->module->addressing_model),
					   expr_uint (pr->module->memory_model), space);


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
	auto storage = spirv_storage_class (spec.storage, sym->type);
	auto type = auto_type (sym->type, init);
	sym->type = type;
	sym->type = tagged_reference_type (storage, sym->type);
	sym->lvalue = !spec.is_const;
	sym->sy_type = sy_var;
	sym->var.storage = spec.storage;
	if (sym->name[0]) {
		symtab_addsymbol (symtab, sym);
		if (is_struct (type) && type_symtab (type)->type == stab_block) {
			auto block = (glsl_block_t *) type_symtab (type)->data;
			glsl_apply_attributes (block->attributes, spec);
		}
	}
	if (symtab->type == stab_param || symtab->type == stab_local) {
		if (init) {
			if (!block && is_constexpr (init)) {
				printf ("!block %s\n", sym->name);
			} else if (block) {
				auto r = new_symbol_expr (sym);
				auto e = assign_expr (r, init);
				append_expr (block, e);
			} else {
				error (init, "non-constant initializer");
			}
		}
	}
}

static bool
spirv_create_entry_point (const char *name, const char *model_name)
{
	for (auto ep = pr.module->entry_points; ep; ep = ep->next) {
		if (strcmp (ep->name, name) == 0) {
			error (0, "entry point %s already exists", name);
			return false;
		}
	}
	attribute_t *mode = nullptr;
	unsigned model = spirv_enum_val ("ExecutionModel", model_name);
	if (model == SpvExecutionModelFragment) {
		mode = new_attribute ("mode",
				new_int_expr (SpvExecutionModeOriginUpperLeft, false));
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

static const expr_t *
spirv_build_element_chain (element_chain_t *element_chain, const type_t *type,
						   const expr_t *eles)
{
	type = unalias_type (type);

	initstate_t state = {};

	if (is_struct (type) || (is_nonscalar (type) && type->symtab)) {
		state.field = type->symtab->symbols;
		// find first initializable field
		while (state.field && skip_field (state.field)) {
			state.field = state.field->next;
		}
		if (state.field) {
			state.type = state.field->type;
			state.offset = state.field->id;
		}
	} else if (is_matrix (type)) {
		state.type = column_type (type);
	} else if (is_nonscalar (type)) {
		state.type = base_type (type);
	} else if (is_array (type)) {
		state.type = dereference_type (type);
	} else {
		return error (eles, "invalid initialization");
	}
	if (!state.type) {
		return error (eles, "initialization of incomplete type");
	}

	for (auto ele = eles->compound.head; ele; ele = ele->next) {
		//FIXME designated initializers
		if (!state.type) {
			return new_error_expr ();
		}
		// FIXME vectors are special (ie, check for overlaps)
		if (state.offset >= type_count (type)) {
			if (options.warnings.initializer) {
				warning (eles, "excessive elements in initializer");
			}
			break;
		}
		if (ele->expr && ele->expr->type == ex_compound) {
			const expr_t *err;
			if ((err = spirv_build_element_chain (element_chain, state.type,
												  ele->expr))) {
				return err;
			}
		} else {
			auto element = new_element (nullptr, nullptr);
			auto expr = ele->expr;
			if (expr) {
				expr = cast_expr (state.type, expr);
			}
			if (is_error (expr)) {
				return expr;
			}
			*element = (element_t) {
				.type = state.type,
				.offset = state.offset,
				.expr = expr,
			};
			append_init_element (element_chain, element);
		}
		state.offset += type_count (state.type);
		if (state.field) {
			state.field = state.field->next;
			// find next initializable field
			while (state.field && skip_field (state.field)) {
				state.field = state.field->next;
			}
			if (state.field) {
				state.type = state.field->type;
				state.offset = state.field->id;
			}
		}
	}

	return nullptr;
}

static const expr_t *
spirv_initialized_temp (const type_t *type, const expr_t *src)
{
	if (src->compound.type) {
		type = src->compound.type;
	}

	scoped_src_loc (src);
	auto new = new_compound_init ();
	const expr_t *err;
	if ((err = spirv_build_element_chain (&new->compound, type, src))) {
		return err;
	}
	new->compound.type = type;
	return new;
}

static const expr_t *
spirv_assign_vector (const expr_t *dst, const expr_t *src)
{
	auto new = vector_to_compound (src);
	return new_assign_expr (dst, new);;
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
	e = new_horizontal_expr (hop, e, &type_int);
	return e;
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

static const expr_t *
spirv_check_types_compatible (const expr_t *dst, const expr_t *src)
{
	auto dst_type = get_type (dst);
	auto src_type = get_type (src);

	if (!is_struct (dst_type) || !is_struct (src_type)) {
		return nullptr;
	}
	auto dst_tab = type_symtab (dst_type);
	auto src_tab = type_symtab (src_type);
	auto dsym = dst_tab->symbols;
	auto ssym = src_tab->symbols;
	int count = 0;
	for (; dsym && ssym; dsym = dsym->next, ssym = ssym->next, count++) {
		if (dsym->type != ssym->type || strcmp (dsym->name, ssym->name) != 0) {
			break;
		}
	}
	// struct symtabs didn't match, or both empty
	if (dsym || ssym || !count) {
		return nullptr;
	}
	expr_t type_expr = {
		.loc = dst->loc,
		.type = ex_type,
		.typ.type = dst_type,
	};
	const expr_t *param_exprs[count];
	int i = 0;
	for (ssym = src_tab->symbols; ssym; ssym = ssym->next, i++) {
		auto e = new_field_expr (src, new_name_expr (ssym->name));
		e->field.type = ssym->type;
		param_exprs[i] = e;
	}
	auto params = new_list_expr (nullptr);
	list_gather (&params->list, param_exprs, count);
	return constructor_expr (&type_expr, params);
}

static void
spirv_init (void)
{
	static module_t module;		//FIXME probably not what I want
	pr.module = &module;
}

target_t spirv_target = {
	.init = spirv_init,
	.value_too_large = spirv_value_too_large,
	.build_scope = spirv_build_scope,
	.build_code = spirv_build_code,
	.declare_sym = spirv_declare_sym,
	.create_entry_point = spirv_create_entry_point,
	.initialized_temp = spirv_initialized_temp,
	.assign_vector = spirv_assign_vector,
	.setup_intrinsic_symtab = spirv_setup_intrinsic_symtab,
	.vector_compare = spirv_vector_compare,
	.shift_op = spirv_shift_op,
	// ruamoko and spirv are mostly compatible for bools other than lbool
	// but that's handled by spirv_mirror_bool
	.test_expr = ruamoko_test_expr,
	.check_types_compatible = spirv_check_types_compatible,
};
