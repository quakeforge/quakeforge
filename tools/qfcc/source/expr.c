#include <stdlib.h>

#include "qfcc.h"
#include "expr.h"

expr_t *
new_expr ()
{
	return calloc (1, sizeof (expr_t));
}
