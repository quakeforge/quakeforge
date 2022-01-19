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

//FIXME use pr_type_names.h, but need to fix unsigned, and add missing types
#define field .int
#define func void()(void)
#define ptr void *
#define uint unsigned
#define ulong unsigned long
#define ushort unsigned short
#define EV_TYPE(type) sizeof(type),
int pr_type_size[ev_type_count] = {
#include <QF/progs/pr_type_names.h>
};

#define EV_TYPE(type) #type,
string pr_type_name[ev_type_count] = {
#include <QF/progs/pr_type_names.h>
	"invalid",
};
