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
