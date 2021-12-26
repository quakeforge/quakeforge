#ifndef __qwaq_debugger_quatview_h
#define __qwaq_debugger_quatview_h

#include "ruamoko/qwaq/debugger/views/defview.h"

@interface QuatView : DefView
{
	quaternion *data;
}
+(QuatView *)withDef:(qdb_def_t)def in:(void *)data type:(qfot_type_t *)type;
@end

#endif//__qwaq_debugger_quatview_h
