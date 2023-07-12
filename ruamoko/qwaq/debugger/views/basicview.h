#ifndef __qwaq_debugger_basicview_h
#define __qwaq_debugger_basicview_h

#include "ruamoko/qwaq/debugger/views/defview.h"

@interface BasicView : DefView
+(DefView *)withDef:(qdb_def_t)def in:(void *)data type:(qfot_type_t *)type;
@end

#endif//__qwaq_debugger_basicview_h
