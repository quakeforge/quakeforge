#include <runtime.h>
#include <types.h>

string ty_meta_name[7] = {
	"basic",
	"struct",
	"union",
	"enum",
	"array",
	"class",
	"alias",
};

//FIXME use pr_type_names.h
int pr_type_size[ev_type_count] = {
	1,			// ev_void
	1,			// ev_string
	1,			// ev_float
	3,			// ev_vector
	1,			// ev_entity
	1,			// ev_field
	1,			// ev_func
	1,			// ev_pointer
	4,			// ev_quat
	1,			// ev_integer
	1,			// ev_uinteger
	0,			// ev_short        value in opcode
	2,			// ev_double
	2,			// ev_long
	2,			// ev_ulong
	0,			// ev_invalid      not a valid/simple type
};

#define EV_TYPE(type) #type,
string pr_type_name[ev_type_count] = {
#include <QF/progs/pr_type_names.h>
	"invalid",
};
