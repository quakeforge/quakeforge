#ifndef __qwaq_debugger_quatview_h
#define __qwaq_debugger_quatview_h

#include "ruamoko/qwaq/debugger/views/defview.h"

@interface QuatView : DefView
{
	quaternion *data;
}
+(QuatView *)withDef:(qdb_def_t)def type:(qfot_type_t *)type in:(void *)data target:(qdb_target_t)target;
@end

#endif//__qwaq_debugger_quatview_h
