#ifndef __qwaq_debugger_intview_h
#define __qwaq_debugger_intview_h

#include "ruamoko/qwaq/debugger/views/defview.h"

@interface BoolView : DefView
{
	void       *data;	// could be int or long
}
+(BoolView *)withDef:(qdb_def_t)def in:(void *)data type:(qfot_type_t *)type target:(qdb_target_t)target;
@end

#endif//__qwaq_debugger_intview_h
