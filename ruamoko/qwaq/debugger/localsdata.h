#ifndef __qwaq_debugger_localsview_h
#define __qwaq_debugger_localsview_h

#include <types.h>
#include "ruamoko/qwaq/ui/tableview.h"
#include "ruamoko/qwaq/debugger/debug.h"

@interface LocalsData : Object <TableViewDataSource>
{
	qdb_target_t target;
	qfot_type_encodings_t target_encodings;
	unsigned    current_fnum;
	qdb_function_t *func;
	qdb_auxfunction_t *aux_func;
	qdb_def_t  *defs;
	void       *data;
}
+(LocalsData *)withTarget:(qdb_target_t)target;
-setFunction:(unsigned)fnum;
-fetchData;
@end

#endif
