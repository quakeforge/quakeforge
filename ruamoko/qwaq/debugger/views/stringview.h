#ifndef __qwaq_debugger_stringview_h
#define __qwaq_debugger_stringview_h

#include "ruamoko/qwaq/debugger/views/defview.h"

@interface StringView : DefView
{
	int        *data;
}
+(StringView *)withDef:(qdb_def_t)def in:(void *)data type:(qfot_type_t *)type;
@end

#endif//__qwaq_debugger_stringview_h
