#include <QF/hash.h>

#include "qfcc.h"
#include "struct.h"

typedef struct {
	const char *name;
	type_t     *type;
} struct_t;

static hashtab_t *structs;

static const char *
structs_get_key (void *s, void *unused)
{
	return ((struct_t *) s)->name;
}

static const char *
struct_field_get_key (void *f, void *unused)
{
	return ((struct_field_t *) f)->name;
}

struct_field_t *
new_struct_field (type_t *strct, type_t *type, const char *name)
{
	struct_field_t *field;

	if (!strct)
		return 0;
	field = malloc (sizeof (struct_field_t));
	field->name = name;
	field->type = type;
	field->offset = strct->num_parms;
	strct->num_parms += pr_type_size[type->type];
	field->next = 0;
	*strct->struct_tail = field;
	strct->struct_tail = &field->next;
	Hash_Add (strct->struct_fields, field);
	return field;
}

struct_field_t *
struct_find_field (type_t *strct, const char *name)
{
	if (!structs)
		return 0;
	return Hash_Find (strct->struct_fields, name);
}

type_t *
new_struct (const char *name)
{
	struct_t   *strct;

	if (!structs) {
		structs = Hash_NewTable (16381, structs_get_key, 0, 0);
	}
	strct = (struct_t *) Hash_Find (structs, name);
	if (strct) {
		error (0, "duplicate struct definition");
		return 0;
	}
	strct = malloc (sizeof (struct_t));
	strct->name = name;
	strct->type = calloc (1, sizeof (type_t));
	strct->type->type = ev_struct;
	strct->type->struct_tail = &strct->type->struct_head;
	strct->type->struct_fields = Hash_NewTable (61, struct_field_get_key, 0, 0);
	Hash_Add (structs, strct);
	return strct->type;
}

type_t *
find_struct (const char *name)
{
	struct_t   *strct;

	if (!structs)
		return 0;
	strct = (struct_t *) Hash_Find (structs, name);
	if (strct)
		return strct->type;
	return 0;
}
