#include <runtime.h>
#include <hash.h>
#include <string.h>

#include "ruamoko/qwaq/ui/curses.h"	// printf FIXME
#include "ruamoko/qwaq/debugger/typeencodings.h"

@implementation TypeEncodings

// these are encodings that we already have so don't need to copy them from
// the target
static hashtab_t *static_encodings;
// these are condings that had to be copied from the target
static hashtab_t *dynamic_encodings;

static qfot_type_t *
next_type (qfot_type_t *type)
{
    int         size = type.size;

	if (!size) {	// null type
		size = TYPESIZE;
	}
	return (qfot_type_t *) ((int *) type + size);
}

static string
type_get_key (void *t, void *unusded)
{
	qfot_type_t *type = (qfot_type_t *) t;
	return type.encoding;
}

static void type_free (void *t, void *unused)
{
	qfot_type_t *type = (qfot_type_t *) t;
	str_free (type.encoding);
	switch (type.meta) {
		case ty_basic:
		case ty_array:
			break;
		case ty_struct:
		case ty_union:
		case ty_enum:
			str_free (type.strct.tag);
			for (int i = 0; i < type.strct.num_fields; i++) {
				str_free (type.strct.fields[i].name);
			}
			break;
		case ty_class:
			str_free (type.class);
			break;
		case ty_alias:
			str_free (type.alias.name);
			break;
	}
	obj_free (t);
}

+initialize
{
	qfot_type_encodings_t *encodings;
	qfot_type_t *type;

	static_encodings = Hash_NewTable (1023, type_get_key, nil, nil);
	dynamic_encodings = Hash_NewTable (1023, type_get_key, type_free, nil);

	encodings = PR_FindGlobal (".type_encodings");
	for (type = encodings.types;
		 ((int *)type - (int *) encodings.types) < encodings.size;
		 type = next_type (type)) {
		Hash_Add (static_encodings, type);
	}
	return self;
}

+(qfot_type_t *) getType:(unsigned)typeAddr fromTarget:(qdb_target_t)target
{
	qfot_type_t  buffer = {};
	string       encoding;
	qfot_type_t *type;

	if (qdb_get_data (target, typeAddr, TYPESIZE, &buffer) < 0) {
		return nil;
	}
	if (!buffer.size) {
		return nil;
	}
	encoding = qdb_get_string (target, buffer.encoding);
	if (!encoding) {
		return nil;
	}
	if ((type = Hash_Find (static_encodings, encoding))) {
		return type;
	}
	if ((type = Hash_Find (dynamic_encodings, encoding))) {
		return type;
	}
	type = obj_calloc (1, buffer.size);
	if (qdb_get_data (target, typeAddr, buffer.size, type) < 0) {
		goto error;
	}
	if (!(type.encoding = qdb_get_string (target, type.encoding))) {
		goto error;
	}
	switch (type.meta) {
		case ty_basic:
		case ty_array:
			goto hash_type;
		case ty_struct:
		case ty_union:
		case ty_enum:
			if (!(type.strct.tag = qdb_get_string (target, type.strct.tag))) {
				goto error;
			}
			for (int i = 0; i < type.strct.num_fields; i++) {
				qfot_var_t *var = &type.strct.fields[i];
				if (!(var.name = qdb_get_string (target, var.name))) {
					goto error;
				}
			}
			goto hash_type;
		case ty_class:
			if (!(type.class = qdb_get_string (target, type.class))) {
				goto error;
			}
			goto hash_type;
		case ty_alias:
			if (!(type.alias.name = qdb_get_string (target, type.alias.name))) {
				goto error;
			}
			goto hash_type;
	}
	goto error;
hash_type:
	printf ("fetched type %s\n", type.encoding);
	Hash_Add (dynamic_encodings, type);
	return type;
	// not a valid type
error:
	type_free (type, nil);
	return nil;
}

@end
