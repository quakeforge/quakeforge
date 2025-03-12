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

#include "compat.h"

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
static bool built;
static strpool_t *extensions;
static uint32_t extension_id;

static void *
spvg_alloc (void *data, size_t size)
{
	return malloc (size);
}

typedef struct parse_sub_array_s {
	size_t      offset;
	void       *data;
	spirv_grammar_t *grammar;
} parse_sub_array_t;

typedef struct parsectx_s {
	spirv_grammar_t *grammar;
	parse_sub_array_t *sub_data;
	void       *array;
} parsectx_t;

static void *
spvg_strid_alloc (void *data, size_t size)
{
	auto parsectx = (parsectx_t *) data;
	auto g = parsectx->grammar;
	auto sub_data = parsectx->sub_data;
	auto arr = (strid_set_t *) ((byte *) g + sub_data->offset);
	unsigned count = size / sizeof (arr->a[0]);
	uint32_t index = arr->size;
	DARRAY_RESIZE (arr, index + count);
	return arr->a + index;
}

static void *
spvg_operand_alloc (void *data, size_t size)
{
	auto parsectx = (parsectx_t *) data;
	auto g = parsectx->grammar;
	auto sub_data = parsectx->sub_data;
	auto arr = (spirv_operand_set_t *) ((byte *) g + sub_data->offset);
	unsigned count = size / sizeof (arr->a[0]);
	uint32_t index = arr->size;
	DARRAY_RESIZE (arr, index + count);
	return arr->a + index;
}

static void *
spvg_enumerant_alloc (void *data, size_t size)
{
	auto parsectx = (parsectx_t *) data;
	auto g = parsectx->grammar;
	auto sub_data = parsectx->sub_data;
	auto arr = (spirv_enumerant_set_t *) ((byte *) g + sub_data->offset);
	unsigned count = size / sizeof (arr->a[0]);
	uint32_t index = arr->size;
	DARRAY_RESIZE (arr, index + count);
	return arr->a + index;
}

static void *
spvg_val_alloc (void *data, size_t size)
{
	auto parsectx = (parsectx_t *) data;
	auto g = parsectx->grammar;
	auto sub_data = parsectx->sub_data;
	auto arr = (spirv_val_set_t *) ((byte *) g + sub_data->offset);
	unsigned count = size / sizeof (arr->a[0]);
	uint32_t index = arr->size;
	DARRAY_RESIZE (arr, index + count);
	return arr->a + index;
}

typedef struct parse_array_s {
	pltype_t    type;
	size_t      stride;
	plparser_t  parser;
	void       *data;
	size_t      value_offset;
	size_t      size_offset;
	void     *(*alloc) (void *ctx, size_t size);
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

static int
parse_strid (const plfield_t *field, const plitem_t *item,
			 void *data, plitem_t *messages, void *context)
{
	auto parsectx = (parsectx_t *) context;
	auto g = parsectx->grammar;
	auto value = (strid_t *) data;

	auto str = PL_String (item);
	*value = *strpool_addstrid (g->strings, str);
	return 1;
}

static int
parse_string (const plfield_t *field, const plitem_t *item,
			  void *data, plitem_t *messages, void *context)
{
	auto parsectx = (parsectx_t *) context;
	auto g = parsectx->grammar;
	auto value = (pr_string_t *) data;

	const char *str = PL_String (item);
	*value = strpool_addstr (g->strings, str);
	return 1;
}

static int
parse_c_string (const plfield_t *field, const plitem_t *item,
			    void *data, plitem_t *messages, void *context)
{
	auto value = (const char **) data;

	const char *str = PL_String (item);
	*value = save_string (str);
	return 1;
}

static int
parse_version (const plfield_t *field, const plitem_t *item,
			   void *data, plitem_t *messages, void *context)
{
	auto value = (uint16_t *) data;

	const char *str = PL_String (item);
	unsigned    maj = 0;
	unsigned    min = 0;
	if (strcmp (str, "None") != 0) {
		if (sscanf (str, "%u.%u", &maj, &min) != 2) {
			PL_Message (messages, item, "error parsing %s: %s",
						field->name, str);
			return 0;
		}
	}

	*value = (maj << 8) | min;
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
		.type   = array->type,
		.stride = array->stride,
		.alloc  = array->alloc,
		.parser = array->parser,
		.data   = array->data,
	};
	plfield_t   f = { 0, 0, 0, 0, &element };

	typedef struct arr_s DARRAY_TYPE(byte) arr_t;
	arr_t       arr;

	if (!PL_ParseArray (&f, item, &arr, messages, context)) {
		return 0;
	}
	*value = arr.a;
	if ((void *) size >= data) {
		*size = arr.size;
	}
	return 1;
}

