#ifndef __qwaq_debugger_stringview_h
#define __qwaq_debugger_stringview_h

#include "ruamoko/qwaq/debugger/views/defview.h"

@interface StringView : DefView
{
	int        *data;
}
+(StringView *)withDef:(qdb_def_t)def type:(qfot_type_t *)type in:(void *)data target:(qdb_target_t)target;
@end

#endif//__qwaq_debugger_stringview_h
