#include <runtime.h>
#include <types.h>

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