static int
parse_sub_array (const plfield_t *field, const plitem_t *item,
				 void *data, plitem_t *messages, void *context)
{
	auto array = (parse_array_t *) field->data;
	auto sub_data = (parse_sub_array_t *) array->data;
	auto value = (uint32_t *) ((byte *)data + array->value_offset);
	auto size = (uint32_t *) ((byte *)data + array->size_offset);

	plelement_t element = {
		.type   = array->type,
		.stride = array->stride,
		.alloc  = array->alloc,
		.parser = array->parser,
		.data   = sub_data->data,
	};
	plfield_t   f = { 0, 0, 0, 0, &element };

	typedef struct arr_s DARRAY_TYPE(byte) arr_t;
	arr_t       arr;

	auto parsectx = *(parsectx_t *) context;
	parsectx.sub_data = sub_data;
	parsectx.array = &arr;

	if (!PL_ParseArray (&f, item, &arr, messages, &parsectx)) {
		return 0;
	}

	auto g = parsectx.grammar;
	auto target_arr = (arr_t *) ((byte *) g + sub_data->offset);
	*value = (arr.a - target_arr->a) / array->stride;
	if ((void *) size >= data) {
		*size = arr.size;
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

static bool
is_enumerant (const spirv_kind_t *kind, const spirv_grammar_t *g)
{
	auto category = g->strings->strings + kind->category;
	if (strcmp (category, "BitEnum") == 0
		|| strcmp (category, "ValueEnum") == 0) {
		return true;
	}
	return false;
}

static plfield_t spirv_operand_fields[] = {
	{"kind", offsetof (spirv_operand_t, kind), QFString, parse_string, nullptr},
	{"quantifier", offsetof (spirv_operand_t, quantifier), QFString, parse_char, nullptr},
	{"name", offsetof (spirv_operand_t, name), QFString, parse_string, nullptr},
	{ }
};

static parse_sub_array_t parse_ecapability_sub_data = {
	.offset = offsetof (spirv_grammar_t, capabilities),
};
static parse_array_t parse_ecapability_data = {
	.type = QFString,
	.stride = sizeof (strid_t),
	.parser = parse_strid,
	.data = &parse_ecapability_sub_data,
	.value_offset = offsetof (spirv_enumerant_t, capabilities),
	.size_offset = offsetof (spirv_enumerant_t, num_capabilities),
	.alloc = spvg_strid_alloc,
};

static parse_sub_array_t parse_eextension_sub_data = {
	.offset = offsetof (spirv_grammar_t, extensions),
};
static parse_array_t parse_eextension_data = {
	.type = QFString,
	.stride = sizeof (const char *),
	.parser = parse_strid,
	.data = &parse_eextension_sub_data,
	.value_offset = offsetof (spirv_enumerant_t, extensions),
	.size_offset = offsetof (spirv_enumerant_t, num_extensions),
	.alloc = spvg_strid_alloc,
};

static parse_sub_array_t parse_ealias_sub_data = {
	.offset = offsetof (spirv_grammar_t, aliases),
};
static parse_array_t parse_ealias_data = {
	.type = QFString,
	.stride = sizeof (pr_string_t),
	.parser = parse_string,
	.data = &parse_ealias_sub_data,
	.value_offset = offsetof (spirv_enumerant_t, aliases),
	.size_offset = offsetof (spirv_enumerant_t, num_aliases),
	.alloc = spvg_val_alloc,
};

static parse_sub_array_t parse_parameter_sub_data = {
	.offset = offsetof (spirv_grammar_t, operands),
	.data = &spirv_operand_fields,
};
static parse_array_t parse_parameter_data = {
	.type = QFDictionary,
	.stride = sizeof (spirv_operand_t),
	.parser = parse_struct,
	.data = &parse_parameter_sub_data,
	.value_offset = offsetof (spirv_enumerant_t, parameters),
	.size_offset = offsetof (spirv_enumerant_t, num_parameters),
	.alloc = spvg_operand_alloc,
};

static plfield_t spirv_enumerant_fields[] = {
	{"enumerant", offsetof (spirv_enumerant_t, enumerant), QFString, parse_string, nullptr},
	{"value", offsetof (spirv_enumerant_t, value), QFMultiType | (1 << QFString) | (1 << QFNumber), parse_value, nullptr},
	{"capabilities", 0, QFArray, parse_sub_array, &parse_ecapability_data},
	{"extensions", 0, QFArray, parse_sub_array, &parse_eextension_data},
	{"parameters", 0, QFArray, parse_sub_array, &parse_parameter_data},
	{"version", offsetof (spirv_enumerant_t, version), QFString, parse_version, nullptr},
	{"lastVersion", offsetof (spirv_enumerant_t, lastVersion), QFString, parse_version, nullptr},
	{"aliases", 0, QFArray, parse_sub_array, &parse_ealias_data},
	{"provisional", 0, QFBool, parse_ignore, nullptr},
	{ }
};

static parse_sub_array_t parse_enumerant_sub_data = {
	.offset = offsetof (spirv_grammar_t, enumerants),
	.data = &spirv_enumerant_fields,
};
static parse_array_t parse_enumerant_data = {
	.type = QFDictionary,
	.stride = sizeof (spirv_enumerant_t),
	.parser = parse_struct,
	.data = &parse_enumerant_sub_data,
	.value_offset = offsetof (spirv_kind_t, enumerants),
	.size_offset = offsetof (spirv_kind_t, num),
	.alloc = spvg_enumerant_alloc,
};

static parse_sub_array_t parse_base_sub_data = {
	.offset = offsetof (spirv_grammar_t, bases),
};
static parse_array_t parse_base_data = {
	.type = QFString,
	.stride = sizeof (pr_string_t),
	.parser = parse_string,
	.data = &parse_base_sub_data,
	.value_offset = offsetof (spirv_kind_t, bases),
	.size_offset = offsetof (spirv_kind_t, num),
	.alloc = spvg_val_alloc,
};

static plfield_t spirv_kind_fields[] = {
	{"category", offsetof (spirv_kind_t, category), QFString, parse_string, nullptr},
	{"kind", offsetof (spirv_kind_t, kind), QFString, parse_string, nullptr},
	{"doc", offsetof (spirv_kind_t, doc), QFString, parse_string, nullptr},
	{"enumerants", 0, QFArray, parse_sub_array, &parse_enumerant_data},
	{"bases", 0, QFArray, parse_sub_array, &parse_base_data},
};

static parse_sub_array_t parse_operand_sub_data = {
	.offset = offsetof (spirv_grammar_t, operands),
	.data = &spirv_operand_fields,
};
static parse_array_t parse_operand_data = {
	.type = QFDictionary,
	.stride = sizeof (spirv_operand_t),
	.parser = parse_struct,
	.data = &parse_operand_sub_data,
	.value_offset = offsetof (spirv_instruction_t, operands),
	.size_offset = offsetof (spirv_instruction_t, num_operands),
	.alloc = spvg_operand_alloc,
};

static parse_sub_array_t parse_capability_sub_data = {
	.offset = offsetof (spirv_grammar_t, capabilities),
};
static parse_array_t parse_capability_data = {
	.type = QFString,
	.stride = sizeof (const char *),
	.parser = parse_strid,
	.data = &parse_capability_sub_data,
	.value_offset = offsetof (spirv_instruction_t, capabilities),
	.size_offset = offsetof (spirv_instruction_t, num_capabilities),
	.alloc = spvg_strid_alloc,
};

static parse_sub_array_t parse_extension_sub_data = {
	.offset = offsetof (spirv_grammar_t, extensions),
};
static parse_array_t parse_extension_data = {
	.type = QFString,
	.stride = sizeof (const char *),
	.parser = parse_strid,
	.data = &parse_extension_sub_data,
	.value_offset = offsetof (spirv_instruction_t, extensions),
	.size_offset = offsetof (spirv_instruction_t, num_extensions),
	.alloc = spvg_strid_alloc,
};

static parse_sub_array_t parse_alias_sub_data = {
	.offset = offsetof (spirv_grammar_t, aliases),
};
static parse_array_t parse_alias_data = {
	.type = QFString,
	.stride = sizeof (pr_string_t),
	.parser = parse_string,
	.data = &parse_alias_sub_data,
	.value_offset = offsetof (spirv_instruction_t, aliases),
	.size_offset = offsetof (spirv_instruction_t, num_aliases),
	.alloc = spvg_val_alloc,
};

static plfield_t spirv_instruction_fields[] = {
	{"opname", offsetof (spirv_instruction_t, opname), QFString, parse_string, nullptr},
	{"opcode", offsetof (spirv_instruction_t, opcode), QFNumber, parse_uint32_t, nullptr},
	{"operands", 0, QFArray, parse_sub_array, &parse_operand_data},
	{"capabilities", 0, QFArray, parse_sub_array, &parse_capability_data},
	{"extensions", 0, QFArray, parse_sub_array, &parse_extension_data},
	{"version", offsetof (spirv_instruction_t, version), QFString, parse_version, nullptr},
	{"lastVersion", offsetof (spirv_instruction_t, lastVersion), QFString, parse_version, nullptr},
	{"class", 0, QFString, parse_ignore, nullptr},
	{"aliases", 0, QFArray, parse_sub_array, &parse_alias_data},
	{"provisional", 0, QFBool, parse_ignore, nullptr},
	{ }
};

static parse_array_t parse_copyright_data = {
	.type = QFString,
	.stride = sizeof (const char *),
	.parser = parse_c_string,
	.data = nullptr,
	.value_offset = offsetof (spirv_grammar_t, copyright),
	.size_offset = offsetof (spirv_grammar_t, num_copyright),
	.alloc = spvg_alloc,
};

static parse_array_t parse_instruction_data = {
	.type = QFDictionary,
	.stride = sizeof (spirv_instruction_t),
	.parser = parse_struct,
	.data = spirv_instruction_fields,
	.value_offset = offsetof (spirv_grammar_t, instructions),
	.size_offset = offsetof (spirv_grammar_t, num_instructions),
	.alloc = spvg_alloc,
};

static parse_array_t parse_operand_kind_data = {
	.type = QFDictionary,
	.stride = sizeof (spirv_kind_t),
	.parser = parse_struct,
	.data = spirv_kind_fields,
	.value_offset = offsetof (spirv_grammar_t, operand_kinds),
	.size_offset = offsetof (spirv_grammar_t, num_operand_kinds),
	.alloc = spvg_alloc,
};

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
	spirv_grammar_t g = {
		.opname_index = DARRAY_STATIC_INIT (256),
		.opcode_index = DARRAY_STATIC_INIT (256),

		.kind_index = DARRAY_STATIC_INIT (64),

		.enumerants = DARRAY_STATIC_INIT (64),
		.enumerant_index = DARRAY_STATIC_INIT (256),
		.value_index = DARRAY_STATIC_INIT (256),

		.operands = DARRAY_STATIC_INIT (256),

		.capabilities = DARRAY_STATIC_INIT (64),
		.extensions = DARRAY_STATIC_INIT (64),

		.aliases = DARRAY_STATIC_INIT (64),
		.bases = DARRAY_STATIC_INIT (64),

		.strings = strpool_new (),
	};
	auto messages = PL_NewArray ();
	parsectx_t parsectx = {
		.grammar = &g,
	};
	bool ret = PL_ParseStruct (spirv_grammar_fields, plitem, &g, messages,
							   &parsectx);
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

static int
spirv_strid_sort_cmp (const void *_a, const void *_b, void *obj)
{
	auto g = (spirv_grammar_t *) obj;
	auto strid_a = (const strid_t *) _a;
	auto strid_b = (const strid_t *) _b;
	auto a = g->strings->strings + strid_a->offset;
	auto b = g->strings->strings + strid_b->offset;
	return strcmp (a, b);
}

static int
spirv_strid_key_cmp (const void *_a, const void *_b, void *obj)
{
	auto g = (spirv_grammar_t *) obj;
	auto a = (const char *) _a;
	auto strid = (const strid_t *) _b;
	auto b = g->strings->strings + strid->offset;
	return strcmp (a, b);
}

static int
spirv_validx_sort_cmp (const void *_a, const void *_b)
{
	auto a = (const validx_t *) _a;
	auto b = (const validx_t *) _b;
	return (int) a->value - (int) b->value;
}

static int
spirv_validx_key_cmp (const void *_a, const void *_b)
{
	auto a = (const uint32_t *) _a;
	auto b = (const validx_t *) _b;
	return (int) *a - (int) b->value;
}

static void
build_grammars (void)
{
	spirv_grammar_t *core;

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
		if (strcmp (builtin_json[i].name, "core") == 0) {
			core = builtin_json[i].grammar;
		}
	}
	if (!core) {
		internal_error (0, "core grammar not found");
	}
	for (int i = 0; builtin_json[i].name; i++) {
		auto parent = builtin_json[i].parent;
		auto grammar = builtin_json[i].grammar;
		if (parent) {
			grammar->parent = parent->grammar;
		}

		for (uint32_t j = 0; j < grammar->num_instructions; j++) {
			auto insn = &grammar->instructions[j];
			DARRAY_APPEND (&grammar->opname_index,
						   ((strid_t) { .offset = insn->opname, .id = j }));
			for (uint32_t k = 0; k < insn->num_aliases; k++) {
				uint32_t alias = grammar->aliases.a[k + insn->aliases];
				DARRAY_APPEND (&grammar->opname_index,
							   ((strid_t) { .offset = alias, .id = j }));
			}
			DARRAY_APPEND (&grammar->opcode_index,
						   ((validx_t) { .value = insn->opcode, .index = j }));
		}
		heapsort_r (grammar->opname_index.a,
					grammar->opname_index.size, sizeof (strid_t),
					spirv_strid_sort_cmp, grammar);
		heapsort (grammar->opcode_index.a,
				  grammar->opcode_index.size, sizeof (validx_t),
				  spirv_validx_sort_cmp);

		for (uint32_t j = 0; j < grammar->num_operand_kinds; j++) {
			auto kind = &grammar->operand_kinds[j];
			kind->grammar = grammar;
			DARRAY_APPEND (&grammar->kind_index,
						   ((strid_t) { .offset = kind->kind, .id = j }));
			if (is_enumerant (kind, grammar)) {
				kind->num_names = kind->num;
				kind->names = grammar->enumerant_index.size;
				for (uint32_t k = 0; k < kind->num; k++) {
					uint32_t index = kind->enumerants + k;
					auto enm = &grammar->enumerants.a[index];
					enm->index = k;
					DARRAY_APPEND (&grammar->enumerant_index,
								   ((strid_t) {
										.offset = enm->enumerant,
										.id = index,
									}));
					DARRAY_APPEND (&grammar->value_index,
								   ((validx_t) {
										.value = enm->value,
										.index = index,
									}));
					kind->num_names += enm->num_aliases;
					for (uint32_t l = 0; l < enm->num_aliases; l++) {
						uint32_t alias = grammar->aliases.a[l + enm->aliases];
						DARRAY_APPEND (&grammar->enumerant_index,
									   ((strid_t) {
											.offset = alias,
											.id = index,
										}));
					}
				}
				heapsort_r (grammar->enumerant_index.a + kind->names,
							kind->num_names, sizeof (strid_t),
							spirv_strid_sort_cmp, grammar);
				heapsort (grammar->value_index.a + kind->enumerants,
						  kind->num, sizeof (validx_t),
						  spirv_validx_sort_cmp);
			}
		}
		heapsort_r (grammar->kind_index.a,
					grammar->kind_index.size, sizeof (strid_t),
					spirv_strid_sort_cmp, grammar);
	}
	built = true;
	extensions = strpool_new ();

	for (int i = 0; builtin_json[i].name; i++) {
		auto grammar = builtin_json[i].grammar;
		for (size_t j = 0; j < grammar->capabilities.size; j++) {
			auto cap = &grammar->capabilities.a[j];
			auto cap_name = grammar->strings->strings + cap->offset;
			auto enm = spirv_enumerant (core, "Capability", cap_name);
			if (enm->version) {
				cap->id = enm->index + 1;
			}
		}
		for (size_t j = 0; j < grammar->extensions.size; j++) {
			auto ext = &grammar->extensions.a[j];
			auto ext_name = grammar->strings->strings + ext->offset;
			auto regext = strpool_addstrid (extensions, ext_name);
			if (!regext->id) {
				regext->id = ++extension_id;
			}
		}
	}
}

const spirv_grammar_t *
spirv_grammar (const char *set)
{
	if (!built) {
		build_grammars ();
	}

	spirv_grammar_t *grammar = nullptr;
	for (int i = 0; builtin_json[i].name; i++) {
		if (strcmp (builtin_json[i].name, set) == 0) {
			grammar = builtin_json[i].grammar;
			break;
		}
	}
	return grammar;
}

const spirv_instruction_t *
spirv_instruction_op (const spirv_grammar_t *grammar, uint32_t op)
{
	validx_t *validx = bsearch (&op, grammar->opcode_index.a,
								grammar->opcode_index.size, sizeof (validx_t),
								spirv_validx_key_cmp);
	if (!validx) {
		return nullptr;
	}
	return &grammar->instructions[validx->index];
}

const spirv_instruction_t *
spirv_instruction (const spirv_grammar_t *grammar, const expr_t *opcode)
{
	if (is_integral_val (opcode)) {
		uint32_t op = expr_integral (opcode);
		auto instruction = spirv_instruction_op (grammar, op);
		if (!instruction) {
			error (opcode, "invalid opcode %d", op);
		}
		return instruction;
	}
	if (opcode->type != ex_symbol) {
		error (opcode, "not a an integer constant or symbol");
		return nullptr;
	}

	const char *opname = opcode->symbol->name;
	strid_t *strid = bsearch_r (opname, grammar->opname_index.a,
							    grammar->opname_index.size, sizeof (strid_t),
							    spirv_strid_key_cmp, (void *) grammar);
	if (!strid) {
		error (opcode, "unknown instruction opcode %s", opname);
		return nullptr;
	}
	return grammar->instructions + strid->id;
}

static strid_t *
spirv_enumerant_strid (spirv_kind_t *kind, const char *name)
{
	auto grammar = kind->grammar;
	return bsearch_r (name, grammar->enumerant_index.a + kind->names,
					  kind->num_names, sizeof (strid_t),
					  spirv_strid_key_cmp, (void *) grammar);
}

static validx_t * __attribute__((pure))
spirv_enumerant_validx (spirv_kind_t *kind, uint32_t val)
{
	auto grammar = kind->grammar;
	return bsearch (&val, grammar->value_index.a + kind->enumerants,
					kind->num, sizeof (strid_t),
					spirv_validx_key_cmp);
}

static symbol_t *
spirv_enumerant_symbol (const char *name, symtab_t *symtab)
{
	symbol_t   *sym = nullptr;
	spirv_kind_t *kind = symtab->procsymbol_data;
	auto grammar = kind->grammar;

	spirv_enumerant_t *enumerant = nullptr;
	auto strid = spirv_enumerant_strid (kind, name);
	if (strid) {
		enumerant = grammar->enumerants.a + strid->id;
	}
	if (enumerant) {
		if (!enumerant->sym) {
			sym = new_symbol_type (name, &type_uint);
			sym->sy_type = sy_const;
			sym->value = new_uint_val (enumerant->value);
			enumerant->sym = sym;
		}
		sym = enumerant->sym;
	}
	return sym;
}

static strid_t *
spirv_kind_strid (const spirv_grammar_t *grammar, const char *name)
{
	return bsearch_r (name, grammar->kind_index.a,
					  grammar->kind_index.size, sizeof (strid_t),
					  spirv_strid_key_cmp, (void *) grammar);
}

static symbol_t *
spirv_kind_symbol (const char *name, symtab_t *symtab)
{
	symbol_t   *sym = nullptr;
	spirv_grammar_t *grammar = symtab->procsymbol_data;

	spirv_kind_t *kind = nullptr;
	auto strid = spirv_kind_strid (symtab->procsymbol_data, name);
	if (strid) {
		kind = grammar->operand_kinds + strid->id;
	}
	if (kind && is_enumerant (kind, grammar)) {
		if (!kind->sym) {
			sym = new_symbol (name);
			sym->sy_type = sy_namespace;
			sym->namespace = new_symtab (nullptr, stab_enum);
			sym->namespace->procsymbol = spirv_enumerant_symbol;
			sym->namespace->procsymbol_data = kind;
			kind->sym = sym;
		}
		sym = kind->sym;
	}
	return sym;
}

bool
spirv_setup_intrinsic_symtab (symtab_t *symtab)
{
	const char *set = "core";
	auto grammar = spirv_grammar (set);
	if (!grammar) {
		error (0, "unrecognized grammar set %s", set);
		return false;
	}
	symtab->procsymbol = spirv_kind_symbol;
	symtab->procsymbol_data = (void *) grammar;
	return true;
}


bool
spirv_enum_val_silent (const char *enum_name, const char *enumerant,
					   uint32_t *val)
{
	const char *set = "core";
	auto grammar = spirv_grammar (set);
	if (!grammar) {
		error (0, "unrecognized grammar set %s", set);
		return false;
	}

	auto strid = spirv_kind_strid (grammar, enum_name);
	if (!strid) {
		error (0, "%s not found", enum_name);
		return false;
	}
	auto kind = grammar->operand_kinds + strid->id;
	if (!is_enumerant (kind, grammar)) {
		error (0, "%s not an enumerant", enum_name);
		return false;
	}
	strid = spirv_enumerant_strid (kind, enumerant);
	if (!strid) {
		return false;
	}
	*val = grammar->enumerants.a[strid->id].value;
	return true;
}

uint32_t
spirv_enum_val (const char *enum_name, const char *enumerant)
{
	uint32_t val = 0;
	if (!spirv_enum_val_silent (enum_name, enumerant, &val)) {
		error (0, "%s not found", enumerant);
		return 0;
	}
	return val;
}

const spirv_enumerant_t *
spirv_enumerant (const spirv_grammar_t *grammar, const char *enum_name,
				 const char *enumerant)
{
	auto strid = spirv_kind_strid (grammar, enum_name);
	if (!strid) {
		error (0, "%s not found", enum_name);
		return nullptr;
	}
	auto kind = grammar->operand_kinds + strid->id;
	strid = spirv_enumerant_strid (kind, enumerant);
	if (!strid) {
		return nullptr;
	}
	return &grammar->enumerants.a[strid->id];
}

const spirv_enumerant_t *
spirv_enumerant_idx (const spirv_grammar_t *grammar, const char *enum_name,
					 uint32_t index)
{
	auto strid = spirv_kind_strid (grammar, enum_name);
	if (!strid) {
		error (0, "%s not found", enum_name);
		return nullptr;
	}
	auto kind = grammar->operand_kinds + strid->id;
	if (!is_enumerant (kind, grammar) || index >= kind->num) {
		return nullptr;
	}
	return &grammar->enumerants.a[kind->enumerants + index];
}

const spirv_enumerant_t *
spirv_enumerant_val (const spirv_grammar_t *grammar, const char *enum_name,
					 uint32_t val)
{
	auto strid = spirv_kind_strid (grammar, enum_name);
	if (!strid) {
		error (0, "%s not found", enum_name);
		return nullptr;
	}
	auto kind = grammar->operand_kinds + strid->id;
	auto validx = spirv_enumerant_validx (kind, val);
	if (!validx) {
		return nullptr;
	}
	return &grammar->enumerants.a[validx->index];
}
