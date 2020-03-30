#ifndef __qwaq_debugger_localsview_h
#define __qwaq_debugger_localsview_h

#include <types.h>
#include "ui/view.h"
#include "debugger/debug.h"

@interface LocalsView : View
{
	qdb_target_t target;
	qfot_type_encodings_t target_encodings;
	unsigned    current_fnum;
	qdb_function_t *func;
	qdb_auxfunction_t *aux_func;
	qdb_def_t  *defs;
	void       *data;
}
-initWithRect:(Rect)rect target:(qdb_target_t) target;
-setFunction:(unsigned) fnum;
@end

#endif
