#ifndef __qwaq_debugger_uintview_h
#define __qwaq_debugger_uintview_h

#include "ruamoko/qwaq/debugger/views/defview.h"

@interface UIntView : DefView
{
	unsigned   *data;
}
+(UIntView *)withDef:(qdb_def_t)def in:(void *)data type:(qfot_type_t *)type;
@end

#endif//__qwaq_debugger_uintview_h
