#include <types.h>

#include "test-harness.h"

typedef int int32_t;
int32_t *int32_ptr;

typedef struct xdef_s {
	qfot_type_t *type;			///< pointer to type definition
	void       *ofs;			///< 32-bit version of ddef_t.ofs
} xdef_t;

void *PR_FindGlobal (string name) = #0;

int
main (void)
{
	//FIXME need a simple way to get at a def's meta-data
	xdef_t     *xdefs = PR_FindGlobal (".xdefs");
	while (xdefs.ofs != &int32_ptr) {
		xdefs++;
	}
	printf ("int32_ptr: %s\n", xdefs.type.encoding);
	return xdefs.type.encoding != "{>^i}";
}
