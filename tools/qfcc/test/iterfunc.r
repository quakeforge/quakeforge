#pragma bug die
#include <types.h>

// can't link against libr.a (may not be built)
void *PR_FindGlobal (string name) = #0;
void printf (string fmt, ...) = #0;

qfot_type_encodings_t *encodings;
qfot_type_t *next_type (qfot_type_t *type);

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
	}
	return 0;
}

qfot_type_t *
next_type (qfot_type_t *type)
{
	int         size = type.size;
	if (!size)
		size = 4;
	return (qfot_type_t *) ((int *) type + size);
}
