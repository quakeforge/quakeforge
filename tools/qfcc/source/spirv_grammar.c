/*
	spirv_grammar.c

	SPIR-V grammar json embedding

	Copyright (C) 2024 Bill Currie

	Author: Bill Currie <bill@taniwha.org>

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

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#include <stdlib.h>

#include "QF/heapsort.h"
#include "QF/plist.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "tools/qfcc/include/qfcc.h"

#include "tools/qfcc/include/diagnostic.h"
#include "tools/qfcc/include/expr.h"
#include "tools/qfcc/include/options.h"
#include "tools/qfcc/include/shared.h"
#include "tools/qfcc/include/strpool.h"
#include "tools/qfcc/include/struct.h"
#include "tools/qfcc/include/symtab.h"
#include "tools/qfcc/include/spirv.h"
#include "tools/qfcc/include/spirv_grammar.h"
#include "tools/qfcc/include/type.h"
#include "tools/qfcc/include/value.h"

typedef struct spirv_json_s {
	struct spirv_json_s *parent;
	const char *name;
	const char *json;
	spirv_grammar_t *grammar;
} spirv_json_t;

static spirv_json_t builtin_json[] = {
	{ .name = "core",
	  .json =
#include "tools/qfcc/source/spirv.core.grammar.jinc"
		},
	{ .parent = &builtin_json[0],
	  .name = "GLSL.std.450",
	  .json =
#include "tools/qfcc/source/extinst.glsl.std.450.grammar.jinc"
		},
	{}
};

static void *
spvg_alloc (void *data, size_t size)
{
	return malloc (size);
}

typedef struct parse_string_s {
	size_t      value_offset;
} parse_string_t;

typedef struct parse_array_s {
	pltype_t    type;
	size_t      stride;
	plparser_t  parser;
	void       *data;
	size_t      value_offset;
	size_t      size_offset;
	__compar_fn_t cmp;
} parse_array_t;

static int
parse_char (const plfield_t *field, const plitem_t *item,
			void *data, plitem_t *messages, void *context)
{
	auto value = (char *) data;

	const char *str = PL_String (item);
	*value = *str;
	return 1;
}

static int
parse_hex (const plfield_t *field, const plitem_t *item,
		   void *data, plitem_t *messages, void *context)
{
	auto value = (uint32_t *) data;

	const char *str = PL_String (item);
	char *end = 0;
	*value = strtol (str, &end, 0);
	if (*end) {
		PL_Message (messages, item, "error parsing %s: %s", field->name, str);
	}
	return !*end;
}

static int
parse_uint32_t (const plfield_t *field, const plitem_t *item,
				void *data, plitem_t *messages, void *context)
{
	auto value = (uint32_t *) data;
	*value = PL_Number (item);
	return 1;
}

static int
parse_value (const plfield_t *field, const plitem_t *item,
			 void *data, plitem_t *messages, void *context)
{
	if (PL_Type (item) == QFString) {
		return parse_hex (field, item, data, messages, context);
	} else {
		return parse_uint32_t (field, item, data, messages, context);
	}
}

static parse_string_t parse_string_array = { 0 };

static int
parse_string (const plfield_t *field, const plitem_t *item,
			  void *data, plitem_t *messages, void *context)
{
	auto string = (parse_string_t *) field->data;
	auto value = (const char **) ((byte *)data + string->value_offset);

	const char *str = PL_String (item);
	*value = save_string (str);
	return 1;
}

static int
parse_array (const plfield_t *field, const plitem_t *item,
			 void *data, plitem_t *messages, void *context)
{
	auto array = (parse_array_t *) field->data;
	auto value = (void **) ((byte *)data + array->value_offset);
	auto size = (uint32_t *) ((byte *)data + array->size_offset);

	plelement_t element = {
		array->type,
		array->stride,
		spvg_alloc,
		array->parser,
		array->data,
	};
	plfield_t   f = { 0, 0, 0, 0, &element };

	typedef struct arr_s DARRAY_TYPE(byte) arr_t;
	arr_t      *arr;

	if (!PL_ParseArray (&f, item, &arr, messages, context)) {
		return 0;
	}
	*value = arr->a;
	if ((void *) size >= data) {
		*size = arr->size;
	}
	if (array->cmp) {
		heapsort (*value, *size, array->stride, array->cmp);
	}
	return 1;
}

static int
parse_struct (const plfield_t *field, const plitem_t *dict,
			  void *data, plitem_t *messages, void *context)
{
	return PL_ParseStruct (field->data, dict, data, messages, context);
}

static int
parse_ignore (const plfield_t *field, const plitem_t *item,
			  void *data, plitem_t *messages, void *context)
{
	return 1;
}

static int
spirv_enumerant_cmp (const void *_a, const void *_b)
{
	auto a = (const spirv_enumerant_t *) _a;
	auto b = (const spirv_enumerant_t *) _b;
	return strcmp (a->enumerant, b->enumerant);
}

static int
spirv_instruction_cmp (const void *_a, const void *_b)
{
	auto a = (const spirv_instruction_t *) _a;
	auto b = (const spirv_instruction_t *) _b;
	return strcmp (a->opname, b->opname);
}

static int
spirv_kind_cmp (const void *_a, const void *_b)
{
	auto a = (const spirv_kind_t *) _a;
	auto b = (const spirv_kind_t *) _b;
	return strcmp (a->kind, b->kind);
}

static plfield_t spirv_operand_fields[] = {
	{"kind", offsetof (spirv_operand_t, kind), QFString, parse_string, &parse_string_array},
	{"quantifier", offsetof (spirv_operand_t, quantifier), QFString, parse_char, nullptr},
	{"name", offsetof (spirv_operand_t, name), QFString, parse_string, &parse_string_array},
	{ }
};

static parse_array_t parse_ecapability_data = {
	.type = QFString,
	.stride = sizeof (const char *),
	.parser = parse_string,
	.data = &parse_string_array,
	.value_offset = offsetof (spirv_enumerant_t, capabilities),
	.size_offset = offsetof (spirv_enumerant_t, num_capabilities),
};

static parse_array_t parse_eextension_data = {
	.type = QFString,
	.stride = sizeof (const char *),
	.parser = parse_string,
	.data = &parse_string_array,
	.value_offset = offsetof (spirv_enumerant_t, extensions),
	.size_offset = offsetof (spirv_enumerant_t, num_extensions),
};

static parse_array_t parse_parameter_data = {
	.type = QFDictionary,
	.stride = sizeof (spirv_operand_t),
	.parser = parse_struct,
	.data = &spirv_operand_fields,
	.value_offset = offsetof (spirv_enumerant_t, parameters),
	.size_offset = offsetof (spirv_enumerant_t, num_parameters),
};

static plfield_t spirv_enumerant_fields[] = {
	{"enumerant", offsetof (spirv_enumerant_t, enumerant), QFString, parse_string, &parse_string_array},
	{"value", offsetof (spirv_enumerant_t, value), QFMultiType | (1 << QFString) | (1 << QFNumber), parse_value, nullptr},
	{"capabilities", 0, QFArray, parse_array, &parse_ecapability_data},
	{"extensions", 0, QFArray, parse_array, &parse_eextension_data},
	{"parameters", 0, QFArray, parse_array, &parse_parameter_data},
	{"version", offsetof (spirv_enumerant_t, version), QFString, parse_string, &parse_string_array},
	{"lastVersion", offsetof (spirv_enumerant_t, lastVersion), QFString, parse_string, &parse_string_array},
	{ }
};

static parse_array_t parse_enumerant_data = {
	.type = QFDictionary,
	.stride = sizeof (spirv_enumerant_t),
	.parser = parse_struct,
	.data = &spirv_enumerant_fields,
	.value_offset = offsetof (spirv_kind_t, enumerants),
	.size_offset = offsetof (spirv_kind_t, num),
	.cmp = spirv_enumerant_cmp,
};

static parse_array_t parse_base_data = {
	.type = QFString,
	.stride = sizeof (const char *),
	.parser = parse_string,
	.data = &parse_string_array,
	.value_offset = offsetof (spirv_kind_t, bases),
	.size_offset = offsetof (spirv_kind_t, num),
};

static plfield_t spirv_kind_fields[] = {
	{"category", offsetof (spirv_kind_t, category), QFString, parse_string, &parse_string_array},
	{"kind", offsetof (spirv_kind_t, kind), QFString, parse_string, &parse_string_array},
	{"doc", offsetof (spirv_kind_t, doc), QFString, parse_string, &parse_string_array},
	{"enumerants", 0, QFArray, parse_array, &parse_enumerant_data},
	{"bases", 0, QFArray, parse_array, &parse_base_data},
};

static parse_array_t parse_operand_data = {
	.type = QFDictionary,
	.stride = sizeof (spirv_operand_t),
	.parser = parse_struct,
	.data = &spirv_operand_fields,
	.value_offset = offsetof (spirv_instruction_t, operands),
	.size_offset = offsetof (spirv_instruction_t, num_operands),
};

static parse_array_t parse_capability_data = {
	.type = QFString,
	.stride = sizeof (const char *),
	.parser = parse_string,
	.data = &parse_string_array,
	.value_offset = offsetof (spirv_instruction_t, capabilities),
	.size_offset = offsetof (spirv_instruction_t, num_capabilities),
};

static parse_array_t parse_extension_data = {
	.type = QFString,
	.stride = sizeof (const char *),
	.parser = parse_string,
	.data = &parse_string_array,
	.value_offset = offsetof (spirv_instruction_t, extensions),
	.size_offset = offsetof (spirv_instruction_t, num_extensions),
};

static plfield_t spirv_instruction_fields[] = {
	{"opname", offsetof (spirv_instruction_t, opname), QFString, parse_string, &parse_string_array},
	{"opcode", offsetof (spirv_instruction_t, opcode), QFNumber, parse_uint32_t, nullptr},
	{"operands", 0, QFArray, parse_array, &parse_operand_data},
	{"capabilities", 0, QFArray, parse_array, &parse_capability_data},
	{"extensions", 0, QFArray, parse_array, &parse_extension_data},
	{"version", offsetof (spirv_instruction_t, version), QFString, parse_string, &parse_string_array},
	{"lastVersion", offsetof (spirv_instruction_t, lastVersion), QFString, parse_string, &parse_string_array},
	{"class", 0, QFString, parse_ignore, nullptr},
	{ }
};

static parse_array_t parse_copyright_data = {
	.type = QFString,
	.stride = sizeof (const char *),
	.parser = parse_string,
	.data = &parse_string_array,
	.value_offset = offsetof (spirv_grammar_t, copyright),
	.size_offset = offsetof (spirv_grammar_t, num_copyright),
};

static parse_array_t parse_instruction_data = {
	.type = QFDictionary,
	.stride = sizeof (spirv_instruction_t),
	.parser = parse_struct,
	.data = spirv_instruction_fields,
	.value_offset = offsetof (spirv_grammar_t, instructions),
	.size_offset = offsetof (spirv_grammar_t, num_instructions),
	.cmp = spirv_instruction_cmp,
};

static parse_array_t parse_operand_kind_data = {
	.type = QFDictionary,
	.stride = sizeof (spirv_kind_t),
	.parser = parse_struct,
	.data = spirv_kind_fields,
	.value_offset = offsetof (spirv_grammar_t, operand_kinds),
	.size_offset = offsetof (spirv_grammar_t, num_operand_kinds),
	.cmp = spirv_kind_cmp,
};

static bool built;
static plfield_t spirv_grammar_fields[] = {
	{"copyright", 0, QFArray, parse_array, &parse_copyright_data},
	{"magic_number", offsetof (spirv_grammar_t, magic_number), QFString, parse_hex, nullptr},
	{"major_version", offsetof (spirv_grammar_t, major_version), QFNumber, parse_uint32_t, nullptr},
	{"minor_version", offsetof (spirv_grammar_t, minor_version), QFNumber, parse_uint32_t, nullptr},
	{"version", offsetof (spirv_grammar_t, version), QFNumber, parse_uint32_t, nullptr},
	{"revision", offsetof (spirv_grammar_t, revision), QFNumber, parse_uint32_t, nullptr},
	{"instructions", 0, QFArray, parse_array, &parse_instruction_data},
	{"operand_kinds", 0, QFArray, parse_array, &parse_operand_kind_data},
	{"instruction_printing_class", 0, QFArray, parse_ignore, nullptr},
	{ }
};

static bool
parse_grammar (plitem_t *plitem, spirv_grammar_t **grammar)
{
	spirv_grammar_t g = {};
	auto messages = PL_NewArray ();
	bool ret = PL_ParseStruct (spirv_grammar_fields, plitem, &g, messages,
							   nullptr);
	if (!ret) {
		for (int i = 0; i < PL_A_NumObjects (messages); i++) {
			fprintf (stderr, "%s\n",
					 PL_String (PL_ObjectAtIndex (messages, i)));
		}
	} else {
		*grammar = malloc (sizeof (**grammar));
		**grammar = g;
	}
	PL_Release (messages);
	return ret;
}

static void
build_grammars (void)
{
	for (int i = 0; builtin_json[i].name; i++) {
		auto plitem = PL_ParseJSON (builtin_json[i].json, nullptr);
		if (!plitem) {
			internal_error (0, "could not parse JSON for %s",
							builtin_json[i].name);
		}
		if (!parse_grammar (plitem, &builtin_json[i].grammar)) {
			internal_error (0, "could not parse grammar spec for %s",
							builtin_json[i].name);
		}
	}
	for (int i = 0; builtin_json[i].name; i++) {
		auto parent = builtin_json[i].parent;
		if (parent) {
			builtin_json[i].grammar->parent = parent->grammar;
		}
	}
}

const plitem_t *
spirv_operand_kind (const char *set, const char *kind)
{
	if (!built) {
		build_grammars ();
		built = true;
	}
	return nullptr;
}

const uint32_t
spirv_instruction_opcode (const char *set, const expr_t *opcode)
{
	if (!built) {
		build_grammars ();
		built = true;
	}

	if (is_integral_val (opcode)) {
		return expr_integral (opcode);
	}
	if (opcode->type != ex_symbol) {
		error (opcode, "not a an integer constant or symbol");
		return 0;
	}
	spirv_grammar_t *grammar = nullptr;
	for (int i = 0; builtin_json[i].name; i++) {
		if (strcmp (builtin_json[i].name, set) == 0) {
			grammar = builtin_json[i].grammar;
			break;
		}
	}
	if (!grammar) {
		error (opcode, "unrecognized grammar set %s", set);
		return 0;
	}

	const char *opname = opcode->symbol->name;
	const spirv_instruction_t *instruction = nullptr;
	instruction = bsearch (&(spirv_instruction_t) { .opname = opname },
						   grammar->instructions, grammar->num_instructions,
						   sizeof (spirv_instruction_t), spirv_instruction_cmp);
	if (!instruction) {
		error (opcode, "unknown instruction opcode %s", opname);
		return 0;
	}
	return instruction->opcode;
}

static symbol_t *
spirv_kind_symbol (const char *name, symtab_t *symtab)
{
	symbol_t   *sym = nullptr;
	spirv_kind_t *kind = symtab->procsymbol_data;

	spirv_enumerant_t *enumerant = nullptr;
	enumerant = bsearch (&(spirv_enumerant_t) { .enumerant = name },
						 kind->enumerants, kind->num,
						 sizeof (spirv_enumerant_t), spirv_enumerant_cmp);
	if (enumerant) {
		sym = new_symbol_type (name, &type_uint);
		sym->sy_type = sy_const;
		sym->value = new_uint_val (enumerant->value);
	}
	return sym;
}

static symbol_t *
spirv_intrinsic_symbol (const char *name, symtab_t *symtab)
{
	symbol_t   *sym = nullptr;
	spirv_grammar_t *grammar = symtab->procsymbol_data;

	spirv_kind_t *kind = nullptr;
	kind = bsearch (&(spirv_kind_t) { .kind = name },
					grammar->operand_kinds, grammar->num_operand_kinds,
					sizeof (spirv_kind_t), spirv_kind_cmp);
	if (kind && strcmp (kind->category, "Composite") != 0
		&& strcmp (kind->category, "Literal") != 1) {
		if (!kind->symtab) {
			kind->symtab = new_symtab (nullptr, stab_enum);
			kind->symtab->procsymbol = spirv_kind_symbol;
			kind->symtab->procsymbol_data = kind;
		}
		sym = new_symbol (name);
		sym->sy_type = sy_namespace;
		sym->namespace = kind->symtab;
	}
	return sym;
}

bool
spirv_setup_intrinsic_symtab (symtab_t *symtab)
{
	if (!built) {
		build_grammars ();
		built = true;
	}
	const char *set = "core";
	spirv_grammar_t *grammar = nullptr;
	for (int i = 0; builtin_json[i].name; i++) {
		if (strcmp (builtin_json[i].name, set) == 0) {
			grammar = builtin_json[i].grammar;
			break;
		}
	}
	if (!grammar) {
		error (0, "unrecognized grammar set %s", set);
		return false;
	}
	symtab->procsymbol = spirv_intrinsic_symbol;
	symtab->procsymbol_data = grammar;
	return true;
}
