int func(string x) { return 1; }
int (*funcptr)(string);
int *(*ptrfuncptr)(string);
int array[7];
int *ptrarray[7];
int (*arrayptr)[7];
int *(*ptrarrayptr)[7];

#include <types.h>

#include "test-harness.h"

typedef struct xdef_s {
	qfot_type_t *type;			///< pointer to type definition
	void       *ofs;			///< 32-bit version of ddef_t.ofs
} xdef_t;

typedef struct xdefs_s {
	xdef_t     *xdefs;
	unsigned    num_xdefs;
} xdefs_t;

void *PR_FindGlobal (string name) = #0;

xdef_t *
find_xdef (void *varaddr)
{
	//FIXME need a simple way to get at a def's meta-data
	xdefs_t    *xdefs = PR_FindGlobal (".xdefs");
	xdef_t     *xdef = xdefs.xdefs;
	while (xdef - xdefs.xdefs < xdefs.num_xdefs && xdef.ofs != varaddr) {
		xdef++;
	}
	if (xdef.ofs != varaddr) {
		return nil;
	}
	return xdef;
}

int
check_var (string name, string encoding)
{
	void       *var = PR_FindGlobal (name);
	if (!var) {
		printf ("could not find var: %s\n", name);
		return 0;
	}
	xdef_t     *xdef = find_xdef (var);
	if (!var) {
		printf ("could not find xdef var: %s @ %p\n", name, var);
		return 0;
	}
	int         ok = xdef.type.encoding == encoding;
	printf ("%s: %p %s %s %s\n", name, var, xdef.type.encoding, encoding,
			ok ? "pass" : "fail");
	return ok;
}

int
main (void)
{
	int         ok = 1;
	ok &= check_var ("PR_FindGlobal", "(^v*)");
	ok &= check_var ("func", "(i*)");
	ok &= check_var ("funcptr", "^(i*)");
	ok &= check_var ("ptrfuncptr", "^(^i*)");
	ok &= check_var ("array", "[7=i]");
	ok &= check_var ("ptrarray", "[7=^i]");
	ok &= check_var ("arrayptr", "^[7=i]");
	ok &= check_var ("ptrarrayptr", "^[7=^i]");
	return 1 & (!ok);
}
