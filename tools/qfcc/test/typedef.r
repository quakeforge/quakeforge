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
main (void)
{
	int         found_foo = 0;
	int         found_bar = 0;
	qfot_type_t *type;
	qfot_type_t *alias;

	baz = snafu;	// must be able to assign without warnings (won't compile)

	encodings = PR_FindGlobal (".type_encodings");

	for (type = encodings.types;
		 ((int *)type - (int *) encodings.types) < encodings.size;
		type = next_type (type)) {
		if (type.meta == ty_alias && type.t.alias.type == ev_integer) {
			alias = type.t.alias.aux_type;
			if (type.t.alias.name == "foo") {
				if (alias.meta == ty_none && alias.t.type == ev_integer) {
					found_foo = 1;
				} else {
					printf ("foo type not aliased to int\n");
				}
			}
			if (type.t.alias.name == "bar") {
				if (alias.meta == ty_none && alias.t.type == ev_integer) {
					found_bar = 1;
				} else {
					printf ("bar type not aliased to int\n");
				}
			}
		}
	}
	if (!(found_bar && found_foo)) {
		printf ("missing typedef: foo: %d bar:%d\n", found_foo, found_bar);
		return 1;
	}
	return 0;
}
