#ifndef __qwaq_debugger_funcview_h
#define __qwaq_debugger_funcview_h

#include "ruamoko/qwaq/debugger/views/defview.h"

@interface FuncView : DefView
{
	unsigned   *data;
}
+(FuncView *)withDef:(qdb_def_t)def in:(void *)data type:(qfot_type_t *)type;
@end

#endif//__qwaq_debugger_funcview_h
