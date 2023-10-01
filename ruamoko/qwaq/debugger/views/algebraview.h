#ifndef __qwaq_debugger_algebraview_h
#define __qwaq_debugger_algebraview_h

#include "ruamoko/qwaq/debugger/views/basicview.h"

@interface AlgebraView : BasicView
+(DefView *)withDef:(qdb_def_t)def in:(void *)data type:(qfot_type_t *)type;
@end

#endif//__qwaq_debugger_algebraview_h
