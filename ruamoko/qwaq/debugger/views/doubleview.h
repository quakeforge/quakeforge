#ifndef __qwaq_debugger_doubleview_h
#define __qwaq_debugger_doubleview_h

#include "ruamoko/qwaq/debugger/views/defview.h"

@interface DoubleView : DefView
{
	double     *data;
}
+(DoubleView *)withDef:(qdb_def_t)def type:(qfot_type_t *)type in:(void *)data target:(qdb_target_t)target;
@end

#endif//__qwaq_debugger_doubleview_h
