#ifndef __qwaq_debugger_pointerview_h
#define __qwaq_debugger_pointerview_h

#include "ruamoko/qwaq/debugger/views/defview.h"

@interface PointerView : DefView
{
	unsigned   *data;
}
+(PointerView *)withDef:(qdb_def_t)def in:(void *)data type:(qfot_type_t *)type;
@end

#endif//__qwaq_debugger_pointerview_h
