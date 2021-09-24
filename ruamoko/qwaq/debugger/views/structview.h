#ifndef __qwaq_debugger_structview_h
#define __qwaq_debugger_structview_h

#include "ruamoko/qwaq/debugger/views/defview.h"

@interface StructView : DefView
{
	unsigned   *data;
	DefView   **field_views;
	int        *field_rows;
}
+(StructView *)withDef:(qdb_def_t)def in:(void *)data type:(qfot_type_t *)type;
@end

#endif//__qwaq_debugger_structview_h
