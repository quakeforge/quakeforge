#ifndef __qwaq_debugger_defview_h
#define __qwaq_debugger_defview_h

#include <types.h>
#include "ruamoko/qwaq/ui/view.h"
#include "ruamoko/qwaq/debugger/debug.h"

@class TableViewColumn;

@interface DefView : View
{
	qdb_def_t   def;
	qfot_type_t *type;
	qdb_target_t target;
}
+(DefView *)withDef:(qdb_def_t)def in:(void *)data type:(qfot_type_t *)type;
+(DefView *)withDef:(qdb_def_t)def in:(void *)data target:(qdb_target_t)target;
+(DefView *)withDef:(qdb_def_t)def type:(qfot_type_t *)type in:(void *)data target:(qdb_target_t)target;
-initWithDef:(qdb_def_t)def type:(qfot_type_t *)type;
-setTarget:(qdb_target_t)target;
-fetchData;
-(int) rows;
-(View *) viewAtRow:(int) row forColumn:(TableViewColumn *)column;
@end

#endif//__qwaq_debugger_defview_h
