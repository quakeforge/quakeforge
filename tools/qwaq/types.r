#include "runtime.h"	//FIXME for PR_FindGlobal
typedef enum {
	ev_void,
	ev_string,
	ev_float,
	ev_vector,
	ev_entity,
	ev_field,
	ev_func,
	ev_pointer,			// end of v6 types
	ev_quat,
	ev_integer,
	ev_uinteger,
	ev_short,			// value is embedded in the opcode
	ev_double,

	ev_invalid,			// invalid type. used for instruction checking
	ev_type_count		// not a type, gives number of types
} etype_t;

typedef enum {
    ty_none,				///< func/field/pointer or not used
	ty_struct,
	ty_union,
	ty_enum,
	ty_array,
	ty_class,
} ty_meta_e;


typedef struct qfot_fldptr_s {
	etype_t     type;
	struct qfot_type_s *aux_type;
} qfot_fldptr_t;

typedef struct qfot_func_s {
	etype_t     type;
	struct qfot_type_s *return_type;
	int         num_params;
	struct qfot_type_s *param_types[1];
} qfot_func_t;

typedef struct qfot_var_s {
	struct qfot_type_s *type;
	string      name;
	int         offset;				// value for enum, 0 for union
} qfot_var_t;

typedef struct qfot_struct_s {
	string      tag;
	int         num_fields;
	qfot_var_t  fields[1];
} qfot_struct_t;

typedef struct qfot_array_s {
	struct qfot_type_s *type;
	int         base;
	int         size;
} qfot_array_t;

typedef struct qfot_type_s {
	ty_meta_e   meta;
	int         size;
	string      encoding;
	union {
		etype_t     type;
		qfot_fldptr_t fldptr;
		qfot_func_t func;
		qfot_struct_t strct;
		qfot_array_t array;
		string      class;
	}           t;
} qfot_type_t;

typedef struct qfot_type_encodings_s {
	qfot_type_t *types;
	int         size;
} qfot_type_encodings_t;

typedef struct xdef_s {
	qfot_type_t *type;
	void        *offset;
} xdef_t;

typedef struct pr_xdefs_s {
	xdef_t     *xdefs;
	int         num_xdefs;
} pr_xdefs_t;

qfot_type_t *
next_type (qfot_type_t *type)
{
	int         size = type.size;
	if (!size)
		size = 4;
	return (qfot_type_t *) ((int *) type + size);
}

string ty_meta_name[6] = {
	"basic",
	"struct",
	"union",
	"enum",
	"array",
	"class",
};

string pr_type_name[ev_type_count] = {
	"void",
	"string",
	"float",
	"vector",
	"entity",
	"field",
	"function",
	"pointer",
	"quaternion",
	"integer",
	"uinteger",
	"short",
	"double"
	"invalid",
};

void
test_types (void)
{
	qfot_type_encodings_t *encodings;
	qfot_type_t *type;
	int         i;

	encodings = PR_FindGlobal (".type_encodings");
	if (!encodings) {
		printf ("Can't find encodings\n");
		return;
	}
	for (type = encodings.types;
		 ((int *)type - (int *) encodings.types) < encodings.size;
		type = next_type (type)) {
		printf ("%p %-6s %-20s", type, ty_meta_name[type.meta],
				type.encoding);
		if (!type.size) {
			printf ("\n");
			continue;
		}
		switch (type.meta) {
			case ty_none:
				printf (" %-10s", pr_type_name[type.t.type]);
				if (type.t.type == ev_func) {
					int         count = type.t.func.num_params;
					printf ("%p %d", type.t.func.return_type, count);
					if (count < 0)
						count = ~count;
					for (i = 0; i < count; i++)
						printf (" %p", type.t.func.param_types[i]);
				} else if (type.t.type == ev_pointer) {
					printf (" *%p", type.t.fldptr.aux_type);
				} else if (type.t.type == ev_field) {
					printf (" .%p", type.t.fldptr.aux_type);
				} else {
					printf (" %p", type.t.fldptr.aux_type);
				}
				printf ("\n");
				break;
			case ty_struct:
			case ty_union:
			case ty_enum:
				printf (" %s\n", type.t.strct.tag);
				for (i = 0; i < type.t.strct.num_fields; i++) {
					printf ("        %p %4x %s\n",
							type.t.strct.fields[i].type,
							type.t.strct.fields[i].offset,
							type.t.strct.fields[i].name);
				}
				break;
			case ty_array:
				printf (" %p %d %d\n", type.t.array.type,
						type.t.array.base, type.t.array.size);
				break;
			case ty_class:
				printf (" %s %p\n", type.t.class,
						obj_lookup_class (type.t.class));
				break;
		}
	 }
}

void
test_xdefs (void)
{
	pr_xdefs_t *xdefs;
	int         i;

	xdefs = PR_FindGlobal (".xdefs");
	if (!xdefs) {
		printf ("Can't find xdefs\n");
		return;
	}
	printf ("%p %i\n", xdefs.xdefs, xdefs.num_xdefs);
	for (i = 0; i < xdefs.num_xdefs; i++) {
		printf ("%p %p\n", xdefs.xdefs[i].type, xdefs.xdefs[i].offset);
	}
}
