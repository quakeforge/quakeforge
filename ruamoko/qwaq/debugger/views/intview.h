#ifndef __qwaq_debugger_intview_h
#define __qwaq_debugger_intview_h

#include "ruamoko/qwaq/debugger/views/defview.h"

@interface IntView : DefView
{
	int        *data;
}
+(IntView *)withDef:(qdb_def_t)def in:(void *)data type:(qfot_type_t *)type;
@end

#endif//__qwaq_debugger_intview_h
