#pragma bug die
#include <types.h>

#include "test-harness.h"

typedef int int32_t;
int32_t *int32_ptr;

typedef struct xdef_s {
	qfot_type_t *type;			///< pointer to type definition
	void       *ofs;			///< 32-bit version of ddef_t.ofs
} xdef_t;

typedef struct xdefs_s {
	xdef_t     *xdefs;
	unsigned    num_xdefs;
} xdefs_t;

void *PR_FindGlobal (string name) = #0;

int
main (void)
{
	//FIXME need a simple way to get at a def's meta-data
	xdefs_t    *xdefs = PR_FindGlobal (".xdefs");
	xdef_t     *xdef = xdefs.xdefs;
	while (xdef - xdefs.xdefs < (int) xdefs.num_xdefs
		   && xdef.ofs != &int32_ptr) {
		xdef++;
	}
	printf ("int32_ptr: %s\n", xdef.type.encoding);
	return xdef.type.encoding != "{>^{int32_t>i}}";
}
