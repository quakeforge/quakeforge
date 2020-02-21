#include <types.h>

// can't link against libr.a (may not be built)
void *PR_FindGlobal (string name) = #0;
void printf (string fmt, ...) = #0;

qfot_type_encodings_t *encodings;

typedef int *foo;
typedef int *bar;

foo baz;
bar snafu;

qfot_type_t *
next_type (qfot_type_t *type)
{
	int         size = type.size;
	if (!size)
		size = 4;
	return (qfot_type_t *) ((int *) type + size);
}

int
check_alias (qfot_type_t *alias)
{
	if (alias.meta != ty_basic && alias.t.type != ev_pointer) {
		printf ("%s is not a *int alias\n", alias.t.alias.name);
		return 0;
	}
	return 1;
}

int
main (void)
{
	baz = snafu;

	int         found_foo = 0;
	int         found_bar = 0;
	qfot_type_t *type;

	encodings = PR_FindGlobal (".type_encodings");

	for (type = encodings.types;
		 ((int *)type - (int *) encodings.types) < encodings.size;
		type = next_type (type)) {
		if (type.meta == ty_alias) {
			if (type.t.alias.name == "foo") {
				found_foo = check_alias (type.t.alias.aux_type);
			}
			if (type.t.alias.name == "bar") {
				found_bar = check_alias (type.t.alias.aux_type);
			}
		}
	}
	if (!(found_bar && found_foo)) {
		printf ("missing typedef: foo: %d bar:%d\n", found_foo, found_bar);
		return 1;
	}
	return 0;
}
