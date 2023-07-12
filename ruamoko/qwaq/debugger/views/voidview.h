#ifndef __qwaq_debugger_voidview_h
#define __qwaq_debugger_voidview_h

#include "ruamoko/qwaq/debugger/views/defview.h"

@interface VoidView : DefView
{
	unsigned   *data;
}
+(VoidView *)withDef:(qdb_def_t)def in:(void *)data type:(qfot_type_t *)type;
@end

#endif//__qwaq_debugger_voidview_h
