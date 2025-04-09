#ifndef __qwaq_debugger_handleview_h
#define __qwaq_debugger_handleview_h

#include "ruamoko/qwaq/debugger/views/defview.h"

@interface HandleView : DefView
{
	int        *data;
}
+(DefView *)withDef:(qdb_def_t)def in:(void *)data type:(qfot_type_t *)type;
@end

#endif//__qwaq_debugger_handleview_h
