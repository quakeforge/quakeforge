#ifndef __qwaq_debugger_uintview_h
#define __qwaq_debugger_uintview_h

#include "ruamoko/qwaq/debugger/views/defview.h"

@interface UIntView : DefView
{
	unsigned   *data;
}
+(UIntView *)withDef:(qdb_def_t)def type:(qfot_type_t *)type in:(void *)data target:(qdb_target_t)target;
@end

#endif//__qwaq_debugger_uintview_h
