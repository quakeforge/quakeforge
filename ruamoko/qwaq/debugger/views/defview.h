#ifndef __qwaq_debugger_defview_h
#define __qwaq_debugger_defview_h

#include <types.h>
#include "ruamoko/qwaq/ui/tableview.h"
#include "ruamoko/qwaq/debugger/debug.h"

@interface DefView : Object <TableCell>
{
	qdb_def_t   def;
	qfot_type_t *type;
	qdb_target_t target;
}
+(DefView *)withDef:(qdb_def_t)def in:(void *)data target:(qdb_target_t)target;
+(DefView *)withDef:(qdb_def_t)def type:(qfot_type_t *)type in:(void *)data target:(qdb_target_t)target;
-initWithDef:(qdb_def_t)def type:(qfot_type_t *)type target:(qdb_target_t)target;
-fetchData;
-fetchData:(uint)ptr;
-(int) rows;
-(DefView *) cellAtRow:(int) row forColumn:(TableColumn *)column level:(int)level;
@end

#endif//__qwaq_debugger_defview_h
