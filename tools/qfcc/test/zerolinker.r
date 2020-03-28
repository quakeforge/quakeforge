#include <types.h>

// can't link against libr.a (may not be built)
void *PR_FindGlobal (string name) = #0;
void printf (string fmt, ...) = #0;

qfot_type_encodings_t *encodings;

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
	int         found_param = 0;
	int         found_zero = 0;
	qfot_type_t *type;

	encodings = PR_FindGlobal (".type_encodings");

	for (type = encodings.types;
		 ((int *)type - (int *) encodings.types) < encodings.size;
		type = next_type (type)) {
		if (type.meta == ty_struct) {
			if (type.t.strct.tag == "tag @param") {
				found_param = 1;
			}
			if (type.t.strct.tag == "tag @zero") {
				found_zero = 1;
			}
		}
	}
	if (!(found_param && found_zero)) {
		printf ("missing struct: param: %d zero:%d\n",
				found_param, found_zero);
		return 1;
	}
	return 0;
}
