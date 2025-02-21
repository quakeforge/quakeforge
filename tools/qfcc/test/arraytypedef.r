#pragma bug die
typedef int int32_t;
typedef int uint32_t;

int32_t foo[4];
uint32_t bar[4];

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

typedef struct xdef_s {
	qfot_type_t *type;			///< pointer to type definition
	void       *addr;			///< 32-bit version of ddef_t.ofs
} xdef_t;

typedef struct xdefs_s {
	xdef_t     *xdefs;
	unsigned    num_xdefs;
} xdefs_t;

xdef_t *
find_xdef (string varname)
{
	void       *varaddr = PR_FindGlobal (varname);
	if (!varname) {
		return nil;
	}
	//FIXME need a simple way to get at a def's meta-data
	xdefs_t    *xdefs = PR_FindGlobal (".xdefs");
	xdef_t     *xdef = xdefs.xdefs;
	while (xdef - xdefs.xdefs < (int) xdefs.num_xdefs && xdef.addr != varaddr) {
		xdef++;
	}
	if (xdef.addr != varaddr) {
		return nil;
	}
	return xdef;
}

int
check_alias (string name, qfot_type_t *alias)
{
	if (alias.meta != ty_basic || alias.type != ev_int) {
		printf ("%s is not an int alias\n", name);
		return 0;
	}
	return 1;
}

int
main (void)
{
	qfot_type_t *type;

	encodings = PR_FindGlobal (".type_encodings");

	xdef_t     *foo_var = find_xdef ("foo");
	xdef_t     *bar_var = find_xdef ("bar");
	if (!(foo_var && bar_var)) {
		printf ("missing var: foo: %d bar:%d\n", foo_var, bar_var);
		return 1;
	}
	printf ("typedef: foo: x:%p t:%p a:%p bar: x:%p t:%p a:%p\n",
			foo_var, foo_var.type, foo_var.addr,
			bar_var, bar_var.type, bar_var.addr);
	if (foo_var.type == bar_var.type) {
		printf ("foo and bar have the same full type: %p %p\n",
				foo_var.type, bar_var.type);
		return 1;
	}
#if 0
	int         found_foo = 0;
	int         found_bar = 0;
	for (type = encodings.types;
		 ((int *)type - (int *) encodings.types) < encodings.size;
		type = next_type (type)) {
		if (type.meta == ty_alias) {
			if (type.alias.name == "int32_t") {
				found_foo = check_alias (type.alias.name,
										 type.alias.aux_type);
			}
			if (type.alias.name == "uint32_t") {
				found_bar = check_alias (type.alias.name,
										 type.alias.aux_type);
			}
		}
	}
	if (!(found_bar && found_foo)) {
		printf ("missing typedef: int32_t: %d uint32_t: %d\n", found_foo, found_bar);
		return 1;
	}
#endif
	return 0;
}
