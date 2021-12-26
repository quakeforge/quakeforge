#ifndef __qwaq_debugger_arrayructview_h
#define __qwaq_debugger_arrayructview_h

#include "ruamoko/qwaq/debugger/views/defview.h"

@interface ArrayView : DefView
{
	unsigned   *data;
	DefView   **element_views;
	int        *element_rows;
}
+(ArrayView *)withDef:(qdb_def_t)def in:(void *)data type:(qfot_type_t *)type;
@end

#endif//__qwaq_debugger_arrayructview_h
